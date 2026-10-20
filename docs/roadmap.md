# CMFM roadmap

The plan as agreed with the user; newest state first. Each design step ends
with an in-game run and an annotated tag (`design-step-N`). Details of how
each part works go into `docs/research/native-menu.md`.

## 0.2.0 (collecting; no release date)

Named 0.2.0 by the user on 2026-10-20 ("a really nice usage overhaul"; until
then collected as 0.1.1). The bones are done; a few things to add and check
remain.

- **The Mods page no longer shifts the game's GUI unit list** (a player's
  report: a speedrun tool indexing slot 46 crashed). Not kept: the title's
  page dies with the title scene; in game made on Mods and released when it
  closes. Tested (tag `unit-list-fix-1`, runs `20261015-guilist-1`,
  `20261015-unitfix-1`, `-2`): hub and quest lists identical to the game's
  own before and after a visit, the page at the end while open, a quest
  ending with Mods open leaves nothing behind. Cost: one slower frame per
  Mods press in game. The layout is requested once (no reference per visit).
- **The panel takes input with a single mod** (a client mod's report: with
  the Overview and one mod the panel drew but took no click or key; the
  "category cursor moved to row 0" warning was its symptom, not cosmetic).
  The off-screen category list keeps at least three rows in split mode, so
  the panel's row 2 always exists. Run `20261017-onemod-1`: the warning
  gone; input with one mod confirmed by the user only.
- **`cmfm.menu` 1.3** (decisions item 15, from a client mod's feedback): the
  callbacks' order promised in the header, the text limits as defines,
  `cmfm_check.h` (add_page's rules outside the game; CMFM's add_page uses
  it), a BUTTON question worded when pressed. Unit-tested; the demo uses all
  of it (asks for 1.2, sets `question`).
- **The mod list handles like the game's Options** (decisions item 16; the
  user: players found it confusing): the highlight shows its entry in the
  panel, one click or Confirm goes in, hovering no longer moves the focus.
  Tested (run `20261020-vanilla-list-1`: "improved a lot"); a click on a
  mod from inside the panel stays (the user: "nice, keep it").
- **A mod's pages as tabs above the panel** (decisions item 17; the user's
  sketch): the mod list flat, the dropdowns gone; a sideways copy of the
  game's category list in CMFM's layout, the panel one row lower; tabs
  scroll past four. Tested over runs `20261020-tabs-1` to `-8`: the look
  (size 22, long names shrunk), the pointer (the bar on hover, the hand,
  the wheel, a horizontal scrollbar), pad and keyboard (Up from a page's
  first row; Left / Right move the bar with the stick and repeats; Confirm
  opens the page and goes in, chosen with the user; Down / Up / Back back
  into the settings), the tabs registered as one of the page's input lists
  (no grey panel side). The keyboard on the tabs: run `-9` pending. The
  title screen only so far; the Start menu path next.
