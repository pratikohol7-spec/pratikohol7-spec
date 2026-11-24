# Installing PossessionV 2.0 on the GTA V iOS port (iPhone)

## 1. Back up first (2 minutes)

PossessionV doesn't touch your other mods or the game files. Still, make a copy first so you can always
go back:

1. Open the **Files** app and go to **On My iPhone › GTA V Port**.
2. Long-press the **Mods** folder, tap **Duplicate** (or **Compress** to make `Mods.zip`), and rename the
   copy to `Mods-backup-<date>`. This copy contains your 8 vehicle mods exactly as they are now.
3. Optional: take a screenshot of **Settings › Mods**, which shows which mods are on.

## 2. Remove the old PossessionV (if installed)

In the app, go to **Settings › Mods**. If **PossessionV** (1.0 or 1.1) is listed, swipe it and remove
it. Leave every other mod alone.

## 3. Install

1. Download `PossessionV-2.0.zip` to the iPhone (Safari saves it to Files › Downloads).
2. In the app, go to **Settings › Mods › Install a mod…** and pick `PossessionV-2.0.zip`.
3. The installer lists what it will install: a script mod with `PossessionV.asi` and `PossessionV.ini`.
   It should replace no game files and no vehicle files. If it shows anything else, cancel.
4. Close the game completely (swipe it away in the app switcher) and start it again.

## 4. First use

1. In game, a notification says "PossessionV loaded".
2. Tap **🧩 Mods** in the top row of the touch controls, then **☰ F7** to open the menu. Use the D-pad
   to move and Enter to select.
3. Optional, port v1.5+: one-tap buttons. Go to **Settings › Mods › PossessionV** and add an on-screen
   button for each key you want:

| Key | Action | Suggested button |
|---|---|---|
| F9 | Possess the marked ped | person icon, "always" |
| F11 | Return to original player | arrow-back icon, "always" |
| F5 | Next target | arrow icon |
| F8 | Vanilla Unicorn doors UNLOCK / LOCK | door/lock icon |
| F12 | Possession mode ON / OFF | toggle icon |
| F10 | Possess nearest animal | paw/star icon |

   Move them in **Edit controls layout** to a free spot, for example the top-left area under the Mods
   button, so they don't cover the stick, sprint or fire buttons.

## 5. Menu

```
Pedestrian Possession: ON/OFF
Possess target (marked ped)
Next target
Possess nearest animal
Return to Original Player
Vanilla Unicorn Doors: UNLOCKED / LOCKED (game)
Give all weapons + ammo / Add money / Restricted areas / Never wanted / Night
Close Menu
```

## 6. Restore / uninstall

- **Doors only:** choose **Vanilla Unicorn Doors** in the menu, or press F8, until it says LOCKED. Each
  door's original state is restored, and the club interior's original state comes back once you're
  outside. You can also set `UnlockDoors=0` in `PossessionV.ini` so the doors are never touched.
- **Remove the mod:** go to **Settings › Mods**, swipe PossessionV and remove it, then fully restart the
  game. Nothing else changes.
- **If anything is wrong:** in Files, delete the current **Mods** folder, rename `Mods-backup-<date>`
  back to **Mods**, and restart the game.
- **If the game stops while starting:** the port offers **Play without mods** on the next launch. Your
  mods stay installed. Then switch PossessionV off in Settings › Mods.

## 7. If something doesn't work

Send the log (**Settings › This device › Send log to the developer**, or share it with me). It lists
every mod file, and any game command the game doesn't have ("game command 0x… isn't in this game
version").
