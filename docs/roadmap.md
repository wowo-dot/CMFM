# CMFM roadmap

The plan as agreed with the user; newest state first. Each design step ends
with an in-game run and an annotated tag (`design-step-N`). Details of how
each part works go into `docs/research/native-menu.md`.

## Done (tested in game)

- Native menu inside the game's own UI: a "Mods" entry below Options on the
  title screen and in the in-game Start menu (System tab).
- CMFM's own page and layout (`assets/nativePC/ui/cmfm/mods.gui`): the mod
  list on the left, the selected mod's settings on the right, mouse on both
  sides (tag `split-layout-1`).
- `cmfm.menu` 1.1 (unreleased): toggles, choices, numbers, read-only lines,
  sections, buttons, live or restart-required settings, locks, events.
- Design steps 1-4 (the first draft, the mockup "cmfm_mods_page_design_v1"):
  1. Overview, section headings, restart marks;
  2. sliders;
  3. the pick list for long choices;
  4. buttons with the game's Yes/No question, locked (greyed) rows.
- The in-game Start menu path with all of the above (run
  `20261011-startmenu-1`).

## The rest of the first draft (agreed 2026-10-11): done

5. **Scrollbars, for the mod list and the panel.** Done (tag
   `design-step-5`, run `scrollbar-2`).
   - 5a. Done (run `20261011-longpage-1`): the panel does not scroll. With 24
     rows it runs past the bottom of the screen (rows 1-15 fit, the help line
     covers row 16). The panel needs a window over its rows, like the mod
     list, and a scrollbar too.
   - 5b. Done (commit 93bbd30): the game's own
     scrollbar pair (`ui\common\scrollbar`, as in the PC graphics settings)
     grafted into CMFM's layout next to the mod list and next to the panel;
     `tools/gui/` now grows layouts (docs/research/gui-format.md).
   - 5c. Done (commits ff12f3f, 5ebcaea): the panel holds at
     most 14 rows; the cursor moving past the edge moves the window (the
     mod list's method, chosen by the user).
   - 5d. Done (commits 93bbd30, 5ebcaea): each bar follows its list's
     window, shows only while the list scrolls, and its thumb can be dragged
     (the user asked for it). The draft's "2 more" line is dropped.
6. **The mod's own header** above the panel. Done (tag `design-step-6`, run
   `header-1`): the page's title, its version (a new optional page field,
   cmfm.menu 1.1) and a yellow "Restart needed" while a change waits;
   centred under "Mods", level with the restart line. "Active" is left out
   (every listed mod is active). An AL service for other plugins' versions
   remains a candidate request.
7. **The slider's number** next to the slider: there since design step 2
   (the value beside the bar). **Fine sliders** added on the user's
   question (tag `fine-sliders-1`, runs `fineslider-1` to `-3`): INT/FLOAT
   up to 101 values (0 to 100 in steps of 1), one shared value text per
   number row, knobs placed exactly, equal zones per value.
8. ~~The gear next to "Mods"~~: already there (the game's own header art,
   seen in run `20261011-longpage-1`).

The first draft is complete.

## Interface 1.2: what mod authors use (chosen 2026-10-12)

Chosen with the user, MCM (Skyrim's and Fallout 4's Mod Configuration Menu)
as inspiration: its success confirms what authors reach for. Not every MCM
feature; the ones MHW mods need. Built in this order, each tested in game:

1. **Conditional rows.** Done (tag `hide-rows-1`, runs `hide-1` to `-3`;
   it also fixed the arrows starting from a stale value on CMFM's rows).
   `CMFM_FLAG_HIDE_LOCKED`: while a row is locked it is hidden instead of
   greyed. One upscaler choice shows only that
   upscaler's settings; "Advanced" shows its section. Uses the `locked`
   callback authors already write (MCM: hidden flag, display conditions).
2. **Reset to defaults per mod.** Done (tag `reset-defaults-1`, run
   `reset-1`; it resets locked and hidden settings too). Since run
   `plain-1` the row looks like the game's "Advanced Graphics Settings"
   row: its label alone, centred and red; headings are centred too. A "Reset to defaults" row CMFM adds at the
   end of every page (with the game's Yes/No question), restoring every
   setting's `default_value` through `set`; a page can opt out. Today each
   author has to build a Reset button (MCM: the Default key).
3. **Live status lines.** Done (tag `live-lines-1`, run `live-1`).
   `CMFM_FLAG_LIVE`: TEXT rows are read again four times a second while
   shown, not only on open: counters, damage numbers, "active" states,
   debug pages. A changed line gets only its value rewritten (no flicker).
4. **Key bindings.** A KEY setting type: Confirm, press a key (with Ctrl,
   Shift, Alt), Esc cancels; the row shows the key's name; CMFM warns when
   another mod's KEY setting already uses it (MCM: key maps with conflict
   detection, its most used feature). Keyboard first.
5. **More settings per page.** The panel scrolls, so a page may hold more
   than 24 rows (the limit to check against the text-id budget).

## Then: config pages without code (1.3, its own design first)

A mod ships a small description file next to its config; CMFM shows the
page and edits the mod's own config file from it. No C interface needed,
so Stracker and SharpPluginLoader plugins can join (MCM Helper's step,
which made MCM support nearly free for authors). The config file stays the
mod's; how CMFM tells a running mod about a change is part of the design.

## Later

- Colour settings (overlay and HUD mods), translations of mod texts, text
  input, a per-row Default key.
- Choice lists longer than 14 entries: a scrolling pick list (arrows today).
- The author guide and the handoff to the DLSS mod: written 2026-10-12
  (`docs/author-guide.md`, `docs/handoff-dlss-mod-20261012.md`); `cmfm.menu`
  1.1 frozen (decisions item 11). Waiting for the DLSS session's feedback.
- ~~Popups and notifications~~: not for now (decisions item 12): greyed
  rows, refusals and status lines cover the known cases. The sketch
  (`cmfm_popups_design_v1`: the game's message window, its message log, a
  Messages list on the Overview) stays on file in case a real need appears.
- A long play session; then drop the old options view fallback and turn the
  data-patch helper into an AL request (decision 9).
- SharpPluginLoader compatibility (it swaps the same Start menu entries).
- Languages for CMFM's own texts.
- Greying out the game's own AA/DLSS options while the DLSS mod owns AA.
- Release 0.1.0: its own MO2 mod, VirusTotal; ask before anything goes to a
  public remote (including the edited layout).