- Parked: an event or query for "a game menu is open" (needs a study of the
  game's menu bytes first).

## Release 0.1.0 (released 2026-10-14; the order agreed with the user, 2026-10-12)

Not a date; the steps in order. Each ends with what it needs from others.

1. **Tell Astera Loader what CMFM needs.** Written: `docs/al-requests.md`
   item 1 (checked writes to two values in the game's read-only data, the
   Start menu's tab lists; the Options table swaps turned out to be in
   writable data and need nothing, commit 4fc3ba2). AL answered the same
   day: `al.patch` in AL 0.2.0 (API 1.8). CMFM uses it and requires API
   1.8; its own read-only helper is gone. Done: confirmed in game (run
   `alpatch-1`: both values written by al.patch and put back by CMFM, owner
   cmfm in AL's log); AL released `v0.2.0`.
2. **The DLSS mod adapts.** Done: `mhw_dlss` 0.16.0 has a "DLSS" mod with
   Settings and Status pages on `cmfm.menu` 1.2 (run game-56 in the DLSS
   repository: 11 live multiplier changes, the ini saved in place); its
   reply raised no blocker in 1.2 (answers at the end of
   `docs/handoff-dlss-mod-20261012.md`).
3. **Test.** The user's call (2026-10-12): no scripted checklist run;
   the features were each tested in game when built, and the DLSS mod's
   pages in its own run (game-56). Done: SharpPluginLoader (run spl-1:
   SPL's own crashes with the Stracker plugins, CMFM not involved; a
   refused Start menu entry now costs only that entry, commit 3221211);
   cost with the menu closed on the title screen (about 27 ns per text
   lookup, 6 us/s; the title scene hook about 0.3 us per frame). Left to
   ordinary play with the measurement build installed (cmfm.log logs it):
   the cost in play, a long session. Never tried, low risk (the page is
   the game's own menu): pad (LB / RB), alt-tab and screen-mode changes
   with the page open; a TOML write in game; Fexty's Mod using a rebound
   key in play.
4. **Release:** written 2026-10-12: README, `docs/player-guide.md`
   (install, use, troubleshooting: the Steam start crash, SPL), and
   `docs/config-pages-guide.md` (description files; linked from the author
   guide). Frozen (the user's call): `cmfm.menu` 1.2 and description
   format 1 (decisions item 14, tag `cmfm-menu-1.2`). Community pages stay
   out of the package (Fexty's stays local). The repository is on GitHub,
   private, for now. The plan (the user, 2026-10-12): the DLSS mod gets
   ready to ship first; then the CMFM repository goes public and CMFM and
   the DLSS mod ship on Nexus together, one after the other.
   Closed beta 2026-10-14 (with the DLSS mod's beta bundles): two list bugs
   found and fixed (beta.2: the mod list's window; beta.3: Up/Down
   direction with few pages). **Released 2026-10-14** on the user's go:
   0.1.0 (`docs/release-notes-0.1.0.md`, tag `v0.1.0`), the package
   (`AsteraLoader\plugins\cmfm\` with cmfm.dll, the player guide, the
   description-file guide and the licence; `nativePC\ui\cmfm\mods.gui`), the
   repository public with it (the edited layout included). No VirusTotal
   for CMFM (the user: Astera Loader needs it, CMFM does not).
   **On Nexus Mods since 2026-10-15** (mod 8856, the same archive as the
   GitHub release). Astera Loader's Nexus page waits for Nexus's review of
   its file, so CMFM's page requires Astera Loader through its GitHub
   releases for now and says it will switch (the user's call). The page
   material is kept out of git (`local/nexus/`, the user's rule).

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
4. **Key bindings.** Done (tag `key-bindings-1`, run `keys-1`).
   `CMFM_TYPE_KEY`: Confirm, press a key (with Ctrl, Shift, Alt; mouse
   buttons 3 to 5 too), Esc or a click cancels, Backspace clears, 10 seconds;
   the row shows the key's name, in red while another mod's KEY setting
   overlaps it (named in the help line). The value is what Astera Loader's
   hotkeys take, so a mod passes it straight on (MCM: key maps with conflict
   detection, its most used feature). Keyboard first; pad bindings later.
5. **More settings per page.** Done (tag `long-pages-1`, run `rows-1`).
   64 settings per page (`CMFM_MAX_SETTINGS`), and the reset row always fits
   on top. The game's tab object holds 24 rows, but CMFM's panel window gives
   the game only the 14 rows it shows; the two views that show a tab whole
   (the game's Options page fallback, CMFM's page without its own layout) get
   a table cut at 24 rows. Still shared by all pages: 200 rows and about 900
   label/value text ids (pages that no longer fit are listed as not shown);
   lifting those is a later step if mods need it.
6. **No shared caps.** Done (tag `no-caps-1`, run `caps-1`: 101 pages, 1347
   rows); asked for by
   the user after item 5 ("lets do it properly"). The study
   (research/native-menu.md, "No caps") found every shared limit to be
   CMFM's own: row texts now live in the game's text file 23, far above its
   own numbers (the game sends every number above 10000 there), setting ids
   run from 100 to 100 + 2^20, the model sizes itself. Left: 1024 settings
   per page and 1024 pages (limits against mistakes), and per setting the
   pick list's 14 rows and the slider's 101 positions (Later: long pick
   lists, finer sliders).
7. **One entry per mod, its pages in a dropdown** (designed with the user,
   2026-10-12, from two mockups). Done (tag `mod-dropdowns-1`, runs
   `modlist-1` and `-2`; the marker is a small grey triangle through the
   game's `<SIZE>` tag). The mod list shows one entry per mod (the
   pages of one Astera Loader plugin); a mod with several pages opens a
   dropdown under its entry listing every page, the default (first) one
   included. A select philosophy (superseded in 0.2.0 by decisions item
   16: the game's own handling): hover and the arrow keys only highlight;
   a click or Confirm selects (a mod: its dropdown opens and its default
   page shows; a page: it shows; the open mod's entry again: the dropdown
   folds, the page stays). One dropdown open at a time; a mod always opens
   on its default page. Q / E (pad LB / RB, once the game is known not to
   use them there) step through the selected mod's pages. The header names
   mod and page; the help line says which page of how many. The Overview
   lists one line per mod. Interface: an optional mod name on the page
   (`cmfm_page_t.mod`, since 1.2; without it, the first page's title).

## 1.3: config pages without code (designed 2026-10-12; steps 1-7 done)

Status: steps 1-6 done: the test pages (tag `config-pages-1`, run
`cfgpages-1`: only the edited values changed, backups byte-identical) and
Fexty's Mod as the real case (tag `fexty-page-1`, runs `fexty-1`, `-2`: a
community page; a hotkey rebound in the in-game menu was written exactly,
the backup byte-identical; the mod using it live in play is not checked
yet; its keys turned out to be hex Windows key codes, `vk-hex`). Step 7,
TOML, done (tag `toml-page-1`, run `toml-1`: the test page
`cmfm_test_toml.json` shows its values in game; its write is unit-tested,
not yet exercised in game). On the way, a crash fixed
(tag `slider-crash-fix-1`): the game's tab show read CMFM's slider rows from
save data when the page opened again in game.

Plan: `docs/design/config-pages.md` (decisions.md item 13). A JSON
description file in CMFM's folder (`AsteraLoader\plugins\cmfm\pages\`)
maps menu rows to plain values in a mod's own config file; CMFM shows the
page and edits that file surgically when the Mods page closes. No code, so
Stracker and SharpPluginLoader plugins and abandoned mods can join, through
their authors or the community (MCM Helper's step). Steps:

1. JSON parser with positions and comments, surgical editor, tests.
2. INI the same way.
3. Description files: discovery, validation with line numbers, pages from
   them, values read when the page opens; conditions.
4. Writing at close: verify, atomic swap, backup, merge outside changes.
5. Keys (`vk`, `dik`, `names`), `expect`, author-over-community precedence,
   the Overview's community notes.
6. A test mod with JSON and INI configs; then Fexty's Mod (with the user's
   say-so before CMFM writes its file).
7. TOML.

Later for 1.3: adding missing values, lists of tables ("for each"; More
Auto Shoutouts, its first case, is a rule engine and out of scope), reload
notifications.

## Later

- Colour settings (overlay and HUD mods), translations of mod texts, text
  input, a per-row Default key.
- Choice lists longer than 14 entries: a scrolling pick list (arrows today).
- The author guide and the handoff to the DLSS mod: written 2026-10-12
  (`docs/author-guide.md`, `docs/handoff-dlss-mod-20261012.md`, the handoff
  updated for 1.2 the same day); `cmfm.menu` 1.1 frozen (decisions item
  11). Release step 2.
- ~~Popups and notifications~~: not for now (decisions item 12): greyed
  rows, refusals and status lines cover the known cases. The sketch
  (`cmfm_popups_design_v1`: the game's message window, its message log, a
  Messages list on the Overview) stays on file in case a real need appears.
- A long play session (release step 3); then drop the old options view
  fallback. The data-patch helper's AL request: written (release step 1).
- SharpPluginLoader compatibility (it swaps the same Start menu entries):
  release step 3.
- Languages for CMFM's own texts.
- Greying out the game's own AA/DLSS options while the DLSS mod owns AA.
- Release 0.1.0: see the top of this file.
