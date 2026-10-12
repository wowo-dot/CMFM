# Config pages without code (cmfm 1.3)

Designed with the user on 2026-10-12 (decisions.md item 13). Status: plan;
built in the steps at the end, each tested in game.

## What it is for

Mods that are not Astera Loader plugins (Stracker's Loader plugins,
SharpPluginLoader plugins, old and abandoned mods) get a page in CMFM's
Mods menu without any code: a small description file says which values of
the mod's own config file the page shows; CMFM reads and writes them in that
file. The C interface (`cmfm.menu`) stays the way for mods built with CMFM
in mind (live values, refusals with reasons, buttons, status lines).

Anyone may write a description: the mod's author (shipped with the mod) or
the community (a page pack for a mod whose author is gone). For the player
both look like any other mod in the list.

## The configs it meets (survey of the user's installed mods)

| Mod | File | Syntax | Notes |
|---|---|---|---|
| Fexty's Mod | `nativePC/plugins/FextyModConfig.json` | JSON, nested, tabs | `true`/`false`, choice strings, hotkeys as Windows key codes in hex without 0x, in strings (`"62"` = Numpad 2, from its Nexus description); hotkeys and cooldowns re-read live |
| foodskill | `nativePC/plugins/foodskills.json` | JSON | numbers in strings (`"5"`), a list of names |
| forcePlayerCountQuest | `nativePC/plugins/quest_post.ini` | INI | `[Post] Players=2`, switches as `1`/`0` |
| Stracker's Loader | `loader-config.json` | JSON | plain switches |
| autofood | `nativePC/plugins/auto_food_settings.json` | JSON | hotkeys as `["Ctrl","Shift","L"]`; large rule lists |
| More Auto Shoutouts | `nativePC/plugins/mas-config.toml` | TOML | a rule engine, not a settings file: out of scope (the user, 2026-10-12) |

## Decisions

- **Where:** `<game>\AsteraLoader\plugins\cmfm\pages\*.json`, CMFM's own
  plugin folder (Astera Loader's guide: what is in a plugin's folder belongs
  to it). Under MO2 any mod can add a file there (an author's mod, a
  community page pack); CMFM reads the folder through the game's virtual
  folder (`al_init_t.folder`, never `folder_real`), so it sees every MO2
  mod's files.
- **Format:** JSON, with `//` and `/* */` comments allowed (people write
  these by hand). Strict otherwise, so a mistake is reported with its line
  on the Overview.
- **Writing:** once, when the Mods page closes.
- **Syntaxes CMFM edits:** JSON, INI and TOML (TOML since step 7). Plain
  values only (switches, numbers, choices, keys, read-only lines); lists and
  rule tables are not menu material.

## The description file

```jsonc
{
  "cmfm": 1,                                  // description format version
  "mod": "Fexty's Mod",                       // the mod list entry
  "version": "1.0",                           // optional: shown in the header
  "source": "community",                      // "author" or "community"
  "made_by": "akif",                          // community pages: shown on the Overview
  "config": {
    "file": "nativePC/plugins/FextyModConfig.json",   // relative to the game folder
    "syntax": "json",                          // optional: from the extension
    "expect": ["Defaults.HeroicsModeDefaultOn"]       // optional: paths that must exist
  },
  "applies": "restart",                       // "restart" (default) or "live"
  "pages": [
    { "title": "Modes", "help": "...", "settings": [
      { "section": "At start" },
      { "id": "heroics", "label": "Heroics mode", "type": "bool",
        "path": "Defaults.HeroicsModeDefaultOn", "default": true, "help": "..." },
      { "label": "Cat helpers", "type": "choice", "path": "Defaults.CatHelpersDefault",
        "choices": { "gajalaka": "Gajalaka", "boaboa": "Boaboa" }, "default": "gajalaka" },
      { "label": "Heroics cooldown", "type": "int", "path": "Cooldowns.HeroicsModeCooldown",
        "min": 0, "max": 600, "step": 10, "unit": " s", "lock_unless": { "heroics": true } },
      { "label": "Heroics hotkey", "type": "key", "path": "Hotkeys.HeroicsModeHotkey",
        "keys": "vk-hex", "default": "Num 2", "applies": "live" },
      { "label": "Config file", "type": "text", "path": "Defaults.CatHelpersDefault" }
    ]}
  ]
}
```

