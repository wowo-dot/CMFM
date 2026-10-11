# GUI layout format study: cross-references, rebuild, append and graft

> Tracked copy (2026-10-11) of the study written in `local/re/gui/`. The
> parser, rule checker and graft (`gui_rebuild.py`, `gui_check.py`,
> `gui_graft.py`) now live in `tools/gui/` and build CMFM's layout
> (`tools/gui/make_mods_gui.py`); the survey scripts and their output stay
> in `local/re/gui/`. The IDA scroll-bar study it cites is
> `docs/research/scrollbar.md`.

Date 2026-10-11. Exe 421810. Static work only: no IDA, no debugger, no game.
Everything here lives in `local/re/gui/` (untracked).

Sources of the field meanings, in order of weight:

1. **The loader's fix-up**, `sub_1422A7310` (decompiled earlier, saved as
   `local/re/rGUI_fixup_1422A7310.c`). It turns every header offset and
   every index field into a pointer, so it names exactly which field points
   into which table or pool, with which record size.
2. **Fexty's structure names** (Ghidra export, `local/ghidra/export/structs.tsv`):
   `GUIFile`, `nGUI::OBJECT`, `nGUI::INSTANCE`, `nGUI::OBJ_SEQUENCE`,
   `nGUI::INIT_PARAM`, `nGUI::PARAM`, `nGUI::KEY`, `nGUI::FLOW`,
   `nGUI::FLOW_PROCESS`, `nGUI::TEXTURE`, `nGUI::SEQUENCE`.
3. **Statistics over all 303 `.gui` files** of `chunk\ui` (`gui_check.py
   stats`, output in `stats.txt`): 101 rules hold in every file, 3 hold
   except for 8 stale references in 5 game files, 5 observations are
   *not* rules (marked INFO).
4. **The IDA scroll-bar study** done in parallel (`local/re/scrollbar-study.md`,
   sections 7-8) agrees on every point it covers and adds the run-time
   side: slot 0xE0 is the flow table (`gui_dump.py`'s "refs" label is
   wrong), the GUI references are slot 0x150; `rGUI::getGuiResourceFromId`
   (0x1422A6F60) and `rGUI::getAnimationFromId` (0x1422A6E20) look up the
   ResourceId / AnimationId by the records' +0 ids; `uGUI::setResource`
   binds the instance entries by instance id; `uGUI::getInstanceFromName`
   scans the instance table.

Confidence used below: **high** = in the fix-up *and* holds in all 303
files; **stats** = holds in all files (or all but a few stale game
references), not seen in code; **loader only** = in the fix-up, but the
table is empty in every game file; **unknown** = meaning not established.

## Naming: old CMFM names and the game's names

| slot | CMFM's older tools (`gui_dump.py`, `gui_edit.py`) | game name (this study) | record |
|---|---|---|---|
| 0xA8 | animations | ANIMATION | 32 |
| 0xB0 | sequences | SEQUENCE | 16 |
| 0xB8 | "instances" | **OBJECT** (`cGUIObj*`, parts of an animation) | 56 |
| 0xC0 | "keysets" | **OBJ_SEQUENCE** (objseq) | 24 |
| 0xC8 | "params" | **INIT_PARAM** | 32 |
| 0xD0 | "anim params" | **PARAM** (keyframed) | 48 |
| 0xD8 | "objects" | **INSTANCE** (`cGUIInst*`, the layout tree) | 56 |
| 0xE0 | "refs" | **FLOW** | 32 |
| 0xE8 | "table e8" | **FLOW_PROCESS** | 48 |
| 0x170 | "ext params" | **KEY** | 16 |
| 0x178/180/188/190 | pools | KeyValue8 / KeyValue32 / KeyValue128 / ExtendData | - |

`gui_dump.py`'s object columns: "keyset" = OBJ_SEQUENCE index, "extra" =
extend-data offset, the bytes in `[..]` = `id, initParamNum (u8),
animateParamNum (u8), isChildAnimRoot (u8), pad, nextIndex, childIndex`.

