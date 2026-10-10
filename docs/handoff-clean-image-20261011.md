# Clean MHW game image: handoff to every project, 2026-10-11

For every project that reads a decoded image of `MonsterHunterWorld.exe`
(Astera Loader, dlssupdate, cpuopt, ai\glavenus) and for the user-level
`reversing` skill. Written by the CMFM session. Your own project rules come
first; this file says what changed and what to check.

## In short

- **The old decoded image is dirty.**
  `D:\mh world modding\ai\glavenus\MonsterHunterWorld_decrypted.exe`
  (SHA-256 `d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`)
  was dumped from a running game with mods loaded. Its code differs from
  the original in **29 places** (table below): 22 functions start with a
  jump out of the image, plus NOP patches, flipped jumps and redirected
  calls.
- **Use the clean image instead:**
  `D:\mh world modding\CMFM\local\runs\20261011-clean-dump\dump\image.exe`,
  SHA-256 `a78ea69b0566414a42f16d0f8ff7d729e095041e2ae76a34cc02dbe0939f1d80`.
- **Nothing is known to be broken.** Measured on both images (CMFM session,
  2026-10-11):

  | Project `src` | Result on the clean image | Same as on the old image? |
  |---|---|---|
  | Astera Loader | 6 of 7 patterns unique; `kExitPattern` 3 matches (used at a fixed, checked address) | yes |
  | dlssupdate | 182 of 182 code gates match; `Publish` 20+ matches (gated address) | yes |
  | cpuopt | 2 of 2 patterns unique | yes |
  | ai\glavenus | not checked | n/a |

  But anything made, read or checked on the old image at one of the 29
  places was based on modified bytes.

## The clean image

- **How it was made:**
  - A plain Steam start (no Astera Loader, no mods), at the title screen.
  - A read-only copy of the module's memory, opened with
    `PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ` only: no debugger,
    no writes, no suspension. Tool: CMFM `tools/re/dump_image.py`.
  - The exe on disk was the tested one, `c2ebbbd2…90ea` (version 421810).
  - The game had 123 modules loaded: Windows, Steam (overlay, client), the
    game's own `steam_api64` and `amd_ags_x64`, and NVIDIA NGX and telemetry.
    No mods.
- **The file:**
  - Section headers point at the virtual layout
    (`PointerToRawData = VirtualAddress`), so IDA, pefile and `sigscan.py`
    load it as a PE.
  - It's for analysis only; it doesn't run.
  - Next to it are `image.bin` (raw memory, SHA-256
    `668291926e42bf21c7fa32b4a74e85a666b34b42696ec8c87550811165f268b6`) and
    `dump.json` (module list, hashes).
- **Verified, three independent dumps agree:**
  - The code section (`.text`, 0x2e11200 bytes) is identical to Fexty's
    15.23 Ghidra dump, and identical to the user's older
    `D:\reverse engine jank\MonsterHunterWorld_dump.exe` (SHA-256 `f4ab2384…914a`).
    That older dump is clean too.
  - `.rdata` differs only in the import table (resolved DLL addresses).
  - `.data` is runtime state, as in any dump.
- **Same build as Fexty's annotated Ghidra database:**
  `D:\reverse engine jank\fexty ghidra\MonsterHunterWorld_15.23_Fexty_20241219.gzf`
  (PDB `World300.pdb`, GUID `7c554b32-2b94-41f7-8e81-dc3593003b74`, age 1).
  - CMFM's `tools/re/ghidra/ExportProgram.java` and
    `tools/re/ida/import_ghidra_export.py` move his names into an IDA
    database: 100,804 names and 2,430 comments.
  - CMFM's headless Ghidra project and export are in `CMFM\local\ghidra\`.

## How to use it

- **`sigscan.py` (scan, make, audit, gates):** reading the file in place is
  fine.
- **IDA:** first copy the file into your project's own untracked `local\`
  folder and check the SHA-256. IDA writes its database next to the input,
  and no project may create databases inside CMFM's folder.
- **IDA on a fresh binary:** open it with `-Opdb:off`. Otherwise IDA asks
  Microsoft's symbol server for `World300.pdb`, which sends a request outside
  the machine.
- **Never** commit or redistribute the image: it is Capcom's code.
- **Leave the old image** and everything in `ai\glavenus` in place unless the
  user says otherwise.

## Checklist for each project

1. **Replace every reference to the old image** (AGENTS.md or CLAUDE.md,
   docs, scripts, tests, tool defaults) with the clean path and hash. Dated
   records (old runs, sealed research notes) may keep the old reference; add
   a note only where a result depended on one of the 29 places.
