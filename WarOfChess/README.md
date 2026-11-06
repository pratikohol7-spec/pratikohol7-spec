# WAR OF CHESS

A realistic 3D medieval battlefield chess game for Android and iOS, built from one shared Unity project.
Each chess piece is a warrior. Every capture plays a short battle cinematic. The chess rules alone decide every outcome.

> **Status:** Phase 1, Development Environment. No game code exists yet. See [DEVELOPMENT_PROGRESS.md](DEVELOPMENT_PROGRESS.md).

## Project setup

| Item | Value |
|---|---|
| Engine | Unity 6 LTS (exact version gets recorded here after installation) |
| Unity version | _Not installed yet. Will be filled in from `WarOfChess/ProjectSettings/ProjectVersion.txt`_ |
| Render pipeline | Universal Render Pipeline (URP) |
| Language | C# |
| Platforms | Android (APK, ARM64) and iOS (Xcode project) |
| Orientation | Landscape |
| Development machine | Apple M1 Mac, 8 GB RAM |

## Folder layout

```
WarOfChess/                  <- this folder (docs, tools, git settings)
├── README.md, PROJECT_PLAN.md, DEVELOPMENT_PROGRESS.md, ...
├── tools/
│   ├── inspect_mac.sh            read-only Mac environment check
│   └── create_asset_folders.sh   creates the Assets/ folder structure
└── WarOfChess/              <- the Unity project (created by Unity Hub)
    ├── Assets/              game content (Art, Audio, Prefabs, Scenes, Scripts, ...)
    ├── Packages/
    └── ProjectSettings/
```

The Unity project sits in its own subfolder because Unity Hub can only create a project in an empty folder.

## How to open the project

1. Open **Unity Hub**.
2. Click **Projects**, then **Add**, then **Add project from disk**.
3. Select the folder `WarOfChess/WarOfChess`, the inner one that contains `Assets`.
4. Click the project to open it.

## How to run the game

_Available from Phase 2._ Open `Assets/Scenes/Main.unity`, then press the ▶ **Play** button at the top of the Unity Editor.

## How to build

- Android: see [BUILD_ANDROID.md](BUILD_ANDROID.md)
- iOS: see [BUILD_IOS.md](BUILD_IOS.md)

## Architecture

See [TECHNICAL_ARCHITECTURE.md](TECHNICAL_ARCHITECTURE.md). The key rule: the chess rules engine is plain C# with no Unity dependency. Animation and cinematics only *display* results the engine has already decided.

## Known limitations

See [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

## What is not committed to Git

Unity's `Library/`, `Temp/`, `Logs/`, `UserSettings/`, all build output (`.apk`, `.aab`, Xcode build folders), and all signing files and secrets (`.keystore`, `.p12`, `.mobileprovision`, `.env`). See `.gitignore`.
