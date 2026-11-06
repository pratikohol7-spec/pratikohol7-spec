# Building for iOS (iPhone 17 Pro)

> Becomes active in **Phase 7**. Steps will be verified and refined then.
> Unity does not produce an APK for iPhone. It produces an **Xcode project**, which Xcode signs and installs.

## Requirements

- Unity 6 LTS with **iOS Build Support** module.
- Xcode from the Mac App Store, opened at least once with the licence accepted.
- A free Apple Account signed into Xcode is enough to install on **your own** iPhone. Apps signed this way expire after 7 days and must be re-installed from Xcode. App Store / TestFlight distribution requires the paid Apple Developer Program.
- iPhone with **Developer Mode** enabled (Settings → Privacy & Security → Developer Mode).

## Planned settings (Edit → Project Settings → Player → iOS tab)

| Setting | Value |
|---|---|
| Product Name | WAR OF CHESS |
| Bundle Identifier | `com.pratikohol.warofchess` |
| Orientation | Landscape Left / Right |
| Target minimum iOS version | to be decided in Phase 7 (Unity 6 default is fine) |
| Graphics API | Metal |

## Steps (summary)

1. **File → Build Profiles** → **iOS** → **Switch Platform** → **Build**, then choose the folder `Builds/iOS/`.
2. Open `Builds/iOS/Unity-iPhone.xcodeproj` in Xcode.
3. Connect your iPhone, and choose it in the device menu at the top.
4. Select the **Unity-iPhone** target → **Signing & Capabilities** → tick **Automatically manage signing** → choose your **Team** (your Apple Account).
5. Press **▶ Run**. On first install, trust the developer on the iPhone: Settings → General → VPN & Device Management.

Your Apple Account password is entered only inside Xcode. It is never put in this project.
