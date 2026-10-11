"""Copy a named cGUIInst* subtree from one .gui into another (exe 421810).

Everything is appended at the end of its table or pool, so every existing
index and offset of the target stays valid (see gui-format-study.md,
"Append and graft"):

- instances: new records at the end of the instance table, new ids from
  the target's InstanceID counter (header 0x18; the table stays sorted by
  id), tree links remapped inside the subtree, the subtree's root linked
  under the given parent (last child by default);
- their init params: new records at the end of the init-param table (the
  instance block is last there), names through the target's strings
  (existing strings reused), pooled values copied into fresh pool entries
  (never shared with the target's own parameters);
- references inside init params: ResourceId (the id of a GUI reference,
  header slot 0x150, 24-byte {id, -, runtime rGUI*, path}; NOT slot 0xE0,
  which is the flow table that gui_dump.py calls "refs") is mapped by path
  to the target's reference with the same path, or a new reference record
  (id = max + 1) is appended; instance-id parameters (CursorInstID, MsgInstID, ...)
  are remapped when they name an instance of the subtree and refused
  otherwise; a local animation (ResourceId -1) is refused (not copied);
- one instance entry per new instance (the keyframed params from header
  0x198 on, which uGUI::setResource binds by instance id: type 3, one key
  per type-2 flow, key frame = flow id, name = the empty string the
  target's entries already use): new param + keys + kv8 values at the ends
  of their tables, values from the source entry by flow name, or copied
  from a named target instance (entries_like);
- header: InstanceID += n, InstExeParamNum (0x8C) = instance count; counts,
  section offsets, vertex offset and file size are recomputed by build().

Not copied (refused or reported): flow-process params animating the
subtree's instances (they sit in the middle of the param table), instance
extend data (no game file has any).

    py -3.12 -B gui_graft.py <source.gui> <instance> <target.gui> <parent> <out.gui>
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gui_rebuild import NONE, VALUE_POOL, VALUE_SIZE, Gui, rd, wr  # noqa: E402
from gui_check import INSTANCE_ID_PARAMS  # noqa: E402

# Init-param types that reference something in the file; instances of the
# game's layouts only carry ResourceId/AnimationId/SequenceId of these.
T_TEXTURE, T_FONT, T_ANIMATION_ID, T_RESOURCE_ID, T_FONTFILTER, T_SEQUENCE_ID, T_EFFECT, T_FONTSTYLE = (
    7, 8, 0xB, 0xD, 0xE, 0x10, 0x13, 0x14)
UNSUPPORTED_REF_TYPES = {T_TEXTURE, T_FONT, T_FONTFILTER, T_EFFECT, T_FONTSTYLE}


class GraftError(Exception):
    pass


def find_instance(g, name):
    hits = [i for i, r in enumerate(g.tables["instance"]) if g.string(rd(r, 24)) == name]
    if len(hits) != 1:
        raise GraftError("%d instances named %r" % (len(hits), name))
    return hits[0]


def subtree(g, root):
    """root and its descendants (child links; the root's next is not
    followed), pre-order."""
    insts = g.tables["instance"]
    out, stack = [], [root]
    while stack:
        i = stack.pop()
        out.append(i)
        kids = []
        c = rd(insts[i], 12)
        while c != NONE:
            kids.append(c)
            c = rd(insts[c], 8)
        stack.extend(reversed(kids))
    return out


def empty_string(g):
    """Offset of an empty string (a NUL at offset 0 or after a NUL)."""
    p = g.pools["strings"]
    for k in range(len(p)):
        if p[k] == 0 and (k == 0 or p[k - 1] == 0):
            return k
    return g.append_pool("strings", b"\0")


def int_value(g, rec):
    kind, raw = rec[0], rd(rec, 24, "<Q")
    pool = VALUE_POOL.get(kind)
    if pool is None:
        return struct.unpack("<i", struct.pack("<I", raw & 0xFFFFFFFF))[0]
    return struct.unpack_from("<i", g.pools[pool], raw)[0]


def graft(dst, src, root_name, parent_name, position="last", log=print, entries_like=None, rename=None):
    """Copies src's instance `root_name` with its subtree under dst's
    instance `parent_name`. Returns {src index: dst index}.

    entries_like: name of a target instance whose instance-entry values
    (one byte per flow) the new instances take; default: the source
    entries' values, matched by flow name.
    rename: {source name: new name} for the copied instances."""
    s_inst, d_inst = src.tables["instance"], dst.tables["instance"]
    root = find_instance(src, root_name)
    parent = find_instance(dst, parent_name)
    order = subtree(src, root)
    rename = rename or {}
    names = [rename.get(src.string(rd(s_inst[i], 24)), src.string(rd(s_inst[i], 24))) for i in order]
    for nm in names:
        if any(dst.string(rd(r, 24)) == nm for r in d_inst):
            log("note: the target already has an instance named %r (names need not be unique, lookups by name may)" % nm)
    base_index = len(d_inst)
    next_id = dst.hu32(0x18)
    if d_inst and next_id <= max(rd(r, 0) for r in d_inst):
        raise GraftError("target InstanceID %d is not above every instance id" % next_id)
    idx_map = {s: base_index + k for k, s in enumerate(order)}
    id_map = {rd(s_inst[s], 0): next_id + k for k, s in enumerate(order)}

    # guiresources of the target by path
    d_gres = {dst.string(rd(r, 16)).lower(): rd(r, 0) for r in dst.tables["guiresource"]}
    s_gres = {rd(r, 0): src.string(rd(r, 16)) for r in src.tables["guiresource"]}

    def map_resource(rid):
        if rid == -1:
            raise GraftError("ResourceId -1 names a local animation of the source; copying animations is not supported")
        path = s_gres.get(rid)
        if path is None:
            raise GraftError("source ResourceId %d has no guiresource" % rid)
        if path.lower() in d_gres:
            return d_gres[path.lower()]
        new_id = max([rd(r, 0) for r in dst.tables["guiresource"]] + [0]) + 1
        rec = bytearray(24)
        wr(rec, 0, new_id)
        wr(rec, 16, dst.add_string(path))
        dst.append_record("guiresource", rec)
        d_gres[path.lower()] = new_id
        log("guiresource %r appended with id %d" % (path, new_id))
        return new_id

    # flow-process params animating these instances are not copied
    s_start = src.hu32(0x198)
    for fp in src.tables["flowprocess"]:
        for k in range(rd(fp, 12), rd(fp, 12) + rd(fp, 8)):
            r = src.tables["param"][k]
            if rd(r, 16) in id_map:
                raise GraftError("source flow-process param %d (%s) animates instance id %d; not supported" % (
                    k, src.string(rd(r, 24)), rd(r, 16)))

    # instances and their init params
    for k, s in enumerate(order):
        sr = s_inst[s]
        if rd(sr, 48) != NONE:
            raise GraftError("instance %s has extend data; not supported" % names[k])
        first_ip = len(dst.tables["initparam"])
        n_ip = rd(sr, 16)
        params = [src.tables["initparam"][rd(sr, 40) + j] for j in range(n_ip)]
        for p in params:
            kind = p[0]
            nm = src.string(rd(p, 16))
            q = bytearray(p)
            wr(q, 16, dst.add_string(nm))
            pool = VALUE_POOL.get(kind)
            if kind in UNSUPPORTED_REF_TYPES:
                raise GraftError("%s.%s: init-param type %#x references a table; not supported" % (names[k], nm, kind))
            if pool is not None:
                raw = rd(p, 24, "<Q")
                size = VALUE_SIZE[kind]
                data = bytes(src.pools[pool][raw:raw + size])
                if kind == 6:
                    so = struct.unpack_from("<Q", data)[0]
                    data = struct.pack("<Q", NONE if so == NONE else dst.add_string(src.string(so)))
                v = struct.unpack_from("<i", data)[0] if size == 4 else None
                if kind == T_RESOURCE_ID:
                    v2 = map_resource(v)
                    data = struct.pack("<i", v2)
                    log("  %s.ResourceId %d -> %d" % (names[k], v, v2))
                elif nm in INSTANCE_ID_PARAMS and v != -1:
                    if v not in id_map:
                        raise GraftError("%s.%s names instance id %d outside the subtree" % (names[k], nm, v))
                    data = struct.pack("<i", id_map[v])
                    log("  %s.%s %d -> %d" % (names[k], nm, v, id_map[v]))
                elif kind == 1 and nm.endswith(("ID", "Id")) and v != -1 and nm not in ("ChildAnimCursorEffectID",):
                    log("  warning: %s.%s = %d copied unchanged (meaning not established)" % (names[k], nm, v))
                off = dst.append_pool(pool, data, 16 if pool == "kv128" else (8 if kind == 6 else None))
                wr(q, 24, off, "<Q")
            dst.append_record("initparam", q)
        rec = bytearray(sr)
        wr(rec, 0, id_map[rd(sr, 0)])
        wr(rec, 24, dst.add_string(names[k]))
        wr(rec, 40, first_ip)
        nx, ch = rd(sr, 8), rd(sr, 12)
        wr(rec, 8, NONE if s == root or nx == NONE else idx_map[nx])
        wr(rec, 12, NONE if ch == NONE else idx_map[ch])
        dst.append_record("instance", rec)

    # link the new root under the parent
    new_root = idx_map[root]
    pr = d_inst[parent]
    if position == "first" or rd(pr, 12) == NONE:
        wr(d_inst[new_root], 8, rd(pr, 12))
        wr(pr, 12, new_root)
    else:
        c = rd(pr, 12)
        while rd(d_inst[c], 8) != NONE:
            c = rd(d_inst[c], 8)
        wr(d_inst[c], 8, new_root)

    # instance entries (one per instance, keyed by type-2 flow ids)
    s_flow_name = {rd(f, 0): src.string(rd(f, 16)) for f in src.tables["flow"]}
    d_flows = [(rd(f, 0), dst.string(rd(f, 16))) for f in dst.tables["flow"] if rd(f, 4) == 2]
    d_start = dst.hu32(0x198)
    template = dst.tables["param"][d_start] if d_start < len(dst.tables["param"]) else None
    if template is not None:  # key order as in the target's first entry
        order_ids = [rd(dst.tables["key"][rd(template, 32) + j], 0) & 0xFFFFFF for j in range(template[1])]
        d_flows.sort(key=lambda f: order_ids.index(f[0]) if f[0] in order_ids else 1 << 30)
    s_entries = {rd(r, 16): r for r in src.tables["param"][s_start:]}
    # the entries' (empty) name: the offset the target's own entries use
    # (0xDCB in mods.gui, the empty string at the pool's end), else any
    # empty string
    nothing = rd(template, 24) if template is not None else empty_string(dst)
    if dst.string(nothing) != "":
        raise GraftError("the target's instance entries do not name the empty string")
    d_flow_name = {rd(f, 0): dst.string(rd(f, 16)) for f in dst.tables["flow"]}

    def entry_values(g, entry, flow_name):
        out = {}
        for j in range(entry[1]):
            key = g.tables["key"][rd(entry, 32) + j]
            out[flow_name.get(rd(key, 0) & 0xFFFFFF)] = g.pools["kv8"][rd(entry, 40) + j]
        return out

    like = None
    if entries_like is not None:
        lid = rd(d_inst[find_instance(dst, entries_like)], 0)
        hits = [r for r in dst.tables["param"][d_start:] if rd(r, 16) == lid]
        if len(hits) != 1:
            raise GraftError("target instance %r has %d instance entries" % (entries_like, len(hits)))
        like = entry_values(dst, hits[0], d_flow_name)
    for s in order:
        sid = rd(s_inst[s], 0)
        se = s_entries.get(sid)
        if se is None:
            raise GraftError("source instance id %d has no instance entry" % sid)
        by_name = like if like is not None else entry_values(src, se, s_flow_name)
        vals = []
        first_key = len(dst.tables["key"])
        for fid, fname in d_flows:
            if fname not in by_name:
                raise GraftError("target flow %r has no counterpart in the source entry of id %d" % (fname, sid))
            vals.append(by_name[fname])
            kr = bytearray(16)
            wr(kr, 0, fid)  # frame = flow id, mode 0
            dst.append_record("key", kr)
        off = dst.append_pool("kv8", bytes(vals))
        pr = bytearray(48)
        pr[0], pr[1] = 3, len(vals)
        wr(pr, 16, id_map[sid])
        wr(pr, 24, nothing)
        wr(pr, 32, first_key)
        wr(pr, 40, off, "<Q")
        dst.append_record("param", pr)
        log("  entry for %s: %s%s" % (names[order.index(s)], dict(zip([f[1] for f in d_flows], vals)),
                                      " (as %s)" % entries_like if like is not None else " (as the source)"))

    dst.set_hu32(0x18, next_id + len(order))
    dst.set_hu32(0x8C, len(d_inst))
    return idx_map


def main():
    if len(sys.argv) != 6:
        print(__doc__)
        return 2
    src = Gui.parse(open(sys.argv[1], "rb").read())
    dst = Gui.parse(open(sys.argv[3], "rb").read())
    m = graft(dst, src, sys.argv[2], sys.argv[4])
    out = dst.build()
    open(sys.argv[5], "wb").write(out)
    print("grafted %d instances; wrote %s (%d bytes)" % (len(m), sys.argv[5], len(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
