"""Compare two dumps of the same MonsterHunterWorld.exe build.

    py -3.12 -B tools/re/diff_dumps.py <our image.exe> <ghidra memory dir> <out.json>

<ghidra memory dir> holds <block>_<rva>.bin files from
tools/re/ghidra/ExportMemory.java. For each block the script counts the
differing bytes; for executable blocks it lists every differing range
(gaps under 16 bytes merged) and classifies each side's bytes:
"jmp-out" (E9 rel32 or FF 25 leaving the image), "movabs-jmp" (48 B8 imm64
FF E0), "int3" (CC) or "code". Two independent dumps that agree are taken
as original; where they differ, the side with a hook pattern is the dirty
one.
"""

import json
import os
import re
import struct
import sys

import pefile


def classify(buf, rva, image_size):
    if len(buf) >= 5 and buf[0] == 0xE9:
        target = rva + 5 + struct.unpack_from("<i", buf, 1)[0]
        return "jmp-out" if not 0 <= target < image_size else "jmp-in"
    if len(buf) >= 6 and buf[0] == 0xFF and buf[1] == 0x25:
        return "jmp-out"
    if len(buf) >= 12 and buf[0] == 0x48 and buf[1] == 0xB8 and buf[10] == 0xFF and buf[11] == 0xE0:
        return "movabs-jmp"
    if buf and buf[0] == 0xCC:
        return "int3"
    return "code"


def ranges(a, b, merge_gap=16):
    out = []
    i, n = 0, min(len(a), len(b))
    while i < n:
        if a[i] != b[i]:
            start = i
            end = i + 1
            j = end
            while j < n and j - end < merge_gap:
                if a[j] != b[j]:
                    end = j + 1
                j += 1
            out.append((start, end))
            i = end
        else:
            i += 1
    return out


def main():
    exe, memdir, out_path = sys.argv[1:4]
    pe = pefile.PE(exe, fast_load=True)
    img = pe.get_memory_mapped_image()
    image_size = pe.OPTIONAL_HEADER.SizeOfImage
    exec_rvas = {s.VirtualAddress for s in pe.sections if s.Characteristics & 0x20000000}
    report = {"image": exe, "blocks": []}
    for name in sorted(os.listdir(memdir)):
        m = re.fullmatch(r"(.+)_([0-9a-f]+)\.bin", name)
        if not m:
            continue
        rva = int(m.group(2), 16)
        theirs = open(os.path.join(memdir, name), "rb").read()
        ours = img[rva:rva + len(theirs)]
        diff_bytes = sum(1 for x, y in zip(ours, theirs) if x != y)
        block = {"block": m.group(1), "rva": hex(rva), "size": hex(len(theirs)),
                 "compared": len(ours), "diff_bytes": diff_bytes}
        if rva in exec_rvas:
            rs = []
            for s, e in ranges(ours, theirs):
                r = rva + s
                rs.append({
                    "rva": hex(r), "len": e - s,
                    "ours": ours[s:min(e, s + 16)].hex(), "theirs": theirs[s:min(e, s + 16)].hex(),
                    "ours_kind": classify(ours[s:s + 16], r, image_size),
                    "theirs_kind": classify(theirs[s:s + 16], r, image_size),
                })
            block["ranges"] = rs
        report["blocks"].append(block)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    for b in report["blocks"]:
        line = "%-10s rva %-9s size %-9s diff %d" % (b["block"], b["rva"], b["size"], b["diff_bytes"])
        if "ranges" in b:
            kinds = {}
            for r in b["ranges"]:
                k = r["ours_kind"] + " / " + r["theirs_kind"]
                kinds[k] = kinds.get(k, 0) + 1
            line += "  ranges %d  (ours / theirs: %s)" % (len(b["ranges"]), kinds)
        print(line)


if __name__ == "__main__":
    main()
