# The game's scroll bar (cGUIInstScrollBar): static study, 2026-10-11

> Tracked copy (2026-10-11) of the study written as
> `local/re/scrollbar-study.md`; its helper scripts and dumps stay in
> `local/re/scrollbar/`. The layout format and the graft that adds the bars
> to CMFM's layout: `docs/research/gui-format.md`, `tools/gui/`.

Static analysis only (IDA on the private `local/ida/mhw421810/mhw.i64`, built from the clean image
`local/ida/mhw421810/mhw.exe`, SHA-256 `a78ea69b0566414a42f16d0f8ff7d729e095041e2ae76a34cc02dbe0939f1d80`,
exe 421810) plus read-only dumps of the game's layout files. Addresses: IDA EA (0x14…) and RVA from image
base 0x140000000. **C** = confirmed from decompiler/disassembly or file bytes; **H** = hypothesis.

Helpers and dumps written for this study (all under `local/re/scrollbar/`): `dump_inst_params.py`
(instance parameters of a .gui), `dump_anims.py` (animations, parts, keysets, keyframes),
`survey_scrollbars.py` (every scroll bar in `chunk/ui`), and their outputs `scrollbar_gui_dump.txt`,
`scrollbar_anims.txt`, `scrollbar_gui_inst_params.txt`, `graphic_option_gui_dump.txt`,
`graphic_option_inst_params.txt`, `survey.txt`. IDA write-back: names with the `_cmfm` suffix, function
comments `[cmfm] …`, and the struct `cmfm_cGUIInstScrollBar` (database saved).

## Short answer

- The scroll bar is a **display widget driven by three floats**: content size `+0x230`, visible size
  `+0x234`, position `+0x238` (0..1). There are no setter methods; every game page writes the fields
  directly. Its per-frame update (vtable slot 14) turns them into the look by **setting the animation
  frame of its three textures to the thumb length in pixels** (the animation in
  `ui\common\scrollbar.gui` is keyed so frame N = N px long) and moving the instance on Y.
- **Mouse:** a non-virtual function `updateMouse` (RVA 0xB89670) that the page calls each frame drags
  the **thumb** (no click on the track) and, if enabled (`+0x262`, on by default), turns the **wheel
  anywhere on the page** into movement. It writes the new position into `+0x238` and returns true;
  the page then scrolls its list to it. Not calling it = a display-only bar.
- **Lists know nothing about the bar.** Every game page bridges the two itself, each frame:
  `if (updateMouse(bar)) list.scroll.setScrollRatio(bar.ratio, bar.wheelDir); else bar.ratio = list.scroll.ratio;`
  CMFM does the same with its own window: write total/visible/ratio when the window moves, read
  `+0x238` back after a drag.
- **Layout:** a layout uses the shared bar through a **GUI reference record in header slot 0x150**
  (not 0xE0; 0xE0 is the flow-state table "Start/End/main/in/out"), e.g. `{id 13, "ui\common\scrollbar"}`.
  The scroll bar instance's parameters `ResourceId` (= that record's id) and `AnimationId` (= the
  animation's id in scrollbar.gui: 1 `scroll_bar`, 2 `scroll_base`, 3 `scroll_bar_auto`,
  4 `scroll_base_auto`) select the animation. mods.gui has no such reference and no scroll bar
  instances, so **the file has to grow** (new ref record, 3 instances, 75 parameters, 3 per-instance
  track records, a few strings). `gui_edit.py` only edits in place today.

## 1. The class (C)

| Item | Value |
|---|---|
| DTI registration | `sub_1400A4CB0`: `MtDti::ctor(&cGUIInstScrollBar::DTI::Instance (0x144F1AF20), "cGUIInstScrollBar", &cGUIInstAnimation::DTI::Instance, 0x270)` |
| Class id | 0x7F2F0CC3 (DTI +0x34); DTI flags 0x0D80009C (size 0x9C*4 = **0x270**) |
| Parent | `cGUIInstAnimation` (size 0x200) → `cGUIInstNull` → `cGUIInstance` |
| DTI vtable | 0x1430CC460; slot 1 `cGUIInstScrollBar::DTI_newObject_cmfm` 0x140B88E30 (allocates 624 bytes, runs the ctor) |
| Constructor | `cGUIInstScrollBar::ctor_cmfm` 0x140B88F90 (RVA 0xB88F90) |
| Object vtable | `cGUIInstScrollBar::vft` 0x1430CC4A0 (RVA 0x30CC4A0), 30 slots like cGUIInstAnimation's (0x1435171D0) |
| populatePropertyList | 0x140B890B0 (slot 3) |
| getDti | 0x140B893D0 (slot 4) |

