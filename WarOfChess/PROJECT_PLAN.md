# WAR OF CHESS: Project Plan

Each phase must pass its acceptance check before the next one starts.

| Phase | Goal | Acceptance check |
|---|---|---|
| 1. Development Environment | Mac inspected; Unity Hub, Unity 6 LTS (+ Android/iOS modules), Xcode and Git ready; Unity URP project created | Unity project opens without errors; environment checklist all green |
| 2. Chess MVP | Board, simple pieces, full legal rules, touch input, restart, basic AI | A complete game of chess is playable, and every rule test passes |
| 3. 3D Warrior System | Warrior placeholders per piece type, team colours, idle and move animations, camera | Chess is playable with 3D warriors |
| 4. Combat Cinematics | Capture → cinematic per piece pairing, camera transitions, skip/shorten setting | Every legal capture plays the correct cinematic and the result never changes |
| 5. Graphics & Presentation | Battlefield, materials, baked lighting, VFX, audio, menus, victory/defeat screens | Polished, presentable build |
| 6. Android Build | Player settings, optimisation, development APK, testing, fixes | APK installed and played on your Android phone (verified by you) |
| 7. iOS Build | Xcode project, signing, install on iPhone 17 Pro, testing, fixes | App runs on your iPhone (verified by you) |
| 8. Final QA | All rules, all capture types, AI, screen sizes, performance, audio, UI, docs | Stable Android and iOS versions |

## Game modes (architecture planned now, built later)

1. **Classic War**: human vs AI. *(MVP)*
2. **Local Two Player**: same device. *(MVP, almost free once rules exist)*
3. **Campaign**: sequence of battles with rising AI difficulty.
4. **Survival War**: endless series of harder opponents.
5. **Custom Battle**: custom positions (FEN) and settings.
6. **Online Multiplayer**: separate future milestone. Needs a server-authoritative design. No paid services without your approval.

## Guiding constraints

- 8 GB RAM Mac: keep packages minimal, use URP Mobile settings, baked lighting, small textures (≤1024 px for characters at first).
- Free/open-source tools and properly licensed free assets only. Placeholders are clearly labelled.
- Chess logic is deterministic and separate from visuals.
