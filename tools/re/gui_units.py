"""Read-only snapshot of the game's GUI unit list (sMhGUI's MtArray at +0x260).

Opens MonsterHunterWorld.exe with PROCESS_QUERY_LIMITED_INFORMATION |
PROCESS_VM_READ only (as Astera Loader's tools/modwatch.py does): no writes,
no threads, no calls into the game, no debugger. Lists every registered GUI
unit by slot with its class (from the vtable: names exported from CMFM's IDA
database to local/ida/vft_names.json; a vtable in another module is shown as
that module).

Why: tools that index this list by slot (IN-Q's Cheat Engine table takes
slot 46 as the item box's Item Loadouts) break when a mod's unit sits in it
(docs/research/native-menu.md, "The game's GUI unit list").

    py -3.12 -B tools/re/gui_units.py --out local/runs/<run>/<label>.json
    py -3.12 -B tools/re/gui_units.py --diff a.json b.json

Exe 421810 only (sMhGUI::Instance at RVA 0x51C4640).
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import datetime
import json
import struct
import sys
from pathlib import Path

EXE = "monsterhunterworld.exe"
RVA_SMHGUI = 0x51C4640
UNITS_COUNT = 0x268  # MtArray at +0x260: length +8, capacity +0xC, data +0x18
UNITS_DATA = 0x278
WATCH = ("uGUICommonSaveLoad", "uGUIHudRadar", "uGUIBoxItemSell", "uGUIBoxItemMySet")
INQ_SLOTS = (4, 46)  # IN-Q's assumptions: SaveLoad, Item Loadouts

PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
PROCESS_VM_READ = 0x0010
TH32CS_SNAPPROCESS = 0x2

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
ntdll = ctypes.WinDLL("ntdll")


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ProcessID", wt.DWORD),
                ("th32DefaultHeapID", ctypes.c_size_t), ("th32ModuleID", wt.DWORD), ("cntThreads", wt.DWORD),
                ("th32ParentProcessID", wt.DWORD), ("pcPriClassBase", ctypes.c_long), ("dwFlags", wt.DWORD),
                ("szExeFile", wt.WCHAR * 260)]


class PROCESS_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [("Reserved1", ctypes.c_void_p), ("PebBaseAddress", ctypes.c_void_p),
                ("Reserved2", ctypes.c_void_p * 2), ("UniqueProcessId", ctypes.c_void_p),
                ("Reserved3", ctypes.c_void_p)]


kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
kernel32.OpenProcess.restype = wt.HANDLE
kernel32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                       ctypes.POINTER(ctypes.c_size_t)]
ntdll.NtQueryInformationProcess.restype = ctypes.c_long
ntdll.NtQueryInformationProcess.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_void_p, wt.ULONG,
                                            ctypes.POINTER(wt.ULONG)]


def find_pid() -> int | None:
    snap = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    entry = PROCESSENTRY32W()
    entry.dwSize = ctypes.sizeof(entry)
    pids = []
    ok = kernel32.Process32FirstW(snap, ctypes.byref(entry))
    while ok:
        if entry.szExeFile.lower() == EXE:
            pids.append(entry.th32ProcessID)
        ok = kernel32.Process32NextW(snap, ctypes.byref(entry))
    kernel32.CloseHandle(snap)
    return pids[0] if len(pids) == 1 else None


class Target:
    def __init__(self, pid: int):
        self.handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, False, pid)
        if not self.handle:
            raise OSError("OpenProcess failed: %d" % ctypes.get_last_error())

    def read(self, address: int, size: int) -> bytes | None:
        buf = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t(0)
        if not kernel32.ReadProcessMemory(self.handle, ctypes.c_void_p(address), buf, size, ctypes.byref(got)):
            return None
        return buf.raw[:got.value] if got.value == size else None

    def u32(self, address: int) -> int | None:
        b = self.read(address, 4)
        return struct.unpack("<I", b)[0] if b else None

    def ptr(self, address: int) -> int | None:
        b = self.read(address, 8)
        return struct.unpack("<Q", b)[0] if b else None

    def modules(self) -> list[tuple[int, int, str]]:
        pbi = PROCESS_BASIC_INFORMATION()
        if ntdll.NtQueryInformationProcess(self.handle, 0, ctypes.byref(pbi), ctypes.sizeof(pbi), None) != 0:
            return []
        ldr = self.ptr(pbi.PebBaseAddress + 0x18)
        head = ldr + 0x10 if ldr else None
        out = []
        link = self.ptr(head) if head else None
        while link and link != head and len(out) < 4096:
            base = self.ptr(link + 0x30)
            size = self.u32(link + 0x40)
            name_len = struct.unpack("<H", self.read(link + 0x58, 2) or b"\0\0")[0]
            name_ptr = self.ptr(link + 0x60)
            raw = self.read(name_ptr, name_len) if name_ptr and name_len else None
            out.append((base or 0, size or 0, raw.decode("utf-16-le", "replace") if raw else "?"))
            link = self.ptr(link)
        return out


def snapshot(names: dict) -> dict:
    pid = find_pid()
    if pid is None:
        raise SystemExit("MonsterHunterWorld.exe is not running (or more than one is)")
    t = Target(pid)
    mods = t.modules()
    exe = next((m for m in mods if m[2].lower() == EXE), None)
    if exe is None:
        raise SystemExit("the game's module was not found")
    base = exe[0]
    gui = t.ptr(base + RVA_SMHGUI)
    count = t.u32(gui + UNITS_COUNT) if gui else None
    data = t.ptr(gui + UNITS_DATA) if gui else None
    if not gui or count is None or not data or count > 4096:
        raise SystemExit("sMhGUI's unit list is not readable yet (at start-up?)")
    rows = []
    for i in range(count):
        unit = t.ptr(data + 8 * i)
        vt = t.ptr(unit) if unit else None
        where, name = "?", "?"
        if vt is not None:
            mod = next((m for m in mods if m[0] <= vt < m[0] + m[1]), None)
            if mod is not None and mod[0] == base:
                where = "exe+0x%x" % (vt - base)
                name = names.get("%x" % (vt - base), where)
            elif mod is not None:
                where = "%s+0x%x" % (mod[2], vt - mod[0])
                name = "(%s)" % mod[2]
        rows.append({"slot": i, "unit": "0x%x" % (unit or 0), "vtable": where, "class": name})
    return {"time": datetime.datetime.now().isoformat(timespec="seconds"), "pid": pid, "count": count,
            "rows": rows}


def summary(d: dict) -> str:
    rows = d["rows"]
    lines = ["%d GUI units (pid %s, %s)" % (d["count"], d.get("pid"), d.get("time"))]
    foreign = [r for r in rows if r["class"].startswith("(")]
    for r in foreign:
        lines.append("  slot %3d: a unit from another module %s" % (r["slot"], r["vtable"]))
    for w in WATCH:
        slots = [r["slot"] for r in rows if r["class"] == w]
        lines.append("  %-20s slot %s" % (w, ", ".join(map(str, slots)) or "-"))
    for s in INQ_SLOTS:
        if s < len(rows):
            lines.append("  slot %3d (IN-Q's): %s" % (s, rows[s]["class"]))
    return "\n".join(lines)


def diff(a: dict, b: dict) -> str:
    ca = [r["class"] for r in a["rows"]]
    cb = [r["class"] for r in b["rows"]]
    lines = ["a: %d units, b: %d units" % (len(ca), len(cb))]
    first = next((i for i in range(min(len(ca), len(cb))) if ca[i] != cb[i]), None)
    if first is None and len(ca) == len(cb):
        lines.append("identical class by class")
        return "\n".join(lines)
    lines.append("first difference at slot %s: a %s, b %s" % (
        first, ca[first] if first is not None else "-", cb[first] if first is not None else "-"))
    only_b = [c for c in cb if cb.count(c) > ca.count(c)]
    only_a = [c for c in ca if ca.count(c) > cb.count(c)]
    lines.append("only in a: %s" % (sorted(set(only_a)) or "-"))
    lines.append("only in b: %s" % (sorted(set(only_b)) or "-"))
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--out", help="write the snapshot (JSON) here")
    ap.add_argument("--names", default=str(Path(__file__).resolve().parents[2] / "local" / "ida" / "vft_names.json"))
    ap.add_argument("--diff", nargs=2, metavar=("A", "B"), help="compare two snapshots")
    args = ap.parse_args()
    if args.diff:
        a, b = (json.loads(Path(p).read_text(encoding="utf-8")) for p in args.diff)
        print(diff(a, b))
        return 0
    names = json.loads(Path(args.names).read_text(encoding="utf-8")) if Path(args.names).exists() else {}
    d = snapshot(names)
    print(summary(d))
    if args.out:
        Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.out).write_text(json.dumps(d, indent=1), encoding="utf-8")
        print("written:", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
