# PossessionV 2.0: pedestrian possession and Vanilla Unicorn access (GTA V iOS port)

A Script Hook V `.asi` for the GTA V iOS port (`kevinerikjs/gta5-ios-port`). It's a real 64-bit
Windows DLL with no C runtime. It imports 6 Script Hook V functions plus `GetModuleFileNameA`,
`GetPrivateProfileIntA` and `GetAsyncKeyState`, all implemented by the port's interpreter, and it
calls 56 game commands, all of them in the port's native table.

- **Install, backup and restore:** [INSTALL.md](INSTALL.md)
- **What works, what's untested, what can't be done:** [COMPATIBILITY.md](COMPATIBILITY.md)

| Folder | Contents |
|---|---|
| `src/` | Source (`PossessionV.cpp`, `shv.h`) and the default `PossessionV.ini` |
| `lib/` | `.def` files used to build the import libraries (ScriptHookV, KERNEL32, USER32) |
| `build/` | Build output: `PossessionV.asi` |
| `release/` | `PossessionV-2.0.zip` (install this) and `PossessionV.asi` |
| `test/` | `verify.sh` (static checks), `run_in_port_interpreter.sh` + `fake_game_host.c` (runs the `.asi` in the port's own interpreter) |

## Features

- **Possession:** take control of a real pedestrian, not a skin swap. A pink marker shows the
  target; **F5** picks the next nearby ped and **F9** takes it over. Dead, dying, player-controlled and
  mission characters are skipped. **F10** takes the nearest animal.
- **Return:** **F11** returns to your original character. It also happens automatically if the
  possessed body dies. While you're away, your original is protected and shown with a green blip.
- **Possession mode ON/OFF:** from the menu or **F12**.
- **Vanilla Unicorn:**
  - Front, back, office and dressing-room doors, plus the other internal doors of those models, are
    unlocked near the club, and the interior is turned on if the game has it disabled.
  - **LOCK** (**F8** or the menu) restores each door's recorded original state, and restores the
    interior once you're outside.
- **Touch menu:** **F7** opens it on the left side of the screen, with the port's D-pad. Any key can
  get an on-screen button in Settings › Mods (port v1.5+).
- **Extras** from 1.1: all weapons, money, no wanted level in restricted areas, never wanted, night.

## Build and test (Linux or macOS with LLVM)

```
./build.sh                                                  # -> build/PossessionV.asi
test/verify.sh <gta5-ios-port checkout>                     # imports, natives, no float compares
test/run_in_port_interpreter.sh <gta5-ios-port checkout>    # 36 checks in the port's interpreter
```

Tested with clang/lld-link 18.1.3. The test builds the port's `NativeASI.c` and its vendored Blink
from the port's source; it doesn't modify the port.
