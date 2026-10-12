# Requests for Astera Loader

What CMFM needs from AL that AL does not offer yet. AL's repository is
read-only from here; the user takes these to the AL session. Each entry:
what, why, the smallest API that would do, and its state.

## 1. Checked writes to the game's read-only data

**State:** requested 2026-10-12 (for CMFM's first release). AL 0.1.0, API
1.7, has no such service.

**What:** a service that writes one 8-byte value in the game exe's
read-only data, only if the expected old value is there, and puts the old
value back when the plugin asks (and at shutdown), but only if the value
is still the plugin's.

**Why:** CMFM's "Mods" entry in the in-game Start menu (System tab). The
game's Start menu tab lists are constant tables in `.rdata`; each tab's
entry holds a pointer to that tab's item table. CMFM points the System
tab's pointer at its own copy of the table, with "Mods" inserted after
"Options". There are two lists, so two values (exe 421810):

| List | Value's RVA | Old value | New value |
|---|---|---|---|
| Start menu | `0x34818B8` | the System tab's item table (RVA `0x3FC0E40`) | CMFM's copy of that table, in cmfm.dll |
| Start menu in a quest | `0x3481868` | the same | the same |

Today CMFM does this itself with a prototype helper (CMFM decisions item
9): `VirtualProtect` to read-write, a compare-and-swap, the old protection
back, the reverse at shutdown. The rule from the start was: once the menu
works in game, the helper becomes an AL service. It works: the entry has
been in every run since 2026-10-11 (run `20261011-startmenu-1` on).

Why it belongs in AL, as code hooks do (`al.hooks`):
- one owner for protection changes on the game's image;
- two plugins aiming at the same value: the second is told which plugin
  holds it, instead of one silently undoing the other;
- AL's logs and crash reports can list the changed values and their
  owners, so a crash near them names the right plugin.

Only these two values. CMFM's other swaps (the Options pages' row-table
pointers, RVAs `0x3FBFE90` and `0x3FC0480` and up) are in `.data`, which is
writable; CMFM keeps those as its own business, with no protection change
(commit 4fc3ba2).

**Smallest API that would do** (names are AL's call; it could equally be
`al.hooks` 1.1):

```c
/* "al.patch" 1.0: values in the game's read-only data, checked and put back. */
#define AL_PATCH_INTERFACE "al.patch"
#define AL_PATCH_VERSION AL_MAKE_VERSION(1, 0)

typedef struct al_patch_t {
    uint32_t size;        /* sizeof(al_patch_t) */
    uint32_t reserved;
    const char* name;     /* for logs and reports, e.g. "Start menu: System tab items" */
    void* address;        /* 8-byte aligned, in the game exe's image, outside its code */
    uint64_t expected;    /* the value that must be there */
    uint64_t replacement; /* the value to write */
} al_patch_t;

typedef struct al_patches_t {
    uint32_t size;
    uint32_t version; /* AL_PATCH_VERSION */
    /* Swaps atomically if `expected` is there. Returns a handle, or 0 with
     * the reason in the plugin's log: another value found (and the plugin
     * that wrote it, if one did), an address outside the image or in its
     * code, unaligned. Any thread, from al_plugin_init on. */
    uint32_t(AL_CALL* apply)(al_plugin_t* self, const al_patch_t* patch);
    /* Puts `expected` back if the value is still `replacement`; otherwise
     * leaves it and logs what it found. */
    al_result_t(AL_CALL* restore)(al_plugin_t* self, uint32_t handle);
} al_patches_t;
```

At shutdown AL puts back whatever is still applied. CMFM applies at
`AL_PHASE_WIN_MAIN` and restores at `AL_PHASE_SHUTDOWN` itself.

Not needed: lengths other than 8 bytes, patterns (CMFM checks the exe's
hash and the table's contents before asking; the expected value is the
final check), writes to `.data` or to code.

**CMFM's side once AL ships it:** CMFM uses the service and drops its
helper. On an older AL, the Start menu entry stays hidden and the Overview
says why ("needs Astera Loader <version> or newer"); the title screen's
Mods entry and everything else still work (they need no data writes).

## Candidates (not requested)

- **Other plugins' versions.** CMFM's page header shows a mod's version
  when the mod gives one; for config pages of plugins that do not talk to
  CMFM there is none. A read-only list of AL's loaded plugins (name,
  version, kind) would fill it. Nice to have, not needed for a release.
- ~~AL's start-up warnings in CMFM~~: not now. They would have gone into
  CMFM's popups, which were dropped (CMFM decisions item 12).
