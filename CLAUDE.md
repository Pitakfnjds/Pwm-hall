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
- **Exponential ramp-down:** Continuous decay every loop tick: `currentOutput *= RAMP_DOWN_DECAY^(dt/RAMP_DOWN_INTERVAL_MS)` (0.85 per 300 ms). 80%→10% in ~3.9 s, full→0 in ~5.4 s. Smooth (no discrete steps), reuses the existing `dt` from the slew rate limiter. Maximizes regenerative energy back into the battery.
- **PWM limiter:** Hard cap on `target` applied BEFORE slew rate limiter. Driver by `DEFAULT_MAX_PWM_PERCENT` (70%) or pot on A2 (30–100%) when `USE_PWM_LIMIT_POT` is defined. Prevents inefficient full-throttle (I²R losses grow with current²).
- **Ramp-up pot (A1):** When `USE_RAMPUP_POT` is defined, A1 sets ramp-up time 2s–4s with IIR-smoothed reading. Otherwise fixed `DEFAULT_RAMP_UP_TIME` (2000 ms).
- **Compile-time pot switches:** `USE_RAMPUP_POT` and `USE_PWM_LIMIT_POT` (both commented out by default) allow flashing the firmware without physical pots wired — falls back to DEFAULT constants. Toggle by uncommenting and recompiling.
- **LED scaling to limiter:** NeoPixel bar shows `currentOutput` scaled to `maxAllowedThrottle` (not to absolute 100%). At maximum reachable output the bar is full red even when limiter is below 100% — driver visually sees they are at the cap.
- **EEPROM calibration system:** First boot runs 5-second auto-calibration (user presses throttle min/max). Subsequent boots load from EEPROM. Force recalibration by holding switch in position 2 at startup.
- **Dead zones:** 5% low, 95% high to prevent jitter.
- **3-position rocker switch:** Forward (D2 LOW) / Neutral (both HIGH) / Reverse (D3 LOW). Physical rocker: always transitions through Neutral (1↔0↔2).
- **Anti-plugging protection:** Direction change blocked while `currentOutput > 0`. Slew rate limiter ensures gradual stop before direction change is allowed. LED blinks orange during braking.
- **Safety layers:** HW pull-downs on PWM line, FW validation of calibration data, out-of-range detection, neutral-cuts-motor, anti-plugging, slew rate limiter, PWM=0 at boot. Cytron MD30C adds HW overcurrent protection and regenerative braking.
- **Hall sensor reading:** 10-sample average on A0 (10-bit ADC).

## Pin Assignments (Arduino Nano)

| Pin | Function |
|-----|----------|
| A0 | Hall sensor analog input |
| A1 | Potentiometer: ramp-up time (2s–4s), only when `USE_RAMPUP_POT` is defined |
| A2 | Potentiometer: PWM limiter (30%–100%), only when `USE_PWM_LIMIT_POT` is defined |
| D5 | PWM output to motor driver (with 2kΩ pull-down) |
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
