"""Parse, rebuild and grow MHW (exe 421810) GUI layout files (.gui, rGUI).

Names follow the game's own structures (Fexty's database: GUIFile,
nGUI::OBJECT, nGUI::INSTANCE, nGUI::OBJ_SEQUENCE, nGUI::INIT_PARAM,
nGUI::PARAM, nGUI::KEY, nGUI::FLOW, nGUI::FLOW_PROCESS, nGUI::TEXTURE) and
the loader's fix-up (sub_1422A7310, local/re/rGUI_fixup_1422A7310.c).
CMFM's older tools use other names for two tables: gui_dump.py's
"instances" (slot 0xB8) are OBJECTs (cGUIObj*, the parts of an animation)
and its "objects" (slot 0xD8) are INSTANCEs (cGUIInst*, the layout tree).

    py -3.12 -B gui_rebuild.py roundtrip <ui folder>
    py -3.12 -B gui_rebuild.py check <file.gui> [...]

Library use: Gui.parse(bytes) -> Gui; gui.build() -> bytes; gui.append_*;
graft() in gui_graft.py.
"""
import glob
import os
import struct
import sys

MAGIC = 0x00495547
VERSION = 0x23316
HEADER_SIZE = 0x1B0
ALIGN = 16
NONE = 0xFFFFFFFF

# name: (header slot, count field, record size). In the file the sections
# follow CANON: these tables in this order (the key table last), then the
# pools kv8, kv32, kv128, extend, strings, then the vertex data; every
# section starts 16-byte aligned, zero padding; an empty section's offset
# is the next section's start (the camera table's offset is 0). All 303
# files of the game's ui folder follow that rule exactly.
TABLES = [
    ("animation", 0xA8, 0x28, 32),
    ("sequence", 0xB0, 0x2C, 16),
    ("object", 0xB8, 0x30, 56),          # nGUI::OBJECT, cGUIObj*
    ("objseq", 0xC0, 0x34, 24),          # nGUI::OBJ_SEQUENCE
    ("initparam", 0xC8, 0x38, 32),       # nGUI::INIT_PARAM
    ("param", 0xD0, 0x3C, 48),           # nGUI::PARAM (keyframed)
    ("instance", 0xD8, 0x44, 56),        # nGUI::INSTANCE, cGUIInst*
    ("flow", 0xE0, 0x48, 32),            # nGUI::FLOW
    ("flowprocess", 0xE8, 0x4C, 48),     # nGUI::FLOW_PROCESS
    ("flowinput", 0xF0, 0x50, 16),
    ("flowswitch", 0xF8, 0x54, 16),
    ("flowfunction", 0x100, 0x58, 16),
    ("action", 0x108, 0x5C, 24),
    ("inputcondition", 0x110, 0x60, 8),
    ("switchoperator", 0x118, 0x68, 12),
    ("switchcondition", 0x120, 0x64, 24),
    ("variable", 0x128, 0x6C, 40),
    ("texture", 0x130, 0x70, 64),        # nGUI::TEXTURE
    ("font", 0x138, 0x74, 24),
    ("fontfilter", 0x140, 0x78, None),   # variable-size records, kept as one blob
    ("message", 0x148, 0x7C, 24),
    ("guiresource", 0x150, 0x80, 24),    # other .gui files (path)
    ("genresource", 0x158, 0x84, 24),    # other resources (DTI id + path)
    ("camera", 0x160, 0x88, 56),         # always empty, offset 0
    ("key", 0x170, 0x40, 16),            # nGUI::KEY
]
TABLE = {t[0]: t for t in TABLES}
POOLS = [("strings", 0x168), ("kv8", 0x178), ("kv32", 0x180), ("kv128", 0x188), ("extend", 0x190)]
POOL = dict(POOLS)
SECTION_SLOT = {**{t[0]: t[1] for t in TABLES}, **POOL}
CANON = [t[0] for t in TABLES] + ["kv8", "kv32", "kv128", "extend", "strings"]
POOL_ALIGN = {"strings": 1, "kv8": 1, "kv32": 4, "kv128": 16, "extend": 16}

# Header fields (GUIFile): 0x08 FileSize, 0x18 InstanceID, 0x1C FlowID,
# 0x20 VariableID, 0x24 StartInstanceIndex, 0x8C InstExeParamNum,
# 0x90 VertexBufferSize, 0xA0 StartFlowIndex, 0x198
# InstanceParamEntryStartIndex, 0x1A0 VertexOffset.
H_FILESIZE = 0x08
H_INSTANCE_ID = 0x18
H_START_INSTANCE = 0x24
H_INSTEXE_NUM = 0x8C
H_VERTEX_SIZE = 0x90
H_START_FLOW = 0xA0
H_INSTPARAM_START = 0x198
H_VERTEX_OFFSET = 0x1A0

