# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**MagLev Throttle** – a non-contact electronic throttle controller for an electric go-kart. Uses a Hall effect sensor (SS49E) between two neodymium magnets to control motor speed via PWM. Educational project for students aged 16+, written in Slovak.

## Architecture

Modular 3-PCB system connected by JST-XH cables:

- **PCB1 (Sensor Board)** – Hall effect sensor inside the throttle button. KiCad 9 project in `pcbs/PCB1_SensorBoard/`.
- **PCB2 (Main Control Board)** – Arduino Nano running the firmware. Reads sensor, drives motor via Cytron MD30C motor driver (PWM + DIR), shows status on NeoPixel LED strip.
- **PCB3 (LED Board)** – WS2812B NeoPixel strip (8 LEDs) for visual throttle feedback.

## Key Files

| Path | Description |
|------|-------------|
| `hall_throttle_tzw/hall_throttle_tzw.ino` | Main firmware (Arduino Nano) |
| `MAGLEV_THROTTLE_PROJECT.md` | Complete technical documentation / knowledge base |
| `PCB2_ZAPOJENIE_SPECIFIKACIA.md` | Authoritative PCB2 wiring spec: pinout, BOM, RC values, reasoning |
| `FIRMWARE_ZMENY_INSTRUKCIE.md` | **Historical** original spec for the efficiency changes (pot switches, PWM limiter, exp. ramp-down). Already IMPLEMENTED on `feature/firmware-efficiency`; details there are outdated (pot pins, decay, ramp-up range) — this file and the firmware are authoritative. |
| `pcbs/PCB1_SensorBoard/` | KiCad 9 PCB project for sensor board |
| `easyeda-pcbs/` | Alternative PCB designs (EasyEDA format) |
| `throttle_v8_2_*.scad` / `*.stl` | 3D mechanical parts (OpenSCAD) |
| `formula/` | Formula button CAD models and reference images |

## Build & Upload (Firmware)

No automated build system – this is Arduino firmware.

1. Open `hall_throttle_tzw/hall_throttle_tzw.ino` in Arduino IDE
2. Board: **Arduino Nano**, Bootloader: **ATmega328P (Old Bootloader)**
3. Upload

**Arduino library dependencies:** `Adafruit_NeoPixel`, `EEPROM` (built-in)

## Firmware Key Concepts

