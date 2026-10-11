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

## Next: the rest of the first draft (agreed 2026-10-11)

5. **Scrollbars, for the mod list and the panel.**
   - 5a. Done (run `20261011-longpage-1`): the panel does not scroll. With 24
     rows it runs past the bottom of the screen (rows 1-15 fit, the help line
     covers row 16). The panel needs a window over its rows, like the mod
     list, and a scrollbar too.
   - 5b. The layout: the game's own scrollbar (`ui\common\scrollbar.gui`,
     used by 20 layouts; the PC graphics settings' `graphic_option.gui` has
     `scrollbar_auto_base` and `scrollbar_auto`) added to CMFM's layout next
     to the mod list and next to the panel. The layout tool learns to add
     parts (today it only edits them).
   - 5c. The panel's window: at most as many rows as fit above the help
     line; the cursor moving past the edge, the wheel and the mouse move the
     window.
   - 5d. The code for the bars: each follows its list's window (opening,
     keys, wheel). They only show where the list is; dragging is a later
     extra, if the game's bar supports it. The draft's "2 more" line is
     dropped.
6. **The mod's own header** on the right panel: its name, version and state
   ("active", "restart needed"), placed so it does not sit on the first
   row's line (why the panel title was dropped). The version: a new optional
   page field while 1.1 is unreleased, or an AL request for other plugins'
   versions (AL has no such service yet).
7. **The slider's number** next to the slider (the draft's "0.35").
8. ~~The gear next to "Mods"~~: already there (the game's own header art,
   seen in run `20261011-longpage-1`).

## After that

- Choice lists longer than 14 entries: a scrolling pick list (arrows today).
- The author guide and the handoff to the DLSS mod (its five settings).
- Popups and notifications for plugins (text, severity, "don't show again"),
  designed with a mockup first; the game's Yes/No dialog is the way in.
- Status pages with live values (the DLSS mod: render size to output size,
  frame generation multiplier, Reflex, last failure); debug pages.
- A long play session; then drop the old options view fallback and turn the
  data-patch helper into an AL request (decision 9).
- SharpPluginLoader compatibility (it swaps the same Start menu entries).
- Languages for CMFM's own texts.
- Greying out the game's own AA/DLSS options while the DLSS mod owns AA.
- Release 0.1.0: its own MO2 mod, VirusTotal; ask before anything goes to a
  public remote (including the edited layout).