Vtable overrides (others are cGUIInstAnimation's):

| Slot | Offset | Function | Does |
|---|---|---|---|
| 0 | +0x00 | 0x140B89040 | destructor |
| 3 | +0x18 | 0x140B890B0 | populatePropertyList (4 properties, then the parent's) |
| 4 | +0x20 | 0x140B893D0 | getDti |
| 9 | +0x48 | `onTreeBuilt_cmfm` 0x140B895B0 | after the instance tree is built: finds `tex_bar_begin`, `tex_bar_center`, `tex_bar_end` in its animation's object root (+448) → +0x248/+0x250/+0x258; stores the layout position PosX/Y/Z (+176/+180/+184) → +0x210/+0x214/+0x218. Called for every instance by `uGUI::onTreeBuiltAll_cmfm` (0x142355940) from `uGUI::setResource`, and by the clone paths (0x1423531C0, 0x1423533B0) |
| 14 | +0x70 | `update_cmfm` 0x140B893F0 | per-frame placement (below), then the parent's animation step `sub_14234E150(this, dt)` |
| 22 | +0xB0 | `copyFrom_cmfm` 0x140B89360 | clone: parent copy, then the 4 properties |

Properties (`populatePropertyList`; layout parameter name → field):

| Name | Field | Type | Getter / setter | Ctor default |
|---|---|---|---|---|
| ScrollType | +0x220 | int (MT type 10) | 0x140B893E0 / 0x140B895A0 | 0 |
| MaxFixFrame | +0x224 | float (MT type 12) | 0x14035D190 (shared stub) / 0x140B89590 | 100.0 |
| BarMinPos | +0x228 | float | 0x140B893C0 / 0x140B89580 | 0 |
| BarMaxPos | +0x22C | float | 0x140B893B0 / 0x140B89570 | 0 |

Runtime fields (struct `cmfm_cGUIInstScrollBar` in IDA):

| Field | Meaning | Source |
|---|---|---|
| +0x210/+0x214/+0x218 | base position (layout PosX/Y/Z at slot 9) | C |
| +0x230 float | **total** (content size, any unit) | C |
| +0x234 float | **visible** (same unit) | C |
| +0x238 float | **position ratio** 0..1 (0 top) | C |
| +0x248/+0x250/+0x258 | the three texture objects | C |
| +0x260 u8 | **full length**: draw MaxFixFrame long, position BarMinPos (used for the track, "base") | C |
| +0x261 u8 | dragging | C |
| +0x262 u8 | wheel enabled (ctor: 1) | C |
| +0x263 u8 | wheel used this frame | C |
| +0x264 int | wheel direction this frame: -1 up, +1 down, 0 none | C |
| +0x268 float | wheel step as a fraction of BarMaxPos per notch; ≤ 0 (ctor -1.0): 800 px/s × dt | C |
| +0x200, +0x208, +0x21C, +0x23C, +0x240 | zeroed by the ctor, no use found | unknown |

## 2. How the bar is drawn (C)

`update_cmfm` (0x140B893F0, slot 14, `xmm1` = dt). Nothing happens (not even the parent's animation
step) while dragging (+0x261) or if a texture was not found:

```
len = 1.0
if total > 1.1920929e-7: len = clamp(visible / total * MaxFixFrame, 1.0, MaxFixFrame)
if fullLength: len = MaxFixFrame
setCurrentFrame(tex_bar_begin, len, true); same for tex_bar_center, tex_bar_end
top = fullLength ? max(BarMinPos, BarMaxPos) : max((1 - len / MaxFixFrame) * BarMaxPos, BarMinPos)
pos = BarMinPos + (top - BarMinPos) * ratio
ScrollType 1: SetPosY(this, pos + base.y)    ScrollType 0: SetPosX(this, pos + base.x)
```

So the instance's own PosY is overwritten every frame (move the parent null instead), and only
`visible / total` matters for the thumb length (units cancel).

**The animation side (file bytes, C):** `ui\common\scrollbar.gui` has four animations (record +0 is
the id): 1 `scroll_bar`, 2 `scroll_base`, 3 `scroll_bar_auto`, 4 `scroll_base_auto`; sequences
1 `DEFAULT`, 7 `DISABLE` (DISABLE greys the thumb, VertexColor 100,100,100). Each is
`null_scroll_bar` > `tex_bar_begin` (16×8 cap), `tex_bar_center` (16×len), `tex_bar_end` (16×8 cap),
texture `ui\common\tex\common00_ID`, all with `FixFrame` 1. Keyframes at **frame 0 and 1000**:
center `Size.y` 0 → 1000, end cap `PosY` 0 → 1000. So **frame = length in px**; the game sets the
frame, it does not scale or move the textures itself. The "auto" variants add `null_all` /
`null_pos` (PosX 127, PosY 45) and texture PosX 124 (a built-in offset); the plain ones sit at 0,0.
Every game layout uses MaxFixFrame = BarMaxPos = track length (20..956 px) and BarMinPos 0 (survey).
Frames above 1000 are outside the keys (**H**: clamped; no game value exceeds 956).

Drawing is the referenced animation's normal draw; the class has no draw override.

## 3. Mouse and wheel: `updateMouse` (C)

`cGUIInstScrollBar::updateMouse_cmfm` 0x140B89670 (RVA 0xB89670), `bool f(bar)`. Not virtual; 24 call
sites in 15 page functions call it once per frame. With `gui = bar+128` (owner GUI):

1. Returns false unless the three textures exist and `mouse+368` (mouse in use, `sub_1404897F0` on
   `qword_1451C53F0`) is set.
2. Not dragging: mouse position (`sub_141FB5D30`), hit test **on the bar (thumb) instance only**
   (`sub_141FB6190`; needs `+88 & 0x1000009` on the bar and its parent) → sets `sMhGUI+0x14785`
   (H: "mouse over GUI"); a left click on it (`sub_141FB6560`) starts dragging (+0x261 = 1) and sets
   `sMhGUI+0x14786` (H: "GUI took the click"). **A click on the track does nothing.**
3. Dragging: continues while the left button is held (`uMhGUI::leftButtonHeld_cmfm` 0x141FB64E0) and
   the GUI's state has `+8856 & 0x80000011`; otherwise +0x261 = 0. When the mouse moved, the delta
   (current − previous position, `uMhGUI::prevMousePos_cmfm` 0x141FB5E00), scaled to layout units
   (`uMhGUI::screenToLayout_cmfm` 0x141FB6070: ÷ gui+1056/+1060), moves the bar, clamped to
   `[base+BarMinPos, base+(1−len/Max)×BarMaxPos]`; ratio = (pos − min) / (max − min).
4. Wheel (if +0x262): `uMhGUI::mouseWheel_cmfm` 0x141FB5CF0 (delta while the GUI has input; **no hit
   test**). Step `BarMaxPos × +0x268` per call (or 800 × dt if the step ≤ 0), up for a positive delta;
   same clamp and ratio; +0x263 = 1, +0x264 = −1 (up) / +1 (down).
5. Returns `dragging || wheelUsed`.

All mouse helpers answer only while the GUI has input (`+8897/+8898`, not `+8860 & 0x400`), the same
gate as CMFM's existing mouse calls.

## 4. Who sets the range and position (C)

No setter functions exist; game code writes the fields. Writers found:

| Function | Writes |
|---|---|
| `uGUIGraphicsOption::setupInstances_cmfm` 0x14053E140 (slot 74) | base `+0x260 = 1`; bar → page+13192; bar `+0x234 = DuplicateButtonSize(list+672) × 12` |
| `uGUIGraphicsOption::open_cmfm` 0x14053DF20 (slot 90) | `+0x230 = rows × DuplicateButtonSize`, `+0x238 = 0`, `+0x268 = 1/rows` (also list+856 = rows) |
| `uGUIGraphicsOption::update_cmfm` 0x140540E00 (slot 81) | `+0x238` each frame (below) |
| `KeyboardConfigPage_setup_scrollbar_cmfm` 0x140547DE0 (names match `pc\keyboard_config2.gui`) | bar `+0x238 = 0.5`, `+0x234 = 500`, `+0x230 = 1200`; base `+0x260 = 1`; update 0x140549DB0 bridges as below |
| `ScrollDetailPanel_fill_cmfm` 0x140639C30 (bowgun detail panel) | ≤ 21 lines: `setPlay(null_scroll, 0)` (hides the whole bar group); else `setPlay(null_scroll, 1)`, `+0x234 = 630` (21 × 30), `+0x230 = lines × 30`, base `+0x260 = 1`, ratio 0 |
| login topics/events setup 0x1410F32D0 | `+0x220 = 1` (layout has ScrollType 0), `+0x234 = 440`, `+88 &= ~4`, `+0x262 = 1` |
| the bar's own `updateMouse` | `+0x238`, `+0x261`, `+0x263`, `+0x264` |

Other `updateMouse` callers: 0x140525C10, 0x140556A50, 0x14055E320, 0x140A9FA80, 0x140AA2880, 0x140AC3F20,
0x140AC92B0, 0x141071990, 0x14109D170, 0x1410B9650, 0x1410D2A90, 0x1410F64D0, 0x141DCE2C0, 0x141DFB0B0,
0x141EB62C0.

## 5. The PC graphics page, uGUIGraphicsOption (C)

DTI 0x144CCD240, vtable 0x142FA9A68; slot 71 `getGuiPath_cmfm` 0x14053ED20 (`ui\pc\graphic_option`),
74 setup 0x14053E140, 81 update 0x140540E00, 90 open 0x14053DF20, 91 close 0x14053DD60.

- **Finding the bar: by name**, `uGUI::getInstanceFromName(page, "scrollbar_auto_base")` and
  `"scrollbar_auto"` (`getInstanceFromName` scans the layout's instance table by name, then
  `getInstanceFromId`; no object-index tricks).
- **Its list** `btnlist_option_param` (page+10992) uses the game's scroll controller
  (`cGUIFixedScrollList` at list+736): `+744 = 0x0101`, `+852 = 13` visible, `+856 = 22` total,
  `sub_140B65B20(list+736, page, onListScroll_cmfm 0x14053DAB0)`.
- **Every frame (slot 81):**
  `if (updateMouse(bar)) cGUIFixedScrollList::setScrollRatio_cmfm(list+736, bar+0x238, bar+0x264) else bar+0x238 = list+864`
  (list+864 = controller+0x80, its eased current ratio, `cGUIFixedScrollList::update_cmfm` 0x140B65D80).
  `setScrollRatio` (0x140B65250) maps the ratio to a first row (`round((total − visible + 1) × r)`),
  forcing at least one row in the wheel direction.
- **Mouse:** drag the thumb, wheel anywhere (the default +0x262 = 1, step 1/rows = one row per
  notch). No track click. Keys and pad move the list, not the bar; the bar follows through the
  `else` branch.

## 6. Other users and the common pattern (C)

- **Scroll controllers don't link to the bar.** Neither `cGUIFixedScrollList` (embedded at +736 in
  `cGUIInstButtonList`; at +664 in another list type) nor `cGUIScrollPageControl` (0x140BBE100…)
  touches a scroll bar field. The page bridges them, always with the same per-frame code (0x140525C10,
  0x140549DB0, 0x140540E00, 0x141DCE2C0, …).
- **Own scrolling** (`ScrollDetailPanel_update_cmfm` 0x140639280): content is a null instance moved on
  Y; `if (updateMouse(bar)) content.PosY = min + (max − min) × bar.ratio`; the right stick moves the
  content and writes `bar.ratio = clamp((pos − min) / (max − min), 0, 1)`. Whole group hidden with
  `cGUIInstance::setPlay(null_scroll, false)` when the text fits. **This is the model for CMFM.**
- **Layouts (survey, 20 files):** always a pair, bar + base (track), with the same MaxFixFrame =
  BarMaxPos and ScrollType 1 (vertical; two horizontal ones in `equip\upgrade_tree`). The base always
  gets `+0x260 = 1` in code. ResourceId → `ui\common\scrollbar`, AnimationId 3/4 (auto) or 1/2
  (plain); chat menus use local animations. A title-screen example: `title\news.gui`
  (plain 1/2, 775 px).

## 7. How a layout refers to scrollbar.gui (C)

- **GUI references: header slot 0x150, count at 0x80**, 24-byte records `{u32 id; u32 pad; u64
  runtime rGUI* (0 in the file); u32 path string offset; u32 pad}`. graphic_option.gui:
  `5 cursor, 2 text_config, 3 window_main, 7 page_num, 13 scrollbar`. mods.gui (from options.gui):
  `6 check_mark, 5 cursor, 7 page_num, 1 slider, 2 text_config, 3 window_main, 4 title,
  8 icon_material`; all eight are in use (instances or child-animation parts: `text_config_L` → 2,
  `checkMark` → 6, `Material_item` in the header `title_options` → 8), so none can be renamed to
  scrollbar.
- **Loading:** the fix-up `sub_1422A7310` loads each ref with `sMhResource` vtable+88
  (`loadResource(rGUI DTI, path, 1)`) when the rGUI's flag `+96 & 0x10` is set, and stores it at
  record +8. CMFM's own layout already gets its refs loaded (its sliders draw `ui\common\slider`), so
  a scrollbar ref will load the same way (**H**, by analogy; the file is in the chunks).
- **Lookup:** `cGUIInstAutoAnimation::setResourceId` (0x14234F660) / `setAnimationId` (0x14234E9F0):
  if ResourceId (+432) ≠ −1, `rGUI::getGuiResourceFromId(instance+136 rGUI, id)` (0x1422A6F60) finds
  the record by its +0 id, and `rGUI::getAnimationFromId` (0x1422A6E20) finds the animation by its
  +0 id in that file; the object tree is built into +448. Memory for it is sized at layout load
  (`uGUI::getInitParamSize` 0x142348B60), and instance memory from each class's DTI size at runtime,
  so changing or adding classes in a layout is safe memory-wise.
- **The scroll bar instance's parameters** (graphic_option `scrollbar_auto`, 27): `ScrollType` 1,
  `MaxFixFrame` 590, `BarMinPos` 0, `BarMaxPos` 590, then the cGUIInstAnimation set: `Visible` 1,
  `Play` 1, `IgnoreMouseReaction` 0, `Priority` 10 (base 0), `LayoutCorrect` 0, `Alignment` 0,
  `ReferencePosition` 1, `ReferenceRotation` 1, `ReferenceScaleX/Y/Z` 3, `PosX/PosY/PosZ` 0,
  `SclX/SclY` 1, `Rotation` 0, `ColorControl` 2, **`ResourceId` 13** (type 0x0D), **`AnimationId` 3**
  (type 0x0B; base: 4), `SequenceId` 1 (type 0x10), `Speed` 1, `Mask` 0. Their parent
  `null_scrollbar_root` (cGUIInstNull, 21 params) sits at PosX 610, PosY 16, Priority 110 under
  `null_prog_color1` (PosX 523, PosY 212), next to the list's `a_mask`.
- **Header slot 0xE0** (count 0x48, 32 bytes) is the flow-state table (`Start`, `End`, `main`, `in`,
  `out`), not references. `tools/re/gui_dump.py` labels it "refs"; worth renaming.

## 8. What mods.gui needs (C for the format, H for untested edits)

Records to add, copied from graphic_option.gui (same format version) and re-indexed:

1. **GUI ref** in slot 0x150: `{id 9, "ui\common\scrollbar"}` (9 = first free id). Count 0x80: 8 → 9.
2. **Instances** in slot 0xD8 (56 bytes: +0 instance id, +8 next index, +12 first child index,
   +16 param count, +24 name string, +32 class id, +40 first param index, +48 extra pool offset or
   0xFFFFFFFF; rest 0):
   - `cmfm_scroll_root` cGUIInstNull (0x419B2827), 21 params, child = base;
   - `cmfm_scroll_base` cGUIInstScrollBar (0x7F2F0CC3), 27 params, next = bar, AnimationId 2 (plain)
     or 4 (auto), Priority 0;
   - `cmfm_scroll_bar` cGUIInstScrollBar, 27 params, AnimationId 1 (plain) or 3 (auto), Priority 10.
   Instance ids from header 0x18 (mods.gui: next free id 100): 100, 101, 102; header 0x18 → 103.
   Counts 0x44 and 0x8C: 91 → 94. Link the root into the tree as a sibling of `btnlist_camera`
   under `null_prog_color0` (one existing next/child link changes; `Scene.walk()` checks it).
   Recommended: the **plain** animations (1/2), so the root's position is the track's top-left
   (as `title\news.gui`).
3. **Parameters**: 75 records appended to slot 0xC8 (count 0x38 += 75). Values can reuse existing
   pool entries (`point_at_value`) where present; new numbers (e.g. the track length) appended to
   pool 0x180. Names: `ScrollType`, `MaxFixFrame`, `BarMinPos`, `BarMaxPos` are **not** in
   options.gui's strings and must be added (all others exist). Alternative: leave those four out
   (23 params) and write +0x220..+0x22C at runtime; the fix-up resolves parameters by name, so a
   missing one keeps the ctor default (ScrollType 0, MaxFixFrame 100).
4. **Per-instance track records** (one per instance; `uGUI::setResource` binds them by instance id):
   the last 0x8C records of the anim-param table (slot 0xD0; the run starts at the index in header
   slot 0x198, 18 in mods.gui). Append 3 records `{type 3, 3 keys, +16 instance id, +24 name 0xDCB
   (the empty string), +32 first key, +40 bool pool offset}` copying `btnlist_camera`'s keys and
   values (frames 3/4/5 = flow states in/main/out, values 1,1,1). Count 0x3C: 109 → 112.
5. **Strings**: 3 instance names, the path, the 4 parameter names. Append after offset 0xDCC: the
   strings pool ends with an **empty string at 0xDCB that every track record names** — new strings
   must not overwrite it.
6. **Growing the file**: section order in mods.gui is A8, B0, B8, C0, C8, D0, D8, E0, E8, (F0–148
   mostly empty), 130 textures, 150 refs, 170 keys, pools 178/180/188/190, strings 168, then the
   0x1A0 blob (a shader resource of header 0x90 = 8576 bytes). Records refer to each other by index
   or by offset inside their own pool, so appending at a section's end and shifting every later
   section (header slot offsets, 16-byte alignment, file size at +8) keeps all references valid.
   `rGUI::Deserialize` checks only magic and version. A `gui_edit.py` "append records to section"
   helper plus `make_mods_gui.py` steps would do it; verify with `gui_dump.py` and a game load.

Positions are guesses to tune in game: the mod list is `btnlist_camera` under `null_prog_color0`
(PosX 170, PosY 250), PosY 33, 11 rows of 40 px + 12 spacing; a track of about 11 × 52 = 572 px
(MaxFixFrame = BarMaxPos ≈ 560), Priority about 110 on the root, PosX to the right of the cursor
bar. For the right-hand panel, a second group (`cmfm_scroll2_*`) at the panel's right edge, its
MaxFixFrame/BarMaxPos set at runtime from the panel's height (the update reads them every frame).

## 9. Call sequence for CMFM

Once, after the page's layout is set (CMFM's open handler):

