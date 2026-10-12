# Settings pages without code: description files

A description file puts a mod's settings into CMFM's Mods menu without any
code. It is a small JSON file that says which values of the mod's own
config file to show, how (a switch, a list, a slider, a key), and with
what labels. CMFM reads the values from that config file, shows them, and
writes the player's changes back into it. The mod itself needs no change:
this works for Stracker plugins, SharpPluginLoader plugins and abandoned
mods alike.

Anyone can write one: the mod's author ("author" page) or a player
("community" page). For a mod written for Astera Loader, the C interface
(`docs/author-guide.md`) can do more (live status lines, values that apply
at once, refusals with a reason); a description file is the way for
everything else.

## 1. Where it goes

`<game folder>\AsteraLoader\plugins\cmfm\pages\<name>.json`

- Under Mod Organizer 2, any mod can bring its own file at that path (an
  author's mod, or a pack of community pages); CMFM sees every mod's files.
- One file describes one config file. A mod with several config files
  needs several description files.
- **Author pages win:** when an author's page and a community page name
  the same config file, the author's page shows and the community page is
  listed on the Overview as replaced. Two pages of the same kind for one
  file: the first by file name shows.
- A file with a mistake is not shown; the Overview lists it as refused
  with the line and the reason (for example `line 7: unknown field
  "lable" in a setting`).

## 2. A first file

A mod's config, `nativePC\plugins\quest_post.ini`:

```ini
[Post]
Players=2
PassEnabled=1      ; 1 on, 0 off
```

Its description file, `AsteraLoader\plugins\cmfm\pages\quest_post.json`:

```jsonc
{
  "cmfm": 1,
  "mod": "Quest post",
  "source": "community",
  "made_by": "your name",
  "config": { "file": "nativePC/plugins/quest_post.ini", "expect": ["Post.Players"] },
  "applies": "restart",
  "pages": [
    {
      "title": "Quest post",
      "settings": [
        { "label": "Players", "type": "int", "path": "Post.Players", "min": 1, "max": 4, "default": 4,
          "help": "How many hunters the quest board waits for." },
        { "label": "Passcode", "type": "bool", "path": "Post.PassEnabled", "default": false }
      ]
    }
  ]
}
```

In game, "Quest post" appears in the mod list. The player changes Players
to 3 and closes the Mods page; CMFM writes `Players=3` into the ini and
changes nothing else in it (not the comment, not its column, not the line
endings).

`//` and `/* */` comments and trailing commas are allowed in description
files. Everything else is strict JSON, so a mistake is reported with its
line.

## 3. The file, field by field

| Field | Required | What |
|---|---|---|
| `cmfm` | yes | `1`, the description format (frozen: later CMFMs keep reading format 1 files) |
| `mod` | yes | the mod list entry, 1 to 24 bytes |
| `version` | no | shown in the page header, 1 to 24 bytes |
| `source` | yes | `"author"` or `"community"` |
| `made_by` | no | who wrote the file; community pages show it on the Overview |
| `config` | yes | the mod's config file, below |
| `applies` | no | `"restart"` (the default) or `"live"`: when the mod picks up a change |
| `pages` | yes | 1 or more pages; several pages form one mod with a dropdown |

`config`:

| Field | Required | What |
|---|---|---|
| `file` | yes | the config file, relative to the game folder (`nativePC/plugins/x.json`); no drive, no `..` |
| `syntax` | no | `"json"`, `"ini"` or `"toml"`; otherwise from the file's extension (`.json`, `.ini`, `.cfg`, `.toml`) |
| `expect` | no | paths that must be in the file; if one is missing, the page is not shown ("another version of the mod?") |

A page: `title` (1 to 24 bytes), `help` (shown when the page is
highlighted in the list), `settings` (1 to 1024; at least one that is not
a heading).

## 4. Settings

Every setting has a `label` (1 to 64 bytes) and, except headings and text
lines, a `type` and a `path`. Optional on all: `help` (up to 200 bytes,
shown in the help line), `id` (to name it in conditions), `applies` (this
setting's own `restart` or `live`), `hide_unless`, `lock_unless`.

| Type | Extra fields | In the menu |
|---|---|---|
| `bool` | `default`: true / false | a switch, Off / On |
| `int` | `min`, `max`, `step` (whole numbers; step 1 if left out), `unit`, `default` | a slider (up to 101 values) |
| `float` | `min`, `max`, `step`, `decimals` (0 to 3), `unit`, `default` | a slider (up to 101 values) |
| `choice` | `choices`, `default` | a list; `choices` is `{ "stored": "Label", ... }` or `["a", "b"]` (1 to 32, labels up to 32 bytes); `default` is a stored value |
| `key` | `keys`, `default` | a key binding (below); `default` is a key's name, like `"F4"` or `"Num 2"` |
| `text` | | the value, read-only (a version, a path) |
| `{ "section": "Heading" }` | `id`, `hide_unless` | a centred heading |

`unit` (up to 8 bytes) follows the number: `"unit": " s"` shows `45 s`.

### Paths

| Config | Path |
|---|---|
| JSON | keys and indices: `Hotkeys.Toggle`, `list[0].name` |
| INI | `Section.Key`; `Key` for a key before any section; matched without regard to case |
| TOML | as JSON: `table.key`, however the file writes it (a `[table]`, a dotted key, an inline table); an array of tables like a list: `trigger[0].name` |

A key that contains a dot is written as a list: `"path": ["Section",
"my.key"]`.

### Keys

`keys` says how the mod's file holds a key:

| `keys` | In the file | Example (Numpad 2) |
|---|---|---|
| `vk` | a Windows key code, decimal (or hex with 0x, kept as written) | `98` or `0x62` |
| `vk-hex` | a Windows key code in hex without 0x | `"62"` |
| `dik` | a DirectInput scan code | `80` |
| `names` | the key's name, modifiers included | `"Num 2"`, `"Ctrl+H"` |

The player binds a key as in any CMFM key row: Confirm, then press the
key (Esc or a click cancels, Backspace clears). A file that holds a code
cannot hold Ctrl, Shift or Alt, so CMFM refuses those combinations with a
reason; a file that holds names cannot hold "no key".

### Defaults and "Reset to defaults"

`default` is what CMFM's "Reset to defaults" row writes. A page gets that
row only when every value on it has a default; leave defaults out where you
do not know the mod's own.

### Live or restart

`applies: "restart"` (the default) marks a changed row with a star and
shows "Restart needed" until the game restarts or the value goes back.
Use `"live"` only for values the mod reads again while the game runs (its
documentation says so, or you checked).

### Conditions

```jsonc
{ "id": "minimap", "label": "Show minimap", "type": "bool", "path": "Display.ShowMinimap" },
{ "label": "Minimap size", "type": "int", "path": "Display.MinimapSize", "min": 50, "max": 150,
  "hide_unless": { "minimap": true } },
{ "label": "Cooldown", "type": "int", "path": "Cooldowns.Heroics", "min": 0, "max": 100,
  "lock_unless": { "heroics": true } }
```

- `hide_unless`: the row is hidden unless every condition holds.
- `lock_unless`: the row is greyed, with "Needs <label>: <value>." in the
  help line, unless every condition holds.
- A condition names another setting's `id` and one of its values: true or
  false for a bool, a number for an int or float, a stored value for a
  choice.

## 5. How CMFM reads and writes the config file

- **Reading:** when the Mods page opens. A value the file does not have
  makes its row read-only, with the reason in the help line (CMFM does not
  add lines). A key that appears twice is read-only too. A config file
  that is missing or does not parse when the game starts hides the page
  (the Overview says why); one that breaks while the game runs makes the
  page read-only.
- **Writing:** once, when the Mods page closes, and only the values the
  player changed. At that moment CMFM reads the file again: if the mod or
  the player changed a value meanwhile, the file's value is kept.
- **Only the value's characters change,** in the form the file already
  uses, so the mod reads the new value as it read the old one:
  - switches keep their words and case: `true`/`false`, `1`/`0`,
    `on`/`off`, `yes`/`no`, `enabled`/`disabled`; `TRUE` becomes `FALSE`;
  - numbers keep their kind: a quoted number stays quoted (`"5"` to
    `"6"`), hex stays hex (`0x75` to `0x76`), a TOML float stays a float;
    floats are written with `decimals` places;
  - strings keep their quotes;
  - comments, order, spacing, unknown keys and line endings stay as they
    were; for INI and TOML, an inline comment keeps its column.
- **Checked before it is saved:** the edited text is parsed again; it must
  hold the new values and every other value unchanged. Otherwise nothing
  is written and cmfm.log says why.
- **Saved safely:** in the file's real place (under MO2, the mod folder it
  comes from; nothing lands in Overwrite), through a temporary file that
  replaces the old one in one step. The first time CMFM writes a file, it
  keeps the original next to it as `<file>.cmfm-backup`.
- **Never edited:** TOML multi-line strings, dates and times (shown by
  `text` rows), lists and tables as a whole.

## 6. Checking your file

- Start the game through Astera Loader and open Mods. Your mod should be in
  the list; if it is not, the Overview lists the file as refused, with the
  line and the reason.
- `AsteraLoader\logs\plugins\cmfm.log` says
  `pages/<name>.json: N page(s) for <mod> (<config file>)` when the file is
  read, logs every value set, and `N change(s) written to <config file>`
  when the Mods page closes.
- Change one value, close the page, and compare the config file with its
  `.cmfm-backup`: only that value should differ.
- `examples/config_pages/` in CMFM's repository holds working pages for a
  JSON, an INI and a TOML config, a page replaced by an author's page, a
  page with a mistake, and a community page for Fexty's Mod.

## 7. Checklist

- [ ] `cmfm: 1`, `mod`, `source`, `config.file`, `pages`.
- [ ] `expect` names two or three paths every version of the mod's file
      has.
- [ ] Each `path` checked against a real config file; key encodings
      checked against the mod's documentation.
- [ ] `default` only where the mod's own default is known.
- [ ] `applies: "live"` only where the mod really reads the file again.
- [ ] A community page: `made_by` filled, and the mod's author's page (if
      one appears) left to replace it.
