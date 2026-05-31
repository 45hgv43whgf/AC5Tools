# AC5Tools

AC5Tools is a standalone x64 ASI plugin for **Assassin's Creed Rogue**.

It adds an in-game DX11 ImGui menu with ship, player, inventory, unlock, Animus Hacks, noclip, movement, input/system, and hotkey options. The plugin is intended to be loaded through Ultimate ASI Loader.

Current version: **v1.01**

## Game Version

AC5Tools was made and tested for the Steam/Uplay single-player executable:

```text
ACC.exe
Size: 67,873,496 bytes
SHA256: E870D89433297F978DFEB9903F37C0BB1553F35D2E7D4C7847E8C40AB6B77A84
```

Other game builds may use different code addresses or bytes. If a supported patch check does not match, or another tool already owns that address, AC5Tools refuses that feature instead of patching unknown code. For byte mismatches, it logs the expected and found bytes. Startup diagnostics show the supported executable size and SHA256, the detected executable size, timestamp, and SHA256, and warn when the detected SHA256 does not match the supported build above.

## Features

### Ship

- God Mode
- Ally Godmode
- Stealth Mode
- Instant Reload: zeros broadside, heavy shot, mortar, and front carronade reload timers while enabled
- Harpoon Godmode

Ship and friendly-ship health refill may not visually update immediately while God Mode or Ally Godmode is active.

### Player

- God Mode
- Stealth Mode
- No Reload
- Lock Consumables
- Unlimited Resources
- Unlimited Selling
- No Fall Damage
- Allow Eagle Vision while sprinting
- Infinite Breath
- Kill civilians without desynchronization: prevents civilian kills from adding desync progress and suppresses the related warning text and screen effect while enabled
- Desynchronize Yourself: one-shot native desync action; briefly bypasses Player God Mode if it is enabled
- Inventory Add/Set: queued one-time edits for money, player items, ship supplies, and cargo

### Inventory Add/Set

Queued inventory edits apply on the next matching in-game inventory change, then auto-clear. They are triggered by spending, using, or buying the matching resource. Store resources/cargo and money can also trigger from buy/sell updates. Gaining or looting resources usually will not trigger queued edits. Avoid queued harpoon edits during sea animal hunt transitions.

Supported entries:

- Player: Money, Smoke Bombs, Bullets, Sleep Darts, Berserk Darts, Firecracker Darts, Shrapnel Grenades, Sleep Grenades, Berserk Grenades, Rope Darts, Throwing Knives
- Ship: Crew, Fire Barrels, Heavy Shot, Mortar, Harpoon
- Cargo: Metal, Wood, Stone, Cloth, Tobacco

Player and ship supplies are clamped to `0-99`; cargo is clamped to `0-9999`; money is clamped to `0-9999999`.

### Game

- Player Super Speed with configurable multiplier
- Time Scale with configurable multiplier
- Player Super Jump with configurable distance and height. When enabled, you may need to double tap Space to trigger the enhanced jump.
- Noclip through the native engine ghost path, with configurable speed and Shift boost speed
- Freeze Mission Timer

### Animus Hacks

- Enable Animus Hacks Save Patch
- Complete All Challenges
- Unlock All In-Game Cheats
- Allow Saving toggle

The save patch and challenge hook are session-only patches. Return to the main menu when the UI tells you to do so for changes that need the game to save its own state.

### Unlocks

The Unlocks tab can temporarily unlock selected loaded records. If the game saves afterward, these unlocks can become permanent in that save.

Back up your save before using Unlocks. Saved unlock changes may be irreversible.

Unlocks can irreversibly affect progression if an unlocked item was supposed to be granted later by a mission, contract, challenge, or other progression event.

- Install Global Unlocks: broad all-at-once unlock handling. Use it from the main menu before loading a save; it stays active until the game exits.
- Single unlock checkboxes: queue individual unlocks by category. Queued entries scan in the background and auto-clear after a successful apply.
- Categories: Swords, Pistols, Outfits, Ship Appearance, Blueprints, Extras

### Tools

- Optional mouse lock to keep the Windows cursor inside the game window while the game is focused and the menu is closed
- Optional mouse input blocking while the AC5Tools UI is open
- Optional keyboard input blocking while the AC5Tools UI is open
- Optional AC5Tools hotkey blocking while the AC5Tools UI is open
- Saved menu window position and size after dragging/resizing
- Configurable hotkeys for the menu and toggleable features
- In-game UI opened with the configured menu hotkey, `B` by default
- Optional console and file logging for patch status and diagnostics
- Console warnings are yellow and errors are red
- DX11 swap-chain resize/reset handling for overlay compatibility

## Notes

`Lock Consumables` prevents guarded consume amounts from lowering supported cargo, money, ammunition, and items while enabled. It does not refill values upward.

