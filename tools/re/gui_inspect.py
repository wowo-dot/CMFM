"""Read MHW's GUI manager state out of the running game, read-only.

    py -3.12 -B tools/re/gui_inspect.py <out dir> --label <name> [--pid N]
    py -3.12 -B tools/re/gui_inspect.py <out dir> --label static --image <clean image.exe>

Opens the game with PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ only
(the Reader from dump_image.py): no debugger, no hooks, no writes. Each run
writes <out dir>/<label>.json and prints a summary:

- the 11 GUI registry groups of sMhGUI (record count, class, layout, flags);
- the live GUI units (class from each unit's getDti, two state words);
- the Start menu's item tables with their label and help texts resolved in
  every loaded message file (category 11 printed);

With --image, only the static tables are read from the image file (no
process), to check the parsing offline.

Addresses are for exe 421810 (c2ebbbd2...); see docs/research/native-menu.md.
"""

import argparse
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dump_image  # noqa: E402

EXPECTED_EXE_SHA256 = "c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea"

# RVAs and offsets (docs/research/native-menu.md).
RVA_SMHGUI_INSTANCE = 0x51C4640      # global holding the sMhGUI object pointer
OFS_GROUPS = 85936                   # 11 x {record table*, u32 count}
GROUP_COUNT = 11
OFS_UNIT_COUNT = 616                 # live units: u32 count
OFS_UNIT_ARRAY = 632                 # live units: pointer array
OFS_UNIT_STATE = 8856                # uGUI state word (0x80000011 tested in update)
OFS_UNIT_FLAGS = 8860                # flags set at creation (0x2000)
VT_SLOT_GETDTI = 4
DTI_NAME = 0x08
DTI_ID = 0x34
MSG_COUNT = 0xD8
MSG_TABLE = 0xF8
# GetGameMessage's category -> rGUIMessage* global, in switch order.
RVA_MESSAGES = [0x5012DC0, 0x5012DC8, 0x5012DD0, 0x5012DD8, 0x5012DE0, 0x5012DE8,
                0x5012DF0, 0x5012DF8, 0x5012E00, 0x5012E08, 0x5012E10, 0x5012E18,
                0x5012E20, 0x5012E28, 0x5012E60, 0x5012E30, 0x5012E38, 0x5012E48,
                0x5012E50, 0x5012E40, 0x5013360, 0x5013358, 0x5013438, 0x5013458]
PRINT_CATEGORY = 11
END_MARKER = 76
RVA_START_MENU_MAIN = 0x3FC0BD0      # 5 groups ended by END_MARKER
START_MENU_SUBTABLES = {             # submenu tables (owner item, where known)
    0x3FC0EB0: None, 0x3FC0EE0: None, 0x3FC0F10: None, 0x3FC0F80: None,
    0x3FC0FB0: None, 0x3FC0FF0: None, 0x3FC1020: None, 0x3FC1050: 57, 0x3FC10B0: 63,
}


class ImageMem:
    """Static reads from the clean image file (virtual layout)."""

    def __init__(self, path):
        import pefile
        self.img = pefile.PE(path, fast_load=True).get_memory_mapped_image()
        self.base = 0x140000000

    def read(self, addr, size):
        o = addr - self.base
        if o < 0 or o + size > len(self.img):
            return None
        return self.img[o:o + size]


def u32(mem, a):
    b = mem.read(a, 4)
    return None if b is None else struct.unpack("<I", b)[0]


def u64(mem, a):
    b = mem.read(a, 8)
    return None if b is None else struct.unpack("<Q", b)[0]


def cstr(mem, a, limit=512):
    if not a:
        return None
    out = b""
    while len(out) < limit:
        b = mem.read(a + len(out), 64)
        if b is None:
            break
        i = b.find(b"\0")
        if i >= 0:
            out += b[:i]
            return out.decode("utf-8", "replace")
        out += b
    return out.decode("utf-8", "replace")


def dti_of_vtable(mem, vt):
    """getDti is `lea rax, [rip+X]; ret`: decode X from the function bytes."""
    f = u64(mem, vt + 8 * VT_SLOT_GETDTI)
    b = mem.read(f, 8) if f else None
    if not b or b[0:3] != b"\x48\x8d\x05" or b[7] != 0xC3:
        return None
    return f + 7 + struct.unpack_from("<i", b, 3)[0]


def read_table(mem, base, rva, groups_wanted):
    entries, groups, a = [], 0, base + rva
    while groups < groups_wanted and len(entries) < 256:
        e = struct.unpack("<4i", mem.read(a, 16))
        entries.append(e)
        if e[0] == END_MARKER:
            groups += 1
        a += 16
    return entries


