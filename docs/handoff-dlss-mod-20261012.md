# Handoff to the DLSS mod: its settings in CMFM (2026-10-12, updated for 1.2)

For the session working on the DLSS / Frame Generation mod
(`D:\mh world modding\dlssupdate`, plugin `mhw_dlss` 0.15.0). CMFM is ready
for its first real client and is heading for its first release (0.1.0).
This note says what CMFM offers, how it fits the mod as it is today, a
suggested page, and what CMFM needs back before release. The DLSS session
decides; CMFM's repository is read-only from there, as `dlssupdate` is
from here.

First written for `cmfm.menu` 1.1 (same day); the mod had not started on
it (`dlssupdate` still at de71426), so this version replaces it and uses
1.2 throughout.

## What CMFM is now

- An Astera Loader plugin (`cmfm`, order -100) that adds **Mods** below
  Options in the title menu and in the Start menu's System tab. The page is
  the game's own settings page (CMFM's edited copy of the title Options
  layout): the mod list on the left, the selected mod's settings on the
  right, a header above (mod, page, version, "Restart needed"), the help
  line below.
- **The mod list:** one entry per mod. A mod with several pages opens a
  dropdown of them under its entry; the first page is the default. Hover
  and the arrow keys only highlight; a click or Confirm selects. Q / E step
  through the selected mod's pages.
- **The interface:** `cmfm.menu` **1.2**,
  `D:\mh world modding\CMFM\include\cmfm\cmfm.h`. 1.1 is frozen (CMFM
  decisions item 11); 1.2 only adds. Copy the header unchanged into the mod
  (e.g. `third_party/cmfm/cmfm.h`) and record the CMFM commit. 1.2 freezes
  when CMFM 0.1.0 is released, so this is the moment to say what gets in
  the way.
- **The author guide:** `D:\mh world modding\CMFM\docs\author-guide.md`.
  The example: `examples/cmfm_demo/cmfm_demo.c` (every feature).
- Tested in game: every feature below (tags up to `mod-dropdowns-1` for the
  interface, `toml-page-1` for config pages).

## What the native menu means for the mod's requirements

AL's `docs/requirements-dlss-mod.md` (sections 3 and 4) was written for an
overlay menu. With the native menu:

- **Presentation (section 3): nothing to do.** CMFM draws nothing itself;
  the game draws its own menu, so there is no back-buffer drawing, no
  resource barriers and no proxy-back-buffer question
  (`docs/research/astera-loader-migration.md:208-213` no longer applies to
  CMFM). Frame generation sees the Mods page as it sees the game's Options.
- **Open/close events** exist (`CMFM_EVENT_OPENED`, `CMFM_EVENT_CLOSED` on
  the page's `on_event`; since 1.2 once per distinct `on_event` and `user`,
  so several pages of one mod sharing them hear each event once). Since the
  page is an ordinary game menu, pausing frame generation while it is open
  is probably unnecessary; the events are the right moments to read fresh
  status and to save the ini.
