"""Read and edit MHW (exe 421810) GUI layout files (.gui, rGUI) in place.

Layout (see tools/re/gui_dump.py and docs/research/native-menu.md):
- header: magic, version, file size; record counts at 0x28..0x64; section
  offsets (u32 in u64 slots) at 0xA8..0x190.
- animations (0xA8, 32 bytes): +4 u16 instance count, +12 u32 first
  instance, +16 name.
- instances (0xB8, 56 bytes): +4 u8 parameter count, +16 name, +24 class id,
  +32 u32 first parameter.
- parameters (0xC8, 32 bytes): +0 u8 type, +16 u32 name, +24 value. Types 2
  (float) and 4 (four floats: vectors, sizes, colours) hold an offset into
  the value pool at slot 0x188; several parameters may share one entry.
- strings at slot 0x168.

Edits here never change the file size: they rewrite values in place.
"""
import hashlib
import struct

MAGIC = 0x00495547
VERSION = 0x23316
PARAM_FLOAT = 2
PARAM_VEC4 = 4


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
        o = self.off[0x188] + slot
        if kind == PARAM_VEC4:
            return struct.unpack_from("<4f", self.d, o)
        if kind == PARAM_FLOAT:
            return struct.unpack_from("<f", self.d, o)
        raise TypeError("parameter type %d is not a pooled float" % kind)

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
        o = self.off[0x188] + slot
        struct.pack_into("<4f" if kind == PARAM_VEC4 else "<f", self.d, o, *values)


def sha256(data):
    return hashlib.sha256(data).hexdigest()
