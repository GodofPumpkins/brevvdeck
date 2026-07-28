# BrevvDeck Firmware — Developer Guide

This document explains every firmware file, how they fit together, and how to build, upload, and debug the firmware on a Teensy 4.1.

---

## Table of Contents

1. [Hardware Overview](#1-hardware-overview)
2. [File Map](#2-file-map)
3. [The Arduino Framework — Why There Is No `main()`](#3-the-arduino-framework)
4. [Build & Upload with PlatformIO](#4-build--upload-with-platformio)
5. [Serial Debug Logging](#5-serial-debug-logging)
6. [Boot Sequence](#6-boot-sequence)
7. [The Main Loop — Scan Timing](#7-the-main-loop--scan-timing)
8. [Module Reference](#8-module-reference)
   - [config.h](#81-configh)
   - [midi_map.h](#82-midi_maph)
   - [analog.h / analog.cpp](#83-analogh--analogcpp)
   - [buttons.h / buttons.cpp](#84-buttonsh--buttonscpp)
   - [encoders.h / encoders.cpp](#85-encodersh--encoderscpp)
   - [motors.h](#86-motorsh)
   - [main.cpp](#87-maincpp)
9. [MIDI Protocol](#9-midi-protocol)
10. [Pin Quick-Reference](#10-pin-quick-reference)

---

## 1. Hardware Overview

BrevvDeck is a 4-deck DJ controller. Its brain is a single **PJRC Teensy 4.1** (ARM Cortex-M7, 600 MHz) which:

- Reads all physical inputs (buttons, knobs, faders, encoders, joysticks)
- Sends USB MIDI messages to Mixxx running on a PC
- Receives USB MIDI messages from Mixxx (motor target positions, LED states)
- Controls 8 motorized faders via a PID loop
- Streams audio from two PCM5102A DACs over USB Audio (master mix + headphone cue)

The Teensy connects to the PC over a single USB-C cable. Windows/macOS/Linux see it as a **composite USB device** with three interfaces simultaneously: MIDI, Audio, and Serial (for debug).

---

## 2. File Map

| File | Purpose |
|------|---------|
| `main.cpp` | Entry point — `setup()` and `loop()`. Wires all modules together. |
| `config.h` | Every pin number, I2C address, timing constant, and tuning value. Change hardware here, nowhere else. |
| `midi_map.h` | All MIDI channel, note, and CC constants. Must stay in sync with `BrevvDeck.midi.xml` and `BrevvDeck-scripts.js`. |
| `analog.h/cpp` | Scans 3× CD74HC4067 MUX ICs (48 channels total). Converts knob/fader/joystick positions to 7-bit MIDI CC. |
| `buttons.h/cpp` | Reads a 10×10 button matrix via 2× MCP23017 I2C GPIO expanders. Sends MIDI Note On/Off. |
| `encoders.h/cpp` | Reads 9 rotary encoders using the PJRC Encoder library (interrupt-driven). Sends relative MIDI CC. |
| `motors.h` | Header for motorized fader control — PID loop, touch detection, ADS1115 position ADC, PCA9685 PWM driver. |
| `platformio.ini` | PlatformIO build configuration — board, USB mode, libraries, upload protocol. |

---

## 3. The Arduino Framework

Teensy uses the **Arduino / Teensyduino framework**. You do not write a `main()` function. Instead you write two functions, and the framework calls them:

```
Power on
    │
    └─► setup()    ← your code runs here once, at boot
            │
            └─► loop()    ← your code runs here, called forever in a tight loop
                    │
                    └─► loop()  ← called again immediately after it returns
                            │
                            └─► loop()  ...and so on, forever
```

Under the hood, Teensyduino generates a `main()` that looks roughly like this:

```cpp
int main() {
    setup();          // your setup()
    while (true) {
        loop();       // your loop(), called as fast as possible
        yield();      // Teensyduino internal: runs USB stack, Serial buffer, etc.
    }
}
```

The `yield()` call is important — it is where Teensyduino services the USB stack (flushes MIDI/audio/serial buffers) on every loop iteration. You get this for free without doing anything.

### What "as fast as possible" means

On a Teensy 4.1 at 600 MHz, an empty `loop()` can execute tens of millions of times per second. BrevvDeck's loop does real work, but it still runs at hundreds of thousands of iterations per second. The **timer-based dispatch** inside `loop()` (see §7) ensures each module only runs at its intended rate regardless of how fast the loop itself executes.

---

## 4. Build & Upload with PlatformIO

### One-time setup

1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install the **PlatformIO IDE** extension (search "PlatformIO" in the Extensions panel)
3. Install [Teensyduino](https://www.pjrc.com/teensy/td_download.html) — this provides the Teensy board support package and the Teensy Loader uploader
4. Open the `brevvdeck-firmware/` folder in VS Code — PlatformIO detects `platformio.ini` automatically and downloads the Teensy toolchain on first open

### Building

Click the **✓ Build** button in the PlatformIO toolbar at the bottom of VS Code, or run in a terminal:

```
pio run
```

### Uploading

1. Plug the Teensy 4.1 into a USB port
2. Click the **→ Upload** button in the PlatformIO toolbar, or run:
   ```
   pio run --target upload
   ```
3. The **Teensy Loader** app opens and flashes the board automatically over USB HID
4. If it does not flash within ~5 seconds, press the small button on the Teensy board once — this forces it into bootloader mode

### Switching between release and debug builds

`platformio.ini` has two `build_flags` sections; one is commented out:

```ini
; Release (default) — no serial output:
build_flags =
    -D USB_MIDI_AUDIO_SERIAL

; Debug — uncomment this and comment out the above:
; build_flags =
;     -D USB_MIDI_AUDIO_SERIAL
;     -D BREVVDECK_DEBUG
```

Change which block is active, then rebuild and upload.

---

## 5. Serial Debug Logging

When built with `-D BREVVDECK_DEBUG`, the `LOG()` macro in `main.cpp` writes timestamped messages to the USB serial port:

```
[   612 ms] I2C started at 400 kHz
[   614 ms] analogInit  done (3x MUX, 12-bit ADC)
[   616 ms] buttonsInit done (2x MCP23017)
[   618 ms] encodersInit done (9 encoders)
[   721 ms] motorsInit  done (8 faders, PID)
[   721 ms] Motor kill switch initial state: OFF
[   721 ms] Boot complete. Entering main loop.
[  1204 ms] Rate   deck=0  target=8192
```

### How to view it

- **PlatformIO Serial Monitor**: click the plug icon in the VS Code status bar (or run `pio device monitor`)
- **Arduino Serial Monitor**: Tools → Serial Monitor (Ctrl+Shift+M)
- **Any terminal on Windows**: `mode` to find the COM port, then use PuTTY or the built-in `Terminal` at 115200 baud

### The LOG() macro

```cpp
#ifdef BREVVDECK_DEBUG
  #define LOG(fmt, ...) Serial.printf("[%6lu ms] " fmt "\n", millis(), ##__VA_ARGS__)
#else
  #define LOG(fmt, ...)
#endif
```

When `BREVVDECK_DEBUG` is not defined, every `LOG(...)` call compiles to nothing — zero flash usage, zero CPU cost. Use `LOG()` freely throughout the codebase; it costs nothing in production.

---

## 6. Boot Sequence

```
Power on / USB enumeration
        │
        ▼
    setup()
        │
        ├─ Serial.begin()          (debug only — waits 600 ms for terminal)
        │
        ├─ Wire.begin()            Start I2C bus at 400 kHz
        │                          (MCP23017 buttons, ADS1115 motor ADC,
        │                           PCA9685 motor PWM all share this bus)
        │
        ├─ analogInit()            Configure MUX address pins as OUTPUT
        │                          Set ADC resolution to 12 bits
        │
        ├─ buttonsInit()           Configure MCP23017 Port A as column outputs
        │                          Configure MCP23017 Port B as row inputs with pull-ups
        │
        ├─ encodersInit()          Reset all encoder position counters to 0
        │                          (Pin interrupts configured by Encoder library constructor)
        │
        ├─ motorsInit()            Configure PCA9685 PWM frequency
        │                          Read ADS1115 for initial fader positions
        │                          Set all motor targets to current positions (no movement)
        │
        ├─ pinMode(PIN_MOTOR_KILL) Read initial kill switch state
        │                          Call motorsEnableKill() with initial state
        │
        ├─ usbMIDI.setHandleControlChange(onControlChange)
        │                          Register MIDI receive callback
        │
        └─ Reset all elapsedMicros timers to 0
                │
                ▼
            loop()  ← runs forever
```

---

## 7. The Main Loop — Scan Timing

Each module is polled at a different rate. Rather than using `delay()` (which would block everything), each module has an `elapsedMicros` timer:

```cpp
if (sinceAnalogScan >= ANALOG_SCAN_INTERVAL_US) {
    sinceAnalogScan = 0;   // reset the timer
    analogScan();
}
```

`elapsedMicros` is a Teensy-specific type that auto-increments using the CPU cycle counter — no interrupt, no overhead. Assigning `0` resets it.

### Scan rates

| Module | Interval | Rate | Reason |
|--------|----------|------|--------|
| `analogScan()` | 1000 µs | 1 kHz | Pots/faders change slowly; 1 kHz is more than enough, avoids flooding MIDI |
| `buttonsScan()` | 2000 µs | 500 Hz | I2C takes ~80 µs per scan; 500 Hz is imperceptibly fast for button presses |
| `encodersSendMidi()` | 1000 µs | 1 kHz | Encoders are ISR-driven; this just reads the cached delta counter |
| `motorsScan()` | 500 µs | 2 kHz | PID loop needs tight timing for smooth motor response; 2 kHz feels immediate |
| `usbMIDI.read()` | Every iteration | ~200 kHz | Drains USB receive buffer; the callback itself is tiny |
| `checkMotorKill()` | Every iteration | ~200 kHz | Reads one GPIO pin; debounce logic prevents false triggers |

### Why modules don't block each other

Because the timers are checked with `if (elapsed >= interval)` (not `while (elapsed < interval)`), a module that runs slightly over its budget simply fires on the next loop iteration. There is no priority inversion or starvation.

The worst-case loop iteration is dominated by `analogScan()` — 48 MUX channels × ~15 µs (switching + ADC settle + read) ≈ 720 µs. This is within the 1000 µs budget with 280 µs headroom.

---

## 8. Module Reference

### 8.1 `config.h`

Central configuration file. **All** pin numbers, I2C addresses, and timing constants live here. No other file should contain magic numbers for hardware.

**Key sections:**

| Section | What it defines |
|---------|----------------|
| I2C | `PIN_I2C_SDA` (18), `PIN_I2C_SCL` (19), device addresses |
| SPI | `PIN_SPI_SCK` (13), `PIN_SPI_MOSI` (11), CS pins |
| I2S / Audio | `PIN_I2S1_*`, `PIN_I2S2_*` — fixed by Teensy hardware |
| MUX | `PIN_MUX_S0-S3` (address lines 2,5,6,8), `PIN_MUX1/2/3_COM` (A0,A1,A2) |
| Encoders | `ENCODER_PINS[]` — struct array with `{pinA, pinB}` for all 9 encoders |
| Motor | `PIN_MOTOR_KILL` (37), PID tuning constants |
| Scan rates | `ANALOG_SCAN_INTERVAL_US`, `BUTTON_SCAN_INTERVAL_US`, etc. |

**Pin conflict verification** is documented in the file header — A4-A7 (pins 18-21) are used by I2C and I2S1 and are therefore never used as ADC inputs.

---

### 8.2 `midi_map.h`

Defines MIDI channel and CC/note constants as C++ `constexpr` values inside namespaces:

```
MidiCh::   — channel numbers (1-8)
DeckNote:: — note numbers for deck buttons (same on all 4 deck channels)
DeckCC::   — CC numbers for deck pots/faders/joystick
MixerNote::/ MixerCC:: — channel 5 (mixer strip)
FxNote:: / FxCC::      — channels 6-7 (FX units)
LibNote:: / LibCC::    — channel 8 (library/browse/samplers)
```

**Important:** These constants must stay in sync with three files:
- `midi_map.h` (this file)
- `BrevvDeck.midi.xml` (Mixxx XML mapping — static bindings)
- `BrevvDeck-scripts.js` (Mixxx JavaScript — dynamic processing)

If you add a new control, update all three.

---

### 8.3 `analog.h` / `analog.cpp`

**What it does:** Reads all 48 analog inputs (knobs, faders, joysticks) by multiplexing three CD74HC4067 ICs, converts to 7-bit MIDI CC, and sends on change.

**Hardware path:**

```
Potentiometer wiper
        │
        └── MUX channel input (one of 16 per MUX)
                │
                CD74HC4067  ← address lines S0-S3 select which channel is connected
                │
                COM output ──► Teensy ADC pin (A0, A1, or A2)
                                    │
                                    analogRead()  →  12-bit value (0-4095)
                                    │
                                    >> 5          →  7-bit CC value (0-127)
                                    │
                                    usbMIDI.sendControlChange()
```

**MUX channel layout:**

| MUX | ADC Pin | Channels |
|-----|---------|---------|
| #1 | A0 (pin 14) | Gains (4), EQ High/Mid/Low (12) |
| #2 | A1 (pin 15) | Quick FX (4), FX1/FX2 knobs (8), mixer pots (3), spare (1) |
| #3 | A2 (pin 16) | Volume faders (4), crossfader (1), scrub joysticks (4), spare (7) |

**Noise gate:** `CC_THRESHOLD` (default: 1 CC unit) prevents sending a message if the value barely moves. This stops pots from spamming MIDI when they are physically steady but electrically noisy.

**`analogInit()`** — called once in `setup()`. Configures MUX address pins as outputs, sets ADC resolution to 12 bits.

**`analogScan()`** — called from `loop()` at 1 kHz. Iterates all three MUXes, steps through all 16 channels on each, reads ADC, and fires `usbMIDI.sendControlChange()` only when the value changes.

---

### 8.4 `buttons.h` / `buttons.cpp`

**What it does:** Scans a 10×10 button matrix (100 buttons) using two MCP23017 GPIO expander ICs over I2C. Sends MIDI Note On / Note Off.

**Hardware path:**

```
MCP23017 Port A (outputs) ── columns 0-9
MCP23017 Port B (inputs, pull-up) ── rows 0-9

Each scan cycle:
  for each column:
    set column LOW (active), all others HIGH
    read all row inputs
    compare against previous state
    → state changed? send Note On (pressed) or Note Off (released)
```

**Button-to-MIDI mapping** uses a quadrant layout:

```
           Col 0-4     Col 5-9
Row 0-4 │  Deck 1  │  Deck 2  │
Row 5-9 │  Deck 3  │  Deck 4  │
```

Within each 5×5 deck quadrant, rows map to button groups: transport controls (Play/Cue/Sync), loop controls, hotcues, shift/PFL, FX assign/load.

**`buttonsInit()`** — configures MCP23017 registers: Port A all-output (columns), Port B all-input with pull-ups (rows).

**`buttonsScan()`** — called from `loop()` at 500 Hz. Reads both MCP23017s, compares against last known state, and sends MIDI Note On/Off for every changed button.

---

### 8.5 `encoders.h` / `encoders.cpp`

**What it does:** Reads 9 rotary encoders using the [PJRC Encoder library](https://www.pjrc.com/teensy/td_libs_Encoder.html), which uses hardware interrupts for accurate quadrature decoding. Sends relative MIDI CC messages.

**How quadrature encoding works:**

A rotary encoder has two contacts (A and B) that produce square waves 90° out of phase. The direction of rotation determines which edge leads:

```
Clockwise:     A: ▔▔╗__╔▔▔    B: _╔▔▔╗___
Counter-CW:    A: ▔▔╗__╔▔▔    B: ▔▔▔╗__╔▔
```

The Encoder library monitors both pins via interrupts and tracks the cumulative position count (+1 per clockwise detent, -1 per counter-clockwise).

**Relative CC format:**
```
Value = 0x40 (64, center) + delta_steps
```
- Clockwise turn  → value > 64 (e.g., 65 for one detent)
- Counter-CW turn → value < 64 (e.g., 63 for one detent)
- No movement     → no message sent

This matches the "relative offset" format expected by Mixxx.

**Detent scaling:** Most EC11 encoders produce 4 edge transitions per physical click/detent. The firmware divides the raw count by 4 so one detent = one MIDI message step.

**`encodersInit()`** — resets all encoder position counters to zero.

**`encodersSendMidi()`** — called from `loop()` at 1 kHz. Reads accumulated delta from each encoder, converts to relative CC, sends if non-zero. Fractional detents (0 < |delta| < 4) are held until the next scan to avoid half-step messages.

---

### 8.6 `motors.h`

Header for the motorized fader subsystem. Implementation is left for the next development phase.

**Four-function API:**

| Function | Called from | Purpose |
|----------|-------------|---------|
| `motorsInit()` | `setup()` | Configure PCA9685, read initial positions from ADS1115, set all targets to current position so motors don't jump |
| `motorsScan()` | `loop()` at 2 kHz | Run PID loop: read position, compute error, write PWM to motor driver |
| `motorsSetTarget(fader, target14bit)` | `onControlChange()` | Set where a fader should move. `fader` 0-3 = rate faders, 4-7 = position faders |
| `motorsEnableKill(kill)` | `checkMotorKill()` | If `true`, all motor output is set to zero regardless of PID |

**PID tuning constants** (in `config.h`):

| Constant | Default | Meaning |
|----------|---------|---------|
| `MOTOR_KP` | 2.0 | Proportional gain — how aggressively to correct error |
| `MOTOR_KI` | 0.1 | Integral gain — eliminates steady-state error (creep) |
| `MOTOR_KD` | 0.5 | Derivative gain — damping, prevents overshoot/oscillation |
| `MOTOR_INTEGRAL_MAX` | 1000 | Anti-windup clamp on the integral accumulator |
| `BEMF_THRESHOLD` | 200 | Position jump that indicates a user has grabbed the fader (14-bit units) |
| `USER_RELEASE_MS` | 200 | How long the fader must be stable before the PID resumes tracking |

**Touch detection:** When the user grabs a fader, the motor's back-EMF causes the position to jump. The firmware detects a jump above `BEMF_THRESHOLD`, pauses PID output, and reads/sends the fader position as MIDI CC so Mixxx can follow. After `USER_RELEASE_MS` of stability, PID resumes.

---

### 8.7 `main.cpp`

The entry point. Contains `setup()`, `loop()`, and two private helpers.

#### `setup()`

Runs once. Starts the I2C bus at 400 kHz, then calls each module's `init()` function. Registers the MIDI input callback. Reads the initial kill switch state. Resets all scan timers.

#### `loop()`

Runs forever. Contains five `if (elapsed >= interval)` blocks — one per module — plus `usbMIDI.read()` and `checkMotorKill()` every iteration.

#### `onControlChange(channel, cc, value)` (private)

MIDI input callback registered with `usbMIDI.setHandleControlChange()`. Receives messages that Mixxx sends to the controller. Currently handles:

- `DeckCC::RATE_MSB` + `DeckCC::RATE_LSB` → calls `motorsSetTarget(deck, target)` for rate fader
- `DeckCC::POSITION_MSB` + `DeckCC::POSITION_LSB` → calls `motorsSetTarget(deck + 4, target)` for position fader

14-bit values arrive in two messages (MSB first, then LSB). The function buffers the MSB and acts on the LSB.

#### `checkMotorKill()` (private)

Reads `PIN_MOTOR_KILL` (pin 37) on every loop iteration and calls `motorsEnableKill()` when the debounced state changes. Uses a 20 ms debounce window to ignore contact bounce.

---

## 9. MIDI Protocol

### Firmware → Mixxx (output)

| MIDI Ch | Type | CC / Note | Meaning |
|---------|------|-----------|---------|
| 1-4 (Deck) | CC | `DeckCC::JOYSTICK` (0x02) | Scrub joystick position 0-127 (64=center) |
| 1-4 (Deck) | CC | `DeckCC::BEATJUMP_ENC` (0x10) | Encoder relative value (64 ± delta) |
| 1-4 (Deck) | CC | `DeckCC::LOOP_ENC` (0x11) | Encoder relative value (64 ± delta) |
| 1-4 (Deck) | CC | `DeckCC::RATE_MSB/LSB` (0x00/0x20) | Motorized rate fader position (14-bit) |
| 1-4 (Deck) | CC | `DeckCC::POSITION_MSB/LSB` (0x01/0x21) | Motorized position fader position (14-bit) |
| 1-4 (Deck) | Note On/Off | `DeckNote::*` | All deck buttons |
| 5 (Mixer) | CC | `MixerCC::*` | All knobs and faders |
| 5 (Mixer) | Note On/Off | `MixerNote::*` | PFL, FX assign, load buttons |
| 6-7 (FX) | CC | `FxCC::*` | FX knobs and dry/wet |
| 6-7 (FX) | Note On/Off | `FxNote::*` | FX enable and select |
| 8 (Library) | CC | `LibCC::BROWSE_ENC` | Browse encoder |
| 8 (Library) | Note On/Off | `LibNote::*` | Library navigation buttons |

### Mixxx → Firmware (input)

| MIDI Ch | CC | Meaning |
|---------|-----|---------|
| 1-4 (Deck) | `DeckCC::RATE_MSB` (0x00) | Rate fader target, upper 7 bits |
| 1-4 (Deck) | `DeckCC::RATE_LSB` (0x20) | Rate fader target, lower 7 bits → triggers motor move |
| 1-4 (Deck) | `DeckCC::POSITION_MSB` (0x01) | Position fader target, upper 7 bits |
| 1-4 (Deck) | `DeckCC::POSITION_LSB` (0x21) | Position fader target, lower 7 bits → triggers motor move |

### 14-bit CC encoding

MIDI CC values are 7 bits (0-127). For motorized faders, 7-bit resolution is too coarse (only 128 positions across the full fader travel). 14-bit encoding uses two standard CC messages:

```
Send CC (MSB number) with bits 13-7 of the value
Send CC (MSB number + 0x20) with bits 6-0 of the value

Receiver reconstructs: value = (msb << 7) | lsb
Range: 0-16383 (14 bits)
```

This is standard MIDI practice — CC numbers 0-31 are "coarse" and CC 32-63 are their matching "fine" (LSB) counterparts.

---

## 10. Pin Quick-Reference

| Pin | Signal | Module |
|-----|--------|--------|
| 0, 1 | Encoder 1 A/B (Deck1 BeatJump) | encoders |
| 2 | MUX S0 | analog |
| 3 | I2S2 LRCLK | audio (fixed) |
| 4 | I2S2 BCLK | audio (fixed) |
| 5 | MUX S1 | analog |
| 6 | MUX S2 | analog |
| 7 | I2S1 TX | audio (fixed) |
| 8 | MUX S3 | analog |
| 9 | CS_7SEG | SPI (MAX7219) |
| 10 | CS_SR | SPI (74HC595) |
| 11 | SPI MOSI | SPI |
| 12, 17 | Encoder 2 A/B (Deck1 Loop) | encoders |
| 13 | SPI SCK | SPI |
| 14 (A0) | MUX #1 COM | analog |
| 15 (A1) | MUX #2 COM | analog |
| 16 (A2) | MUX #3 COM | analog |
| 18 | I2C SDA | buttons, motors |
| 19 | I2C SCL | buttons, motors |
| 20 | I2S1 LRCLK | audio (fixed) |
| 21 | I2S1 BCLK | audio (fixed) |
| 22-23 | Encoder 3 A/B (Deck2 BeatJump) | encoders |
| 24-25 | Encoder 4 A/B (Deck2 Loop) | encoders |
| 26-27 | Encoder 5 A/B (Deck3 BeatJump) | encoders |
| 28-29 | Encoder 6 A/B (Deck3 Loop) | encoders |
| 30-31 | Encoder 7 A/B (Deck4 BeatJump) | encoders |
| 32 | I2S2 TX | audio (fixed) |
| 33-34 | Encoder 8 A/B (Deck4 Loop) | encoders |
| 35-36 | Encoder 9 A/B (Browse) | encoders |
| 37 | Motor Kill Switch | main |
| 38-41 | Spare (A14-A17) | — |

**Pins 18-21 are never used as ADC inputs.** A4 (18) and A5 (19) are I2C; A6 (20) and A7 (21) are I2S1. All analog inputs route through MUXes to A0, A1, A2 only.
