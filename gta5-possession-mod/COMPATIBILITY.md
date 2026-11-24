# PossessionV 2.0: compatibility report

All findings below come from the port's own source, snapshot `9357c18` (2 Nov 2026), which documents
features up to v1.5. Since then the public repository `kevinerikjs/gta5-ios-port` has started returning
404, so newer port versions could not be checked.

## 1. The port and how it runs mods

| Question | Finding (source) |
|---|---|
| What runs custom code? | Script Hook V scripts: `.asi` files, which are 64-bit Windows DLLs. The port runs them in the Blink x86-64 interpreter (`app/Source/NativeASI.c`). |
| Is ScriptHookV.dll / an ASI loader needed? | No. The port provides Script Hook V's functions itself and ignores `ScriptHookV.dll` and `dinput8.dll` (`docs/MODS.md`). |
| Lua (`ControlPed.lua`)? | **Not supported.** The installer (`app/App/ModInstaller.swift`) accepts `.asi` scripts with their `.ini`/`.xml`/`.json`/`.cfg` settings, plus a fixed list of game-file types (`yft`, `ytd`, `ydd`, `ydr`, `ymt`, `ybn`, `ycd`, `ypt`, `ymap`, `ytyp`, `ynv`, `ynd`, `awc`, `rel`, `meta`, `xml`, `dat`, `gxt2`, `ymf`, `ypdb`, `rpf`). `.lua` is in neither list, so it's skipped as "not a game file". No Lua runtime exists in the port. |
| ScriptHookVDotNet, RAGE Plugin Hook | Not supported: they need .NET (`docs/MODS.md`, `docs/FAQ.md`). |
| Game version reported to mods | 2189 (`EngineHost.c`, `PGAsiHost`). |
| Script globals (`getGlobalPtr`) | Not provided: the host passes 0 (`EngineHost.c`). |
| `worldGetAllPeds/Vehicles/Objects` | Not provided: the host passes 0, so these return nothing. PossessionV uses `GET_PED_NEARBY_PEDS` and `GET_CLOSEST_OBJECT_OF_TYPE` instead. |
| Direct game-memory patches | Off on iOS ("features that change the PC game's memory directly stay off"). PossessionV uses none. |
| Touch controls | The 🧩 Mods key tray lists the keys a mod polls. A menu drawn with `DRAW_RECT` makes the touch D-pad appear. Since v1.5, Settings › Mods › *mod* can add an on-screen button for any of its keys, which you can move and resize in Edit controls layout. There is no other overlay API, so PossessionV uses this one. |
| Text input | `DISPLAY_ONSCREEN_KEYBOARD` opens the iPhone keyboard (`EngineHost.c`). PossessionV doesn't need it. |
| Mod limit | 16 `.asi` script mods (`NativeASI.c`, `modules[16]`). Vehicle mods don't count toward this. |

## 2. Your 8 vehicle mods

The installer puts each mod in its own folder in Mods (`docs/MODS.md`). PossessionV contains only
`PossessionV.asi` and `PossessionV.ini`. It replaces no game file and no vehicle file, and it never
writes outside its own folder; the only file it reads is its `.ini`. Vehicle mods are game-file
replacements or add-on packs, and they don't load as scripts. The two kinds can't overwrite each other.
The installer also shows you what it will install before it does anything.

## 3. Feature status

Legend: **Verified here** = compiled, statically checked against the port's tables, and run inside the
port's own interpreter against a simulated game world (36 checks pass). **Untested in game** = not yet
run on an iPhone in the real game.

| Feature | Status | Notes |
|---|---|---|
| Possess a real pedestrian (`CHANGE_PLAYER_PED`) | Verified here; untested in game | You control the ped itself, with its own model, clothes and look. The game gives it the player's controls, camera, running, jumping and so on, as it does for character switching. Nothing swaps a skin or model. |
| Select a ped (marker + F5 next target) | Verified here; untested in game | A pink marker shows which ped F9 will take. |
| Safety checks before switching | Verified here | Skips dead, dying, non-ped, player and out-of-range entities, and by default mission characters (`IS_ENTITY_A_MISSION_ENTITY`). It re-checks just before switching, and if the game refuses it releases the target and tells you. |
| Possess animals | Verified here; untested in game | Animals are peds; how well each animal plays depends on the game. |
| Return to original player (F11, auto on death) | Verified here; untested in game | The original is kept as a mission entity: protected, standing still, with a green blip. If it dies or despawns, the mod tells you, and you stay in your current body. |
| Restore camera and controls on return | Handled by the game | `CHANGE_PLAYER_PED` moves the camera and controls with the player. The mod clears the original's tasks and protection. |
| Wanted level on return | Kept by default | The wanted level belongs to the player, not the ped. `ClearWantedOnReturn=1` clears it when you return. |
| Switch to Michael/Franklin/Trevor | Return only | Returning puts you back in whichever of the three you were. Switching to a *different* story character needs the story's character-switch state (script globals), which the iOS port doesn't give to mods. Doing it without that state could break the story save, so it's not done. |
| Vanilla Unicorn front and back entrances | Verified here; untested in game | `prop_strip_door_01` (127.96, -1298.50, 29.42) and `prop_magenta_door` (96.09, -1284.85, 29.44). These models and positions were cross-checked in several community door-lock configs. |
| Office and dressing-room doors | Verified here; untested in game | `v_ilev_roc_door2` (99.08, -1293.70, 29.42) and `v_ilev_door_orangesolid` (113.98, -1297.43, 29.42). Other doors of those models inside the club are found at run time with `GET_CLOSEST_OBJECT_OF_TYPE`. |
| Interior access | Verified here; untested in game | If the game has the club interior disabled or capped, it's turned back on, and the original flags are saved. |
| LOCK / restore original doors (F8, menu) | Verified here | Each door's door-system state and lock flag are saved before the first change and written back on LOCK. The interior flags are restored once you're more than 40 m from the club, so it isn't switched off around you. |
| Doors without story progress | Partly | Doors and interior are opened. Story-gated *activities* (dancers, the club's mission scripts) are started by story scripts that use script globals, which mods can't reach on iOS. |
| Touch: menu + on-screen buttons | Uses the port's documented system | F7 opens the menu, drawn on the left side, away from the right-hand drive and fire controls. F9, F11, F5, F8, F12 and F10 can each get an on-screen button in Settings › Mods (v1.5+). |
| Extras (weapons, money, wanted, time) | Verified here; untested in game | Kept from PossessionV 1.1. |

## 4. A port bug found while building this

The port's Blink `OpCmppsd` (`Vendor/blink/blink/ssefloat.c`) writes `-1.0f` instead of an all-ones mask
for `CMPSS`/`CMPPS`. Float compares that the compiler turns into branchless code therefore give wrong
answers on the iPhone. PossessionV compares distances as integers instead, and `test/verify.sh` checks
that the binary contains no such instructions.

## 5. What would need port changes

- **Switching to another story character:** needs script globals (`getGlobalPtr`), which the port could provide.
- **Story-gated club activities:** same requirement.
- **Lua scripts:** would need a Lua runtime and a script loader in the port. Rewriting a Lua mod as an `.asi`, as done here, is the supported route.
