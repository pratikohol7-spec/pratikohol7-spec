# Ember & the Iron Tide

An original 1930s-style run-and-gun boss fight, rendered in real-time 3D with Three.js.
Ember, a steam-powered tinkerer with an arc-cannon, takes on **Brineheart, the Leviathan Engine**,
a clockwork sea monster that has risen out of a stormy harbor.

**Play:** open `index.html` in a modern desktop browser (Chrome, Edge, Firefox or Safari with WebGL 2).
It loads Three.js from the jsDelivr CDN, so you need an internet connection.

## Controls

**Easy controls** are on by default (toggle on the title screen). Your cannon fires on its own and
locks onto the boss, or onto the nearest torpedo, mine or drone that is getting close. You only move, jump and dodge.

| Action | Keyboard | Phone / tablet | Gamepad |
|---|---|---|---|
| Move | A / D or ← → | Drag your left thumb anywhere on the left half | Left stick / D-pad |
| Jump (hold for higher) | W / ↑ / Space | Push the stick up, or tap **JUMP** | A |
| Duck · drop through a crate | S / ↓ | Pull the stick down | Stick down |
| Dash (brief invulnerability) | Shift / L | **DASH** | B / RB |
| Tempest Lance (gauge full) | K / V | **LANCE** (it glows when ready) | Y |
| Pause | Esc / P | **II**, or the Android back button | Start |

**Classic controls** (Easy off): hold J / X (or **FIRE**) to shoot. W / ↑ aims up instead of jumping, Space / Z jumps,
and C plants your feet to aim in 8 directions.

## Android app

A ready-to-install APK is in [`../android/dist/EmberIronTide.apk`](../android/dist/EmberIronTide.apk). See [`../android/README.md`](../android/README.md).

## The fight

* **Phase I:** torpedo volleys in two lanes (jump the low ones, duck the high ones), a claw-tentacle slam, and homing spark mines you can shoot down.
* **Phase II (66%):** a smokestack blows off. Adds anchor throws that bounce across the deck, and steam geysers that erupt in sequence.
* **Phase III (33%):** the chest armor blows off to expose the core. Adds lightning beams at ground or platform height, and homing jelly drones.

Your hits fill the steam gauge. When the needle reaches the red zone, fire the Tempest Lance for heavy damage.
You get a letter grade based on time and hull hits.

## Graphics

* Procedurally modeled, jointed 3D hero with physically based skin, cloth sheen, leather and brass, plus procedural run, jump, duck, dash and aim animation and a verlet-simulated scarf
* Gerstner-wave ocean shader with fresnel sky reflections and foam, plus a volumetric-style storm sky and lightning
* Wet clearcoat pier deck, soft shadows, a PMREM environment map built from the sky, and an ACES filmic tone map
* Unreal bloom, plus a custom film-reel pass with grain, gate weave, scratches, dust, chromatic aberration and print-stock grading (you can turn this off)
* Fully synthesized audio: a swing-jazz score and all sound effects come from WebAudio

Graphics presets: **Ultra / High / Low** on the title screen (or append `?q=low` to the URL).