Doc fix for `docs/research/native-menu.md` ("Format details"): it says
types 2 *and* 4 point into the pool at 0x188. Type 2 (float) points into
0x180 (KeyValue32); only type 4 (four floats) uses 0x188.
`tools/gui/gui_edit.py`'s docstring is correct.

## File layout

- Header 0x1B0 bytes (`GUIFile`). Then the sections in **one fixed order**:
  animation, sequence, object, objseq, initparam, param, instance, flow,
  flowprocess, flowinput, flowswitch, flowfunction, action,
  inputcondition, switchoperator (0x118), switchcondition (0x120),
  variable, texture, font, fontfilter, message, guiresource, genresource,
  key, then the pools kv8, kv32, kv128, extend, strings, then the vertex
  data. (The dumper's table looked like "strings before extend" in 57
  files: there the extend pool is empty and shares the strings' offset.)
- Every section starts 16-byte aligned; padding is zeros; an empty section's
  offset is the next section's start; the camera table (0x160) is always
  empty with offset 0. Proof: rebuilding all 303 files from records alone
  (`rt_canonical.py`, no stored padding) is byte-identical.
- Header 0x08 FileSize == file size (all files). 0x1A0 VertexOffset +
  0x90 VertexBufferSize == file size (all files): the vertex data closes
  the file. Offsets are u32 in u64 slots (high dwords always 0).
- **No absolute file offsets inside records.** Every record field is an
  index (times a record size) or an offset relative to a pool's start; the
  only absolute values are the header's section offsets and 0x1A0, which
  `build()` recomputes. So sections can move and grow freely.

## Header (GUIFile)

| off | name | meaning | conf. |
|---|---|---|---|
| 0x00 | FileType | `GUI\0` | high |
| 0x04 | Version | 0x23316 | high |
| 0x08 | FileSize | must equal the file size | high (all files) |
| 0x0C | Attr | 0/1/2 | unknown |
| 0x10 | RevisionDate | time_t (2017..2019 dates) | name |
| 0x18 | InstanceID | next free instance id: **> every instance id** (== max+1 in 222 files, larger in 70: ids are not reused) | stats |
| 0x1C | FlowID | next free flow id (> every flow id) | stats |
| 0x20 | VariableID | 1 (variables never present) | name |
| 0x24 | StartInstanceIndex | instance index of the tree's root | stats |
| 0x28..0x8C | counts | see the tables; 0x68 is SwitchOperatorNum (for 0x118), out of slot order | high |
| 0x8C | InstExeParamNum | **== instance count** (= number of instance entries, below) | stats |
| 0x90 | VertexBufferSize | size of the vertex data at 0x1A0 | high |
| 0x94 | OptionBitFlag | 0/1/2/0x20; copied into rGUI | loader |
| 0x98/0x9C | ViewSize | 1920 x 1080 (two files 1920 x 1820) | high |
| 0xA0 | StartFlowIndex | flow index, always the flow named "Start" | high |
| 0xA8..0x190 | section offsets | see layout | high |
| 0x198 | InstanceParamEntryStartIndex | param index where the instance entries begin | high |
| 0x1A0 | VertexOffset | absolute | high |

## Cross-reference map

"→ T[i]" = index into table T; "→ pool+o" = offset into that pool;
"(n at +X)" = the count of a range starting at the index.

### ANIMATION (0xA8, 32 B)
| field | meaning | conf. |
|---|---|---|
| +00 u32 | id (unique per file; what `AnimationId` names) | stats |
| +04 u16 / +06 u16 | object count / sequence count | high |
| +08 u32 | unknown (two u16, small numbers; not the object count, not a max id) | unknown |
| +0C u32 | → object[first]; the animations' ranges partition the object table in order | high |
| +10 u32 | name → strings | high |
| +18 u32 | → sequence[first]; ranges partition the sequence table in order | high |
| first object | always `cGUIObjRoot` with 0 init params | stats |

### SEQUENCE (0xB0, 16 B)
+00 id (what `SequenceId`/`ChildSequenceId` name; values 1.., 10001..,
1000001..), +04 frameCount, +08 name → strings. high.

### OBJECT (0xB8, 56 B; `cGUIObj*`)
| field | meaning | conf. |
|---|---|---|
| +00 | id, unique within its animation; keyframed params name it (`parentId`) | stats |
| +04 u8 | initParamNum; +20 → initparam[first] | high |
| +05 u8 | animateParamNum == sum of its objseqs' paramNum | stats |
| +06 u8 | isChildAnimRoot: 0 in files, set by the loader | high |
| +08 / +0C | nextIndex / childIndex → object, **inside its animation's range**, -1 = none; tree from the animation's first object reaches every object once; table order is the tree's pre-order (all files) | stats |
| +10 | name → strings | high |
| +18 | class DTI id | high |
| +28 | → objseq[first], a block of n_seq (animation +06) objseqs; the blocks run consecutively over the whole objseq table | high |
| +30 | → extend+o, or -1. Only `cGUIObjChildAnimationRoot` (3247) and `cGUIObjTextureSet` (1827) have one; its size is not known | high / size unknown |

### OBJ_SEQUENCE (0xC0, 24 B)
+00 u16 attr, +02 u8 initParamNum, +03 u8 paramNum, +04 u16 loopStart,
+06 u16 frameCount (not equal to the sequence's in 45 %), +08 →
initparam[first] (per-sequence overrides), +10 → param[first]. Param ranges
run consecutively from param 0. high (attr/loopStart: name only).

### INIT_PARAM (0xC8, 32 B)
+00 u8 type, +01 u8 "use" flag (0/1), +10 name → strings, +18 value
(inline, or pool offset by type; table below). The init-param ranges of
objects, objseqs and instances never overlap and cover the table; order:
object/objseq blocks first, then the instances' blocks in instance order.
high.

### PARAM (0xD0, 48 B; keyframed)
+00 u8 type, +01 u8 key count, +10 parentId, +18 name → strings, +20 →
key[first] (key ranges consecutive in param order, covering the key
table), +28 value: an array of key-count values in the pool of its type.
The param table has **three consecutive regions**:
1. params of objseqs (parentId = the owning object's id);
2. params of flow processes (parentId = an instance id, or 1 = the layout
   root; e.g. `null_all` ColorScale fading during "in"/"out");
3. from header 0x198: **one instance entry per instance** (parentIds are
   exactly the instance ids, in no particular order), type 3, empty name,
   one key per type-2 flow whose *frame field holds the flow id*, kv8
   values (one byte per flow; meaning not established, see risks).
All stats (regions 1-2 also by the loader's pointers).

### INSTANCE (0xD8, 56 B; `cGUIInst*`)
| field | meaning | conf. |
|---|---|---|
| +00 | id, unique, >= 2; **the table is sorted by id, strictly increasing** (all files) | stats |
| +04 | attr: 0 in files (the loader sets bit 1 for some classes) | high |
| +08 / +0C | nextIndex / childIndex → instance, -1 = none; the tree from StartInstanceIndex reaches every instance once. Table order is *not* the tree's pre-order (215 files), the root may have siblings (66 files) | stats |
| +10 u32 | initParamNum; +28 → initparam[first] | high |
| +18 | name → strings | high |
| +20 | class DTI id | high |
| +30 | extend offset: always -1 | high |

### FLOW (0xE0, 32 B) and FLOW_PROCESS (0xE8, 48 B)
- FLOW: +00 id, +04 type, +10 name → strings, +18 by type: 0 ("Start") →
  flow index or -1; 1 ("End") → 0; 2 (e.g. "main", "in", "out") →
  flowprocess index. The loader turns type 0's +18 into a flow pointer and
  types 3/4/5 into FlowInput/FlowSwitch/FlowFunction pointers (types 3-5
  never occur). high.
- FLOW_PROCESS: +00 isLoop:8/loopStart:24, +04 totalFrame, +08 paramNum,
  +0C → param[first] (region 2), +10 actionNum (0), +14 endConditionType,
  +18 endConditionParam, +20 next → flow or -1, +28 → action (0). high.

### Tables that are empty in every game file (loader only)
flowinput (+08 → inputcondition), flowswitch (+08 → switchcondition),
flowfunction (+00, +08 → flow or -1), action (24 B, +10 string),
inputcondition (8 B, +00 → flow), switchoperator (12 B, no pointers),
switchcondition (24 B, +08 → flow or -1, +10 → switchoperator or -1),
variable (40 B, +08 string, +10 DTI id), font (24 B, +10 path or -1),
message (24 B, +10 path), camera (56 B, +30 string or -1).

### Resources
- TEXTURE (0x130, 64 B): +00 id (what `Texture` params name), +04 u16
  isRenderTarget, +0C texRes, +10.. clamp/invSize floats, +30 path or -1,
  +38 name. high.
- FONTFILTER (0x140): **variable-size records** `{u32 class DTI id, u32
  id, payload}`: cGUIFontFilterBorder 20 B, Shadow 20 B, DistanceField
  64 B, GradationOverlay 16 B, class 0 = 8 B; the walk accounts for every
  file's blob. Handled by `sub_1422A67E0` (not read). Kept as one blob.
  stats.
- GUIRESOURCE (0x150, 24 B; the **GUI references**, not slot 0xE0): +00 id
  (what `ResourceId` names; looked up by id, `rGUI::getGuiResourceFromId`),
  +08 runtime rGUI* (0 in the file), +10 path of another `.gui` (every path
  exists in the extract). The loader loads them when the rGUI's attribute
  has bit 0x10. In mods.gui all 8 references are in use, so a ninth is
  needed for scrollbar.gui. high.
- GENRESOURCE (0x158, 24 B): +00 id (what cGUIObjEffect `Resource` names),
  +04 class DTI id, +10 path or -1. high.

### KEY (0x170, 16 B)
+00 frame:24 | mode:8 (modes 0, 3 most common; 2, 5, 6, 7, 8), +08: for
mode 8 an extend-pool offset (curve data, size not known), else 0. high.

### Pools and strings
- kv8 (type 3, bools), kv32 (types 1, 2, 7-0xE, 0x10, 0x13, 0x14; type 6
  keeps a u64 string offset here), kv128 (type 4, 16-byte aligned), extend.
  Values are shared between parameters (gui_edit.py's `sharers`). Offsets
  are aligned to min(size, 4), kv128 to 16 (all files).
- Strings: NUL-terminated, shared, every reference points at a string
  start (no suffix sharing). The instance entries all name one empty
  string (in mods.gui the one at 0xDCB, the pool's last string); appended
  strings go after the pool's whole span (0xDD0 in mods.gui), so it stays
  intact.

### Parameter types and what their values name
| type | storage | meaning / reference | conf. |
|---|---|---|---|
| 1 | kv32 int | numbers; several **name an instance id**: CursorInstID, MsgInstID, Next/PrevArrowInstID, SyncCursorID, SyncListID, GridInstID[0..7] (all values -1 or an instance id of the file) | stats |
| 2 | kv32 float | | high |
| 3 | kv8 | bool (Visible, Play, ...) | high |
| 4 | kv128 | 4 floats (Size, colours) | high |
| 6 | kv32 → string | (never used by init params in the game) | loader |
| 7 | kv32 | texture id (`Texture`, material textures) | stats |
| 8 | kv32 | font (system font number; font table always empty) | stats |
| 0xB | kv32 | **AnimationId**: an animation id of the file named by ResourceId (3785 of 3786 local, 1848 of 1849 in another file) | stats |
| 0xC | kv32 | ChildAnimFrame (keyframed) | name |
| 0xD | kv32 | **ResourceId**: -1 = this file, else a guiresource id | stats |
| 0xE | kv32 | font filter: -1, an id of the file's own filters, or 0x01000000 + id of `ui\font\common_filter_world`'s filters | stats |
| 0x0F | inline (keyframed only) | Event00.. | stats |
| 0x10 | kv32 | **SequenceId** / ChildSequenceId: a sequence id of that animation | stats |
| 0x11 / 0x12 | inline | bool / enum | stats |
| 0x13 | kv32 | cGUIObjEffect `Resource`: a genresource id | stats |
| 0x14 | kv32 | FontStyleId | name |

Unknown meaning, copied unchanged by the graft with a warning: int
params such as ChildAnimCursorEffectID, CursorID, GaugeID, ChildItemGridId,
ChildNewMarkId, MsgNumObjID/MsgMaxNumObjID (look like object ids inside
the instance's animation).

## Round trip

`gui_rebuild.py` parses a file into header, tables (lists of raw
records), the font-filter blob, pools (raw spans, padding included) and
the vertex data, and builds it back in the canonical order.

**Result: 303 of 303 files byte-identical** (`py -3.12 -B gui_rebuild.py
roundtrip <ui>`), also with every stored padding dropped (`rt_canonical.py`,
304 of 304 with mods.gui).

## Append and graft

**Append API** (`Gui`): `append_record(table, rec)` (end of a table),
`append_pool(pool, bytes)` (after the pool's current span, aligned),
`add_string(s)` (reuses an existing equal string). `build()` recomputes
every count, section offset, the vertex offset and the file size. Because
only ends grow, every existing index and offset stays valid. Test
(`test_append.py`): a string, a value in each pool and a copied record in
four tables appended to every one of the 303 files: all rebuild, reparse,
keep every old record and pool byte, and report no new rule break.

Inserting in the middle is not implemented. It would need shifting every
field that points past the insertion point (the map above lists them; e.g.
a param insert shifts objseq +10, flowprocess +0C, header 0x198 and, for
keys, every param +20 after it).

**Graft** (`gui_graft.py`, `graft(dst, src, root_name, parent_name)`):
1. the subtree (root + descendants, not the root's siblings) in pre-order;
2. new instances at the end of the instance table with ids from the
   target's InstanceID counter, so the table stays sorted by id; links
   remapped inside the subtree; the root becomes the parent's last child
   (or first); InstanceID += n; InstExeParamNum = instance count;
3. their init params at the end of the init-param table (the instance
   block is last there, in instance order, as in the game's files); names
   through `add_string`; every pooled value copied into a fresh pool entry
   (never shared with the target, so later in-place edits stay local);
4. references: ResourceId mapped by path to the target's guiresource, or a
   new guiresource record (id = max + 1); AnimationId/SequenceId kept (they
   name things in that other file); instance-id params remapped inside the
   subtree, **refused** when they point outside it; ResourceId -1 (a local
   animation) refused, since copying animations (objects, objseqs, params,
   keys, textures, extend data) is not implemented; texture/font/
   font-filter/effect references refused (no game instance has them);
5. one instance entry per new instance at the end of the param table,
   keys at the end of the key table, one key per target type-2 flow (frame
   = target flow id, order as the target's first entry), name = the empty
   string the target's entries already use, values from the source entry
   by flow *name* or, with `entries_like=<instance>`, copied from that
   target instance's entry (`rename={old: new}` renames the copied
   instances, e.g. to CMFM's own `cmfm_scroll_*` names);
6. flow-process params that animate a subtree instance are refused (they
   sit in region 2, the middle of the param table).

Graft robustness test: every uniquely named subtree of 8 game layouts
grafted into mods.gui: 17 grafted and pass every rule; 41 refused with a
reason (29 local animations, 4 flow animations, 8 instance ids outside
the subtree such as `num_input_*.MsgInstID`); none produced a broken file.

## Proof: the scrollbar in a copy of mods.gui

`make_scrollbar_proof.py` grafts `null_scrollbar_root` (cGUIInstNull) with
`scrollbar_auto_base` and `scrollbar_auto` (cGUIInstScrollBar) from
`ui\pc\graphic_option.gui` (SHA-256 a0e4092c...2b99) under
`null_prog_color0` (the mod list's parent) of a copy of
`assets/nativePC/ui/cmfm/mods.gui` (7230787d...0e36, not modified):

- output `local/re/gui/mods_with_scrollbar.gui`, 149,472 bytes (+3,200),
  SHA-256 `3a95cc8058827029c555a29b9cfdddc479dd7f30da00876b2c76b95f856f50a1`;
- new instances 91, 92, 93 with ids 100, 101, 102 (InstanceID 100 -> 103,
  InstExeParamNum 91 -> 94), 75 init params, 3 instance entries named
  0xDCB with in/main/out = 1/1/1 copied from `btnlist_camera` (as every
  instance of mods.gui; graphic_option has 0/1/1 for its bars, used when
  `entries_like` is not given), 9 keys, 8 new strings (3 instance names,
  the path, ScrollType, MaxFixFrame, BarMinPos, BarMaxPos; the rest
  reused): the same record list the IDA study derived (its section 8);
- guiresource `ui\common\scrollbar` appended with id 9; both bars'
  ResourceId 13 -> 9; AnimationId 4 / 3 (`scroll_base_auto` /
  `scroll_bar_auto` in scrollbar.gui) and SequenceId 1 kept;
- checks: reparse + rebuild identical; `gui_check` with the extract finds
  no broken rule (AnimationId/SequenceId followed into scrollbar.gui);
  every old record and pool byte unchanged except one field
  (`a_title_menu_camera` +08, the parent's last child now points at the
  new root) and the header counters/offsets; copied init params equal the
  source's field by field except the remapped ResourceId;
  `tools/re/gui_dump.py` and `tools/gui/gui_edit.py` (`Scene.walk`: 94 of
  94 instances) read the file.

## Open risks and what only the game can answer

1. **Whether the new reference loads.** The lookup by id is confirmed by
   the IDA study; the loading (fix-up, rGUI flag 0x10) is the same path
   that already loads CMFM's slider reference, so the ninth reference
   should load the same way, but no grown layout has been loaded yet.
   This is the first thing a game run must show.
2. **AnimationId / SequenceId**: ids of an animation in the referenced file
   (confirmed by the IDA study: 1 `scroll_bar`, 2 `scroll_base`,
   3 `scroll_bar_auto`, 4 `scroll_base_auto`) and of one of its sequences
   (statistics). The proof keeps graphic_option's "auto" pair (4/3); the
   IDA study recommends the plain pair (2/1), a value edit after the graft.
3. **Instance entries' bytes** (one per type-2 flow): bound by instance id
   (IDA study); the meaning of the byte is not established (likely
   active/visible per flow state). The proof uses mods.gui's own 1/1/1.
4. **Order assumptions:** the game's instance tables are always sorted by
   id (kept by the graft); entry order and tree pre-order are arbitrary in
   the game's files (so appending is consistent with them); name lookups
   scan the table. The CMFM layout already runs with a non-pre-order tree
   (make_mods_gui.py's relinks, tested in game).
5. **The page's code** (uGUIOption, which CMFM's page reuses) indexes
   instances by fixed index; appending keeps every existing index, so that
   still holds. The scrollbar itself needs code to drive it (value, range,
   visibility); none of that is in the layout. The copied geometry
   (PosX 610, PosY 16, MaxFixFrame/BarMaxPos 590) is graphic_option's and
   must be fitted to CMFM's list.
6. **Unknown fields** not touched by the graft: animation +08, objseq
   attr/loopStart, key modes, extend-data sizes, the font-filter payloads
   (`sub_1422A67E0` not read), the vertex data. The always-empty tables are
   written by `build()` but no file with them non-empty has been tested.
7. Possible run-time limits on a layout's size or counts were not looked
   for (the file grew by 3,200 bytes; the loader reads the whole file and
   allocates from the header counts).

## Files

- `gui_rebuild.py`: parse/build, append API, `roundtrip` and `check` commands.
- `gui_check.py`: the 109 rules; `stats <ui>` over all files, or check files.
- `gui_graft.py`: the graft (library and command line).
- `make_scrollbar_proof.py`: the proof above; `test_append.py`: append and
  graft robustness tests.
- `gui_show.py`, `gui_tree.py`, `peek_flows.py`: read-only viewers.
- `survey_*.py`, `rt_canonical.py`: the statistics behind this note;
  `stats.txt`, `survey_fields.txt`, `survey_types.txt`: their output.
- `mods_with_scrollbar.gui` (+ `mods_with_scrollbar_dump.txt`): the proof
  output. Not installed anywhere.
