# Weiks CS2 Autoaccept

Native Windows tool for Counter-Strike 2 with two separated modules: auto-accept and derank macro.

## Key Features

- Auto-clicks the CS2 `ACCEPT` button.
- Pauses all automation for 2 minutes after a successful accept click.
- Optional CS2 focus when CS2 plays audio in the background.
- Derank macro converted from the new AutoHotkey script: sends the configured action key, then clicks reconnect in a loop.
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
- Default macro key: `O`.
- Default macro delay: `8000 ms`.
- Macro loop:
  1. Focus CS2.
  2. Send the macro key, default `O`.
  3. Wait `500 ms`.
  4. Click reconnect at `1500,66` three times.
  5. Wait the configured delay and repeat.

## Settings

Settings are saved next to the executable in `WeiksCS2Autoaccept.ini`.

- `Macro key`: action key sent before reconnect, default `O`.
- `Macro delay (ms)`: delay between macro cycles, default `8000`.
- `Scan interval (ms)`: accept scan speed, default `700`.
- `GER` / `ENG`: UI language.

## Build

```bat
build-cs2-watcher.bat
```

No external downloads are required. If CS2 runs as administrator, start this tool as administrator too.