2. **Re-run your signature and gate checks** on the clean image
   (`sigscan.py audit` and `gates <clean image> <src>`), and compare them
   with the old image's results. Look into any result that changed.
3. **Compare your work with the 29 places** below: signatures, gates, hook
   sites, research notes, IDA names and comments.
4. **IDA databases built from the old image** still hold the old bytes at
   the 29 places.
   - If you make signatures from a database, rebuild it from the clean image.
   - Otherwise, confirm that none of your work is near the 29 places.
   - Names and comments can move over with IDA's own export or CMFM's
     scripts.
5. **Report to the user** what changed, what you re-checked and anything
   that was affected.

## The 29 places

RVA from image base 0x140000000 (IDA EA = 0x140000000 + RVA). The bytes
are the first ones of each differing range: old image, then original.
Function names are from the clean IDA database with Fexty's names.

| RVA | Bytes | Function (+offset) | Old image | Original |
|---|---|---|---|---|
| 0x478a20 | 5 | sub_140478A20 | `e9b083afff` jump out | `4053554883` |
| 0x5f8350 | 5 | sub_1405F8350 | `e9408c98ff` jump out | `4c8d1509d7` |
| 0x5f83b0 | 5 | sub_1405F83B0 | `e91f8c98ff` jump out | `40534883ec` |
| 0x61e190 | 5 | sub_14061E190 | `e93e2e9bff` jump out | `4053555657` |
| 0xf2c320 | 5 | sub_140F2C320 | `e96e4b04ff` jump out | `48895c2408` |
| 0xf2c4e0 | 5 | sub_140F2C4E0 | `e96e4a03ff` jump out | `48895c2410` |
| 0xf9fa00 | 5 | sub_140F9FA00 | `e9ce1502ff` jump out | `48895c2408` |
| 0x10c5a70 | 5 | sub_1410C5A70 | `e920b5f0fe` jump out | `488bc44889` |
| 0x10ca7a5 | 4 | sub_1410CA600 +0x1a5 | `5758f1fe` call target | `e73955ff` |
| 0x10cb9ab | 4 | sub_1410CB860 +0x14b | `5f46f1fe` call target | `e12755ff` |
| 0x1145a1c | 1 | sub_141145500 +0x51c | `eb` (jmp) | `74` (jz) |
| 0x134ef50 | 5 | sub_14134EF50 | `e97e20c2fe` jump out | `48895c2408` |
| 0x134efe0 | 5 | sub_14134EFE0 | `e9ae1fc2fe` jump out | `48895c2408` |
| 0x1355c40 | 21 | sub_141355C40 | `e993b2c1fe…` jump out | `4863c2488d…` |
| 0x19751b0 | 5 | sub_1419751B0 | `e99fbc5ffe` jump out | `8d812b39ff` |
| 0x1976110 | 5 | sub_141976110 | `e93fae5ffe` jump out | `8d819f15ff` |
| 0x197d8e4 | 17 | uWeapon::calcBloatedRaw +0x14 | `909090909090…` NOPs | `f3420f590481…` |
| 0x197d920 | 5 | sub_14197D900 +0x20 | `9090909090` NOPs | `8d048003c0` |
| 0x197d948 | 21 | sub_14197D900 +0x48 | `9090909090…` NOPs | `8d048003c0…` |
| 0x1991690 | 5 | sub_141991690 | `e93ef95cfe` jump out | `488bc45355` |
| 0x1a10170 | 5 | sub_141A10170 | `e99e0c56fe` jump out | `48895c2408` |
| 0x1a509a4 | 6 | sub_141A50990 +0x14 | `e9c800000090` (jmp) | `0f87c7000000` (ja) |
| 0x1a6c1c0 | 5 | sub_141A6C1C0 | `e90e4e53fe` jump out | `4056574157` |
| 0x1b34ae0 | 5 | sub_141B34AE0 | `e9afc442fe` jump out | `41574883ec` |
| 0x1b66ea0 | 5 | sub_141B66EA0 | `e92ea142fe` jump out | `48895c2410` |
| 0x1b69bd0 | 5 | sub_141B69BD0 | `e9be7348fe` jump out | `4055564156` |
| 0x1ccf6c0 | 5 | Monster::generateFilePaths | `e9ce182dfe` jump out | `48895c2408` |
| 0x21e3810 | 5 | sub_1421E3810 | `e9bed7dcfd` jump out | `4053554155` |
| 0x239ea20 | 5 | sub_14239EA20 | `e9b325c5fd` jump out | `4533c0488d` |

The full comparison, with all bytes, is
`D:\mh world modding\CMFM\local\ghidra\diff_ours_vs_fexty.json`.
