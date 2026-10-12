# Adding your mod's settings to CMFM

CMFM ("Configuration Menu For Mods") puts a **Mods** entry right below
Options in Monster Hunter: World's title menu and in the in-game Start menu
(System tab). The page it opens is the game's own settings page: a list of
mods on the left and the highlighted mod's settings on the right, with the
game's arrows, sliders, pick lists, Yes/No question and help line. Your
Astera Loader plugin describes its settings once; CMFM draws them and
reads and writes every value through your callbacks.

Your config file stays the source of truth. CMFM stores none of your
values, so a broken or missing CMFM can never touch them, and players can
keep editing your ini by hand.

The interface is `include/cmfm/cmfm.h` (`cmfm.menu` 1.2, plain C). Its
comments are the reference; this guide is the walk-through. The example
plugin `examples/cmfm_demo/cmfm_demo.c` uses every feature. CMFM runs on
Astera Loader 0.2.0 or newer.

Not an Astera Loader plugin (a Stracker or SharpPluginLoader plugin, say),
or only plain values in a config file? A description file puts them in the
menu without code: `docs/config-pages-guide.md`.

## 1. Getting the interface

Copy `include/cmfm/cmfm.h` unchanged into your project (record the CMFM
commit you copied it from, as you do for Astera Loader's header). Then, in
`al_plugin_init` or any later phase:

```c
const cmfm_menu_t* menu = (const cmfm_menu_t*)api->get_interface(
    self, CMFM_MENU_INTERFACE, CMFM_MENU_VERSION);
if (menu == NULL) {
    /* CMFM is not installed (or too old): run without the menu. */
}
```

- **Keep CMFM optional.** Do not list it in your plugin's dependencies;
  players without CMFM still edit your ini.
- **When to ask:** CMFM loads early (order -100), so `al_plugin_init` works
  unless your `load_after`/`load_before` put you before it; PRE_MAIN or later
  always works.
- **Version:** ask for the version whose features you use: 1.1 is the
  first CMFM publishes, 1.2 adds the features marked "since 1.2" in the
  header (rows that hide, live lines, key bindings, CMFM's reset row,
  pages of up to 1024 settings, no shared limits between mods, several
  pages as one mod). Within a major
  version CMFM only adds: later CMFMs keep accepting your pages, and an
  older CMFM returns NULL when you ask for more than it has.

## 2. Describing your page

Fill a `cmfm_page_t` and an array of `cmfm_setting_t`, then call
`menu->add_page`. A plugin may add several pages: since 1.2 they form one mod
in the mod list, an entry with a dropdown of its pages, the first one the
default (section 4). Give them a `mod` name ("DLSS" for pages "Upscaling" and
"Frame generation"), or the first page's title names the mod; pages with
different `mod` names make separate mods (a debug mod of its own, say).

```c
static const char* const kModes[] = {"Off", "2x", "3x", "4x"};

cmfm_setting_t s[2];
memset(s, 0, sizeof s);
s[0].size = sizeof s[0];
s[0].id = 1;                        /* your id, unique on the page */
s[0].type = CMFM_TYPE_CHOICE;
s[0].label = "Frame generation";
s[0].help = "Generated frames per rendered frame.";
s[0].choices = kModes;
s[0].choice_count = 4;
s[0].default_value.i = 1;           /* the default (see section 4) */

s[1].size = sizeof s[1];
s[1].id = 2;
s[1].type = CMFM_TYPE_TEXT;         /* a read-only line */
s[1].label = "Status";

cmfm_page_t page;
memset(&page, 0, sizeof page);
page.size = sizeof page;
page.plugin = "my_mod";             /* your Astera Loader name */
page.title = "My mod";              /* the mod list entry, 24 bytes at most */
page.help = "What this mod does, in one line.";
page.version = "1.2.0";             /* shown in the page's header */
page.mod = "My mod";                /* since 1.2: the mod list entry of your pages; may be NULL */
page.settings = s;
page.setting_count = 2;
page.user = &my_state;              /* passed to every callback */
page.get = my_get;
page.set = my_set;
page.get_text = my_text;
page.on_event = my_event;

char why[200];
if (menu->add_page(&page, why, sizeof why) == 0) {
    /* refused: log `why` (CMFM logs it and lists it on its Overview too) */
}
```

CMFM copies the page, its settings and every string during `add_page`; your
arrays may go away afterwards. `user` and the callbacks must stay valid for
the whole session. A page added while a Mods page is open appears the next
time one opens. Pages cannot be changed or removed once added; what changes
at run time goes through the callbacks (values, TEXT lines, locks).

### Setting types and how they look

| Type | Value | On the Mods page |
|---|---|---|
| `CMFM_TYPE_BOOL` | `value.i` 0 or 1 | Off / On with the game's arrows |
| `CMFM_TYPE_CHOICE` | `value.i`: the index into `choices` | up to 5 names: arrows; 6 to 14: the game's pick list on Confirm; more: arrows. `CMFM_FLAG_LIST` / `CMFM_FLAG_ARROWS` force either |
| `CMFM_TYPE_INT` | `value.i` from `min` to `max` in steps of `step` | 3 or more values: the game's slider with the value beside it (`unit` after the number); keys move one step, the mouse drags. Up to 101 values, so 0 to 100 in steps of 1 works |
| `CMFM_TYPE_FLOAT` | `value.f`, as INT; `decimals` 0 to 3 | as INT |
| `CMFM_TYPE_TEXT` | none; `get_text` writes the line | a read-only line: label left, your text right |
| `CMFM_TYPE_SECTION` | none; only `id` and `label` | a heading over the rows below it, centred and yellow; the cursor skips it |
| `CMFM_TYPE_BUTTON` | none | a row whose value box shows `button` ("Run" if NULL); Confirm calls `press`, after the game's Yes/No question when `confirm` is set |
| `CMFM_TYPE_KEY` (since 1.2) | `value.i`: a Windows virtual-key code plus `CMFM_KEY_CTRL`, `_SHIFT`, `_ALT`; 0 is no key | the key's name ("Ctrl+F5", "Mouse 4", "None"); Confirm, then the player presses the new key (Esc or a click cancels, Backspace sets no key, 10 seconds to choose); red while another KEY setting of any mod overlaps it, that setting named in the help line |

### Flags

- `CMFM_FLAG_RESTART`: the setting takes effect after the game restarts.
  CMFM says so in the help line, and once the player changes the value away
  from the one the game started with, CMFM marks the row with a star, shows
  "Restart needed" in your page's header, counts it in the line above the
  mod list and on the Overview, until the game restarts or the value goes
  back. "The one the game started with" is the value CMFM reads first in
  the session (the first time your page shows, or just before the first
  change); later reads do not move it, so a `get` that reloads your file
  at OPENED shows hand edits without hiding a pending restart. Only a hand
  edit made before CMFM's first read counts as the start value.
- `CMFM_FLAG_LIST`, `CMFM_FLAG_ARROWS`: CHOICE only, one or the other (see
  the table).
- `CMFM_FLAG_LIVE` (since 1.2, TEXT only): CMFM asks `get_text` about four
  times a second while the line is on screen, so counters and states stay
  current. Keep `get_text` quick (no file access, no locks held long).
- `CMFM_FLAG_HIDE_LOCKED` (since 1.2, any type): while your `locked`
  callback says the row is locked, the row is **hidden** instead of greyed.
  CMFM asks again after every change, so rows come and go as other settings
  change. To hide a whole section, give its heading and its rows the flag
  and lock them together.

### Limits (a page beyond them is refused, with the reason)

| What | Limit |
|---|---|
| settings per page | 1 to 1024 (headings count; 24 before 1.2) |
| plugin name / title / version / mod name | 64 / 24 / 24 / 24 bytes |
| label / help | 64 / 200 bytes |
| CHOICE names | 1 to 32, each 32 bytes at most |
| INT/FLOAT values | (max - min) / step + 1, at most 101; INT needs whole min, max, step |
| unit | 8 bytes |
| BUTTON text / question | 32 / 200 bytes |
| pages shown | 1024 (more are listed on the Overview as not shown; 24 before 1.2) |
| all pages together | no shared limit since 1.2 (one mod's page never pushes out another's) |

A page needs at least one setting besides its headings; ids must be unique
on the page (ids from `CMFM_RESERVED_IDS`, 0xFFFFFF00, are CMFM's); a
default must lie in range. Strings are UTF-8. The game's own
text styles work in labels, help and TEXT lines, for example
`<STYL MOJI_YELLOW_DEFAULT>warning</STYL>` (also RED, LIGHTBLUE, ORANGE,
LIGHTGREEN, GREEN, BLUE, PURPLE, WHITE_DEFAULT2 a light grey) and
`<SIZE 16>smaller</SIZE>` (20 is the usual size).

## 3. The callbacks

All run on the **game's menu thread** while a Mods page is opening, open or
closing. They must return quickly, must not block or throw, and must not
call `add_page`. `get` runs every time a row is drawn.

| Callback | Required | When | You do |
|---|---|---|---|
| `get(user, id, value)` | unless every row is TEXT | every draw of the row | fill `value` from your current state |
| `set(user, id, value)` | unless every row is TEXT | the player picked a value (in range, validated) | apply it (now, or after a restart for RESTART rows) and keep it for your config file; `CMFM_ERROR` keeps the old value |
| `validate(user, id, value, why, size)` | no | before `set` | refuse a combination: return `CMFM_ERROR` with a short reason, shown in the help line |
| `get_text(user, id, text, size)` | if a row is TEXT | right after `CMFM_EVENT_OPENED`; for LIVE lines also four times a second while shown | write the line (cut at 128 bytes) |
| `on_event(user, event)` | no | `CMFM_EVENT_OPENED`, `CMFM_EVENT_CLOSED`; since 1.2 once per distinct `on_event` and `user` (pages of one mod sharing them hear it once) | read fresh status at OPENED; save your config file at CLOSED |
| `press(user, id)` | if a row is BUTTON | Confirm (after Yes) | run the action; CMFM reads every value and text of the page again afterwards |
| `locked(user, id, why, size)` | no | when a page opens and after every change CMFM makes | return nonzero to grey the row and refuse changes; write the reason (help line) |

**Patterns that work well:**

- **Apply on your own thread.** If your settings must change on a worker
  or render thread, let `set` store the new value (an atomic or a small
  queue) and apply it from there; answer `get` from the stored value so the
  row shows the choice at once.
- **Save at CLOSED.** Write your ini once when the page closes, keeping
  comments and keys you don't know about; players edit it by hand. Saving
  in `set` works too.
- **Read at OPENED.** If your ini may have been edited while the game runs,
  read it again when the page opens.
- **Locks for dependencies.** "Multiplier" locked while "Frame generation"
  is Off, with the reason "Turn frame generation on first." CMFM asks again
  after every change, so the lock follows at once.
- **Hide what does not apply.** An "Upscaler" choice (DLSS, FSR, XeSS) and
  each upscaler's settings with `CMFM_FLAG_HIDE_LOCKED`, locked unless their
  upscaler is chosen: the page shows only the chosen one's settings. An
  "Advanced settings" toggle can do the same for a section. Grey (no flag)
  when the player should see that the option exists but cannot use it now;
  hide when it does not apply at all.
- **Validate for refusals that depend on the machine.** "3x needs an RTX
  50" refuses the value with the reason; the row keeps its old value.
- **Hotkeys with a KEY setting.** Its value is what Astera Loader's
  `al.input` hotkeys take: `key = CMFM_KEY_VK(value)`, `modifiers =
  CMFM_KEY_MODS(value)` in an `al_hotkey_t`. Ask for `al.input` in
  `al_plugin_init`, add your hotkey from the value in your ini, and in `set`
  remove the old hotkey and add the new one: the new key works at once (the
  demo's "Demo hotkey" does exactly this). Use `validate` to refuse keys you
  reserve ("Enter is the game's Confirm"). While CMFM waits for a key it holds
  the game's input, so no hotkey fires meanwhile.
- **Overlaps.** Astera Loader fires a hotkey while its key and at least its
  modifiers are held, so F5 also fires with Ctrl+F5. CMFM marks two KEY
  settings that overlap that way (any mods, also two of yours) in red and
  names the other one; it does not refuse them, since the player may want
  both. Keys of mods that do not use CMFM are not known to it.

## 4. What the player sees

- **The mod list** on the left: "Overview" first (CMFM's version and every
  mod's state: Active, Restart needed, Refused with the reason, or Not
  shown), then one entry per mod, in the order their first pages were
  added. It scrolls with a scrollbar when it is long. A mod with several
  pages is marked with a triangle and opens a dropdown of all its pages
  under its entry when selected (one dropdown at a time). Hovering and the
  arrow keys only highlight; a click or Confirm selects (a mod always opens
  on its default page; Confirm on what already shows enters the panel).
  Q / E (pad LB / RB) switch between the selected mod's pages, also from
  inside the panel. What shows is marked in light green.
- **The page's header** above the panel: your title (the mod's name and
  the page's title for a mod with several pages), your `version` and
  "Restart needed" while one of your changes waits for a restart.
- **The rows**, one per setting, at most 14 at a time; a longer page
  scrolls (keys, mouse wheel, its scrollbar). Keep the important settings
  near the top and group the rest with headings. (The game's own Options
  page, CMFM's fallback when the Mods page cannot open, shows only a page's
  first 24 rows.)
- **The help line** at the bottom: the row's help, the reason a value was
  refused or a row is locked, the restart note.
- **Reset to defaults** (since 1.2): CMFM ends your page with a "Reset to
  defaults" row of its own, drawn like the game's "Advanced Graphics
  Settings" row (the label alone, centred, red; no value box). After
  the game's Yes/No question it sets every
  BOOL, CHOICE, INT and FLOAT setting to its `default_value` through
  `validate` and `set`, locked and hidden ones too, so fill your defaults
  carefully. Opt out with `page.flags = CMFM_PAGE_NO_RESET` (for example if
  you offer your own reset).

## 5. Testing

- Install CMFM (`AsteraLoader\plugins\cmfm\cmfm.dll` plus its
  `nativePC\ui\cmfm\mods.gui` layout, one MO2 mod) next to your plugin and
  start the game through Astera Loader.
- `AsteraLoader\logs\plugins\cmfm.log` says when your page was added
  (`cmfm.menu: page "My mod" from my_mod added (N settings, ...)`) or why it
  was refused, and logs every value the player sets.
- The demo plugin (`cmfm_demo`) shows every type, a validate refusal, a
  lock, a button with a question, a restart setting, sections, rows that
  hide, live lines, a real hotkey bound through a KEY setting (and, with
  `extra_pages`, a long page and a second mod's key that overlaps it).

## 6. Checklist

- [ ] `cmfm.h` copied unchanged; CMFM commit recorded.
- [ ] CMFM optional: a NULL interface means no menu, nothing else.
- [ ] `size` set on the page and on every setting; unused fields zero.
- [ ] Ids unique; defaults in range; texts within the limits.
- [ ] Callbacks fast, on the menu thread; heavy work moved to your thread.
- [ ] Your ini written by you (at `set` or CLOSED), comments kept.
- [ ] RESTART on every setting that needs a restart.
- [ ] `version` set (one constant, also used for Astera Loader's info).
- [ ] The page tried in game: values, refusals, locks, buttons.
