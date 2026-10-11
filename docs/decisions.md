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
