"""Dump an MHW (exe 421810) GUI layout file (.gui, rGUI) read-only.

The layout follows rGUI::Deserialize (0x1422A70B0) and its fix-up
sub_1422A7310: a header with record counts (0x28..0x64) and section
offsets (u32 in u64 slots, 0xA8..0x190), then fixed-size record tables and
string/value pools. Class ids are MT DTI ids, (~crc32(name)) & 0x7FFFFFFF;
names come from a JSON map {"0x<id>": "name"} (local/re/dti_ids.json, made
from our IDA database).

    py -3.12 -B tools/re/gui_dump.py <file.gui> [--dti local/re/dti_ids.json]

Prints the header, the animations with their instance ranges, the
instances and the objects. Writes nothing.
"""
import argparse
import json
import struct
import sys

MAGIC = 0x00495547
VERSION = 0x23316

# Header slot -> (count field offset, record size, name)
SECTIONS = {
    0xA8: (0x28, 32, "animations"),
    0xB0: (0x2C, 16, "sequences"),
    0xB8: (0x30, 56, "instances"),
    0xC0: (0x34, 24, "keysets"),
    0xC8: (0x38, 32, "params"),
    0xD0: (0x3C, 48, "anim params"),
    0xD8: (0x44, 56, "objects"),
    0xE0: (0x48, 32, "refs"),
    0xE8: (0x4C, 48, "table e8"),
    0xF0: (0x50, 16, "table f0"),
    0xF8: (0x54, 16, "table f8"),
    0x100: (0x58, 16, "table 100"),
    0x108: (0x5C, 24, "table 108"),
    0x110: (0x60, 8, "table 110"),
    0x120: (0x64, 24, "table 120"),
    0x170: (0x40, 16, "ext params"),
}
POOLS = {0x168: "strings", 0x178: "pool 178", 0x180: "pool 180", 0x188: "pool 188", 0x190: "pool 190"}


class Gui:
    def __init__(self, data):
        self.d = data
        magic, version, size = struct.unpack_from("<III", data, 0)
        if magic != MAGIC or version != VERSION:
            raise ValueError("not a 421810 GUI file (magic %#x, version %#x)" % (magic, version))
        if size != len(data):
            print("note: header size %d, file %d" % (size, len(data)))
        self.offsets = {slot: struct.unpack_from("<I", data, slot)[0] for slot in range(0xA8, 0x198, 8)}
        self.strings = self.offsets[0x168]

    def u32(self, o):
        return struct.unpack_from("<I", self.d, o)[0]

    def count(self, field):
        return self.u32(field)

    def table(self, slot):
        cnt_field, size, _ = SECTIONS[slot]
        base = self.offsets[slot]
        return [base + i * size for i in range(self.count(cnt_field))] if base else []

    def string(self, rel):
        if rel == 0xFFFFFFFF:
            return None
        o = self.strings + rel
        end = self.d.index(b"\0", o)
        return self.d[o:end].decode("utf-8", "replace")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gui")
    ap.add_argument("--dti", default="local/re/dti_ids.json")
    a = ap.parse_args()
    data = open(a.gui, "rb").read()
    g = Gui(data)
    try:
        names = {int(k, 16): v for k, v in json.load(open(a.dti)).items()}
    except OSError:
        names = {}
    cls = lambda i: names.get(i, "0x%08x" % i)

    print("%s: %d bytes, design size %dx%d, flags %#x" % (a.gui, len(data), g.u32(0x98), g.u32(0x9C), g.u32(0x94)))
    for slot, (cf, size, nm) in sorted(SECTIONS.items()):
        print("  %-12s @%#07x  %5d x %2d" % (nm, g.offsets[slot], g.count(cf), size))
    for slot, nm in sorted(POOLS.items()):
        print("  %-12s @%#07x" % (nm, g.offsets[slot]))

    print("\nanimations:")
    for i, e in enumerate(g.table(0xA8)):
        n_inst, n_seq = struct.unpack_from("<HH", data, e + 4)
        first_inst = g.u32(e + 12)
        name = g.string(g.u32(e + 16))
        first_seq = g.u32(e + 24)
        print("  %2d %-28s instances %3d..%-3d sequences %3d..%d" % (
            i, name, first_inst, first_inst + n_inst - 1, first_seq, first_seq + n_seq - 1))

    print("\nsequences:")
    print("  " + ", ".join("%d %s" % (i, g.string(g.u32(e + 8))) for i, e in enumerate(g.table(0xB0))))

    print("\ninstances:")
    for i, e in enumerate(g.table(0xB8)):
        nparams = data[e + 4]
        flag = data[e + 6]
        name = g.string(g.u32(e + 16))
        cid = g.u32(e + 24)
        first_param = g.u32(e + 32)
        keyset = g.u32(e + 40)
        extra = g.u32(e + 48)
        head = data[e:e + 16].hex()
        print("  %3d %-32s %-28s params %3d@%-5d keyset %-4d extra %s  [%s]" % (
            i, name, cls(cid), nparams, first_param, keyset, "-" if extra == 0xFFFFFFFF else extra, head))

    print("\nobjects:")
    for i, e in enumerate(g.table(0xD8)):
        flags = g.u32(e + 4)
        nparams = g.u32(e + 16)
        name = g.string(g.u32(e + 24))
        cid = g.u32(e + 32)
        first_param = g.u32(e + 40)
        head = data[e:e + 16].hex()
        print("  %3d %-32s %-28s params %3d@%-5d flags %#x  [%s]" % (
            i, name, cls(cid), nparams, first_param, flags, head))
    return 0


if __name__ == "__main__":
    sys.exit(main())
