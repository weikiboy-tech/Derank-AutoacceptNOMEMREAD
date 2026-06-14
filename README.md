# Weiks CS2 Autoaccept

A small native Windows tool for Counter-Strike 2 that can auto-click the match `ACCEPT` button and run a disconnect/reconnect macro.

## Features

- Auto-detects the CS2 `ACCEPT` button and clicks it.
- Pauses all automation for 2 minutes after a successful accept click.
- Brings CS2 to the foreground when CS2 plays audio while it is not active.
- Includes a native C++ disconnect/reconnect macro converted from the AutoHotkey script.
- Global `F8` / `F9` macro control through a Windows low-level keyboard hook.
- Language toggle in the app: `GER` / `ENG`.
- Clean separated UI sections for auto-accept and the derank macro.
- `FORCE ACCEPT PAUSE` button to manually start the 2-minute automation pause.
- Saves settings in `WeiksCS2Autoaccept.ini` next to the executable.

## Controls

- `F8`: start or stop the disconnect/reconnect macro.
- `F9`: immediately stop the macro.
- `GER` / `ENG`: switch the app language.
- `FORCE ACCEPT PAUSE`: immediately pause all automation for 2 minutes.
- `ACCEPT automatisch klicken` / `Auto-click ACCEPT`: enable or disable auto-accept.
- `CS2 bei Audio automatisch fokussieren` / `Focus CS2 when audio plays`: enable or disable audio focus.
- `Disconnect/Reconnect-Makro` / `Disconnect/reconnect macro`: enable or disable the macro from the UI.

## Macro Defaults

- Console hotkey: `^`
- Internal key sent for `^`: `SC029`
- Delay between macro cycles: `7900 ms`
- Reconnect click position: `1500,66`
- Reconnect click count: `3`

The macro performs:

1. Focus the target window selected when `F8` is pressed.
2. Open the console with the configured console hotkey.
3. Type `disconnect`.
4. Press `Enter`.
5. Close the console.
6. Click reconnect at `1500,66` three times.
7. Wait the configured delay before repeating.

## Build

Run:

```bat
build-cs2-watcher.bat
```

This builds:

```text
Weiks CS2 Autoaccept.exe
```

No external dependencies or downloads are required. The build uses the local Visual Studio C++ toolchain.

## Important Notes

- If CS2 runs as administrator, start `Weiks CS2 Autoaccept.exe` as administrator too. Otherwise Windows may block keyboard hooks or input into the game.
- Press `F8` while CS2 is the active foreground window so the macro uses the correct target window.
- The 2-minute pause after an accept click disables all automation during that time: no sound detection, no accept scan, no clicking, no CS2 focus, and no macro cycle.
