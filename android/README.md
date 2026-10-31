# Iron Tide for Android

`dist/EmberIronTide.apk` is the game packaged as an Android app. It is fully offline: Three.js and the fonts are bundled inside.
It runs full-screen in landscape and keeps the screen awake. The back button pauses the game, and on the title screen it exits.

* Requires Android 7.0 (API 24) or newer with an up-to-date Android System WebView / Chrome (WebGL 2).
* Package: `com.pratikohol.irontide` · version 1.1 (code 2) · signed with APK Signature Scheme v2.

## Installing

1. Copy `EmberIronTide.apk` to your phone, or download it from GitHub on the phone.
2. Open it. Android will ask you to allow installs from that app (Files / Chrome). Allow it, then tap **Install**.
3. Play Protect may warn that the app is from an unknown developer. That is normal for sideloaded apps; tap **Install anyway**.

If the phone gets warm or stutters, pick **Graphics: Low** on the title screen.

## Rebuilding

No Android SDK or Gradle needed:

```bash
python3 android/build_apk.py
```

The script bundles `game/index.html` with esbuild and compiles `src/.../MainActivity.java` with `javac` + `dx`.
It writes the binary manifest and resource table itself, then signs with `apksig`.
It needs Python 3 with Pillow, Node/npm, a JDK, and network access to Maven Central and npm. Downloads are cached in `android/.cache/`.

**Signing key:** the first build creates `android/release.p12`, which git ignores. Keep that file safe and private.
Android only accepts an update if it is signed with the same key, so a build with a new key has to be installed after uninstalling the old one.
Set `IRONTIDE_KEYSTORE` / `IRONTIDE_KEYPASS` to use your own key.
