# CMFM: Configuration Menu For Mods (Monster Hunter: World)

## Objective

CMFM ("Configuration Menu For Mods") is the in-game settings menu for
Monster Hunter: World mods on PC (DirectX 12). It is a plugin of Astera
Loader (AL), the user's open-source loader, and builds on AL's services
only.

- **For players:** one menu, in the game's own menus, for every mod that
  registers with it: settings, status lines, and warnings where they apply
  (a greyed row with its reason, a refused value, a status line).
- **For plugin authors:** a versioned C interface that CMFM publishes
  through AL (`api->publish_interface`, name `cmfm.<...>`):
  - typed settings (bool, enum, int or float range, read-only lines) with
    get, set and validate callbacks; each setting is `live` or
    `restart required`;
  - a status page per plugin, and optional debug pages;
  - locks with a reason and refusals with a reason; no popups unless a real
    need appears (decisions.md item 12);
  - menu open and close events.
- **Each plugin's own config file stays the source of truth.** CMFM reads
  and writes through the plugin's callbacks and never owns another
  plugin's settings, so a broken CMFM store can never wipe them.

Until 2026-10-09 the loader itself was called CMFM; older AL docs,
handoffs and runs use that name for the loader (AL `docs/decisions.md`
item 11). Since 2026-10-11 CMFM has its own repository (this one). The
first client is the sibling DLSS / Frame Generation mod.

Start with `docs/handoff-20261011.md`, then `docs/decisions.md` and
`docs/roadmap.md` (the agreed plan, kept current), then the AL header `third_party/al/al_plugin.h` and AL's plugin guide
(`D:\mh world modding\AsteraLoader\docs\plugin-guide.md`).

## Canonical Local Paths

- Repository: `D:\mh world modding\CMFM`
- Astera Loader (read-only reference from here; its own agent owns it):
  `D:\mh world modding\AsteraLoader`. Released 0.1.0 (tag `v0.1.0`,
  commit ba04bd1), API 1.7; 0.2.0 in progress (API 1.8, `al.patch`), which
  CMFM requires since 2026-10-12.
- Sibling DLSS mod (read-only reference): `D:\mh world modding\dlssupdate`
- Game directory: `D:\SteamLibrary\steamapps\common\Monster Hunter World`
- Game executable: `D:\SteamLibrary\steamapps\common\Monster Hunter World\MonsterHunterWorld.exe`
  (tested exe SHA-256 `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`, version 421810)
- Mod Organizer 2 (portable): `C:\Modding\MO2\ModOrganizer.exe`, data
  directory `D:\MO2\MHW`. The user's AL install is the MO2 mod
  `AsteraLoader`, started with MO2's "Astera Loader" entry. Test profile
  `al-stracker` (19 Stracker plugins, MHW DLSS, SPL present but not
  force-loaded).