def messages(mem, base):
    cats = []
    for rva in RVA_MESSAGES:
        p = u64(mem, base + rva)
        if not p:
            cats.append(None)
            continue
        cats.append({"ptr": p, "count": u32(mem, p + MSG_COUNT), "table": u64(mem, p + MSG_TABLE)})
    return cats


def msg(mem, cat, i):
    if not cat or i < 0 or cat["count"] is None or i >= cat["count"]:
        return None
    return cstr(mem, u64(mem, cat["table"] + 8 * i))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--label", required=True)
    ap.add_argument("--pid", type=int)
    ap.add_argument("--image")
    a = ap.parse_args()

    report = {"label": a.label}
    if a.image:
        mem, base = ImageMem(a.image), 0x140000000
        report["source"] = a.image
    else:
        pids = [a.pid] if a.pid else dump_image.find_pids()
        if len(pids) != 1:
            sys.exit("expected exactly one game process, found %r" % pids)
        mem = dump_image.Reader(pids[0])
        sha = dump_image.sha256_file(mem.image_path())
        if sha != EXPECTED_EXE_SHA256:
            sys.exit("exe hash %s is not the tested 421810 build" % sha)
        base = mem.q(mem.peb() + dump_image.PEB_IMAGE_BASE)
        report.update(source="pid %d" % pids[0], exe_sha256=sha)

    # Start menu tables (static data, same in the image and in memory).
    tables = {"main": read_table(mem, base, RVA_START_MENU_MAIN, 5)}
    for rva, owner in START_MENU_SUBTABLES.items():
        tables["sub_%x" % rva] = read_table(mem, base, rva, 1)

    cats = None
    if not a.image:
        gui = u64(mem, base + RVA_SMHGUI_INSTANCE)
        report["smhgui"] = hex(gui) if gui else None
        if gui:
            groups = []
            for g in range(GROUP_COUNT):
                tab = u64(mem, gui + OFS_GROUPS + 16 * g)
                cnt = u32(mem, gui + OFS_GROUPS + 16 * g + 8)
                recs = []
                for i in range(min(cnt or 0, 512)):
                    r = tab + 0x18 * i
                    d = u64(mem, r)
                    recs.append({"i": i, "class": cstr(mem, u64(mem, d + DTI_NAME)) if d else None,
                                 "layout": cstr(mem, u64(mem, r + 8)), "flags": u32(mem, r + 16)})
                groups.append({"group": g, "table": hex(tab) if tab else None, "count": cnt, "records": recs})
            report["groups"] = groups

            units = []
            n = u32(mem, gui + OFS_UNIT_COUNT) or 0
            arr = u64(mem, gui + OFS_UNIT_ARRAY)
            for i in range(min(n, 2048)):
                up = u64(mem, arr + 8 * i)
                vt = u64(mem, up) if up else None
                d = dti_of_vtable(mem, vt) if vt else None
                units.append({"i": i, "unit": hex(up) if up else None, "vtable_rva": hex(vt - base) if vt else None,
                              "class": cstr(mem, u64(mem, d + DTI_NAME)) if d else None,
                              "state": hex(u32(mem, up + OFS_UNIT_STATE) or 0),
                              "flags": hex(u32(mem, up + OFS_UNIT_FLAGS) or 0)})
            report["units"] = units
        cats = messages(mem, base)
        report["message_files"] = [None if c is None else {"count": c["count"]} for c in cats]

    resolved = {}
    for name, entries in tables.items():
        rows = []
        for e in entries:
            row = {"id": e[0], "label_id": e[1], "help_id": e[2], "extra": e[3]}
            if cats and e[0] != END_MARKER:
                row["label"] = {c: msg(mem, cats[c], e[1]) for c in range(len(cats)) if msg(mem, cats[c], e[1])}
                row["help"] = {c: msg(mem, cats[c], e[2]) for c in range(len(cats)) if msg(mem, cats[c], e[2])}
            rows.append(row)
        resolved[name] = rows
    report["start_menu"] = resolved

    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, "%s.json" % a.label)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1, ensure_ascii=False)

    # Summary.
    print("source:", report["source"])
    if "groups" in report:
        print("groups:", ", ".join("%d:%s" % (g["group"], g["count"]) for g in report["groups"]))
        live = [u for u in report["units"] if u["class"]]
        print("live units: %d (%d with a class)" % (len(report["units"]), len(live)))
        for u in report["units"]:
            print("  %-4d %-28s state %-10s flags %s" % (u["i"], u["class"], u["state"], u["flags"]))
    for name, rows in resolved.items():
        print("%s:" % name)
        for r in rows:
            if r["id"] == END_MARKER:
                print("  --")
                continue
            lab = (r.get("label") or {}).get(PRINT_CATEGORY, "")
            print("  item %-3d label %-5d %s" % (r["id"], r["label_id"], lab))
    print("written", path)


if __name__ == "__main__":
    main()