# Parameter value types (INIT_PARAM +0 low byte, PARAM +0 low byte) and
# where the value lives, as the fix-up resolves it.
KV32_TYPES = {1, 2, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0x10, 0x13, 0x14}
VALUE_POOL = {**{t: "kv32" for t in KV32_TYPES}, 3: "kv8", 4: "kv128", 6: "kv32"}
VALUE_SIZE = {**{t: 4 for t in KV32_TYPES}, 3: 1, 4: 16, 6: 8}  # 6: u64 string offset in kv32


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def align(n, a=ALIGN):
    return (n + a - 1) // a * a


class Gui:
    """A .gui as header + tables (lists of raw records) + pools (raw byte
    blobs, each the full span to the next section, padding included)."""

    def __init__(self):
        self.header = bytearray(HEADER_SIZE)
        self.tables = {}        # name -> list[bytearray]
        self.fontfilter = b""   # raw blob (records + padding), count in header
        self.fontfilter_count = 0
        self.pools = {}         # name -> bytearray
        self.vertex = b""
        self.order = list(CANON)  # section names in file order
        self.tails = {}         # table name -> bytes between records' end and next section

    # -- parse ---------------------------------------------------------------
    @classmethod
    def parse(cls, data):
        g = cls()
        if u32(data, 0) != MAGIC or u32(data, 4) != VERSION:
            raise ValueError("not a 421810 GUI file")
        if u32(data, H_FILESIZE) != len(data):
            raise ValueError("header file size %d != %d" % (u32(data, H_FILESIZE), len(data)))
        for s in range(0xA8, 0x1A8, 8):
            if u32(data, s + 4):
                raise ValueError("section offset slot %#x has a high dword" % s)
        g.header[:] = data[:HEADER_SIZE]
        off = {name: u32(data, slot) for name, slot in SECTION_SLOT.items()}
        vstart = u32(data, H_VERTEX_OFFSET)
        vsize = u32(data, H_VERTEX_SIZE)
        if vstart + vsize != len(data):
            raise ValueError("vertex data does not end the file")
        g.vertex = bytes(data[vstart:])
        present = [n for n in CANON if not (n == "camera" and off[n] == 0)]
        for a, b in zip(present, present[1:]):
            if off[a] > off[b]:
                raise ValueError("section %s (%#x) after %s (%#x): not the canonical order" % (a, off[a], b, off[b]))
        g.order = present
        for i, n in enumerate(g.order):
            start = off[n]
            end = off[g.order[i + 1]] if i + 1 < len(g.order) else vstart
            if start < HEADER_SIZE or end < start:
                raise ValueError("section %s at %#x..%#x" % (n, start, end))
            if n in POOL:
                g.pools[n] = bytearray(data[start:end])
            elif n == "fontfilter":
                g.fontfilter = bytes(data[start:end])
                g.fontfilter_count = u32(data, TABLE[n][2])
            else:
                _, slot, cf, size = TABLE[n]
                cnt = u32(data, cf)
                if start + cnt * size > end:
                    raise ValueError("table %s overruns (%d x %d at %#x, next %#x)" % (n, cnt, size, start, end))
                g.tables[n] = [bytearray(data[start + k * size:start + (k + 1) * size]) for k in range(cnt)]
                g.tails[n] = bytes(data[start + cnt * size:end])
        g.tables.setdefault("camera", [])
        return g

    # -- build ---------------------------------------------------------------
    def build(self):
        h = bytearray(self.header)
        out = bytearray(HEADER_SIZE)
        offsets = {}
        for n in self.order:
            pos = len(out)
            if pos % ALIGN:
                raise AssertionError("unaligned section start")
            offsets[n] = pos
            if n in POOL:
                blob = self.pools[n]
            elif n == "fontfilter":
                blob = self.fontfilter
            else:
                recs = self.tables[n]
                blob = b"".join(recs)
                tail = self.tails.get(n, b"")
                # keep the original tail when it still fits the alignment
                # (round trip); otherwise pad with zeros
                if (len(blob) + len(tail)) % ALIGN == 0 and len(tail) < ALIGN and not any(tail):
                    blob += tail
                elif len(blob) % ALIGN == 0 and len(tail) % ALIGN == 0 and not any(tail):
                    blob += tail  # e.g. an empty table
                else:
                    blob += bytes(-len(blob) % ALIGN)
            out += blob
            out += bytes(-len(out) % ALIGN)
        for name, slot, cf, size in TABLES:
            if name == "camera" and "camera" not in self.order:
                struct.pack_into("<Q", h, slot, 0)
            else:
                struct.pack_into("<Q", h, slot, offsets[name])
            if name == "fontfilter":
                struct.pack_into("<I", h, cf, self.fontfilter_count)
            else:
                struct.pack_into("<I", h, cf, len(self.tables.get(name, [])))
        for name, slot in POOLS:
            struct.pack_into("<Q", h, slot, offsets[name])
        struct.pack_into("<Q", h, H_VERTEX_OFFSET, len(out))
        struct.pack_into("<I", h, H_VERTEX_SIZE, len(self.vertex))
        out += self.vertex
        struct.pack_into("<I", h, H_FILESIZE, len(out))
        out[:HEADER_SIZE] = h
        return bytes(out)

    # -- access --------------------------------------------------------------
    def count(self, table):
        return len(self.tables[table])

    def hu32(self, o):
        return u32(self.header, o)

    def set_hu32(self, o, v):
        struct.pack_into("<I", self.header, o, v)

    def string(self, off):
        if off == NONE:
            return None
        p = self.pools["strings"]
        return bytes(p[off:p.index(b"\0", off)]).decode("utf-8", "replace")

    def find_string(self, s):
        """Offset of an existing NUL-terminated string equal to s (whole
        string, starting after a NUL), or None."""
        p = self.pools["strings"]
        needle = s.encode("utf-8") + b"\0"
        start = 0
        while True:
            k = p.find(needle, start)
            if k < 0:
                return None
            if k == 0 or p[k - 1] == 0:
                return k
            start = k + 1

    # -- append (existing indices and offsets stay valid) --------------------
    def append_record(self, table, rec):
        if TABLE[table][3] is None:
            raise TypeError("variable-size table")
        if len(rec) != TABLE[table][3]:
            raise ValueError("record size")
        if table not in self.order:
            raise ValueError("table %s missing from section order" % table)
        self.tables[table].append(bytearray(rec))
        return len(self.tables[table]) - 1

    def append_pool(self, pool, data, alignment=None):
        """Appends bytes after the pool's current span (padding included)
        and returns their offset inside the pool."""
        p = self.pools[pool]
        a = alignment or POOL_ALIGN[pool]
        p += bytes(-len(p) % a)
        off = len(p)
        p += data
        return off

    def add_string(self, s, reuse=True):
        if reuse:
            k = self.find_string(s)
            if k is not None:
                return k
        return self.append_pool("strings", s.encode("utf-8") + b"\0")


