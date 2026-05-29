@echo off
setlocal

set "PROJECT_DIR=%~dp0"
set "ASI_PATH=%~1"
if "%ASI_PATH%"=="" set "ASI_PATH=%PROJECT_DIR%bin\AC5Tools.asi"

set "RELEASE_DIR=%PROJECT_DIR%release"
set "RELEASE_SCRIPTS=%RELEASE_DIR%\scripts"
set "ASI_LOADER=%PROJECT_DIR%release_assets\dinput8.dll"

if not exist "%ASI_PATH%" (
 echo Error: AC5Tools ASI not found at "%ASI_PATH%".
 exit /b 1
)

if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_SCRIPTS%"
copy /y "%ASI_PATH%" "%RELEASE_SCRIPTS%\AC5Tools.asi" >nul
copy /y "%PROJECT_DIR%AC5Tools.ini" "%RELEASE_SCRIPTS%\AC5Tools.ini" >nul

(
 echo AC5Tools v1.00
 echo.
 echo Standalone x64 ASI plugin for Assassin's Creed Rogue ACC.exe.
 echo Supported ACC.exe size: 67,873,496 bytes
 echo Supported ACC.exe SHA256: E870D89433297F978DFEB9903F37C0BB1553F35D2E7D4C7847E8C40AB6B77A84
 echo.
 echo Copy these files into the Assassin's Creed Rogue game folder:
 echo - dinput8.dll
 echo - scripts\AC5Tools.asi
 echo - scripts\AC5Tools.ini
 echo.
 echo Open/close menu: configured in scripts\AC5Tools.ini under [Hotkeys] MenuOpen. Default is B.
 echo Logging is off by default. Set [Logging] EnableConsole or EnableFile to 1 in scripts\AC5Tools.ini to enable diagnostics.
 echo.
 echo Current tabs:
 echo - Ship: God Mode, Ally Godmode, Stealth Mode, Instant Reload, and Harpoon Godmode.
 echo - Player: God Mode, Stealth Mode, No Reload, Lock Consumables, Unlimited Resources, Unlimited Selling, No Fall Damage, Desynchronize Yourself, and Inventory Add/Set.
 echo - Animus Hacks: save patch, Complete All Challenges, Unlock All In-Game Cheats, and Allow Saving.
 echo - Game: Player Super Speed, Player Super Jump, Noclip with speed controls, and Freeze Mission Timer.
 echo - Unlocks: Install Global Unlocks button and queued single unlock checkboxes.
 echo - Hotkeys: configurable menu and feature hotkeys.
 echo - System: input/window-lock toggles, logging status, process info, executable fingerprint, and patch-safety diagnostics.
 echo.
 echo Patch safety:
 echo - Ship God Mode, Ally Godmode, Ship Stealth Mode, Ship Instant Reload, Harpoon Godmode, Stealth Mode, Player God Mode, Desynchronize Yourself, No Reload, Lock Consumables, Inventory Add/Set, Unlimited Resources, Unlimited Selling, No Fall Damage, Animus Hacks, Player Super Speed, Player Super Jump, Noclip, Freeze Mission Timer, and Unlocks use guarded Rogue signatures/functions.
 echo - Inventory Add/Set queues edits until the matching in-game item changes, then auto-clears the queued row.
 echo - Use Install Global Unlocks at the main menu before loading a save; it stays active until the game exits.
 echo - Missing, non-unique, already-hooked, or mismatched targets are refused cleanly.
 echo - Console warnings are yellow and errors are red.
) > "%RELEASE_DIR%\README.txt"

if exist "%ASI_LOADER%" (
 copy /y "%ASI_LOADER%" "%RELEASE_DIR%\dinput8.dll" >nul
) else (
 echo Warning: ASI Loader not found at "%ASI_LOADER%".
)
