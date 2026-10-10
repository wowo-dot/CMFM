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
   at signature-verified sites only.
3. **Settings storage: per-plugin files** (AL item 5). Each plugin's own
   config file is the source of truth; CMFM reads and writes through the
   plugin's callbacks.
4. **Licence: MIT** (AL item 4), like Astera Loader, Stracker's Loader and
   SharpPluginLoader.
5. **Own repository** (the user's call, 2026-10-11): CMFM moved out of the
   AL repository into `D:\mh world modding\CMFM`, with its own sessions,
   like the DLSS mod. AL is read-only from here; CMFM's needs from AL go
   into `docs/al-requests.md`.
