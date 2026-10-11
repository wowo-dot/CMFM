"""Cross-reference rules of MHW (exe 421810) .gui files, checked on a parsed
Gui. Each rule is a named hypothesis; `check(g)` returns the broken ones as
text, `check(g, stats)` also counts passes and failures per rule, so the
same code confirms the rules over the game's 303 layouts and validates a
grown file.

    py -3.12 -B gui_check.py stats <ui folder>      # every rule over every file
    py -3.12 -B gui_check.py <file.gui> [...]       # problems in given files

Field names: see gui_rebuild.py and gui-format-study.md.
"""
import collections
import json
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gui_rebuild import NONE, VALUE_POOL, VALUE_SIZE, Gui, rd, ui_files  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def dti_id(name):
    return (~zlib.crc32(name.encode()) & 0xFFFFFFFF) & 0x7FFFFFFF


try:
    # class names for messages only (local/re/dti_ids.json, made from the IDA database; untracked)
    DTI = {int(k, 16): v for k, v in json.load(open(os.path.join(HERE, "..", "..", "local", "re", "dti_ids.json"))).items()}
except OSError:
    DTI = {}

# init params whose int value names an instance by its id (+0), as the
# statistics confirm (value is -1 or an instance id of the same file)
INSTANCE_ID_PARAMS = {"CursorInstID", "MsgInstID", "NextArrowInstID", "PrevArrowInstID", "SyncCursorID",
                      "SyncListID"} | {"GridInstID[%d]" % k for k in range(8)}
# cGUIFontFilterBorder, Shadow, DistanceField, GradationOverlay; 0 = none
FONTFILTER_SIZES = {0x6DF3AB1E: 20, 0x5F704244: 20, 0x63930740: 64, 0x77214222: 16, 0: 8}


def fontfilter_ids(g):
    out, o = set(), 0
    for _ in range(g.fontfilter_count):
        c, i = struct.unpack_from("<II", g.fontfilter, o)
        out.add(i)
        o += FONTFILTER_SIZES.get(c, 1 << 30)
        if o > len(g.fontfilter):
            break
    return out