`Inventory Add/Set` can be queued before the inventory pointer is ready. Load into gameplay as the player, then spend/use/buy the matching item to trigger the queued edit. For store resources/cargo and money, buying or selling at a store can trigger the queued edit.

`Noclip` uses the game's native ghost movement path. Speed and Shift boost speed are restored to the game's original values when Noclip is disabled.

Most gameplay feature on/off states are session-only and are not saved. The INI saves numeric settings, hotkey bindings, System-tab options, and the menu window position/size. Unlock changes are different: if the game saves after an unlock is applied, that unlock can persist in the save.

## Release Layout

Building the project creates a ready-to-copy folder:

```text
release/
  dinput8.dll
  README.txt
  scripts/
    AC5Tools.asi
    AC5Tools.ini
```

Copy the contents of `release/` into the Rogue game folder, next to `ACC.exe`.

## Building

### Visual Studio

Open:

```text
AC5Tools.sln
```

Build `Release|x64`. The project targets Visual Studio 2022 toolset `v143`.

The build output is written to:

```text
bin/AC5Tools.asi
```

The post-build step refreshes the local `release/` folder. It does not copy anything into your game directory.

### Command Line

Run:

```bat
build.bat
```

The script locates Visual Studio C++ tools through `vswhere`, builds the x64 ASI, and refreshes `release/`.

## Optional Install Script

To install from the prepared release folder:

```powershell
.\install_to_game.ps1 -GameDir "C:\Path\To\Assassin's Creed Rogue"
```

The install script requires a game path argument. No local game path is hardcoded.

When an existing game-folder INI is present, the install script updates the INI schema while keeping current values for every option that still exists in the new default config. New options are added with default values, removed options are dropped, and a timestamped `.bak-*` backup is written before the INI is replaced. If `dinput8.dll` is locked but already installed, the script keeps the existing loader and continues. The INI is not rewritten if the ASI copy fails.

## Configuration

Default config:

```ini
[Logging]
EnableConsole = 0
EnableFile = 0

[System]
LockMouseToWindow = 0
DisableMouseInputWhenUiOpen = 0
DisableKeyboardInputWhenUiOpen = 0
DisableHotkeysWhileUiOpen = 0

[UI]
WindowPosX = 60
WindowPosY = 40
WindowSizeX = 680
WindowSizeY = 420

[Game]
PlayerSuperSpeed = 2.750000
PlayerSuperJumpDistance = 8.000000
PlayerSuperJumpHeight = -3.000000

[Noclip]
Speed = 1.000000
BoostSpeed = 5.000000

[Hotkeys]
MenuOpen = 66
ShipGodmode = 0
NoCannonCooldown = 0
AllyGodmode = 0
ShipStealthMode = 0
PlayerGodmode = 0
DesynchronizeYourself = 0
StealthMode = 0
NoReload = 0
NoFallDamage = 0
LockConsumables = 0
UnlimitedResources = 0
UnlimitedSelling = 0
HarpoonGodmode = 0
FreezeMissionTimer = 0
PlayerSuperSpeed = 0
PlayerSuperJump = 0
Noclip = 0
```

`EnableConsole = 1` opens a console window. `EnableFile = 1` writes timestamped lines to `AC5Tools.log` next to the ASI. When console logging is enabled, warnings are yellow and errors are red. Startup diagnostics print the detected `ACC.exe` size, timestamp, and SHA256; a SHA256 mismatch is a warning that the game executable may not be the fully supported build.

`LockMouseToWindow = 1` confines the Windows cursor to the Rogue window while the game is focused and the menu is closed. It releases automatically when Rogue loses focus, such as when you alt-tab.

`DisableMouseInputWhenUiOpen = 1` blocks mouse input from reaching the game while the AC5Tools UI is open. `DisableKeyboardInputWhenUiOpen = 1` does the same for keyboard input. `DisableHotkeysWhileUiOpen = 1` stops AC5Tools feature hotkeys from firing while the UI is open, while still letting the menu open/close hotkey work.

`WindowPosX`, `WindowPosY`, `WindowSizeX`, and `WindowSizeY` store the last menu position and size after you finish dragging or resizing the window.

Hotkey values are Win32 virtual-key codes. `MenuOpen = 66` is `B`. Use the in-game Hotkeys tab to set hotkeys instead of editing by hand.

## Third-Party Components

- Dear ImGui by Omar Cornut and contributors (v1.90.9)
- MinHook by Tsuda Kageyu (includes HDE disassembler code by Vyacheslav Patkov)
- Ultimate ASI Loader by ThirteenAG (9.7.1.0-2155f21)

## Compatibility

AC5Tools is designed to run standalone. Gameplay features use guarded signatures and expected-byte checks; if a target is missing, non-unique, already hooked, or no longer matches the supported bytes during installation, AC5Tools refuses that feature cleanly instead of patching unknown code. Overlay and input hooks still depend on the usual DX11, DirectInput, and Win32 hook compatibility with other loaded tools.
