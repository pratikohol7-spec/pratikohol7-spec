#!/bin/bash
# WAR OF CHESS — creates the agreed Assets/ folder structure inside the Unity project.
#
# Run this ONCE, after Unity Hub has created the Unity project at WarOfChess/WarOfChess.
# It only creates empty folders (never deletes or overwrites anything).
#
# How to run (from the WarOfChess folder, in Terminal):
#   bash tools/create_asset_folders.sh

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT="$SCRIPT_DIR/../WarOfChess"

if [ ! -f "$PROJECT/ProjectSettings/ProjectVersion.txt" ]; then
  echo "Unity project not found at: $PROJECT"
  echo "Create it first in Unity Hub (see DEVELOPMENT_PROGRESS.md, step 'Create the Unity project')."
  exit 1
fi

folders=(
  Art/Characters Art/Environments Art/Materials Art/Textures Art/Effects
  Animations
  Audio/Music Audio/SFX
  Prefabs/Characters Prefabs/Board Prefabs/UI Prefabs/Effects
  Scenes
  Scripts/Core Scripts/Chess Scripts/Characters Scripts/Combat Scripts/AI
  Scripts/UI Scripts/Audio Scripts/Camera Scripts/Platform
  Settings
  UI
  Resources
)

for f in "${folders[@]}"; do
  mkdir -p "$PROJECT/Assets/$f"
  # Git does not store empty folders; a .gitkeep file keeps them in version control.
  [ -n "$(ls -A "$PROJECT/Assets/$f")" ] || touch "$PROJECT/Assets/$f/.gitkeep"
done

echo "Done. Folders created under $PROJECT/Assets"
echo "Switch back to Unity — it will import the new folders automatically."
