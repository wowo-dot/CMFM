# CMFM

Configuration Menu For Mods: an in-game settings menu for Monster Hunter:
World mods on PC (DirectX 12), built as a plugin for
[Astera Loader](https://github.com/wowo-dot/AsteraLoader).

Mods register their settings, status pages and warnings with CMFM, and
players change them in game with one hotkey. Each mod's own config file
stays the source of truth.

Early development: the plugin loads and logs its version; there is no
menu yet.

## Building

Windows with Visual Studio (C++ for x64) and CMake 3.25 or newer:

```
cmake -S . -B local/build -A x64
cmake --build local/build --config Release
```

The plugin is `local\build\Release\cmfm.dll`. Install it as
`<game>\AsteraLoader\plugins\cmfm\cmfm.dll` (under Mod Organizer 2: a mod
of its own with that layout) and start the game through Astera Loader.

## Licence

MIT (`LICENSE`). `third_party/al` is Astera Loader's plugin header (MIT).
