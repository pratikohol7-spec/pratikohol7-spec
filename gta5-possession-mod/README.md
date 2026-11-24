# PossessionV — ped/animal possession + Vanilla Unicorn doors

A Script Hook V `.asi` mod for the GTA V iOS port (`kevinerikjs/gta5-ios-port`), written against
that port's own Script Hook V interpreter. It is a real 64-bit Windows DLL with no C runtime.

| Folder | Contents |
|---|---|
| `src/` | Source code (`PossessionV.cpp`, `shv.h`) and the default `PossessionV.ini` |
| `lib/` | `.def` files used to generate import libraries for ScriptHookV / KERNEL32 / USER32 |
| `build/` | Build output: `PossessionV.asi` |
| `release/` | Installation ZIP: `PossessionV-1.0.zip` (`PossessionV.asi` + `PossessionV.ini`) |
| `test/` | Static checks and an end-to-end run inside the port's interpreter |

## Features

- **Possess the nearest pedestrian** (F9): you take control of the ped with `CHANGE_PLAYER_PED`.
  This switches who you play as; it doesn't change your model.
- **Possess the nearest animal** (F10), and "anyone" from the menu. Animals are peds of type 28 or
  non-human peds.
- **Return to your original character** (F11). While you're away, your original character is
  protected (invincible, stands still, ignores events) and marked with a green blip. If the
  possessed body dies, you go back automatically. Possessing again from a possessed body releases
  the old body to the game.
- **Vanilla Unicorn doors**: within 120 m of the club, every second the mod sets these to unlocked:
  - front entrance `prop_strip_door_01` (127.96, -1298.50, 29.42)
  - back entrance `prop_magenta_door` (96.09, -1284.85, 29.44)
  - office `v_ilev_roc_door2` (99.08, -1293.70, 29.42)
  - dressing room `v_ilev_door_orangesolid` (113.98, -1297.43, 29.42)

  It also unlocks any other door of the office or dressing-room model that it finds inside the club,
  and turns the club interior back on if the game has it disabled or capped. It uses the
  door-system hash if the game has registered the door, plus `SET_STATE_OF_CLOSEST_DOOR_OF_TYPE`.
- **Touch menu** (F7): large rows drawn with `DRAW_RECT`. While the menu is on screen, the port
  shows its Mods D-pad. The mod reads Up/Down/Enter/Backspace and NumPad 8/2/5/0.

## Installing on iPhone (GTA V iOS port)

1. Download `release/PossessionV-1.0.zip` to the iPhone (Files › Downloads).
2. In the app, go to **Settings › Mods › Install a mod…** and pick the ZIP. The app shows that it
   installs `PossessionV.asi` and `PossessionV.ini` as a script mod in their own Mods folder.
   Your other mods, including vehicle mods, are left as they are.
   - Manual alternative: copy both files to the top of the Mods folder: **Files › On My iPhone › GTA V Port › Mods**.
3. Close the game completely and start it again (mods load when the game starts).
4. In game, tap **🧩 Mods** (top row of the touch controls). **☰ F7** opens the menu. The port
   learns it from `MenuKey=118` in the `.ini`. Use the D-pad and Enter in the menu.
5. Optional one-tap buttons (port v1.5+): **Settings › Mods › PossessionV** lists F7, F9, F10 and
   F11. Add an **on-screen button** for each one, for example F9 "possess" with the person icon and
   F11 "return". Then move them in Edit controls layout.

Remove the mod by swiping it away in **Settings › Mods**. The game's own files are never changed.

## Settings (`PossessionV.ini`)

Keys use decimal Windows key codes. `MaxDistance` (5–300 m), `ProtectOriginal`, `AutoReturnOnDeath`,
`UnlockDoors` and `EnableInterior` each take 1 for on and 0 for off. The menu can also switch door
unlocking on and off while you play.

## Building

Requires clang, lld-link and llvm-dlltool (LLVM 15+, tested with 18.1.3). No Visual Studio is needed.

```
./build.sh                              # -> build/PossessionV.asi
test/verify.sh <gta5-ios-port checkout>  # static checks against the port's sources
test/run_in_port_interpreter.sh <gta5-ios-port checkout>  # runs the .asi in the port's interpreter
```

## What has been verified, and what has not

Verified here:
- `build/PossessionV.asi` is a PE32+ x86-64 DLL with entry point `DllMain`. It has no TLS and no CRT.
  It imports only 6 Script Hook V functions plus `GetModuleFileNameA`, `GetPrivateProfileIntA` and
  `GetAsyncKeyState`. Each of these is in the port's import table (`app/Source/NativeASI.c`).
- All 40 natives it calls are in the port's native table (`app/Scripts/asi_native_hashes.txt`).
- The port's own loader and interpreter (`NativeASI.c` + its vendored Blink) were built for Linux.
  In them, the mod loads, registers its script and reads its `.ini`; the port logs "names key 0x76 as
  a menu key". With game commands answered by a simulated world, it passes 24 checks: possess a ped,
  hop to an animal, return, menu navigation, auto-return on death, and door/interior calls. The port
  logs no faults and no missing imports.
- Workaround for a bug in the port: the port's Blink `OpCmppsd` writes `-1.0f` instead of an all-ones
  mask for `CMPSS`/`CMPPS`. Float compares that the compiler turns into branchless code therefore give
  wrong answers. This broke the nearest-ped search in testing. The mod now compares distances as
  integers, and `verify.sh` checks that the binary contains no such instructions.

Not verified (needs the real game on the phone):
- How the real engine behaves. The native table above lists the natives the port can map, but whether
  each one exists depends on the game build the port uses (it reports 2189). If one is missing, the
  port logs "game command … isn't in this game version" and that call does nothing.
- Story-mode side effects of playing as a non-story ped or animal: mission triggers, character switching,
  phone, and the death/respawn flow. Return to your original character before starting missions or
  switching characters.
- Whether the club scripts lock a door again faster than the 1-second refresh, and whether a given
  dressing-room door is a real door in the door system.