- **Motor driver:** Cytron MD30C, sign-magnitude mode (PWM + DIR). PWM 0–100% (0–255).
- **Slew rate limiter:** Linear ramp-up (configurable via `RAMP_UP_TIME`, default 2s) and exponential ramp-down. Applies to all transitions including STOP.
- **Exponential ramp-down:** Continuous decay every loop tick: `currentOutput *= RAMP_DOWN_DECAY^(dt/RAMP_DOWN_INTERVAL_MS)` (0.72 per 300 ms). 80%→10% in ~1.9 s, full→5% in ~2.7 s; below 2% the output snaps to exact 0 (~3.6 s from full) so the anti-plugging check sees a true zero. Smooth (no discrete steps), reuses the existing `dt` from the slew rate limiter. Maximizes regenerative energy back into the battery. (Was 0.85 → ~5.4 s; halved for a quicker stop.)
- **PWM limiter:** Hard cap on `target` applied BEFORE slew rate limiter. Driver by `DEFAULT_MAX_PWM_PERCENT` (70%) or pot on A1 (30–100%) when `USE_PWM_LIMIT_POT` is defined. Prevents inefficient full-throttle (I²R losses grow with current²). The serial loop line prints `LIM:xx%` (plus raw pot value when the pot is enabled).
- **Ramp-up pot (A2):** When `USE_RAMPUP_POT` is defined, A2 sets ramp-up time 2s–4s with IIR-smoothed reading. Otherwise fixed `DEFAULT_RAMP_UP_TIME` (2000 ms).
- **Inverted throttle (`INVERT_THROTTLE`, defined by default):** Magnet orientation is such that the button at rest gives HIGH ADC and pressed gives LOW. Mapping is `map(raw, cal_max, cal_min, 0, 100)`. Comment out the define for the original orientation. Calibration still records raw min/max, so it is orientation-agnostic.
- **Stable pot reads (`analogReadStable`):** Both pots are read via a throwaway `analogRead`, 50 µs settle, then a 4-sample average. Without this the ATmega328P sample-and-hold capacitor carries charge over from A0 (Hall, ~800) into the pot channel and produces spikes to 1023.
- **Compile-time pot switches:** `USE_RAMPUP_POT` and `USE_PWM_LIMIT_POT` (both commented out by default) allow flashing the firmware without physical pots wired — falls back to DEFAULT constants. Toggle by uncommenting and recompiling.
- **LED scaling to limiter:** NeoPixel bar shows `currentOutput` scaled to `maxAllowedThrottle` (not to absolute 100%). At maximum reachable output the bar is full red even when limiter is below 100% — driver visually sees they are at the cap.
- **EEPROM calibration system:** First boot runs 5-second auto-calibration (user presses throttle min/max). Subsequent boots load from EEPROM and verify a 1-byte XOR checksum over `cal_min`/`cal_max` (rejects bit-flips and stray-magic-byte coincidences from foreign firmware → triggers re-cal). Force recalibration by holding switch in position 2 at startup; the old EEPROM cal is kept until the new cal completes (atomic overwrite via `eepromSaveCalibration`).
- **Dead zones:** 5% low, 95% high to prevent jitter.
- **3-position rocker switch:** Forward (D2 LOW) / Neutral (both HIGH) / Reverse (D3 LOW). Physical rocker: always transitions through Neutral (1↔0↔2).
- **Anti-plugging protection:** Direction change blocked while `currentOutput > 0`. Slew rate limiter ensures gradual stop before direction change is allowed. LED blinks orange during braking.
- **HW safety net (PCB2):** PWM line (D5) has 2× 10kΩ pull-down (R4‖R5, redundant for fail-open); Hall input (A0) has 10kΩ pull-down (R6) plus 100nF RC filter (C5, 159 Hz cutoff). A disconnected Hall cable pulls A0 to GND (raw≈0), preventing a floating-input false-throttle. See `PCB2_ZAPOJENIE_SPECIFIKACIA.md` for the full schematic.
- **Safety layers:** HW pull-downs on PWM and Hall lines, FW validation of calibration data, out-of-range detection, neutral-cuts-motor, anti-plugging, slew rate limiter, PWM=0 at boot, Hall sensor fault detection, watchdog timer. Cytron MD30C adds HW overcurrent protection and regenerative braking.
- **Hall sensor fault detection:** Each loop, raw ADC is checked against `[cal_min - HALL_MARGIN, cal_max + HALL_MARGIN]` and against absolute rails `[5, 1018]`. On first violation, `sensorFault` latches: throttle is forced to 0, state shows `SENS`, NeoPixel pulses red (200 ms blink). The latch only clears on reboot. Combined with R6 pull-down on A0, this catches: wire disconnect (raw≈0), short to GND, short to +5V, and sensor output stuck at either rail. Does NOT catch sensor output stuck inside the calibrated band (a true sensor internal failure — would require redundant sensing).
- **Watchdog timer:** `wdt_disable()` runs as the very first line of `setup()` (mandatory for Old Bootloader to avoid reset loop), then `wdt_enable(WDTO_2S)` at end of `setup()`. `wdt_reset()` fires at the top of every `loop()` iteration, and inside long blocking animations (calibration retry / success blinks). On a firmware hang, the MCU resets within ~2 s; PWM=0 is reasserted at the start of `setup()` and the HW pull-down (R4‖R5) holds D5 low during the brief reset window. `WDTO_2S` (not 1S) is chosen so the WDT timeout safely exceeds the Old Bootloader's ~1 s wait, preventing race-condition reset loops.
- **Hall sensor reading:** 10-sample average on A0 (10-bit ADC).

## Pin Assignments (Arduino Nano)

| Pin | Function |
|-----|----------|
| A0 | Hall sensor analog input |
| A1 | Potentiometer: PWM limiter (30%–100%), only when `USE_PWM_LIMIT_POT` is defined |
| A2 | Potentiometer: ramp-up time (2s–4s), only when `USE_RAMPUP_POT` is defined |
| D5 | PWM output to motor driver (HW: R4‖R5 = 2× 10kΩ pull-down on PCB2) |
| D4 | DIR output to motor driver |
| D2 | Switch forward position (INPUT_PULLUP, active LOW) |
| D3 | Switch reverse position (INPUT_PULLUP, active LOW) |
| D9 | NeoPixel data |
| D13 | Built-in LED status |

## Design Tools

- **KiCad 9** – primary PCB EDA (open-source)
- **EasyEDA** – alternative PCB designs
- **OpenSCAD** – 3D mechanical parts
- **Arduino IDE** – firmware development

## Language

All documentation and code comments are in **Slovak**. Maintain this convention.