- IDA 9.4: `C:\Program Files\IDA Professional 9.4\ida.exe` (the Hex-Rays
  IDA MCP needs 9.4). Make a private database under this repository's
  `local\ida\` when the game's own menu is reversed.
- Clean game image: `local\ida\mhw421810\mhw.exe`, SHA-256
  `a78ea69b0566414a42f16d0f8ff7d729e095041e2ae76a34cc02dbe0939f1d80`, a
  read-only memory copy of the tested exe taken at the title screen with no
  mods loaded (run `local/runs/20261011-clean-dump`); its code section is
  identical to Fexty's 15.23 dump. The exe on disk has SteamStub-encrypted
  code, so offline signature checks run on this image. Other projects read
  the same bytes from `local\runs\20261011-clean-dump\dump\image.exe`
  (`docs/handoff-clean-image-20261011.md`, the reversing skill): never move
  or delete that copy. Do not use the old
  `ai\glavenus\MonsterHunterWorld_decrypted.exe` (d786…845c): it was dumped
  with mods loaded and has hooks or patches in 29 places.
- Fexty's annotated Ghidra database (MHW 15.23, same build): the user's
  `D:\reverse engine jank\fexty ghidra\*.gzf`, read only. Our headless
  project is `local\ghidra\project`, its export `local\ghidra\export`
  (Ghidra 12.0.2: `D:\reverse engine jank\ghidra_12.0.2_PUBLIC`).
- Claude Code reversing tools: `C:\Users\akifs\.claude\re-tools\` (Python
  3.12: `py -3.12 -B`); x64dbg: `D:\reverse engine jank\x64dbg\release\x96dbg.exe`.
- Python 3.10: `C:\Users\akifs\AppData\Local\Programs\Python\Python310\python.exe` (run with `-B`)
- CMake/CTest (Visual Studio 18 2026, x64):
  `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`

These paths are machine-local. Do not encode them into distributable
binaries or assume other contributors use them.

## Working With Astera Loader

- CMFM is an AL plugin: `<game>\AsteraLoader\plugins\cmfm\cmfm.dll`
  (under MO2: its own mod with that layout). It exports `al_plugin_info`
  and `al_plugin_init` and talks to the game only through AL:
  - drawing: `al.d3d12` events (PRE_PRESENT, PRE_RESIZE, POST_RESIZE).
    Never hook D3D12 or DXGI itself;
  - input: `al.input` hotkeys and capture. Never hook input itself;
  - game-thread work: `al.frame`;
  - any game-function hook (the game's own menus): `al.hooks`, at
    signature-verified sites only;
  - writes to the game's read-only data: `al.patch` (API 1.8) only,
    checked by the expected old bytes and put back at shutdown. CMFM's own
    prototype helper for this is gone (decisions.md item 9,
    `docs/al-requests.md` item 1). Swaps in the game's writable data
    (`src/native/data_patch.cpp`) check the old value and change no
    protection.
- `third_party/al/al_plugin.h` is AL's header, copied unchanged; its
  README records the AL commit. Update by copying a newer header and
  recording the commit.
- AL is read-only from here. When CMFM needs something AL does not offer,
  write it into `docs/al-requests.md` (what, why, the smallest API that
  would do); the user takes it to the AL session.

## Safety and Scope

- Treat the game directory as read-only. Install test builds as MO2 mods,
  never into the game directory.
- Never change the installed game on disk: not `MonsterHunterWorld.exe`,
  not its chunk archives, nothing in the game directory. In-memory hooks
  only through AL, at signature-verified sites, with clean removal.
- CMFM may ship game files it edited, such as a GUI layout (decisions.md
  item 10). They live in `assets/`, ship inside CMFM's own mod folder and
  are loaded from there. Ship only files we actually changed, never
  unmodified Capcom files or bulk extracts.
- Do not disable, bypass or weaken integrity checks, CRC/anti-tamper code,
  DRM, anti-cheat, licensing or platform protections. CMFM itself must not
  change gameplay or network behaviour.
- Never touch `D:\mh world modding\ai\glavenus` IDA databases (another
  project). Work on private copies under `local/` only.
- Do not commit or redistribute Capcom binaries, decoded images,
  unmodified game assets, NVIDIA binaries, other mods' binaries, crash
  dumps or IDA databases. Edited game files that ship (`assets/`) are the
  exception; ask the user before pushing them to a public remote.
- Never send project data, logs or binaries to external services. The one
  exception: CMFM's own release binaries, built from this repository, go to
  VirusTotal (as AL's do, AL `docs/decisions.md` item 15). VirusTotal's
  upload asks for a reCAPTCHA, which Claude may not answer: Claude attaches
  the files, the user ticks the box, Claude reads the results by SHA-256.
- Do not reverse engineer NVIDIA binaries. Third-party mod DLLs are studied
  from their published source where it exists.

## Runtime Testing Protocol

- The user launches the game, always through MO2's "Astera Loader" entry.
  Never launch MHW or the launcher through Steam, a script or computer-use.
  One exception, the user's call each time: a plain start with no AL and
  no mods (for example from Steam) when a reference copy of the game's
  memory is needed, as for `local/runs/20261011-clean-dump`. Nothing of
  ours runs in such a start; only read-only tools look at it.
- Before every launch, prepare a run folder (`local/runs/<name>`) and the
  installed build, and say what the launch should answer. After the game
  closes, seal the folder with a short result file.
- Keep the installed test build in place between runs; announce any
  intentional stop or removal.
- Run logs identify: the exe SHA-256, AL's version and commit, CMFM's
  version and commit and DLL SHA-256, the MO2 profile, and every loaded
  plugin with its version. AL's logs are in the AL mod's
  `AsteraLoader\logs` (core log, `plugins\cmfm.log`, crash reports).
- Measurement tools reading the game process are strictly read-only
  (`PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ`; no writes, no
  thread suspension, no remote threads or allocations). AL's
  `tools/modwatch.py` is one.
- When screenshotting the desktop, crop to the game window.

## Reverse-Engineering Tools

Claude Code has an IDA MCP (Hex-Rays), an x64dbg MCP, a Frida MCP and
`sigscan.py`; the user-level `reversing` skill describes them. This file's
rules come first. Reversing comes early: the native menu depends on it
(decisions.md item 7), and it also serves the AA/DLSS option lockout. The
private database is `local\ida\mhw421810\mhw.i64`, built from the clean
image with Fexty's names applied (`tools/re/`). Open fresh binaries in IDA
with `-Opdb:off`, so it does not ask Microsoft's symbol server.

- **IDA:** open only a private database under this repository's `local\ida\`,
  never anything in `ai\glavenus`. Ask through the MCP's `execute_python`.
  Exports go to `local\`.
- **Signatures:** `sigscan.py audit|make|scan` on the clean image. A
  pattern the code searches for must match exactly once. Record the
  pattern, RVA and exe hash in the design doc.
- **x64dbg and Frida on the running game:** the user says OK in the session
  before each attach; never during startup; prefer hardware and
  non-stopping logging breakpoints (the game re-checks its code with CRC);
  Frida hooks only functions AL or a plugin already hooks; offline
  preferred; detach when done and note it in the run folder.
- Point the tools at the game's code only, never at NVIDIA DLLs or other
  mods' binaries.

## Engineering Rules

- C++20 and CMake; 64-bit Windows only; MSVC, static runtime (no Visual
  C++ runtime needed on the player's machine).
- The plugin boundary is a C ABI: plain structs, function pointers,
  explicit sizes and versions. No STL types, C++ classes or exceptions
  cross it. That holds for CMFM's own published interface too: every table
  starts with `uint32_t size` and `version`, and minor versions only add.
- Menu technology: native, inside the game's own menu, if the reversing
  study shows it can be done; otherwise Dear ImGui (MIT, pinned and
  vendored with its licence, themed close to MHW's look). decisions.md
  item 7. The plugin interface stays independent of the front end.
- Fail closed and visibly:
  - a plugin's bad schema, a failed callback or a missing AL service is
    logged and skipped, and the user can see why (the Overview or a status
    line);
  - nothing CMFM does is fatal to the game.
- Near-zero per-frame cost with the menu closed: no per-frame allocation,
  no draw work, no per-draw hooks. Measure the cost on the present thread.
- Graphics with a presentation owner (the DLSS mod with frame generation):
  draw into the game-facing back buffer before the owner's Present, on the
  game's queue, with correct resource barriers. Release every back-buffer
  reference at PRE_RESIZE and take new ones at POST_RESIZE.
- Paths under MO2: GetModuleFileNameW returns the virtual game-folder path,
  while canonical() and GetFinalPathNameByHandle resolve to the real mod
  folder. Never compare paths from the two sources; AL's `al_init_t` gives
  both (`folder`, `folder_real`).
- Keep third-party dependencies explicit and pinned, and record their
  licences.

## Verification

- Build with warnings as errors in project code.
- Unit-test pure logic (schema validation, settings values, layout, the
  published interface's tables) outside the game.
- In game: open and close the menu, keyboard, mouse and pad, capture and
  release, alt-tab, resize and full-screen toggles, loading screens, with
  and without the DLSS mod (frame generation on), and with the plugins that
  register settings.

## Git Hygiene

- Git is the source of truth: focused commits, and an annotated tag for
  each version the user has tested in game.
- Commit locally; do not push without the user saying so. No remote yet.
- Keep build trees, logs, runs, dumps, game files and third-party binaries
  untracked (`local/`).
- Do not rewrite or discard user changes without explicit approval.

## Working With the User

- The user grants broad autonomy: decide, act and report; they object when
  they disagree. Present plans briefly and proceed on "go ahead". The user
  calls this "our project".
- Report measured facts and say plainly what is unverified.
- Use plain language in user-facing summaries.
- The user plays online; that is fine for testing.
