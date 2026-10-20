# CMFM decisions

Numbered, newest last. A later item can supersede an earlier one; say so in
both. Items 1 to 4 come from Astera Loader's `docs/decisions.md` (items 2,
4, 5 and 11 there), from when the loader and the menu were one project.

1. **CMFM is an Astera Loader plugin** (AL item 11, 2026-10-09). The loader
   became Astera Loader; CMFM ("Configuration Menu For Mods") kept its name
   for the in-game menu. It talks to the game only through AL's services.
2. **Menu technology: Dear ImGui for v1** (AL item 2, 2026-10-08), themed to
   sit close to MHW's look. Later, reverse the game's own menu system so
   CMFM can add entries or pages inside the game's UI. That work also
   unlocks the AA/DLSS option lockout the DLSS mod asked for. Reversing is
   done on private IDA copies under `local/`, with hooks through `al.hooks`
   at signature-verified sites only. *Superseded by item 7: native first.*
3. **Settings storage: per-plugin files** (AL item 5). Each plugin's own
   config file is the source of truth; CMFM reads and writes through the
   plugin's callbacks.
4. **Licence: MIT** (AL item 4), like Astera Loader, Stracker's Loader and
   SharpPluginLoader.
5. **Own repository** (the user's call, 2026-10-11): CMFM moved out of the
   AL repository into `D:\mh world modding\CMFM`, with its own sessions,
   like the DLSS mod. AL is read-only from here; CMFM's needs from AL go
   into `docs/al-requests.md`.
6. **Distribution: a separate mod** (the user's call, 2026-10-11). CMFM
   ships as its own download, not inside AL's package. This supersedes AL
   item 11's "bundled first-party plugin" wording. Players install it next
   to AL (`<game>\AsteraLoader\plugins\cmfm\cmfm.dll`, or its own MO2 mod).
7. **Menu: native first, decided by a reversing study** (the user's call,
   2026-10-11). Supersedes item 2's "Dear ImGui for v1". The goal is an
   MCM-style menu inside the game's own UI (a "Mods" entry or page), drawn
   by the game, so it looks and handles like MHW and needs no overlay
   graphics under frame generation. Order:
   1. The plugin interface `cmfm.menu` first, limited to what a native
      menu can show (toggle, choice list, slider, read-only text line), so
      plugins never depend on which front end draws them.
   2. Reverse the game's Options menu in a private IDA database
      (`local/ida/`) until three questions have answers: can CMFM add a row
      or a page, show its own text, and see when the player changes a
      value. The same work serves the DLSS mod's AA/DLSS option lockout.
   3. A native front end if the answers are yes; otherwise Dear ImGui,
      themed close to MHW. A small ImGui layer may come later anyway, for
      debug pages and as a fallback when hook sites do not match.

   Native means reusing the game's layouts and resources, in memory or as
   edited files CMFM ships (item 10 supersedes "never ships edited game
   files"). No deadline: this is the user's fun project, and the reversing
   takes as long as it takes.
8. **Menu placement: a "Mods" entry right below Options, in both menus**
   (the user's design, 2026-10-11). In the in-game Start menu's System tab
   (Options, **Mods**, View Mode, Steam Store, Save, Return to Title, Quit
   Game) and in the title screen menu (Start Game, Gallery, Steam Store,
   Options, **Mods**, Credits, Support, Quit). Both open the same CMFM page,
   registered in a GUI group that is alive at the title and in the world
   (like Keyboard Settings, which both Options menus open). Order: the
   in-game System tab first, then the title screen. Fallback if a hook site
   proves unsafe: a "Mods" row inside the Options page (a data-only table
   swap, `docs/research/native-menu.md`).
9. **Prototype first, AL requests after** (the user's call, 2026-10-11).
   CMFM builds the native menu with what AL already offers (`al.hooks` for
   game functions) and confirms it in game before asking AL for anything
   new. The one missing piece, writing the game's read-only data (the System
   tab's table pointers), is a small CMFM helper during the prototype: it
   checks the expected old bytes, logs the change and restores it on unload.
   When the menu works, what was actually used becomes the AL request.
   Done 2026-10-12: `docs/al-requests.md` item 1 (two values in `.rdata`,
   the Start menu's tab lists; the Options table swaps turned out to be in
   writable `.data` and need no request). AL answered with `al.patch`
   (API 1.8) the same day; CMFM uses it and requires API 1.8.
10. **CMFM may ship edited game files** (the user's call, 2026-10-11).
    Supersedes item 7's "never ships edited game files". The trigger: the
    title Options layout has exactly five panels with row lists, so a page
    of CMFM's own (any number of mods, a clipped scrolling list, room for
    sliders and side lists) may need a layout file CMFM edits. Boundaries:
    the installed game is never changed on disk (exe, chunk archives, game
    directory); edited files live in `assets/`, ship inside CMFM's own mod
    folder and are loaded from there; only files CMFM actually changed ship,
    never unmodified Capcom files or bulk extracts; ask the user before
    pushing them to a public remote. How the game would load them (loose
    files through AL or MO2) is part of the layout study.
11. **`cmfm.menu` 1.1 is the first published interface** (2026-10-12, with
    the author guide and the DLSS-mod handoff). CMFM 1.0 never shipped; its
    struct sizes stay accepted. From the handoff on, 1.1 is frozen: clients
    copy `include/cmfm/cmfm.h` and build against it. Anything new goes
    into 1.2, additive as the header's rules say; comments may still be
    corrected.
12. **No popups for now** (the user's call, 2026-10-12). The "warning popups
    and notifications" requirement came from AL's DLSS requirements doc
    (2026-10-08, written for an overlay menu); its examples (frame
    generation needs an RTX 40/50, 3x needs an RTX 50, a conflicting game
    option, "disabled: reason") are covered in place by what 1.1 has: a
    greyed row with its reason (`locked`, asked whenever a page opens, so it
    follows a check made after `add_page`), a refused value with its reason
    (`validate`), and status lines. Popups come back only for a real need a
    client brings.
13. **Config pages without code: description files in CMFM's folder**
    (designed with the user, 2026-10-12; `docs/design/config-pages.md`). A
    JSON description (comments allowed) in
    `AsteraLoader\plugins\cmfm\pages\` maps menu rows to plain values in a
    mod's own config file; anyone may write one (`source`: author or
    community; the author's page wins). CMFM edits JSON and INI first, then
    TOML, surgically (only the value's characters, the old token's style,
    verified, swapped in atomically, a one-time backup), and writes when the
    Mods page closes. Rule engines like More Auto Shoutouts' are not
    configs and stay out (the user's call).
14. **`cmfm.menu` 1.2 and description format 1 are frozen** (the user's
    call, 2026-10-12, before CMFM 0.1.0). The header
    `include/cmfm/cmfm.h` (its declarations unchanged since commit
    95a0ffa; the DLSS mod builds against it) and the description format
    (`docs/config-pages-guide.md`) no longer change in meaning. Anything
    new to the C interface goes into 1.3, additive as the header's rules
    say; comments may still be corrected. Description files are read
    strictly (an unknown field refuses the file, so a typo is caught), so
    new fields go into description format 2, which a newer CMFM reads
    beside format 1; an older CMFM refuses a format 2 file with a clear
    reason ("this CMFM reads description format 1"). Community pages
    such as Fexty's Mod's stay out of the release package for now (kept
    in `examples/config_pages/community/` and the user's own install).
15. **`cmfm.menu` 1.3: what a client mod asked for** (2026-10-17, from a
    client mod's feedback; the user: "let's add these too"). Additive, 1.2
    stays frozen: the header promises that callbacks never overlap and
    arrive in order (`press` returns before CLOSED; true of CMFM since 1.0);
    the text limits are defines (`CMFM_MAX_LABEL` and the rest, 1.0's
    values); `include/cmfm/cmfm_check.h`, header-only C, applies add_page's
    rules outside the game, and CMFM's own add_page calls it so the two
    cannot drift; a BUTTON's question can be worded when pressed (an
    optional page callback `question`, read when `size` covers it; an older
    CMFM ignores it and asks `confirm`). Plugins should ask for the lowest
    version they need (1.2 unless they rely on 1.3). Not taken: an event or
    query for "a game menu is open" (CMFM's hooks do not follow the game's
    own menus; a candidate signal, the menu bytes every game menu sets each
    frame, would need its own study).
16. **The mod list handles like the game's own Options** (the user's call,
    2026-10-20: players found it confusing). Supersedes the select
    philosophy of roadmap item 7 (2026-10-12: the highlight only
    highlights, a click or Confirm selects). As on the game's page: the
    pointer and the arrow keys move the highlight and the panel shows the
    highlighted entry; one click or Confirm goes into the panel (a mod's
    dropdown opens then); hovering never moves the focus, either way; in the
    panel, Back or a right click returns. Only a click, Confirm or Q / E
    opens or folds a dropdown, so no entry moves under the pointer.
17. **A mod's pages are tabs above the panel** (designed with the user,
    2026-10-20, from their sketch). Supersedes the dropdowns of roadmap item
    7. The mod list is flat, one entry per mod; a mod with several pages
    gets a row of tabs between its header and the panel (hidden for one
    page). The game's own tab bars hold 2 to 4 fixed, drawn tabs, so CMFM's
    is a copy of the game's category list laid out sideways (its text rows
    and cursor) in CMFM's layout; the panel moved down one row for it. A
    click on a tab shows that page (and goes into the panel, the one-click
    rule); Q / E (pad LB / RB) step through the pages; more pages than fit
    (four) scroll, the shown tab kept in view, a « or » on an edge tab
    when more lie beyond. Each mod shows the page it showed last. The
    header names the mod, no longer the page. "We can improve it after"
    (the user): the look is a first version.

