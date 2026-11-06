# WAR OF CHESS: Development Progress

## Current phase: 1, Development Environment (in progress)

### Completed (2026-11-06)

- Created the project documentation set: README, PROJECT_PLAN, TECHNICAL_ARCHITECTURE, BUILD_ANDROID, BUILD_IOS, KNOWN_ISSUES and this file.
- Created a Unity-specific `.gitignore`, which excludes Library/Temp/Logs, builds and signing secrets, and a `.gitattributes`.
- Created `tools/inspect_mac.sh`, a **read-only** Mac environment check.
- Created `tools/create_asset_folders.sh`, which builds the agreed `Assets/` folder structure after Unity creates the project.

### Files created

`WarOfChess/README.md`, `PROJECT_PLAN.md`, `DEVELOPMENT_PROGRESS.md`, `TECHNICAL_ARCHITECTURE.md`, `BUILD_ANDROID.md`, `BUILD_IOS.md`, `KNOWN_ISSUES.md`, `.gitignore`, `.gitattributes`, `tools/inspect_mac.sh`, `tools/create_asset_folders.sh`

### Tests performed

- Shell scripts checked for syntax (`bash -n`) in the cloud session. They have **not** been run on a Mac yet.

### Errors encountered

- None. Limitation: the Claude session that wrote these files runs in a cloud container, so it could not inspect your Mac directly.

---

## Manual actions required from you (in this order)

### Step 1: Get this project onto your Mac

1. Open **Terminal** (press `⌘ Space`, type `Terminal`, press Enter).
2. Type `git --version` and press Enter.
   - If a window pops up asking to install **Command Line Tools**, click **Install** and wait for it to finish. This is Apple's official installer and it includes Git.
3. Then run these lines one at a time. They create a `Projects` folder in your home folder and download this repository:
   ```
   mkdir -p ~/Projects
   cd ~/Projects
   git clone https://github.com/pratikohol7-spec/pratikohol7-spec.git
   cd pratikohol7-spec
   git checkout claude/war-of-chess-setup-tia2qu
   cd WarOfChess
   ```

### Step 2: Run the read-only inspection

```
bash tools/inspect_mac.sh
```
Copy **all** of the output and paste it into Claude Code. It changes nothing on your Mac.
I will then turn it into the checklist: installed / missing / needs update / needs your permission / free storage.

> Tip for 8 GB RAM: close browsers and other apps while installing Xcode and Unity.

### Step 3: Install software (I'll confirm the exact list after Step 2)

**A. Xcode** (about 15 to 20 GB, can take an hour)
1. Open the official App Store page: https://apps.apple.com/app/xcode/id497799835, then click **Get / Install**.
2. Open **Xcode** once. Click **Agree** on the licence. Enter your **Mac login password** if asked (this is your Mac's password, not your Apple Account's, and only macOS sees it).
3. When asked which platforms to install, make sure **iOS** is ticked, then click **Download & Install**.
4. Go to **Xcode → Settings… → Accounts → +** → **Apple Account** and sign in. You type your password into Apple's window only. Never paste it to Claude.

**B. Unity Hub**
1. Go to https://unity.com/download and download **Unity Hub for macOS (Apple Silicon)**.
2. Open the downloaded `.dmg` and drag **Unity Hub** into **Applications**.
3. Open Unity Hub. Sign in or create a free **Unity account**, and accept the free **Personal** licence when offered.

**C. Unity 6 LTS Editor**
1. In Unity Hub, click **Installs → Install Editor**.
2. On the **Official releases** tab, pick the **newest version marked "LTS"** in the Unity 6 series (`6000.x`). Do **not** pick anything marked Beta, Alpha or Tech Stream/Supported-only if an LTS exists.
3. On the **Add modules** screen, tick **only**:
   - ☑ **Android Build Support** → and underneath it ☑ **OpenJDK** and ☑ **Android SDK & NDK Tools**
   - ☑ **iOS Build Support**
   - Leave everything else (Windows, Linux, WebGL, Visual Studio, documentation) unticked to save disk space.
4. Accept the Android SDK licence when asked, then click **Install**.

### Step 4: Create the Unity project

1. Unity Hub → **Projects → New project**.
2. Choose the Unity 6 LTS editor you installed (selector at the top).
3. Template: **Universal 3D** (this is URP). Click **Download template** if asked.
4. **Project name:** `WarOfChess`
5. **Location:** the folder `~/Projects/pratikohol7-spec/WarOfChess` (click the folder icon and browse to it).
   The project will be created at `~/Projects/pratikohol7-spec/WarOfChess/WarOfChess`.
6. Untick **Connect to Unity Cloud** / **Use Unity Version Control**. We use Git instead.
7. Click **Create project** and wait until the Unity Editor opens with no red errors at the bottom (the Console).
8. Back in Terminal (in the `WarOfChess` folder), run:
   ```
   bash tools/create_asset_folders.sh
   ```
9. Tell Claude Code it's done. I will then set landscape orientation, platform settings, the mobile URP quality levels and the first Git checkpoint.

---

## Remaining work in Phase 1

- [ ] Inspection output received and checklist produced
- [ ] Xcode installed, licence accepted, iOS platform installed
- [ ] Unity Hub installed and signed in
- [ ] Unity 6 LTS installed with Android (SDK/NDK/OpenJDK) and iOS modules, verified by re-running `inspect_mac.sh`
- [ ] `claude --version` verified on the Mac, and Claude Code started inside the `WarOfChess` folder
- [ ] Git name/email configured on the Mac
- [ ] Unity URP project created, opens with no errors
- [ ] Asset folder structure created
- [ ] Unity version recorded in README
- [ ] Player settings: landscape, product name, identifiers
- [ ] Git checkpoint "Phase 1 complete"

## Next recommended task

Run **Step 1 and Step 2** above and paste the inspection output into Claude Code.