1. `root/base/bar = uGUI::getInstanceFromName(page, "cmfm_scroll_root" / "_base" / "_bar")`
   (existing pattern). Null → log and skip (fail closed).
2. Type check without a pattern: `dti = instance->vtable[4](instance)`; require `*(u32*)(dti+0x34) ==
   0x7F2F0CC3` and `(*(u32*)(dti+0x30) & 0x7FFFFF) * 4 == 0x270`.
3. `base+0x260 = 1` (track). `bar+0x262 = 0` (no bar wheel: CMFM keeps its own wheel from al.input;
   leave 1 only if the bar should own the wheel, then set `+0x268 = 1/(total − visible)`).
4. Optional runtime layout values: `+0x220 = 1`, `+0x224 = +0x22C = track length`, `+0x228 = 0` on both.

Whenever the mod list is rebuilt or its window moves (n entries, v visible rows, top row t):

- `n <= v`: `cGUIInstance::setPlay(root, false)` (hides the group and makes it un-hittable), done.
- else `setPlay(root, true)`; `bar+0x230 = n`, `bar+0x234 = v`, `bar+0x238 = t / (float)(n − v)`.

Every frame in CMFM's page update (slot 81), while the bar is shown:

- `if (updateMouse(bar))`: `t' = round(bar+0x238 × (n − v))`, clamp; if `t' != t` move the window
  (keep the highlighted row inside it, refresh rows). While `bar+0x261` (dragging) is set, skip CMFM's
  own hover handling.
