# Weiks CS2 Autoaccept

Native Windows tool for Counter-Strike 2 with two separated modules: auto-accept and derank macro.

## Key Features

- Auto-clicks the CS2 `ACCEPT` button.
- Pauses all automation for 2 minutes after a successful accept click.
- Optional CS2 focus when CS2 plays audio in the background.
- Derank macro that opens the CS2 console, sends `disconnect`, closes the console, and clicks reconnect in a loop.
- Global macro control: `F8` starts/stops, `F9` stops instantly.
- `FORCE ACCEPT PAUSE` manually blocks all automation for 2 minutes.
- `GER` / `ENG` language switch.

## Auto-Accept

- Enable or disable accept scanning with `Auto-click ACCEPT`.
- Enable or disable automatic CS2 focusing with `Focus CS2 when audio plays`.
- `Scan interval (ms)` controls how often the accept button is searched.
- After an accept click, the app pauses every automation feature for 2 minutes.
- `FORCE ACCEPT PAUSE` starts the same 2-minute pause manually.

## Derank Macro

- Press `F8` while CS2 is the active window to start or stop the macro.
- Press `F9` to stop the macro immediately.
- The macro target is the window that was active when `F8` was pressed.
- Default console hotkey: `^` / `SC029`.
- Default macro delay: `7900 ms`.
- After 14 disconnect cycles, the macro stops reconnecting and clicks the lobby return buttons once each.
- Macro loop:
  1. Focus CS2.
  2. Open console.
  3. Send `disconnect`.
  4. Press `Enter`.
  5. Close console.
  6. Click reconnect at `1500,66` three times.
  7. Wait the configured delay and repeat.
- Final cycle:
  1. After the 14th `disconnect`, close the console.
  2. Click lobby return point 1 once.
  3. Click lobby return point 2 once.
  4. Stop the macro.

## Settings

Settings are saved next to the executable in `WeiksCS2Autoaccept.ini`.

- `Console hotkey`: CS2 console key, default `^`.
- `Macro delay (ms)`: delay between macro cycles, default `7900`.
- `Scan interval (ms)`: accept scan speed, default `700`.
- `GER` / `ENG`: UI language.

## Build

```bat
build-cs2-watcher.bat
```

No external downloads are required. If CS2 runs as administrator, start this tool as administrator too.
