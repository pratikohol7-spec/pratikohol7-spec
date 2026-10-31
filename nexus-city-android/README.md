# Nexus City — Android APK

`NexusCity.apk` is the game packaged as a native Android app (package `com.nexuscity.game`, Android 7.0+), full-screen landscape, playable offline.

## Install on your phone
1. Download `NexusCity.apk` to the phone.
2. Open it, and allow "Install unknown apps" for your browser/file manager when Android asks.
3. Tap **Install**. If Play Protect warns about an unrecognised developer, choose **Install anyway** — the app is self-signed, not from the Play Store.

## How it's built
The APK is a small WebView shell (`src/.../MainActivity.java`) that serves the game from `assets/www` over a virtual `https://appassets.androidplatform.net` origin (so ES modules and save data work).

`build.py` builds it without the Android SDK or Gradle:
- `javac --release 8` against compile-only framework stubs in `stubs/` (not shipped)
- `dx` (dalvik-dx 9.0.0_r3 from Maven Central) → `classes.dex`
- hand-encoded binary `AndroidManifest.xml` and `resources.arsc` (launcher icon)
- 4-byte aligned zip + APK Signature Scheme v2 (RSA-2048/SHA-256)

```bash
mkdir -p tools && curl -L -o tools/dx.jar https://repo1.maven.org/maven2/com/jakewharton/android/repackaged/dalvik-dx/9.0.0_r3/dalvik-dx-9.0.0_r3.jar
python3 build.py            # creates signing-key.pem on first run (keep it private & safe)
```
Rebuild after changing anything in `../nexus-city/`. To upgrade an installed copy in place, the new APK must be signed with the same `signing-key.pem`; with a different key, uninstall the old app first.
