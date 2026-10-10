"""Copy MonsterHunterWorld.exe's memory image out of the running game, read-only.

    py -3.12 -B tools/re/dump_image.py <out dir> [--pid N] [--expect-sha256 HEX]

Opens the process with PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ
only: no debugger, no writes, no thread suspension, no remote threads or
allocations. Run it once the title screen is up (the code is unpacked by
then), with no mods loaded, to get a hook-free image for offline analysis.

Writes to <out dir>:
  image.bin    the module's memory, SizeOfImage bytes from its base
  image.exe    the same bytes with section headers pointing at the virtual
               layout (PointerToRawData = VirtualAddress), so IDA and pefile
               load it; for analysis only, not runnable
  dump.json    process, exe hash, base, unreadable pages, loaded modules,
               output hashes
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import datetime
import hashlib
import json
import os
import struct
import sys

import pefile

PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
PROCESS_VM_READ = 0x0010
ACCESS = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ
TH32CS_SNAPPROCESS = 0x2
EXE_NAME = "monsterhunterworld.exe"
PAGE = 0x1000

# x64 PEB / loader offsets (as in AL's tools/modwatch.py).
PEB_IMAGE_BASE = 0x10
PEB_LDR = 0x18
LDR_IN_LOAD_ORDER = 0x10
E_DLL_BASE = 0x30
E_SIZE_OF_IMAGE = 0x40
E_FULL_NAME = 0x48

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
ntdll = ctypes.WinDLL("ntdll")


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ProcessID", wt.DWORD),
        ("th32DefaultHeapID", ctypes.c_size_t), ("th32ModuleID", wt.DWORD),
        ("cntThreads", wt.DWORD), ("th32ParentProcessID", wt.DWORD),
        ("pcPriClassBase", ctypes.c_long), ("dwFlags", wt.DWORD),
        ("szExeFile", ctypes.c_wchar * 260),
    ]


class PROCESS_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("ExitStatus", ctypes.c_long), ("PebBaseAddress", ctypes.c_void_p),
        ("AffinityMask", ctypes.c_size_t), ("BasePriority", ctypes.c_long),
        ("UniqueProcessId", ctypes.c_size_t), ("InheritedFromUniqueProcessId", ctypes.c_size_t),
    ]


kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
kernel32.CreateToolhelp32Snapshot.argtypes = [wt.DWORD, wt.DWORD]
kernel32.Process32FirstW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
kernel32.Process32NextW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
kernel32.OpenProcess.restype = wt.HANDLE
kernel32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
kernel32.CloseHandle.argtypes = [wt.HANDLE]
kernel32.ReadProcessMemory.argtypes = [
    wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
kernel32.QueryFullProcessImageNameW.argtypes = [
    wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD)]
ntdll.NtQueryInformationProcess.argtypes = [
    wt.HANDLE, ctypes.c_int, ctypes.c_void_p, wt.ULONG, ctypes.POINTER(wt.ULONG)]


def find_pids():
    snap = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    pids = []
    e = PROCESSENTRY32W()
    e.dwSize = ctypes.sizeof(e)
    ok = kernel32.Process32FirstW(snap, ctypes.byref(e))
    while ok:
        if e.szExeFile.lower() == EXE_NAME:
            pids.append(e.th32ProcessID)
        ok = kernel32.Process32NextW(snap, ctypes.byref(e))
    kernel32.CloseHandle(snap)
    return pids


class Reader:
    def __init__(self, pid):
        self.h = kernel32.OpenProcess(ACCESS, False, pid)
        if not self.h:
            raise OSError(ctypes.get_last_error(), "OpenProcess(%d) failed" % pid)

    def read(self, addr, size):
        buf = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        if not kernel32.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf, size, ctypes.byref(got)):
            return None
        return buf.raw[:got.value] if got.value == size else None

    def q(self, addr):
        b = self.read(addr, 8)
        if b is None:
            raise OSError("read failed at %x" % addr)
        return struct.unpack("<Q", b)[0]

    def ustr(self, addr):
        b = self.read(addr, 16)
        length, _, ptr = struct.unpack("<HH4xQ", b)
        s = self.read(ptr, length) if length else b""
        return s.decode("utf-16-le", "replace") if s else ""

    def image_path(self):
        buf = ctypes.create_unicode_buffer(1024)
        n = wt.DWORD(1024)
        if not kernel32.QueryFullProcessImageNameW(self.h, 0, buf, ctypes.byref(n)):
            raise OSError(ctypes.get_last_error(), "QueryFullProcessImageNameW failed")
        return buf.value

    def peb(self):
        pbi = PROCESS_BASIC_INFORMATION()
        st = ntdll.NtQueryInformationProcess(self.h, 0, ctypes.byref(pbi), ctypes.sizeof(pbi), None)
        if st != 0:
            raise OSError("NtQueryInformationProcess failed: %x" % (st & 0xFFFFFFFF))
        return pbi.PebBaseAddress

    def modules(self, peb):
        head = self.q(peb + PEB_LDR) + LDR_IN_LOAD_ORDER
        out, link = [], self.q(head)
        while link != head and len(out) < 4096:
            out.append({
                "base": hex(self.q(link + E_DLL_BASE)),
                "size": hex(struct.unpack("<I", self.read(link + E_SIZE_OF_IMAGE, 4))[0]),
                "path": self.ustr(link + E_FULL_NAME),
            })
            link = self.q(link)
        return out

    def close(self):
        kernel32.CloseHandle(self.h)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--pid", type=int)
    ap.add_argument("--expect-sha256")
    a = ap.parse_args()

    pids = [a.pid] if a.pid else find_pids()
    if len(pids) != 1:
        sys.exit("expected exactly one %s, found %r" % (EXE_NAME, pids))
    pid = pids[0]
    r = Reader(pid)
    try:
        exe_path = r.image_path()
        exe_sha = sha256_file(exe_path)
        if a.expect_sha256 and exe_sha.lower() != a.expect_sha256.lower():
            sys.exit("exe hash %s does not match the expected %s" % (exe_sha, a.expect_sha256))
        peb = r.peb()
        base = r.q(peb + PEB_IMAGE_BASE)
        hdr = r.read(base, PAGE)
        size = pefile.PE(data=hdr + b"\0" * PAGE, fast_load=True).OPTIONAL_HEADER.SizeOfImage

        image = bytearray(size)
        unreadable = []
        chunk = 0x10000
        for off in range(0, size, chunk):
            n = min(chunk, size - off)
            b = r.read(base + off, n)
            if b is not None:
                image[off:off + n] = b
                continue
            for p in range(off, off + n, PAGE):
                pb = r.read(base + p, PAGE)
                if pb is None:
                    unreadable.append(hex(p))
                else:
                    image[p:p + PAGE] = pb
        modules = r.modules(peb)
    finally:
        r.close()

    os.makedirs(a.out, exist_ok=True)
    raw_path = os.path.join(a.out, "image.bin")
    with open(raw_path, "wb") as f:
        f.write(image)

    pe = pefile.PE(data=bytes(image), fast_load=True)
    secs = sorted(pe.sections, key=lambda s: s.VirtualAddress)
    for i, s in enumerate(secs):
        end = secs[i + 1].VirtualAddress if i + 1 < len(secs) else size
        s.PointerToRawData = s.VirtualAddress
        s.SizeOfRawData = end - s.VirtualAddress
    exe_out = os.path.join(a.out, "image.exe")
    with open(exe_out, "wb") as f:
        f.write(pe.write())

    info = {
        "time": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
        "pid": pid,
        "exe_path": exe_path,
        "exe_sha256": exe_sha,
        "image_base": hex(base),
        "size_of_image": hex(size),
        "unreadable_pages": unreadable,
        "modules": modules,
        "image_bin_sha256": sha256_file(raw_path),
        "image_exe_sha256": sha256_file(exe_out),
        "access": "PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ",
    }
    with open(os.path.join(a.out, "dump.json"), "w", encoding="utf-8") as f:
        json.dump(info, f, indent=1, ensure_ascii=False)
    print("pid %d  base %s  size %s  unreadable pages %d  modules %d" % (
        pid, hex(base), hex(size), len(unreadable), len(modules)))
    print("exe sha256 %s" % exe_sha)
    print("image.exe sha256 %s" % info["image_exe_sha256"])


if __name__ == "__main__":
    main()
