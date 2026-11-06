# Building for Android

> Becomes active in **Phase 6**. Steps will be verified and refined then.

## Requirements

- Unity 6 LTS with **Android Build Support**, **Android SDK & NDK Tools** and **OpenJDK** modules (installed in Phase 1).
- Your Android phone and a USB cable (optional, since you can also transfer the APK by cloud or file sharing).

## Planned settings (Edit → Project Settings → Player → Android tab)

| Setting | Value |
|---|---|
| Product Name | WAR OF CHESS |
| Package Name | `com.pratikohol.warofchess` (can be changed before any store release; must never change after) |
| Default Orientation | Landscape Left (auto-rotate between Landscape Left/Right) |
| Scripting Backend | IL2CPP |
| Target Architectures | ARM64 only |
| Graphics APIs | Vulkan, OpenGLES3 fallback |
| Texture compression | ASTC |

## Development APK (testing)

1. **File → Build Profiles** → select **Android** → **Switch Platform**.
2. Tick **Development Build**.
3. Click **Build**, then save to the `Builds/Android/` folder (Git ignores it).
4. Confirm that `Builds/Android/WarOfChess.apk` exists.

## Installing on your phone

1. Copy the APK to your phone (USB, Google Drive, etc.).
2. On the phone, open the APK file. If Android asks, allow **Install unknown apps** for the app you opened it with.
3. Tap **Install**.

## Release APK

Uses a private keystore that **you** create and keep safe. Never commit it to Git (`.gitignore` blocks `*.keystore`). Steps get added in Phase 6.