class Checker:
    def __init__(self, g, stats=None, root=None):
        self.g = g
        self.stats = stats
        self.errs = []
        self.root = root  # ui folder, to follow ResourceId into other files

    def h(self, rule, ok, msg="", info=False):
        """info=True: an observation, not a rule (counted, never an error)."""
        if self.stats is not None:
            st = self.stats.setdefault(rule, [0, 0, None, info])
            st[0 if ok else 1] += 1
            if not ok and st[2] is None:
                st[2] = msg
        if not ok and not info:
            self.errs.append("%s: %s" % (rule, msg))
        return ok

    def strok(self, off):
        p = self.g.pools["strings"]
        return off < len(p) and (off == 0 or p[off - 1] == 0) and p.find(b"\0", off) >= 0

    def valid_index(self, table, first, n):
        return n == 0 or (first != NONE and first + n <= self.g.count(table))

    # -----------------------------------------------------------------------
    def run(self):
        g = self.g
        h = self.h
        C = g.count
        anims, seqs, objs = g.tables["animation"], g.tables["sequence"], g.tables["object"]
        oseqs, ips, prms = g.tables["objseq"], g.tables["initparam"], g.tables["param"]
        insts, flows, fps, keys = g.tables["instance"], g.tables["flow"], g.tables["flowprocess"], g.tables["key"]
        span = {k: len(v) for k, v in g.pools.items()}

        # ---- animations ------------------------------------------------------
        nexto = nexts = 0
        ids = set()
        for a, r in enumerate(anims):
            no, ns = rd(r, 4, "<H"), rd(r, 6, "<H")
            fo, fs = rd(r, 12), rd(r, 24)
            h("anim.name is a string", self.strok(rd(r, 16)), "anim %d" % a)
            h("anim.objects (+0C first, +04 u16 n) in range", fo + no <= C("object"), "anim %d" % a)
            h("anim.sequences (+18 first, +06 u16 n) in range", fs + ns <= C("sequence"), "anim %d" % a)
            h("anim object ranges partition the object table in order", fo == nexto, "anim %d first %d expected %d" % (a, fo, nexto))
            h("anim sequence ranges partition the sequence table in order", fs == nexts, "anim %d" % a)
            nexto, nexts = fo + no, fs + ns
            h("anim.id unique", rd(r, 0) not in ids, "anim %d" % a)
            ids.add(rd(r, 0))
            h("anim +14 +1C zero", rd(r, 20) == 0 and rd(r, 28) == 0)
            if no:
                h("anim first object is cGUIObjRoot", rd(objs[fo], 24) == dti_id("cGUIObjRoot"), "anim %d" % a)
                h("anim first object's initParamNum is 0", objs[fo][4] == 0, "anim %d" % a)
        h("anim object ranges cover the object table", nexto == C("object"), "%d of %d" % (nexto, C("object")))
        h("anim sequence ranges cover the sequence table", nexts == C("sequence"), "%d of %d" % (nexts, C("sequence")))

        for k, r in enumerate(seqs):
            h("seq.name is a string", self.strok(rd(r, 8)), "seq %d" % k)

        # ---- objects (cGUIObj*) per animation ---------------------------------
        next_oseq = 0
        obj_ip = []
        obj_of_oseq = {}
        for a, r in enumerate(anims):
            no, ns = rd(r, 4, "<H"), rd(r, 6, "<H")
            fo, fs = rd(r, 12), rd(r, 24)
            if fo + no > C("object"):
                continue
            rng = range(fo, fo + no)
            oids = set()
            for i in rng:
                o = objs[i]
                nx, ch = rd(o, 8), rd(o, 12)
                h("obj.next/child inside its animation's range or -1", (nx == NONE or nx in rng) and (ch == NONE or ch in rng),
                  "obj %d" % i)
                h("obj.name is a string", self.strok(rd(o, 16)), "obj %d" % i)
                h("obj.initparams (+20 first, +04 u8 n) in range", self.valid_index("initparam", rd(o, 32), o[4]), "obj %d" % i)
                obj_ip.append((rd(o, 32), o[4], "obj %d" % i))
                h("obj.objseq (+28) is the running per-object block of n_seq", rd(o, 40) == next_oseq,
                  "obj %d at %d expected %d" % (i, rd(o, 40), next_oseq))
                for j in range(ns):
                    obj_of_oseq[rd(o, 40) + j] = (i, a, j)
                next_oseq = rd(o, 40) + ns
                ext = rd(o, 48)
                h("obj.extend (+30) is -1 or inside the extend pool", ext == NONE or ext < span["extend"], "obj %d" % i)
                h("obj byte +6 (isChildAnimRoot, set at load) is 0 and +7 is 0", o[6] == 0 and o[7] == 0, "obj %d" % i)
                h("obj.id unique within its animation", rd(o, 0) not in oids, "obj %d" % i)
                oids.add(rd(o, 0))
                h("obj +14 +1C +24 +2C +34 zero", all(rd(o, x) == 0 for x in (20, 28, 36, 44, 52)), "obj %d" % i)
                if ns:
                    # animateParamNum (+5) against this object's objseqs
                    pn = [oseqs[rd(o, 40) + j][3] for j in range(ns) if rd(o, 40) + j < C("objseq")]
                    h("obj.animateParamNum (+5) == sum of its objseqs' paramNum", o[5] == sum(pn),
                      "obj %d: %d vs %r" % (i, o[5], pn))
            # tree from the animation's first object
            if no:
                seen = []
                stack = [fo]
                ok = rd(objs[fo], 8) == NONE
                while stack and len(seen) <= no:
                    i = stack.pop()
                    while i != NONE and len(seen) <= no:
                        if i not in rng:
                            ok = False
                            break
                        seen.append(i)
                        if rd(objs[i], 12) != NONE:
                            stack.append(rd(objs[i], 12))
                        i = rd(objs[i], 8)
                h("obj tree from the anim's first object (root has no next) reaches every object once",
                  ok and sorted(seen) == list(rng), "anim %d" % a)
                h("obj table order is the tree's pre-order", self.preorder(objs, fo) == list(rng), "anim %d" % a)
        h("objseq blocks cover the objseq table", next_oseq == C("objseq"), "%d of %d" % (next_oseq, C("objseq")))

        # ---- objseqs ----------------------------------------------------------
        oseq_ip = []
        next_param = 0
        for k, s in enumerate(oseqs):
            nip, npr = s[2], s[3]
            h("objseq.initparams (+08 first, +02 u8 n) in range", self.valid_index("initparam", rd(s, 8), nip), "objseq %d" % k)
            h("objseq.params (+10 first, +03 u8 n) in range", self.valid_index("param", rd(s, 16), npr), "objseq %d" % k)
            if nip:
                oseq_ip.append((rd(s, 8), nip, "objseq %d" % k))
            if npr:
                h("objseq param ranges are consecutive from 0", rd(s, 16) == next_param,
                  "objseq %d at %d expected %d" % (k, rd(s, 16), next_param))
                next_param = rd(s, 16) + npr
            h("objseq +0C +14 zero", rd(s, 12) == 0 and rd(s, 20) == 0)
            if k in obj_of_oseq:
                i, a, j = obj_of_oseq[k]
                sq = rd(anims[a], 24) + j
                if sq < C("sequence"):
                    h("objseq.frameCount (+06) == sequence.frameCount (+04)", rd(s, 6, "<H") == rd(seqs[sq], 4),
                      "objseq %d: %d vs %d" % (k, rd(s, 6, "<H"), rd(seqs[sq], 4)), info=True)
                for p in range(rd(s, 16), rd(s, 16) + npr):
                    if p < C("param"):
                        h("objseq params' parentId (+10) == owning object's id", rd(prms[p], 16) == rd(objs[i], 0),
                          "param %d parent %d obj id %d" % (p, rd(prms[p], 16), rd(objs[i], 0)))
        objseq_param_end = next_param

        # ---- instances (cGUIInst*) --------------------------------------------
        inst_ids = {}
        inst_ip = []
        for i, r in enumerate(insts):
            nx, ch = rd(r, 8), rd(r, 12)
            h("inst.next/child in range or -1", (nx == NONE or nx < C("instance")) and (ch == NONE or ch < C("instance")),
              "inst %d" % i)
            h("inst.name is a string", self.strok(rd(r, 24)), "inst %d" % i)
            h("inst.initparams (+28 first, +10 u32 n) in range", self.valid_index("initparam", rd(r, 40), rd(r, 16)),
              "inst %d" % i)
            inst_ip.append((rd(r, 40), rd(r, 16), "inst %d" % i))
            h("inst.extend (+30) is -1", rd(r, 48) == NONE, "inst %d" % i)
            h("inst.attr (+04) is 0 in files", rd(r, 4) == 0, "inst %d" % i)
            h("inst +14 +1C +24 +2C +34 zero", all(rd(r, x) == 0 for x in (20, 28, 36, 44, 52)), "inst %d" % i)
            h("inst.id unique", rd(r, 0) not in inst_ids, "inst %d id %d" % (i, rd(r, 0)))
            inst_ids[rd(r, 0)] = i
        if insts:
            start = g.hu32(0x24)
            h("StartInstanceIndex (0x24) in range", start < C("instance"), "%d" % start)
            reach = self.preorder(insts, start, limit=C("instance") + 1)
            h("inst tree from StartInstanceIndex reaches every instance once",
              len(reach) == C("instance") and len(set(reach)) == C("instance"), "%d reached of %d" % (len(set(reach)), C("instance")))
            h("inst table order is the tree's pre-order", reach == list(range(C("instance"))), "", info=True)
            h("inst root (StartInstanceIndex) has no next", rd(insts[start], 8) == NONE if start < C("instance") else False,
              info=True)
            idl = [rd(r, 0) for r in insts]
            h("inst table is sorted by id, strictly increasing", all(a < b for a, b in zip(idl, idl[1:])), "")
            h("instance ids are >= 2 (1 is the layout root, see flowprocess params)", min(idl) >= 2, "%d" % min(idl))
            h("InstanceID (0x18) > every instance id", g.hu32(0x18) > max(inst_ids), "%d vs %d" % (g.hu32(0x18), max(inst_ids)))
            h("InstanceID (0x18) == max instance id + 1", g.hu32(0x18) == max(inst_ids) + 1,
              "%d vs %d" % (g.hu32(0x18), max(inst_ids)), info=True)
        h("InstExeParamNum (0x8C) == instance count", g.hu32(0x8C) == C("instance"), "%d vs %d" % (g.hu32(0x8C), C("instance")))

        # ---- init params ------------------------------------------------------
        for k, r in enumerate(ips):
            kind = r[0]
            h("initparam.name is a string", self.strok(rd(r, 16)), "ip %d" % k)
            h("initparam byte +1 (use flag) is 0/1, +2 +3 zero", r[1] in (0, 1) and r[2] == 0 and r[3] == 0, "ip %d" % k)
            h("initparam +04..+0F, +14, +1C zero", rd(r, 4) == 0 and rd(r, 8) == 0 and rd(r, 12) == 0 and rd(r, 20) == 0
              and rd(r, 28) == 0, "ip %d" % k)
            self.value(kind, rd(r, 24), 1, "initparam", "ip %d" % k)
        # who owns which init params
        owners = sorted(obj_ip + oseq_ip + inst_ip)
        cover = collections.Counter()
        for first, n, who in owners:
            for p in range(first, first + n):
                cover[p] += 1
        h("initparam ranges (objects, objseqs, instances) never overlap", all(v == 1 for v in cover.values()), "")
        h("initparam ranges cover the initparam table", len(cover) == C("initparam"), "%d of %d" % (len(cover), C("initparam")))
        pos = 0
        ok = True
        for first, n, who in sorted(obj_ip + oseq_ip, key=lambda t: t[0]) + inst_ip:
            if n:
                ok &= first == pos
                pos = first + n
        h("initparam table order: objects/objseqs blocks, then instances in instance order", ok)

        # init params that reference other things
        guires_ids = {rd(r, 0): rd(r, 16) for r in g.tables["guiresource"]}
        tex_ids = {rd(r, 0) for r in g.tables["texture"]}
        anim_ids = {rd(r, 0): a for a, r in enumerate(anims)}
        gen_ids = {rd(r, 0) for r in g.tables["genresource"]}
        for i, r in enumerate(insts):
            got = {}
            for k in range(rd(r, 40), min(rd(r, 40) + rd(r, 16), C("initparam"))):
                p = ips[k]
                nm = g.string(rd(p, 16))
                got[nm] = (p[0], self.ival(p))
            for nm, (kind, v) in got.items():
                if nm in INSTANCE_ID_PARAMS:
                    h("instance-id params (%s...) name -1 or an instance id of the file" % "CursorInstID", v == -1 or v in inst_ids,
                      "inst %d %s=%d" % (i, nm, v))
            if "ResourceId" in got:
                rid = got["ResourceId"][1]
                h("ResourceId (t0D) is -1 or a guiresource id of the file", rid == -1 or rid in guires_ids,
                  "inst %d ResourceId %d" % (i, rid))
                aid = got.get("AnimationId", (0, None))[1]
                sid = got.get("SequenceId", (0, None))[1]
                if rid == -1:
                    ok = aid in anim_ids
                    h("AnimationId (t0B) with ResourceId -1 is an animation id of the file", ok, "inst %d aid %d" % (i, aid))
                    if ok:
                        self.seq_rule(g, anims[anim_ids[aid]], sid, "inst %d" % i, "local")
                elif rid in guires_ids and self.root:
                    other = self.load_other(g.string(guires_ids[rid]))
                    if other is not None:
                        oids = {rd(x, 0): x for x in other.tables["animation"]}
                        ok = aid in oids
                        h("AnimationId with ResourceId set is an animation id of that other file", ok,
                          "inst %d aid %d in %s" % (i, aid, g.string(guires_ids[rid])))
                        if ok:
                            self.seq_rule(other, oids[aid], sid, "inst %d" % i, "other file")
        for k, r in enumerate(ips):
            nm = g.string(rd(r, 16))
            if r[0] == 7 and nm == "Texture":
                v = self.ival(r)
                h("Texture (t07) is -1 or a texture id of the file", v == -1 or v in tex_ids, "ip %d %d" % (k, v))
            if r[0] == 0x13:
                v = self.ival(r)
                h("Resource (t13, cGUIObjEffect) is a genresource id of the file", v in gen_ids, "ip %d %d" % (k, v))
            if r[0] == 0x0E:
                v = self.ival(r)
                if v != -1 and v >> 24 == 1 and self.root:
                    glob = self.load_other("ui/font/common_filter_world")
                    h("FontFilter (t0E) 0x01000000|id is a filter id of ui/font/common_filter_world (global)",
                      glob is not None and (v & 0xFFFFFF) in fontfilter_ids(glob), "ip %d %#x" % (k, v))
                elif v != -1 and v >> 24 == 0:
                    h("FontFilter (t0E) below 0x01000000 is a filter id of the file's own font filters",
                      v in fontfilter_ids(g), "ip %d %d" % (k, v))

        # ---- keyframed params -------------------------------------------------
        nexk = 0
        for k, r in enumerate(prms):
            kind, n = r[0], r[1]
            h("param.name is a string", self.strok(rd(r, 24)), "param %d" % k)
            h("param +02 +03 zero, +04..+0F +14 +1C +24 +2C zero", r[2] == 0 and r[3] == 0 and all(
                rd(r, x) == 0 for x in (4, 8, 12, 20, 28, 36, 44)), "param %d" % k)
            h("param.keys (+20 first, +01 u8 n) in range", self.valid_index("key", rd(r, 32), n), "param %d" % k)
            h("param key ranges are consecutive and in param order", rd(r, 32) == nexk, "param %d at %d expected %d" % (
                k, rd(r, 32), nexk))
            nexk = rd(r, 32) + n
            self.value(kind, rd(r, 40), n, "param", "param %d" % k)
        h("param key ranges cover the key table", nexk == C("key"), "%d of %d" % (nexk, C("key")))
        st = g.hu32(0x198)
        h("InstanceParamEntryStartIndex (0x198) <= param count", st <= C("param"), "%d vs %d" % (st, C("param")))
        fp_next = objseq_param_end
        for k, r in enumerate(fps):
            n, first = rd(r, 8), rd(r, 12)
            h("flowprocess.params (+0C first, +08 n) in range", self.valid_index("param", first, n), "fp %d" % k)
            if n:
                h("flowprocess param ranges follow the objseq params, consecutive", first == fp_next,
                  "fp %d at %d expected %d" % (k, first, fp_next))
                fp_next = first + n
                for p in range(first, first + n):
                    if p < C("param"):
                        h("flowprocess params' parentId is an instance id or 1 (the layout root)",
                          rd(prms[p], 16) in inst_ids or rd(prms[p], 16) == 1, "param %d parent %d" % (p, rd(prms[p], 16)))
            nxt = rd(r, 32)
            h("flowprocess.next (+20) is -1 or a flow index", nxt == NONE or nxt < C("flow"), "fp %d" % k)
            h("flowprocess.actions (+10 n, +28 index) are 0 (no action records)", rd(r, 16) == 0 and rd(r, 40) == 0, "fp %d" % k)
        h("flowprocess params end at InstanceParamEntryStartIndex", fp_next == st, "%d vs %d" % (fp_next, st))
        # one entry per instance from InstanceParamEntryStartIndex
        type2 = {rd(f, 0) for f in flows if rd(f, 4) == 2}
        entry_parents = []
        for k in range(min(st, C("param")), C("param")):
            r = prms[k]
            entry_parents.append(rd(r, 16))
            h("instance entries: type 3 (kv8), empty name", r[0] == 3 and g.string(rd(r, 24)) == "", "param %d" % k)
            fr = [rd(keys[rd(r, 32) + j], 0) for j in range(r[1]) if rd(r, 32) + j < C("key")]
            h("instance entries: one key per type-2 flow, key frame == flow id, mode 0",
              sorted(fr) == sorted(type2), "param %d frames %r flows %r" % (k, fr, sorted(type2)))
        h("instance entries: parentIds are exactly the instance ids", sorted(entry_parents) == sorted(inst_ids),
          "%d entries, %d instances" % (len(entry_parents), len(inst_ids)))
        h("instance entries are in instance order", entry_parents == [rd(r, 0) for r in insts], "", info=True)

        # ---- keys -------------------------------------------------------------
        for k, r in enumerate(keys):
            mode = r[3]
            if mode == 8:
                h("key mode 8 (+03): +08 is an extend-pool offset", rd(r, 8) < span["extend"], "key %d" % k)
            else:
                h("key mode != 8: +08 is 0", rd(r, 8) == 0, "key %d mode %d" % (k, mode))
            h("key +04 +0C zero", rd(r, 4) == 0 and rd(r, 12) == 0, "key %d" % k)

        # ---- flows ------------------------------------------------------------
        fids = set()
        for k, r in enumerate(flows):
            t, tgt = rd(r, 4), rd(r, 24)
            h("flow.name is a string", self.strok(rd(r, 16)), "flow %d" % k)
            h("flow.type (+04) in 0..2", t in (0, 1, 2), "flow %d" % k)
            h("flow.id unique", rd(r, 0) not in fids)
            fids.add(rd(r, 0))
            if t == 2:
                h("flow type 2: +18 is a flowprocess index", tgt < C("flowprocess"), "flow %d %d" % (k, tgt))
            if t == 0:
                h("flow type 0 (Start): +18 is -1 or a flow index", tgt == NONE or tgt < C("flow"), "flow %d %d" % (k, tgt))
            if t == 1:
                h("flow type 1 (End): +18 is 0", tgt == 0, "flow %d %d" % (k, tgt))
        if flows:
            h("StartFlowIndex (0xA0) is a flow index", g.hu32(0xA0) < C("flow"))
            h("FlowID (0x1C) > every flow id", g.hu32(0x1C) > max(fids), "%d vs %d" % (g.hu32(0x1C), max(fids)))
            h("StartFlowIndex names the flow called 'Start'", g.string(rd(flows[g.hu32(0xA0)], 16)) == "Start"
              if g.hu32(0xA0) < C("flow") else False)

        # ---- resources --------------------------------------------------------
        tids = set()
        for k, r in enumerate(g.tables["texture"]):
            h("texture.path (+30) is -1 or a string", rd(r, 48) == NONE or self.strok(rd(r, 48)), "tex %d" % k)
            h("texture.name (+38) is a string", self.strok(rd(r, 56)), "tex %d" % k)
            h("texture.id unique", rd(r, 0) not in tids)
            tids.add(rd(r, 0))
        gids = set()
        for k, r in enumerate(g.tables["guiresource"]):
            h("guiresource.path (+10) is a string", self.strok(rd(r, 16)), "gres %d" % k)
            h("guiresource.id unique", rd(r, 0) not in gids)
            gids.add(rd(r, 0))
            h("guiresource +04..+0F +14 zero", rd(r, 4) == 0 and rd(r, 8) == 0 and rd(r, 12) == 0 and rd(r, 20) == 0)
            if self.root:
                h("guiresource.path names a .gui of the ui folder", self.load_other(g.string(rd(r, 16))) is not None,
                  g.string(rd(r, 16)))
        for k, r in enumerate(g.tables["genresource"]):
            h("genresource.path (+10) is -1 or a string", rd(r, 16) == NONE or self.strok(rd(r, 16)), "genres %d" % k)
        # ---- font filters (variable records) ----------------------------------
        blob, n, o = g.fontfilter, g.fontfilter_count, 0
        ok = True
        for _ in range(n):
            if o + 4 > len(blob):
                ok = False
                break
            c = struct.unpack_from("<I", blob, o)[0]
            if c not in FONTFILTER_SIZES:
                ok = False
                break
            o += FONTFILTER_SIZES[c]
        h("fontfilter records walk by class size, rest is zero padding < 16", ok and len(blob) - o < 16 and not any(blob[o:]),
          "walked %d of %d" % (o, len(blob)))
        return self.errs

    # -----------------------------------------------------------------------
    def preorder(self, recs, start, limit=None):
        out, stack = [], [start]
        limit = limit or len(recs) + 1
        while stack and len(out) < limit:
            i = stack.pop()
            if i == NONE or i >= len(recs):
                continue
            out.append(i)
            nx, ch = rd(recs[i], 8), rd(recs[i], 12)
            if nx != NONE:
                stack.append(nx)
            if ch != NONE:
                stack.append(ch)
        return out

    def ival(self, p):
        kind, raw = p[0], rd(p, 24, "<Q")
        pool = VALUE_POOL.get(kind)
        if pool is None:
            return struct.unpack("<i", struct.pack("<I", raw & 0xFFFFFFFF))[0]
        pp = self.g.pools[pool]
        if raw + 4 > len(pp):
            return None
        return struct.unpack_from("<i", pp, raw)[0]

    def value(self, kind, raw, n, what, where):
        pool = VALUE_POOL.get(kind)
        h = self.h
        if pool is None:
            if what == "initparam":
                h("initparam inline types are 0x11 (bool) and 0x12 (enum)", kind in (0x11, 0x12), "%s type %#x" % (where, kind))
            else:
                h("param non-pooled types: 0x0F (event)", kind == 0x0F, "%s type %#x" % (where, kind))
            return
        size = VALUE_SIZE[kind]
        if raw == NONE:
            h("%s pooled value offset is not -1" % what, False, where)
            return
        h("%s value (%s) offset + n*size inside the pool" % (what, pool), raw + max(n, 1) * size <= len(self.g.pools[pool]),
          "%s type %#x off %#x n %d" % (where, kind, raw, n))
        h("%s value offset aligned to min(size, 4) (kv128: 16)" % what,
          raw % (16 if pool == "kv128" else min(size, 4)) == 0, "%s type %#x off %#x" % (where, kind, raw))
        if kind == 6:
            for j in range(n):
                so = struct.unpack_from("<Q", self.g.pools[pool], raw + 8 * j)[0]
                h("type 6 values are string offsets", so == NONE or self.strok(so), where)

    def seq_rule(self, gg, anim, sid, where, kind):
        first, n = rd(anim, 24), rd(anim, 6, "<H")
        sids = [rd(gg.tables["sequence"][first + j], 0) for j in range(n)]
        self.h("SequenceId (t10) is a sequence id of the named animation (%s)" % kind, sid in sids,
               "%s sid %r in %r" % (where, sid, sids[:8]))

    _cache = {}

    def load_other(self, path):
        if path is None:
            return None
        f = os.path.join(self.root, *path.replace("\\", "/").split("/")[1:]) + ".gui"
        if f not in Checker._cache:
            Checker._cache[f] = Gui.parse(open(f, "rb").read()) if os.path.exists(f) else None
        return Checker._cache[f]


def check(g, stats=None, root=None):
    return Checker(g, stats, root).run()


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "stats":
        root = sys.argv[2]
        stats = {}
        files = ui_files(root)
        perfile = collections.Counter()
        for f in files:
            before = {k: v[1] for k, v in stats.items()}
            check(Gui.parse(open(f, "rb").read()), stats, root)
            for k, v in stats.items():
                if v[1] > before.get(k, 0):
                    perfile[k] += 1
        print("%d files" % len(files))
        for rule, (ok, bad, ex, info) in stats.items():
            print("%-7s %-100s pass %-7d fail %-6d%s" % ("INFO" if info else "OK" if not bad else "BROKEN", rule, ok, bad,
                                                        "" if not bad else " in %d files, e.g. %s" % (perfile[rule], ex)))
        return 0
    rc = 0
    for f in sys.argv[1:]:
        errs = check(Gui.parse(open(f, "rb").read()))
        print("%s: %s" % (f, "consistent" if not errs else "%d problems" % len(errs)))
        for e in errs[:40]:
            print("  " + e)
        rc |= bool(errs)
    return rc


if __name__ == "__main__":
    sys.exit(main())
