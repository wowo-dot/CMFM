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

## Next

- Choice lists longer than 14 entries: a scrolling pick list (arrows today).
- The author guide and the handoff to the DLSS mod: written 2026-10-12
  (`docs/author-guide.md`, `docs/handoff-dlss-mod-20261012.md`); `cmfm.menu`
  1.1 frozen (decisions item 11). Waiting for the DLSS session's feedback.
- ~~Popups and notifications~~: not for now (decisions item 12): greyed
  rows, refusals and status lines cover the known cases. The sketch
  (`cmfm_popups_design_v1`: the game's message window, its message log, a
  Messages list on the Overview) stays on file in case a real need appears.
- Status pages with live values (the DLSS mod: render size to output size,
  frame generation multiplier, Reflex, last failure); debug pages.
- A long play session; then drop the old options view fallback and turn the
  data-patch helper into an AL request (decision 9).
- SharpPluginLoader compatibility (it swaps the same Start menu entries).
- Languages for CMFM's own texts.
- Greying out the game's own AA/DLSS options while the DLSS mod owns AA.
- Release 0.1.0: its own MO2 mod, VirusTotal; ask before anything goes to a
  public remote (including the edited layout).
