# Toolbox

A digital Swiss Army knife for [Tab5 OS](https://github.com/tab5os/tab5os) on the M5Stack Tab5: one app with 38 small tools for converting, calculating and measuring. Each opens on its own page with a back arrow.

| Category | Tools |
| --- | --- |
| Convert | Length, mass, temperature, volume, area, speed, pressure, energy, power (with dBm), data size, time, angle, fuel economy, cooking measures |
| Numbers | Number bases (decimal, hex, octal, binary, 8 to 64 bits, signed view, tappable bits), percentage (four kinds), ratio and aspect ratio, random numbers, dice and coins, passwords |
| Money | Tip and bill split, discount (with a second discount and tax), unit price comparison |
| Electronics | Ohm's law (any two of V, I, R, P), resistor colour codes (4, 5 and 6 bands, both ways), LED series resistor, voltage divider (any three of four), battery life |
| Date & time | Days between dates (with weekdays), add or subtract days, day of the week and ISO week, age and next birthday, stopwatch with laps, countdown timer with an alarm |
| Colour | HEX, RGB, HSL and HSV with a swatch, sliders and WCAG contrast |
| Measure | On-screen ruler (cm and inches), spirit level and protractor from the accelerometer, sound level meter from the microphone |

Numbers are typed on the app's own keypad, beside the fields in landscape and under them in portrait. Results update as you type. The app rotates with the device.

Toolbox remembers what you were doing: the tool on screen when you left, every tool's inputs, the tools you used last and your last eight conversions (under **Recent** on the home screen; tap one to open it again).

## Measuring with the Tab5

- **Ruler.** It runs full screen so the scale starts at the edge of the display: millimetres along one long edge, sixteenths of an inch along the other. The scale is 11.564 px/mm. That is worked out from the panel's published size (5.0 inch diagonal, 720 × 1280), because M5Stack doesn't publish the module's active area. It fits the 128 × 80 mm case with about 8.6 mm of bezel on each side. **Calibrate** shows a bank-card outline (85.60 × 53.98 mm, ISO/IEC 7810 ID-1). Lay a card on it and nudge the scale until they match. The result is saved.
- **Level and protractor.** These use the BMI270 accelerometer through the same axes the OS uses for auto-rotate, so readings follow the screen when it turns. Lying flat, the level shows a bubble and the left-right and front-back tilt. On an edge, it shows how far the edge is from level. **Calibrate** saves a zero for a surface you know is level. The protractor measures the angle of the Tab5's edge, from level or from a zero you set, and **Hold** freezes the reading.
- **Sound level.** This needs the microphone. Toolbox asks for it when you open the meter, and every other tool works without it. Readings are unweighted RMS levels over 100 ms, shown with the max, the energy average (Leq) and the min, and a 12-second graph. The microphone isn't calibrated: dB SPL is estimated from a typical MEMS sensitivity (−26 dBFS at 94 dB SPL), and the offset can be set against a reference meter.

## Layout

| Path | What |
| --- | --- |
| `engine/` | The maths, in plain C with no UI: number formatting and keypad entry, unit tables, number and money tools, electronics, dates and timers, colour, tilt and sound level |
| `tests/test_engine.c` | Host tests for the engine (`tests/run.sh`) |
| `main.c` | The app: tool list, home screen, opening and closing tools, saved settings |
| `ui.c` | Shared UI: fonts, buttons, chips, and the form, a column of fields with the number keypad |
| `tools_*.c` | The tools, by category |
| `fonts/` | Ω, −, ≈, ≤ and ≥ in Roboto at the OS's sizes, which the OS fonts lack. Every other glyph comes from the OS fonts. Made by `tools/gen_fonts.py` |
| `icon.png` | The launcher icon, drawn by `tools/make_icon.py` |
| `tests/t5do.py`, `tests/shots.py` | Device helpers: install over USB-C, console commands, screenshots over Wi-Fi, and screenshots of every tool |

## Build

You need ESP-IDF v6.1 (or just its RISC-V toolchain), CMake 3.22+, Ninja, Python 3.11+ and curl.

```sh
./build.sh                    # build/com.t5.example.toolbox.t5app
tests/run.sh                  # the engine tests, with the computer's C compiler
```

The first build downloads the Tab5 OS SDK named in `tab5os-sdk.txt` into `.t5sdk/`, and checks it against the release's `SHA256SUMS`. If `riscv32-esp-elf-gcc` isn't on your `PATH` and `IDF_PATH` is set, the script sources ESP-IDF's `export.sh` itself.

## Install

Copy the `.t5app` to the SD card and tap it in Files. Or, with the Tab5 on USB-C:

```sh
python3 -m venv .t5sdk/venv && .t5sdk/venv/bin/pip install pyserial pillow
.t5sdk/venv/bin/python3 tests/t5do.py install
.t5sdk/venv/bin/python3 tests/t5do.py cmd "app run com.t5.example.toolbox"
```

A tool's id as the launch argument opens that tool, for example `app run com.t5.example.toolbox measure.level`. Use `home` for the home screen.

## Device tests

`tests/shots.py HOST [--install] [--rot 90] [tool ids]` opens each tool by launch argument and saves a screenshot to `build/shots/`. It doesn't inject any touches. It needs the Tab5 on USB-C for the console, and in developer mode on Wi-Fi, paired with `t5 pair`, for the screenshots. It keeps the screen on during the run and puts the rotation back afterwards. `app stress com.t5.example.toolbox 10` on the console checks that memory goes back to baseline.

## CI and releases

GitHub Actions runs the engine tests and builds the package on every push and pull request. To publish a release, set `version` in `manifest.toml`, commit, and push a matching tag (`v1.0.0`). The release workflow attaches the `.t5app`.

## License

MIT No Attribution ([LICENSE](LICENSE)). The fonts in `fonts/` are generated from Roboto, under the Apache License 2.0.