- else `bar+0x238 = t / (float)(n − v)` (snaps the thumb to whole rows after a drag).
- The bar's slot 14 places the thumb on the next tree update (one frame later, as in the game).
- On close: `bar+0x261 = 0` (a drag can't survive a reopen).

## 10. Signatures (clean image, SHA-256 a78ea69b…9d80ea; each matches exactly once, `sigscan.py scan`)

| Function | RVA | Pattern | Use |
|---|---|---|---|
| `cGUIInstScrollBar::updateMouse` | 0xB89670 | `40 57 48 81 EC A0 00 00 00 48 83 B9 50 02 00 00 00 48 8B F9 0F 84 ? ? ? ? 48 83 B9 48 02 00 00 00` | call each frame (drag) |
| `cGUIInstScrollBar::update` (slot 14) | 0xB893F0 | `40 53 48 83 EC 50 48 83 B9 50 02 00 00 00 48 8B D9 44 0F 29 44 24 20 44 0F 28 C1 0F 84 ? ? ? ? 48 8B 89 48 02 00 00` | check only (vtable slot 14 of a found bar) |
| `cGUIInstScrollBar::onTreeBuilt` (slot 9) | 0xB895B0 | `40 53 48 83 EC 20 48 8B D9 E8 ? ? ? ? 48 83 BB 80 00 00 00 00 0F 84 ? ? ? ? 48 8B 8B C0 01 00 00` | check only |
| `cGUIInstScrollBar` ctor; `--rel 17` → vtable 0x30CC4A0 | 0xB88F90 | `40 53 48 83 EC 20 48 8B D9 E8 ? ? ? ? 48 8D 05 ? ? ? ? 48 89 03 33 C0 48 89 83 00 02 00 00 F3 0F 10 05` | optional vtable check |
| `cGUIFixedScrollList::setScrollRatio` | 0xB65250 | `48 89 5C 24 18 57 48 83 EC 30 8B 41 78` | reference only |
| `uMhGUI::mouseWheel` | 0x1FB5CF0 | `48 85 D2 74 09 80 BA 72 01 00 00 00 74 0D 8B 81 9C 22 00 00 C1 E8 0A A8 01 75 1E 80 B9 C2 22 00 00 00 75 09 80 B9 C1 22 00 00 00 74 0C 48 8B 0D ? ? ? ? E9 ? ? ? ? 33 C0` | reference only |
| `uGUIGraphicsOption::update` (slot 81) | 0x540E00 | `40 53 48 83 EC 20 F3 0F 10 89 10 34 00 00` | reference only |
| `uGUIGraphicsOption::setupInstances` (slot 74) | 0x53E140 | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 83 EC 40 48 8B F9` | reference only |
| `uGUIGraphicsOption::open` (slot 90) | 0x53DF20 | `48 89 5C 24 08 57 48 83 EC 20 C6 05 ? ? ? ? 01` | reference only |

Already in `src/native/game_421810.hpp` and re-checked (1 match each): `uGUI::getInstanceFromName`
0x23537A0 (`kGetInstanceFromNamePattern`), `cGUIInstance::setPlay` 0x18BAEA0 (`kSetPlayPattern`).
Nothing needs hooking: CMFM calls `updateMouse` from its existing slot-81 handler and writes fields.

## 11. Confirmed vs. hypothesis

Confirmed (static): class size, vtable, slots 9/14/22, properties and field offsets; the update
formula and that it sets texture frames; frame = px from the keyframes; `updateMouse` behaviour
(thumb drag, wheel anywhere, no track click, return value, fields written); the graphics page's
lookup by name and its per-frame bridge; the scroll controllers not touching the bar; the ref table
at 0x150 and the ResourceId/AnimationId lookup by record ids; `getInstanceFromName` scanning the
layout's instance table; the empty-string name of the track records.

Hypotheses (untested): that a grown mods.gui with these records loads and draws (format followed
exactly, never tried); that the scrollbar ref loads like the slider ref; the meaning of
`sMhGUI+0x14785/0x14786`; that slot 14 runs every frame from the instance-tree update (it is the
animation step, so it must, but the caller was not traced); whether the game list's own hover moves
the cursor when the pointer strays onto rows during a drag; behaviour of frames above 1000; the
unknown fields +0x200/+0x208/+0x21C/+0x23C/+0x240.
