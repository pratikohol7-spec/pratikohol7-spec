# SuperGarageV — every super car and super bike, with engine-sound choice

A Script Hook V `.asi` for the GTA V iOS port (`kevinerikjs/gta5-ios-port`). It's a real 64-bit
Windows DLL with no C runtime, and it imports only functions the port implements. It is separate
from PossessionV, and both can be installed together.

| Folder | Contents |
|---|---|
| `src/` | Source (`SuperGarageV.cpp`, `shv.h`) and the default `SuperGarageV.ini` |
| `build/` | Build output: `SuperGarageV.asi` |
| `release/` | `SuperGarageV-1.2.zip` (install this) and `SuperGarageV.asi` |
| `test/` | Static checks and an end-to-end run inside the port's interpreter |

## What it does

- **F6** opens the menu. It has the pages **Super cars (N)** and **Super bikes (N)**, which list the
  game's own vehicles by their in-game names. Pick one to spawn it in front of you and get in.
- The lists come from the game itself. Each candidate model is checked with `IS_MODEL_IN_CDIMAGE`,
  and vehicles the game build doesn't have are left out. The vehicle class comes from
  `GET_VEHICLE_CLASS_FROM_NAME`: class 7 (Super) goes in the car list and class 8 (Motorcycles) in
  the bike list.
- **Dodge Challenger first:** the top menu row spawns the Bravado Gauntlet Hellfire, GTA's Dodge
  Challenger look-alike (the plain Gauntlet if your game lacks the Hellfire). The second row,
  **Challenger V8 sound on every vehicle**, gives every vehicle you get in the Challenger's engine
  sound. You can also switch it on at start with `ChallengerSoundEverywhere=1` in the `.ini`.
- **Real-world picks:** the game's own vehicles whose designs are widely known to be based on the
  real ones you asked for:

  | Real vehicle | In-game vehicle (model) |
  |---|---|
  | Bugatti Chiron | Truffade Nero (`nero`, `nero2`) |
  | Bugatti Veyron | Truffade Adder (`adder`) |
  | Pagani Huayra | Pegassi Osiris (`osiris`) |
  | Koenigsegg CCX / Agera | Överflöd Entity XF / Entity XXR (`entityxf`, `entity2`) |
  | McLaren P1 / Senna / F1 | Progen T20 / Emerus / GP1 (`t20`, `emerus`, `gp1`) |
  | Aston Martin Vanquish / One-77 | Dewbauchee Massacro / Seven-70 (`massacro`, `seven70`) |
  | Bentley Continental GT | Enus Paragon R (`paragon`) |
  | Dodge Challenger / Hellcat | Bravado Gauntlet / Gauntlet Hellfire (`gauntlet`, `gauntlet4`) |
  | Suzuki Hayabusa | Shitzu Hakuchou / Hakuchou Drag (`hakuchou`, `hakuchou2`) |
  | Ducati superbike | Pegassi Bati 801 (`bati`) |

  These are game vehicles with game sounds, not licensed real cars. These had no clear in-game
  counterpart, so they aren't on this page: Kawasaki Ninja H2/H2R, ZX-10R and Z900; BMW M4 and BMW
  bikes; Royal Enfield Bullet; Benelli.
- **Add-on vehicles:** if you install real-brand add-on cars or bikes yourself (`dlc.rpf` packs), put
  their spawn names in `SuperGarageV.ini` under `[AddOns]` (`Vehicle1=` … `Vehicle32=`). They appear
  on the **Add-on vehicles** page when the game has loaded them. Add-ons keep the engine sound their
  author gave them. Note the port's v1.4.1 limit: an add-on that brings its own engine sound stops the
  game at start (see below).
- **Spawn by name (keyboard):** opens the game's text box, which the port answers with the iPhone
  keyboard. Type any vehicle's spawn name, including add-ons.
- **Engine sound:** by default each vehicle uses its own game engine sound. You can also pick any listed
  vehicle's engine sound (`FORCE_USE_AUDIO_GAME_OBJECT`). It applies to new spawns, and
  **Apply engine sound to my vehicle** sets it on the car you're sitting in.
- Two options: put you in the vehicle, and delete your previous spawn. A spawn you're sitting in is
  never deleted.
- In the menu: Up/Down move, Left/Right page by 10, Enter selects, Back goes back. The port's touch
  D-pad maps to these keys.

## Engine sounds on the iPhone

The vehicles use GTA V's own engine sounds. This mod doesn't add recordings of real cars. The port's
MODS.md explains why many engines sound faint or alike on iPhone: the common web game files carry
about 6 MB of engine audio, against hundreds of MB on PC. To get each car's full sound, copy the audio
archives from your own PC copy into the Mods folder, at the same path as on PC. Don't put them in the
game folder.

```
Mods/x64/audio/sfx/STREAMED_VEHICLES_GRANULAR.rpf
Mods/x64/audio/sfx/...   (the other vehicle audio archives from your PC copy)
```

Add-on car mods with their own real-car sound packs come from other modders. The port can install
them as `dlc.rpf` packs, but in v1.4.1 an add-on car that brings its own engine sound stops the game
at start (MODS.md); the port's next update is meant to load those sounds.

## Install

1. Download `release/SuperGarageV-1.2.zip` on the iPhone.
2. **Settings › Mods › Install a mod…**, pick the ZIP, then close the game completely and start it again.
3. In game, tap **🧩 Mods** and then **☰ F6**. You can add an on-screen F6 button in
   **Settings › Mods › SuperGarageV** (port v1.5+).

## Build and checks

```
./build.sh
test/verify.sh <gta5-ios-port checkout>
test/run_in_port_interpreter.sh <gta5-ios-port checkout>
```

## Verified, and not verified

Verified here:
- The `.asi` compiles with no errors or warnings.
- Static checks pass: all 10 imports and all 37 natives are in the port's tables, and the binary has no
  float-compare instructions.
- It loads and runs in the port's own interpreter (built from its source on Linux) against a simulated
  game world. It passes 30 checks: the Challenger shortcut and sound-everywhere option, class filtering, real-world picks, add-ons from the `.ini`, spawn by name
  through the keyboard, previous-spawn handling, paging, sound choice and apply, and the menu.

Not verified in the real game on an iPhone:
- Which DLC vehicles your game files contain. The menu shows the real counts at run time.
- How long models take to stream in.
- How a borrowed engine sound sounds with the port's reduced audio files.
