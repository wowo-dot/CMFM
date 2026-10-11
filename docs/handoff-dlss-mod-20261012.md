# Handoff to the DLSS mod: its settings in CMFM (2026-10-12)

For the session working on the DLSS / Frame Generation mod
(`D:\mh world modding\dlssupdate`, plugin `mhw_dlss` 0.15.0). CMFM is ready
for its first real client. This note says what CMFM offers, how it fits the
mod as it is today, and a suggested page. The DLSS session decides; CMFM's
repository is read-only from there, as `dlssupdate` is from here.

## What CMFM is now

- An Astera Loader plugin (`cmfm`, order -100) that adds **Mods** below
  Options in the title menu and in the Start menu's System tab. The page is
  the game's own settings page (CMFM's edited copy of the title Options
  layout): mod list on the left, the highlighted mod's settings on the
  right, header above, help line below. Tested in game through design step
  6 and fine sliders (tags `design-step-1` to `-6`, `fine-sliders-1`).
- The interface: `cmfm.menu` **1.1**, `D:\mh world modding\CMFM\include\cmfm\cmfm.h`,
  frozen from this handoff on (CMFM decisions item 11). Copy it unchanged
  into the mod (e.g. `third_party/cmfm/cmfm.h`) and record the CMFM commit.
- The author guide: `D:\mh world modding\CMFM\docs\author-guide.md`. The
  example: `examples/cmfm_demo/cmfm_demo.c` (every feature).

## What the native menu means for the mod's requirements

AL's `docs/requirements-dlss-mod.md` (sections 3 and 4) was written for an
overlay menu. With the native menu:

- **Presentation (section 3): nothing to do.** CMFM draws nothing itself;
  the game draws its own menu, so there is no back-buffer drawing, no
  resource barriers and no proxy-back-buffer question
  (`docs/research/astera-loader-migration.md:208-213` no longer applies to
  CMFM). Frame generation sees the Mods page as it sees the game's Options.
- **Open/close events** exist (`CMFM_EVENT_OPENED`, `CMFM_EVENT_CLOSED` on
  the page's `on_event`). Since the page is an ordinary game menu, pausing
  frame generation while it is open is probably unnecessary; the events are
  the right moments to read fresh status and to save the ini.
- **Typed settings, live or restart, the ini as source of truth
  (section 4): yes.** BOOL, CHOICE (arrows or the game's pick list), INT and
  FLOAT (the game's slider, up to 101 values), read-only TEXT lines,
  headings, buttons with the game's Yes/No question, locks with a reason,
  `validate` refusals with a reason, and `CMFM_FLAG_RESTART` (CMFM marks a
  change that waits for a restart on the row, the page's header and the
  Overview).
- **Warnings: in place, no popups** (CMFM decisions item 12). AL's
  examples are covered by what 1.1 has: a row greyed with its reason
  (`locked`: "Frame generation needs an RTX 40 or 50 graphics card."), a
  value refused with its reason (`validate`: "3x and up need an RTX 50."),
  and status lines ("Frame generation: off, <reason>"). `locked` is asked
  whenever the page opens, so a page added at `al_plugin_init` still greys
  rows once the worker has checked the GPU. If the mod finds a case these
  do not cover, say so.
- **Status page:** TEXT rows are read when the page opens (live updates
  while it stays open are on CMFM's roadmap).
- **AA/DLSS option lockout in the game's own Options:** on CMFM's roadmap,
  later.

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

## Suggested page

| Row | Type | Values | Notes |
|---|---|---|---|
| Frame generation | SECTION | | |
| Frame generation | BOOL, RESTART | Off / On | ini `off` vs a multiplier; DLSS-G loads only at start |
| Multiplier | CHOICE | 2x, 3x, 4x, 6x | live through `game::multiplier`; `locked` while Frame generation is Off ("Turn frame generation on first."); `validate` refuses 3x and up without an RTX 50 ("3x and up need an RTX 50.") |
| Status | SECTION | | |
| DLSS | TEXT | e.g. "Performance, 1280x720 to 2560x1440" | read at OPENED |
| Frame generation | TEXT | e.g. "2x, active" / "off" / "disabled: <reason>" | |
| Reflex | TEXT | "Low latency" | the setting has one value today |
| Mod status | TEXT | "Active" / "Degraded: <reason>" | the reason the worker failed, if it did |

- One ini key stays one ini key: Off writes `frame_generation = off`; On
  writes the multiplier. Remember the multiplier in memory while Off (2x if
  none) so turning it back on restores it.
- Splitting the old single value into Off/On (restart) and a live
  multiplier says honestly which part needs the restart. With one CHOICE
  marked RESTART, a 2x -> 3x change would claim a restart it does not need.
- If DLSS-G is unsupported, the worker fails closed today; the menu lets
  the player turn frame generation Off (restart) to get DLSS back. The
  status rows should say why.
- `page.version`: one constant for AL's info and the page.

## Implementation notes

- **Threads:** CMFM calls back on the game's menu thread; callbacks must be
  quick. `set` for the multiplier may call `game::multiplier` directly
  (atomic); anything worker-only (`game::arm`) goes through a flag the
  100 ms worker loop picks up (`pp.cpp:926-1005`).
- **Writing the ini:** new for the mod. Write at `set` or at
  `CMFM_EVENT_CLOSED`, keep comments and unknown keys. Check under MO2 that
  the write lands in the mod's own file (MO2 redirects new files to
  Overwrite); AL's per-plugin data folder (`init->data_folder`) is the
  alternative if it does not.
- **Optional dependency:** ask `api->get_interface(self, "cmfm.menu",
  CMFM_MAKE_VERSION(1, 1))` in `al_plugin_init` (CMFM initialises earlier)
  or at PRE_MAIN; NULL means no menu. Do not list CMFM in `dependencies`.
- **Refusals:** `add_page` returns 0 with the reason (log it); CMFM also
  logs it in `AsteraLoader\logs\plugins\cmfm.log` and lists it on its
  Overview.

## Testing

- MO2 mods: `CMFM (dev)` (CMFM's current test build and layout) and the
  mod's own (`MHW DLSS (CMFM plugin)`), profile `al-stracker`, started
  through MO2's "Astera Loader" entry.
- `cmfm.log` shows `cmfm.menu: page "DLSS" from mhw_dlss added (...)` and
  every value set; the demo plugin can stay installed alongside.

## What CMFM would like back

1. The page as built (or why it differs), and anything in 1.1 that got in
   the way.
2. Any warning the greyed rows, refusals and status lines do not cover.
3. Which status values the mod can offer and how often they change (for
   live TEXT rows later).
