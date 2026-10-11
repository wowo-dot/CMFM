"""Read and edit MHW (exe 421810) GUI layout files (.gui, rGUI) in place.

Layout (see tools/re/gui_dump.py and docs/research/native-menu.md):
- header: magic, version, file size; record counts at 0x28..0x64; section
  offsets (u32 in u64 slots) at 0xA8..0x190.
- animations (0xA8, 32 bytes): +4 u16 instance count, +12 u32 first
  instance, +16 name.
- instances (0xB8, 56 bytes): +4 u8 parameter count, +16 name, +24 class id,
  +32 u32 first parameter.
- parameters (0xC8, 32 bytes): +0 u8 type, +16 u32 name, +24 value. As the
  loader's fix-up resolves them: types 1 (int), 2 (float), 7-0xE, 0x10,
  0x13, 0x14 hold an offset into the pool at slot 0x180; type 3 (animated
  values such as Visible, Play) into the pool at 0x178; type 4 (four floats:
  vectors, sizes, colours) into the pool at 0x188; types 0x11 and 0x12 are
  inline. Several parameters may share one pool entry.
- strings at slot 0x168.

Edits here never change the file size: they rewrite values in place.
"""
import hashlib
import struct

MAGIC = 0x00495547
VERSION = 0x23316
PARAM_INT = 1
PARAM_FLOAT = 2
PARAM_VEC4 = 4
PARAM_ANIMATION_ID = 0x0B
PARAM_RESOURCE_ID = 0x0D
POOL = {PARAM_INT: 0x180, PARAM_FLOAT: 0x180, PARAM_VEC4: 0x188, PARAM_ANIMATION_ID: 0x180,
        PARAM_RESOURCE_ID: 0x180}
FORMAT = {PARAM_INT: "<i", PARAM_FLOAT: "<f", PARAM_VEC4: "<4f", PARAM_ANIMATION_ID: "<i",
          PARAM_RESOURCE_ID: "<i"}


class GuiFile:
    def __init__(self, data):
        self.d = bytearray(data)
        magic, version = struct.unpack_from("<II", self.d, 0)
        if magic != MAGIC or version != VERSION:
            raise ValueError("not a 421810 GUI file (magic %#x, version %#x)" % (magic, version))
        self.off = {slot: self.u32(slot) for slot in range(0xA8, 0x198, 8)}

    def u32(self, o):
        return struct.unpack_from("<I", self.d, o)[0]

    def string(self, rel):
        o = self.off[0x168] + rel
        return self.d[o:self.d.index(b"\0", o)].decode("utf-8")

    def animation(self, name):
        base, count = self.off[0xA8], self.u32(0x28)
        for i in range(count):
            e = base + i * 32
            if self.string(self.u32(e + 16)) == name:
                n = struct.unpack_from("<H", self.d, e + 4)[0]
                return range(self.u32(e + 12), self.u32(e + 12) + n)
        raise KeyError("no animation %r" % name)

    def instance(self, animation, name):
        hits = [i for i in self.animation(animation) if self.string(self.u32(self.off[0xB8] + i * 56 + 16)) == name]
        if len(hits) != 1:
            raise KeyError("%d instances %r in %r" % (len(hits), name, animation))
        return hits[0]

    def instance_name(self, index):
        return self.string(self.u32(self.off[0xB8] + index * 56 + 16))

    def params(self, index):
        e = self.off[0xB8] + index * 56
        first = self.u32(e + 32)
        return range(first, first + self.d[e + 4])

    def param(self, animation, instance, name):
        index = self.instance(animation, instance)
        for k in self.params(index):
            if self.string(self.u32(self.off[0xC8] + k * 32 + 16)) == name:
                return k
        raise KeyError("no parameter %r on %s/%s" % (name, animation, instance))

    def param_record(self, k):
        r = self.off[0xC8] + k * 32
        return self.d[r], struct.unpack_from("<Q", self.d, r + 24)[0]

    def value(self, k):
        kind, slot = self.param_record(k)
        if kind not in POOL:
            raise TypeError("parameter type %d is not a pooled number" % kind)
        return struct.unpack_from(FORMAT[kind], self.d, self.off[POOL[kind]] + slot)

    def sharers(self, k):
        """Instance names of every parameter that uses the same pool entry."""
        kind, slot = self.param_record(k)
        names = set()
        for i in range(self.u32(0x30)):
            for p in self.params(i):
                if self.param_record(p) == (kind, slot):
                    names.add(self.instance_name(i))
        return names

    def set_value(self, k, values):
        kind, slot = self.param_record(k)
        if kind not in POOL:
            raise TypeError("parameter type %d is not a pooled number" % kind)
        struct.pack_into(FORMAT[kind], self.d, self.off[POOL[kind]] + slot, *values)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


class Scene:
    """The layout's instance tree (slot 0xD8, cGUIInst*, 56 bytes each): +8
    next sibling index, +12 first child index (-1: none), +16 u32 parameter
    count, +24 name, +32 class id, +40 first parameter."""

    def __init__(self, gui):
        self.g = gui
        self.base = gui.off[0xD8]
        self.count = gui.u32(0x44)

    def _e(self, i):
        return self.base + i * 56

    def name(self, i):
        return self.g.string(self.g.u32(self._e(i) + 24))

    def find(self, name):
        hits = [i for i in range(self.count) if self.name(i) == name]
        if len(hits) != 1:
            raise KeyError("%d instances named %r" % (len(hits), name))
        return hits[0]

    def links(self, i):
        return struct.unpack_from("<ii", self.g.d, self._e(i) + 8)

    def set_links(self, i, next_index, child_index):
        struct.pack_into("<ii", self.g.d, self._e(i) + 8, next_index, child_index)

    def params(self, i):
        first = self.g.u32(self._e(i) + 40)
        return range(first, first + self.g.u32(self._e(i) + 16))

    def param(self, i, name):
        for k in self.params(i):
            if self.g.string(self.g.u32(self.g.off[0xC8] + k * 32 + 16)) == name:
                return k
        raise KeyError("no parameter %r on %s" % (name, self.name(i)))

    def point_at(self, k, source):
        """Makes parameter k use parameter `source`'s pooled value (same type)."""
        r, rs = self.g.off[0xC8] + k * 32, self.g.off[0xC8] + source * 32
        if self.g.d[r] != self.g.d[rs]:
            raise TypeError("parameter types differ")
        self.g.d[r + 24:r + 32] = self.g.d[rs + 24:rs + 32]

    def point_at_value(self, k, value):
        """Makes parameter k use a pool entry that already holds `value`."""
        kind, _ = self.g.param_record(k)
        base, fmt = self.g.off[POOL[kind]], FORMAT[kind]
        size = struct.calcsize(fmt)
        end = self.g.off[{0x180: 0x188, 0x188: 0x190}[POOL[kind]]]
        for o in range(0, end - base - size + 1, 4):
            if struct.unpack_from(fmt, self.g.d, base + o)[0] == value:
                struct.pack_into("<Q", self.g.d, self.g.off[0xC8] + k * 32 + 24, o)
                return o
        raise KeyError("no pool entry holds %r" % (value,))

    def walk(self):
        """Every index reached from the root (0), checking for cycles."""
        seen, stack = [], [0]
        while stack:
            i = stack.pop()
            while i != -1:
                if i in seen:
                    raise ValueError("instance %s reached twice" % self.name(i))
                seen.append(i)
                next_index, child = self.links(i)
                if child != -1:
                    stack.append(child)
                i = next_index
        return seen
