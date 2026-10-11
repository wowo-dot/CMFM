"""Read-only peek at the live uGUIOption (title screen Options) unit.

    py -3.12 -B tools/re/peek_title_options.py [label]

Prints the unit's state, and for each of its 7 category sub-objects
(0x968 bytes from +0x2918): vtable RVA, tab index (+80), selected row
(+84), its button list (+24: class, rows +0x1D8, DuplicateNum +0x298, cursor
+0x274), and the first rows' labels. Same read-only access as
dump_image.py (PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ).
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dump_image as d  # noqa: E402
import gui_inspect as g  # noqa: E402

SUB_FIRST = 0x2918
SUB_SIZE = 0x968
SUB_COUNT = 7
ROW_STRIDE = 96  # row entries from +96, row table pointer at +16


def cls_of(m, ptr):
    if not ptr:
        return None
    dti = g.dti_of_vtable(m, g.u64(m, ptr))
    return g.cstr(m, g.u64(m, dti + g.DTI_NAME)) if dti else None


def main():
    label = sys.argv[1] if len(sys.argv) > 1 else ""
    m = d.Reader(d.find_pids()[0])
    base = m.q(m.peb() + d.PEB_IMAGE_BASE)
    cats = g.messages(m, base)
    gui = g.u64(m, base + g.RVA_SMHGUI_INSTANCE)
    unit = None
    for i in range(g.u32(m, gui + g.OFS_UNIT_COUNT)):
        up = g.u64(m, g.u64(m, gui + g.OFS_UNIT_ARRAY) + 8 * i)
        if cls_of(m, up) == "uGUIOption":
            unit = up
            break
    if unit is None:
        print("no uGUIOption unit alive")
        return
    print("== %s uGUIOption %s state %s" % (label, hex(unit), hex(g.u32(m, unit + g.OFS_UNIT_STATE))))
    for o in (10488, 27376):
        p = unit + o
        print("  +%d vtable rva %s" % (o, hex(g.u64(m, p) - base)))
    for i in range(SUB_COUNT):
        s = unit + SUB_FIRST + i * SUB_SIZE
        vt = g.u64(m, s)
        tab = g.u32(m, s + 80)
        sel = struct.unpack("<i", m.read(s + 84, 4))[0]
        lst = g.u64(m, s + 24)
        line = "  sub %d vt %s tab %s sel %d" % (i, hex(vt - base) if vt else None, tab, sel)
        if lst:
            line += " list %s rows %s dup %s cursor %s" % (
                cls_of(m, lst), g.u32(m, lst + 0x1D8), g.u32(m, lst + 0x298), g.u32(m, lst + 0x274))
        print(line)
        labels = []
        for r in range(4):
            row = g.u64(m, s + ROW_STRIDE * (r + 1) + 16)
            if not row:
                break
            kind, lab = struct.unpack("<ii", m.read(row, 8))
            sid = struct.unpack("<i", m.read(row + 20, 4))[0]
            text = g.msg(m, cats[23], lab - 10000) if lab > 10000 else g.msg(m, cats[12], lab)
            labels.append("%d/%d %r" % (kind, sid, text))
        if labels:
            print("      rows: " + "; ".join(labels))
    m.close()


if __name__ == "__main__":
    main()
