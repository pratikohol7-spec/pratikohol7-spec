# Nexus City: Anomaly Hunters

An original urban-supernatural open-world action game for mobile browsers (and desktop), built with Three.js. No build step, no downloads: open it and play.

## Features
- **Open-world neon city**: a 9×9-block procedural city with hundreds of buildings, neon signs, parks, streetlights, traffic and pedestrians, plus the towering Nexus Spire.
- **Day/night cycle**: windows and streetlights glow at night, and more anomalies roam after dark.
- **Team of 3 espers you can swap between mid-fight** (each swap triggers a swap-in attack):
  - **Rei (Pyro)**: blade combos, *Blaze Whirl*, ultimate *Inferno Requiem* (meteor rain)
  - **Mika (Volt)**: ranged bolts, *Chain Bolt*, ultimate *Thunderdome*
  - **Sora (Void)**: heavy gauntlets, *Singularity* black hole, ultimate *Event Horizon*
- **Traversal**: climb any building, double-jump, glide, dash. Dodging an attack at the last moment triggers slow motion.
- **Driving**: get into any parked car or take one from traffic, with a boost and enemy ramming.
- **Story questline**: meet Iris at Nocturne Curios, seal 5 Anomaly Rifts (wave fights), then fight the boss, **The Hollow Clock**.
- **Progression**: 37 Echo Shards (one is on top of the Spire), credits, and Resonance upgrades. Progress auto-saves to your device.
- **Mobile-first UI**: floating joystick, touch camera, action buttons, minimap, full map and quest waypoint. It can be installed as a PWA (Add to Home Screen).

## Play
Serve the folder with any static server, then open it on your phone:

```bash
cd nexus-city && python3 -m http.server 8000
# open http://<your-computer-ip>:8000 on your phone (same Wi-Fi)
```

You can also enable **GitHub Pages** on this repo and open `/nexus-city/`.

**Touch:** left side = move, right side = look, buttons = attack / skill / ultimate / jump / dash, tap a portrait to swap, tap the minimap for the full map.

**Keyboard:** WASD, mouse drag, click/J attack, E skill, Q ultimate, Space jump (press again in the air to glide), Shift dash, F interact/drive, 1-2-3 swap, M map, Esc pause.

Three.js (MIT) is vendored in `lib/`.