- `type`: `bool`, `int`, `float` (`min`, `max`, `step`, `decimals`, `unit`,
  up to 101 values as in the C interface), `choice` (`choices`: stored value
  -> label, in order), `key` (`keys`: `vk` decimal or 0x hex, `vk-hex` hex
  digits without 0x, `dik` DirectInput scan codes, or `names`), `text` (the
  value shown read-only), `section`.
- `applies` on a setting overrides the file's (`restart` / `live`).
- `path`: JSON `a.b.c` (array items `a[0].b`); INI `Section.Key` (`Key` for
  keys before any section); TOML as JSON: `table.key`, whether the file
  writes it as a `[table]`, a dotted key or an inline table; an array of
  tables like a list (`trigger[0].name`, `trigger[1].check[0].value`). A key
  that holds a dot is written as a list: `["Section", "my.key"]`.
- `default`: what "Reset to defaults" writes, in the label's terms (a
  choice's stored value, a key's name).
- `hide_unless` / `lock_unless`: `{ "<id>": <value> }`, all must hold; the
  row hides (or greys, with "Needs <label> <value>.") otherwise.
- Several pages form one mod (the dropdown of 1.2). `id`s are unique in the
  file; rows without `id` cannot be named in conditions.
- **Precedence:** when an author's page and a community page name the same
  config file, the author's page shows and the community page is listed on
  the Overview as replaced. Two pages of the same source for one file: the
  first by file name shows, the other is listed.

## Editing a config file (surgical edits)

1. **Parse with positions:** every value's start and end in the file and
   its token kind (string with its quote, number with its decimals, bare
   literal with its case).
2. **New token in the old token's style:** `true` -> `false`; `1` -> `0`;
   `"5"` -> `"6"` (quotes kept); `"62"` -> `"63"` (hex key codes: Numpad 2 -> 3);
   `ON` -> `OFF`; `1.50` -> `2.00`. The mod's own parser read the old form,
   so it reads the new one the same way. TOML: a basic string (`"..."`) is
   written with TOML's escapes, a literal string (`'...'`) keeps its quotes
   (a value holding `'` or a line break is refused); a float stays a float
   (`3` is written `3.0`), an integer an integer (`3.0` is written `3`, a
   fraction is refused); hex stays hex (`0x75` -> `0x76`).
3. **Splice:** bytes before the value + the new token + bytes after it.
   Comments, order, whitespace, line endings and unknown keys stay
   byte-identical; for INI and TOML inline comments the spaces before `;`
   or `#` are padded so the comment column stays.
4. **Verify:** parse the result again; it must parse, the edited paths must
   hold the new values, every other value must be unchanged. Otherwise no
   write, the reason on the Overview.
5. **Save safely:** in the file's real place (under MO2 the mod folder it
   comes from, found through a handle: `GetFinalPathNameByHandleW`), write
   `<file>.cmfm-new`, keep the original as `<file>.cmfm-backup` the first
   time CMFM ever writes it, then swap the new file in (`MoveFileExW`, replace
   and write through). At write time the file is read again: if
   it changed while the page was open, CMFM's edits are applied to the fresh
   text (they are by path); a value changed in both places keeps the file's.
6. **Refused, read-only with the reason:** a value not in the file (adding
   lines is a structural edit, later), a key that appears twice, an encoding
   other than UTF-8 / ASCII, a token that does not fit its type, a file that
   does not parse (the whole page read-only). TOML multi-line strings,
   dates and times are shown (`text` rows) but never edited.

## Steps

1. A JSON parser with positions and comments (descriptions and JSON
   configs), with the surgical editor and its tests.
2. INI the same way.
3. Description files: discovery, validation (errors with line numbers on
   the Overview), pages built from them (bool, int, float, choice, text,
   section; conditions), values read when the page opens.
4. Writing at close (verify, swap, backup, merge with outside changes).
5. Keys (`vk`, `dik`, `names`), `expect`, precedence and the Overview's
   "Community page by ..." / "replaced".
6. A test mod with its own JSON and INI configs; then Fexty's Mod as the
   real case (with the user's say-so before CMFM writes its file).
7. TOML: a reader with positions (`src/config/toml.cpp`: tables, arrays of
   tables, inline tables, dotted and quoted keys, every string kind,
   integers in four bases, floats, dates and times), the editor's TOML
   rules, a TOML test page.

Later: adding a missing value (insert a line), lists of tables ("for each"),
reload notifications for mods that re-read their file.