- **Typed settings, live or restart, the ini as source of truth
  (section 4): yes.** BOOL, CHOICE (arrows or the game's pick list), INT and
  FLOAT (the game's slider, up to 101 values), KEY (key bindings, since
  1.2), read-only TEXT lines, headings, buttons with the game's Yes/No
  question, locks with a reason, `validate` refusals with a reason, and
  `CMFM_FLAG_RESTART` (CMFM marks a change that waits for a restart on the
  row, the page's header and the Overview).
- **Warnings: in place, no popups** (CMFM decisions item 12): a row greyed
  with its reason (`locked`: "Frame generation needs an RTX 40 or 50
  graphics card."), a value refused with its reason (`validate`: "3x and up
  need an RTX 50."), status lines ("Frame generation: off, <reason>").
  `locked` is asked whenever the page opens and after every change, so a
  page added at `al_plugin_init` still greys rows once the worker has
  checked the GPU. If the mod finds a case these do not cover, say so.
- **New in 1.2, useful here:**
  - `CMFM_FLAG_LIVE` on TEXT rows: read about four times a second while
    shown (generated frames, applied multiplier, render size);
  - `CMFM_FLAG_HIDE_LOCKED`: a locked row hides instead of greying (for
    rows that only make sense in one mode; greying with a reason is better
    when the player should learn why);
  - CMFM's own "Reset to defaults" row at the end of each page, from each
    setting's `default_value` (opt out: `CMFM_PAGE_NO_RESET`);
  - `page.mod`: several pages under one mod entry ("DLSS": "Settings",
    "Status").
- **AA/DLSS option lockout in the game's own Options:** on CMFM's roadmap,
  after the release.

## The mod today (read-only survey, `dlssupdate` at de71426)

- ini `mhw_dlss.ini` next to the DLL (`AsteraLoader\plugins\mhw_dlss\`),
  read once by the worker (`src\reflex\presentation_probe.cpp:613`, parser
  `src\alpha\config.hpp:14-27`, `:57-77`); the mod never writes it.
- `frame_generation`: `off`, `2x`, `3x`, `4x`, `6x` (3x and up need an RTX
  50), default `2x`. Off <-> on needs a restart (DLSS-G is not loaded when
  off, `pp.cpp:275`); the multiplier could change live:
  `mhw::frame_generation::game::multiplier(unsigned)` is an atomic store,
  callable from any thread, clamped to the GPU's limit
  (`src\frame_generation\game_session.hpp:38-40`); the research build did
  it through `.mfg-N` markers (`pp.cpp:961-975`, compiled out in release).
- `reflex`: only `on` (low latency, fixed); `runtime_dir`, `log_dir`:
  paths, restart only (not menu material).
- Upscaling is fixed at Performance (`src\continuous_dlaa\runtime.cpp:946`,
  `:972`); AL's requirements table (upscaling modes, Reflex Boost, HUD-less,
  status overlay) lists settings the mod does not have yet.
- Status it already knows: frame generation armed/on, requested and
  applied multiplier, generated frames (`game::write()`), DLSS active, render
  size and swap-chain size (`continuous_dlaa::write_summary`), Reflex mode,
  failures (`failure`, `startup_failure`, AL status ACTIVE/DEGRADED/DISABLED).
- Version `"0.15.0"` appears three times (`pp.cpp:714`, `:1152`, `:1214`).

## Suggested pages

One mod, `page.mod = "DLSS"`, two pages sharing one `on_event` and `user`.

**Settings** (the default page):

| Row | Type | Values | Notes |
|---|---|---|---|
| Frame generation | SECTION | | |
| Frame generation | BOOL, RESTART | Off / On, default On | ini `off` vs a multiplier; DLSS-G loads only at start |
| Multiplier | CHOICE | 2x, 3x, 4x, 6x, default 2x | live through `game::multiplier`; `locked` while Frame generation is Off ("Turn frame generation on first."); `validate` refuses 3x and up without an RTX 50 ("3x and up need an RTX 50.") |
| DLSS | SECTION | | |
| Upscaling | TEXT | "Performance" | fixed today; becomes a CHOICE when the mod offers modes |
| Reflex | TEXT | "Low latency" | the setting has one value today |

**Status:**

| Row | Type | Notes |
|---|---|---|
| Mod | TEXT | "Active" / "Degraded: <reason>" / "Disabled: <reason>" |
| DLSS | TEXT, LIVE | e.g. "Performance, 1280x720 to 2560x1440" |
| Frame generation | TEXT, LIVE | e.g. "2x, active" / "off" / "disabled: <reason>" |
| Generated frames | TEXT, LIVE | the counter |

- One ini key stays one ini key: Off writes `frame_generation = off`; On
  writes the multiplier. Remember the multiplier in memory while Off (2x if
  none) so turning it back on restores it.
- Splitting the old single value into Off/On (restart) and a live
  multiplier says honestly which part needs the restart. With one CHOICE
  marked RESTART, a 2x -> 3x change would claim a restart it does not need.
- If DLSS-G is unsupported, the worker fails closed today; the menu lets
  the player turn frame generation Off (restart) to get DLSS back. The
  status rows should say why.
- `page.version`: one constant for AL's info and the pages.
- Reset to defaults on the Settings page: Frame generation On, 2x. On the
  Status page (TEXT only) CMFM adds no reset row.

## Implementation notes

- **Threads:** CMFM calls back on the game's menu thread; callbacks must be
  quick, `get_text` of LIVE rows especially (four times a second: no file
  access, no long locks). `set` for the multiplier may call
  `game::multiplier` directly (atomic); anything worker-only (`game::arm`)
  goes through a flag the 100 ms worker loop picks up (`pp.cpp:926-1005`).
- **Writing the ini:** new for the mod. Write at `set` or at
  `CMFM_EVENT_CLOSED`, keep comments and unknown keys. Check under MO2 that
  the write lands in the mod's own file (MO2 redirects new files to
  Overwrite); AL's per-plugin data folder (`init->data_folder`) is the
  alternative if it does not. (CMFM's config pages write other mods' files
  in place through the file's real path and swap them in atomically:
  `src/native/config_loader.cpp`, `WinFiles::write`, if a model helps.)
- **Optional dependency:** ask `api->get_interface(self, "cmfm.menu",
  CMFM_MAKE_VERSION(1, 2))` in `al_plugin_init` (CMFM initialises earlier)
  or at PRE_MAIN; NULL means no menu (CMFM missing or older). Do not list
  CMFM in `dependencies`.
- **Refusals:** `add_page` returns 0 with the reason (log it); CMFM also
  logs it in `AsteraLoader\logs\plugins\cmfm.log` and lists it on its
  Overview.
- **Without code** (not recommended here, for completeness): since 1.3 a
  description file in `AsteraLoader\plugins\cmfm\pages\` can put an ini's
  plain values in the menu with no code (`docs/design/config-pages.md`).
  It cannot show status or change the multiplier live, so the code page
  above is the right fit for this mod.

## Testing

- MO2 mods: `CMFM (dev)` (CMFM's current test build and layout) and the
  mod's own (`MHW DLSS (CMFM plugin)`), profile `al-stracker`, started
  through MO2's "Astera Loader" entry.
- `cmfm.log` shows `cmfm.menu: page "Settings" from mhw_dlss added (...)`
  and every value set; the demo plugin can stay installed alongside.
- CMFM's release checklist includes a run with this mod's page and frame
  generation on (menu open and closed, alt-tab, resize, loading screens).

## What CMFM would like back (before 1.2 freezes)

1. The pages as built (or why they differ), and anything in 1.2 that got in
   the way or is missing.
2. Any warning the greyed rows, refusals and status lines do not cover.
3. Which status values the mod can offer and how often they change.
4. A build of the mod with the pages, for CMFM's release runs.

## Answers to the DLSS session's reply (2026-10-12)

The reply (`dlssupdate/docs/handoff-cmfm-20261012.md`, pages in v0.16.0,
run game-56) was read; the differences from the suggestion are fine
(no headings for two settings, Upscaling and Reflex on Status, the card's
real limit in the refusal, Frame generation never locked).

- **The restart baseline:** taken once, at CMFM's first read of the
  setting in the session (the page's first showing, or just before the
  first change), and kept until the game exits. Later reads, such as the
  mod's reload at OPENED, do not move it. So "Restart needed" compares
  against the start value, as the mod expects. One edge: a hand edit made
  before the page first shows becomes the baseline (CMFM never saw the
  older value). Now in the author guide (CMFM_FLAG_RESTART).
- **`locked` and `validate` at open and after changes only:** agreed,
  nothing needed now. If the late card limit ever matters, CMFM could
  re-ask `locked` while a page with LIVE rows is shown; say so then.
- **The boot crash at the text lookup:** noted for CMFM's player guide
  (troubleshooting: a crash at start in `rGUIMessage::getMessage` with a
  null message file means Steam was started badly; restart Steam), and as
  a candidate for Astera Loader's crash report (`docs/al-requests.md`).
  CMFM does not guard the null: the game crashes the same way without
  CMFM, and hiding it would change the game's behaviour.
- **Open items from game-56** (Frame generation Off with its restart mark
  and the Multiplier lock, Reset to defaults, alt-tab with the page open,
  the Status lines on screen) are in CMFM's run `check-1`, together with
  CMFM's own checklist and frame generation on in a quest.

## Answer to the DLSS session's handoff-cmfm-20261014 (the mod list window)

- **Confirmed and fixed** (commit 45c5ad1, CMFM 0.1.0-beta.2, commit
  52ffd36). The diagnosis was exact: `rebuildRows` stores the row count
  into `DuplicateNum` (`mov [r14+0x298], ebx`, RVA 0xB7FEF0 + 0x1F8, also
  checked on CMFM's clean image). CMFM now reads the mod list's window once
  per list, before its first resize, and uses that size everywhere (the
  five places named in the handoff); the "no mod list" check stays on the
  live field. In-game check: CMFM's run `20261014-listwindow-1`.
- **For the next bundle, take the release build:**
  `D:\mh world modding\CMFM\local\release\cmfm-0.1.0-beta.2\` (the
  `AsteraLoader` and `nativePC` folders as they go into the bundle;
  SHA-256 in `cmfm-0.1.0-beta.2.sha256` next to it: cmfm.dll
  `8fb5d064...f34e`, mods.gui `5792b008...857c`, unchanged). The 0.28.4
  bundle carried `edce63cc...dcd0`, CMFM's measurement build from MO2's
  "CMFM (dev)": it works, but it times its hooks and logs "measurement
  build ... (not for release)".
- **FextyMod's exit crash** (`FextyMod.dll+0x179b`, its own thread while
  the game exits): known since Astera Loader 0.1.0 (its release notes);
  not CMFM.

## Answer to the DLSS session's handoff-cmfm-20261014-dpad (D-pad Down moved up)

- **Confirmed and fixed** (commit f7bb928, CMFM 0.1.0-beta.3). The cause
  was as described: the hidden category list holds min(pages, 5) rows with
  its cursor parked on row 2, the last row with three pages, so Down
  wrapped to row 0 and the row comparison read it as Up. The fix is the
  first suggestion: the game's step functions (next, previous), which keys,
  D-pad and stick go through, now move the mod list's highlight for that
  list in the pressed direction and keep the parked cursor (moveCursor
  ignores an unchanged row), so short lists and the missing row 2 (two
  pages or fewer) no longer matter. The moveCursor hook still catches any
  other move, a wrap counted the right way. In-game check: CMFM's run
  `20261014-beta3-1` (three entries, then two).
- **For the next bundle:** `D:\mh world modding\CMFM\local\release\cmfm-0.1.0-beta.3\`
  (supersedes beta.2; both fixes), SHA-256 in `cmfm-0.1.0-beta.3.sha256`
  next to it: cmfm.dll `f50d26ad...9a8e`, mods.gui unchanged
  (`5792b008...857c`).

## CMFM 0.1.0 for the release bundle (2026-10-14)

CMFM 0.1.0 is released (tag `v0.1.0`; the code of beta.3 plus the version
string). For the DLSS mod's release bundle take
`D:\mh world modding\CMFM\local\release\cmfm-0.1.0\` (the `AsteraLoader`
and `nativePC` folders; the plugin folder also holds CMFM's player guide,
its description-file guide, release notes and licence), or the archive
`CMFM-0.1.0.zip` beside it. SHA-256 in `CMFM-0.1.0.sha256`: cmfm.dll
`82f679e5...32c2c`, mods.gui unchanged (`5792b008...857c`). The repository
is public from this release on: https://github.com/wowo-dot/CMFM

