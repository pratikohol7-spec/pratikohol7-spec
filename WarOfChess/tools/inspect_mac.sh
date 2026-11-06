#!/bin/bash
# WAR OF CHESS — Mac development environment inspection
#
# READ-ONLY. This script does not install, change or delete anything.
# It only prints information about your Mac so we can plan the setup.
#
# How to run (in Terminal):
#   bash tools/inspect_mac.sh
#
# Then copy the full output and paste it back to Claude Code.

line() { printf '%s\n' "------------------------------------------------------------"; }
check_app() {
  # $1 = display name, $2 = path
  if [ -d "$2" ]; then echo "  [FOUND]   $1  ($2)"; else echo "  [MISSING] $1"; fi
}

echo "WAR OF CHESS — Mac inspection report"
echo "Generated: $(date)"
line

echo "1. macOS"
sw_vers 2>/dev/null | sed 's/^/  /'
line

echo "2. Processor architecture"
echo "  uname -m: $(uname -m)   (arm64 = Apple Silicon)"
echo "  Chip: $(sysctl -n machdep.cpu.brand_string 2>/dev/null)"
mem_bytes=$(sysctl -n hw.memsize 2>/dev/null)
[ -n "$mem_bytes" ] && echo "  RAM: $((mem_bytes / 1073741824)) GB"
# Rosetta is optional; Unity Hub and Unity 6 run natively on Apple Silicon.
if /usr/bin/pgrep -q oahd 2>/dev/null; then echo "  Rosetta 2: installed"; else echo "  Rosetta 2: not detected (usually not required)"; fi
line

echo "3. Disk space (main drive)"
df -h / | sed 's/^/  /'
echo "  Recommended free space: at least 40 GB (Xcode ~15-20 GB, Unity + modules ~10-15 GB, project + builds ~5 GB)"
line

echo "4. Command Line Tools / Git"
if xcode-select -p >/dev/null 2>&1; then
  echo "  Developer directory: $(xcode-select -p)"
else
  echo "  [MISSING] Command Line Tools (xcode-select not configured)"
fi
if command -v git >/dev/null 2>&1 && git --version >/dev/null 2>&1; then
  echo "  Git: $(git --version)"
  echo "  git user.name:  $(git config --global user.name 2>/dev/null || echo '(not set)')"
  echo "  git user.email: $(git config --global user.email 2>/dev/null || echo '(not set)')"
  if git lfs version >/dev/null 2>&1; then echo "  Git LFS: $(git lfs version)"; else echo "  Git LFS: not installed (optional, useful later for large 3D assets)"; fi
else
  echo "  [MISSING] Git"
fi
line

echo "5. Xcode"
check_app "Xcode" "/Applications/Xcode.app"
if [ -d "/Applications/Xcode.app" ]; then
  /usr/bin/xcodebuild -version 2>/dev/null | sed 's/^/  /' || echo "  xcodebuild could not run (licence may not be accepted yet)"
  echo "  Installed iOS SDKs:"
  /usr/bin/xcodebuild -showsdks 2>/dev/null | grep -i iphoneos | sed 's/^/    /' || echo "    (unable to list)"
fi
line

echo "6. Unity Hub and Unity Editors"
check_app "Unity Hub" "/Applications/Unity Hub.app"
if [ -d "/Applications/Unity Hub.app" ]; then
  defaults read "/Applications/Unity Hub.app/Contents/Info" CFBundleShortVersionString 2>/dev/null | sed 's/^/  Unity Hub version: /'
fi
UNITY_DIR="/Applications/Unity/Hub/Editor"
if [ -d "$UNITY_DIR" ]; then
  for ed in "$UNITY_DIR"/*; do
    [ -d "$ed" ] || continue
    v=$(basename "$ed")
    echo "  Unity Editor: $v"
    pe="$ed/PlaybackEngines"
    [ -d "$pe/AndroidPlayer" ] && echo "    - Android Build Support: yes" || echo "    - Android Build Support: NO"
    [ -d "$pe/AndroidPlayer/SDK" ] && echo "    - Android SDK: yes" || echo "    - Android SDK: NO"
    [ -d "$pe/AndroidPlayer/NDK" ] && echo "    - Android NDK: yes" || echo "    - Android NDK: NO"
    [ -d "$pe/AndroidPlayer/OpenJDK" ] && echo "    - OpenJDK: yes" || echo "    - OpenJDK: NO"
    [ -d "$pe/iOSSupport" ] && echo "    - iOS Build Support: yes" || echo "    - iOS Build Support: NO"
  done
else
  echo "  No Unity Editors found in $UNITY_DIR"
fi
line

echo "7. Other Android tools (not required if Unity's built-in SDK is used)"
command -v adb >/dev/null 2>&1 && echo "  adb: $(command -v adb)" || echo "  adb: not on PATH (Unity's bundled copy can be used)"
check_app "Android Studio (optional)" "/Applications/Android Studio.app"
[ -n "$ANDROID_HOME" ] && echo "  ANDROID_HOME=$ANDROID_HOME"
command -v java >/dev/null 2>&1 && echo "  java on PATH: $(java -version 2>&1 | head -n1)" || echo "  java: not on PATH (Unity's bundled OpenJDK will be used)"
line

echo "8. Claude Code and other command-line tools"
command -v claude >/dev/null 2>&1 && echo "  Claude Code: $(claude --version 2>/dev/null)" || echo "  [MISSING] claude command not found on PATH"
command -v brew >/dev/null 2>&1 && echo "  Homebrew: $(brew --version 2>/dev/null | head -n1)" || echo "  Homebrew: not installed (not required)"
command -v python3 >/dev/null 2>&1 && echo "  python3: $(python3 --version 2>&1)"
command -v node >/dev/null 2>&1 && echo "  node: $(node --version)"
line

echo "9. Current folder"
echo "  $(pwd)"
line
echo "Inspection finished. Nothing was changed. Please paste this whole output back to Claude Code."