# -- record field helpers ------------------------------------------------------
def rd(rec, o, fmt="<I"):
    return struct.unpack_from(fmt, rec, o)[0]


def wr(rec, o, v, fmt="<I"):
    struct.pack_into(fmt, rec, o, v)


def ui_files(root):
    return sorted(glob.glob(os.path.join(root, "**", "*.gui"), recursive=True))


def roundtrip(root):
    files = ui_files(root)
    same = 0
    bad = []
    for f in files:
        data = open(f, "rb").read()
        try:
            out = Gui.parse(data).build()
        except Exception as e:  # noqa: BLE001 - report every failure
            bad.append((f, "error %s" % e))
            continue
        if out == data:
            same += 1
        else:
            k = next((i for i in range(min(len(out), len(data))) if out[i] != data[i]), min(len(out), len(data)))
            bad.append((f, "differs at %#x (sizes %d/%d)" % (k, len(out), len(data))))
    print("round trip: %d of %d files byte-identical" % (same, len(files)))
    for f, why in bad[:20]:
        print("  %s: %s" % (os.path.relpath(f, root), why))
    return 0 if not bad else 1


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "roundtrip":
        return roundtrip(sys.argv[2])
    if len(sys.argv) >= 3 and sys.argv[1] == "check":
        from gui_check import check  # noqa: E402
        rc = 0
        for f in sys.argv[2:]:
            errs = check(Gui.parse(open(f, "rb").read()))
            print("%s: %s" % (f, "ok" if not errs else "%d problems" % len(errs)))
            for e in errs[:30]:
                print("  " + e)
            rc |= bool(errs)
        return rc
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
