# BrevvDeck — Custom 4-Deck DJ Controller Design Document

**Version:** 0.4
**Target Software:** Mixxx (open source DJ software)
**Communication Protocol:** USB MIDI + USB Audio (composite device)
**MCU:** Teensy 4.1 (i.MX RT1062, ARM Cortex-M7 @ 600 MHz)

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Protocol Decision: MIDI vs HID](#2-protocol-decision-midi-vs-hid)
3. [Controller Sections & Control Surface](#3-controller-sections--control-surface)
4. [Control Mapping Summary](#4-control-mapping-summary)
5. [Microcontroller & Hardware Architecture](#5-microcontroller--hardware-architecture)
   - 5.1 [Teensy 4.1 MCU](#51-microcontroller-teensy-41)
   - 5.2 [Input Architecture](#52-input-architecture)
   - 5.3 [Motor Control](#53-motor-control)
   - 5.4 [Button Matrix Wiring & Anti-Ghosting](#54-button-matrix-wiring--anti-ghosting)
6. [Audio System](#6-audio-system)
7. [Motorized Faders & Haptic Feedback](#7-motorized-faders--haptic-feedback)
8. [Power & USB Architecture](#8-power--usb-architecture)
9. [Physical Layout](#9-physical-layout)
10. [Wiring Diagram](#10-wiring-diagram)
11. [Pin Assignment Tables](#11-pin-assignment-tables)
12. [Bill of Materials](#12-bill-of-materials)
13. [Firmware Architecture](#13-firmware-architecture)
14. [Mixxx Mapping Strategy](#14-mixxx-mapping-strategy)
15. [Open Questions & Future Work](#15-open-questions--future-work)

---

## 1. Project Overview

BrevvDeck is a custom-built 4-deck DJ controller designed specifically for Mixxx. Key design goals:

- **4 full deck sections** (stacked 2×2) with play/cue, beat jumping, hotcues/slip-rolls, loop controls, and motorized position/BPM faders
- **4-channel mixer** with gain, 3-band EQ, quick effects, and channel faders
- **2 assignable FX sections** with 4 knobs and 4 buttons each
- **Library browser** and **global** sections
- **4 sample trigger buttons**
- **No jog wheels** — replaced by beat-jump encoders and 1-axis scrub joysticks
- **8 motorized 100mm faders** (2 per deck: BPM rate + playback position) with haptic feedback
- **2-digit 7-segment display per deck** showing loop size or beat-jump size
- **USB class-compliant audio** with 2 stereo outputs (master + headphones) — same USB connection as MIDI
- **Single USB cable** carries both MIDI control and audio (composite USB device)
- **< 10 ms latency** target for control responsiveness
- **Motor kill switch** to disable all motorized fader motors globally
- **Single MCU** — Teensy 4.1 handles MIDI, audio, and all I/O

---

## 2. Protocol Decision: MIDI vs HID

### Recommendation: **USB MIDI**

| Factor | MIDI | HID |
|--------|------|-----|
| **Mixxx XML mapping** | Full XML input/output — declarative, no JS needed for most controls | Script-only — all parsing in JavaScript |
| **Output/feedback** | Native XML `<output>` — easy motorized fader feedback | Custom HID output reports in JS |
| **Resolution** | 7-bit (0-127) standard; 14-bit with MSB/LSB | Arbitrary (8/10/12/16-bit natively) |
| **Firmware** | Teensy USB MIDI built-in | Also supported, but more complex |
| **Debugging** | Many MIDI monitoring tools | Fewer tools |

### Why MIDI

1. **XML mapping simplicity**: Custom CC/Note assignments map directly to Mixxx controls.
2. **Motorized fader feedback**: XML output mappings send `playposition` and `rate` values back automatically.
3. **14-bit resolution**: CC MSB/LSB pairs give 16,384 steps — ~0.006 mm on a 100mm fader.
4. **Teensy has native MIDI**: `usb_midi.h` provides zero-effort USB MIDI with the Teensyduino core.

### MIDI Channel Assignment

| MIDI Channel | Purpose |
|-------------|---------|
| Ch 1 (0x00) | Deck 1 |
| Ch 2 (0x01) | Deck 2 |
| Ch 3 (0x02) | Deck 3 |
| Ch 4 (0x03) | Deck 4 |
| Ch 5 (0x04) | Mixer |
| Ch 6 (0x05) | FX Unit 1 |
| Ch 7 (0x06) | FX Unit 2 |
| Ch 8 (0x07) | Library / Global / Samplers |

---

## 3. Controller Sections & Control Surface

### 3.1 Deck Section (×4)

Each deck contains:

| Control | Type | Count | Purpose |
|---------|------|-------|---------|
| Play/Pause | LED button | 1 | Toggle playback |
| Cue | LED button | 1 | Set/trigger cue point |
| Sync | LED button | 1 | Sync BPM to leader |
| Keylock | LED button | 1 | Toggle key lock |
| Quantize | LED button | 1 | Toggle quantize on/off |
| Hotcue/Roll 1-8 | LED buttons | 8 | Hotcues or slip-roll loops (mode set by toggle) |
| Hotcue/Roll Mode | Toggle switch | 1 | Hardware toggle: hotcue mode vs slip-roll mode |
| Loop In | Button | 1 | Set loop-in point |
| Loop Out | Button | 1 | Set loop-out point |
| Loop Deactivate | LED button | 1 | Exit/deactivate current loop |
| Beat Jump Encoder | Rotary encoder (infinite) | 1 | Rotate: jump fwd/back. Shift+rotate: halve/double jump size. Push: cycle jump size |
| Loop Encoder | Rotary encoder (infinite) | 1 | Rotate: halve/double loop. Push: reloop toggle |
| BPM/Rate Fader | Motorized 100mm fader | 1 | Adjust playback speed. Motor for sync |
| Playback Position Fader | Motorized 100mm fader | 1 | Track position 0-100%. Motor tracks playback |
| Scrub Joystick | 1-axis spring-return joystick | 1 | Variable-speed scrub (left=backward, right=forward, speed ∝ deflection) |
| 7-Segment Display | 2-digit 7-seg | 1 | Shows loop size; shift held → shows beat-jump size |
| Shift | Button | 1 | Modifier (shared per deck pair or per deck) |

**Per deck: 18 buttons + 1 toggle + 2 encoders + 2 motorized faders + 1 joystick + 1 display = 25 controls**
**4 decks total: 100 controls + 4 displays**

### 3.2 Mixer Section (4-channel)

| Control | Type | Count | Purpose |
|---------|------|-------|---------|
| Channel Volume | 60mm fader | 4 | Channel volume |
| Gain | Rotary pot | 4 | Channel input gain (pregain) |
| EQ High | Rotary pot | 4 | High frequency EQ |
| EQ Mid | Rotary pot | 4 | Mid frequency EQ |
| EQ Low | Rotary pot | 4 | Low frequency EQ |
| Quick Effect / Filter | Rotary pot | 4 | Quick effect super knob |
| PFL/Cue | LED button | 4 | Toggle headphone monitoring |
| FX 1 Assign | LED button | 4 | Send to FX unit 1 |
| FX 2 Assign | LED button | 4 | Send to FX unit 2 |
| Load Track | Button | 4 | Load selected track to deck |
| Crossfader | 60mm fader | 0-1 | Optional (TBD) |
| Headphone Volume | Rotary pot | 1 | Hardware headphone volume |
| Headphone Mix | Rotary pot | 1 | Cue/master mix |
| Master Volume | Rotary pot | 1 | Master output level |

**Mixer total: 4-5 faders + 23 pots + 16 buttons = ~44 controls**

### 3.3 Effects Section (×2)

| Control | Type | Count | Purpose |
|---------|------|-------|---------|
| Effect Knob 1-3 | Rotary pot | 3 | Effect meta parameters |
| Dry/Wet Knob | Rotary pot | 1 | Effect unit mix |
| Effect Enable 1-3 | LED button | 3 | Toggle effects |
| Effect Select | Button | 1 | Cycle/focus effects |

**Per FX: 4 pots + 4 buttons = 8. Total: 16 controls**

### 3.4 Library / Browser Section

| Control | Type | Count | Purpose |
|---------|------|-------|---------|
| Browse Encoder | Rotary encoder | 1 | Scroll library. Push = go to item. Shift+rotate = waveform zoom |
| Library Fullscreen | Button | 1 | Toggle library fullscreen |
| Back | Button | 1 | Navigate back in library tree |
| Preview | Button | 1 | Preview selected track |

**Library total: 1 encoder + 3 buttons = 4 controls**

### 3.5 Sampler Section

| Control | Type | Count | Purpose |
|---------|------|-------|---------|
| Sample 1-4 | LED button | 4 | Trigger sampler 1-4 |

**Sampler total: 4 buttons**

### 3.6 Global / Utility

| Control | Type | Count | Purpose |
|---------|------|-------|---------|
| Shift L | Button | 1 | Left shift (decks 1/3) |
| Shift R | Button | 1 | Right shift (decks 2/4) |
| Motor Kill | Toggle switch | 1 | Disable all motorized fader motors |

### 3.7 Grand Total

| Section | Buttons | Toggles | Pots | Encoders | Faders | Mot. Faders | Joysticks | Displays |
|---------|---------|---------|------|----------|--------|-------------|-----------|----------|
| 4× Decks | 60 | 4 | 0 | 8 | 0 | 8 | 4 | 4 |
| Mixer | 16 | 0 | 23 | 0 | 4-5 | 0 | 0 | 0 |
| 2× FX | 8 | 0 | 8 | 0 | 0 | 0 | 0 | 0 |
| Library | 3 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| Sampler | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Global | 2 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| **TOTAL** | **93** | **5** | **31** | **9** | **4-5** | **8** | **4** | **4** |

**Grand total: ~154 physical controls + 4 displays**

---

## 4. Control Mapping Summary

### 4.1 Deck Controls → Mixxx

Per deck (`[ChannelN]`):

| Physical Control | Mixxx Group | Mixxx Key | MIDI | Notes |
|-----------------|-------------|-----------|------|-------|
| Play/Pause | `[ChannelN]` | `play` | Note | LED: `play_indicator` |
| Cue | `[ChannelN]` | `cue_default` | Note | LED: `cue_indicator` |
| Sync | `[ChannelN]` | `sync_enabled` | Note | LED: `sync_enabled` |
| Keylock | `[ChannelN]` | `keylock` | Note | LED: `keylock` |
| Quantize | `[ChannelN]` | `quantize` | Note | LED: `quantize` |
| Hotcue 1-8 | `[ChannelN]` | `hotcue_N_activate` | Note | Shift: `hotcue_N_clear`. Via JS |
| Slip Roll 1-8 | `[ChannelN]` | `beatlooproll_N_activate` | Note | Sizes: 1/8,1/4,1/2,1,2,4,8,16. Via JS |
| HC/Roll Mode | — | — | — | Hardware toggle, read by firmware |
| Loop In | `[ChannelN]` | `loop_in` | Note | |
| Loop Out | `[ChannelN]` | `loop_out` | Note | |
| Loop Deactivate | `[ChannelN]` | `reloop_toggle` | Note | LED: `loop_enabled` |
| Beat Jump Turn | `[ChannelN]` | `beatjump_forward`/`_backward` | CC rel | Via JS |
| Shift+BJ Turn | `[ChannelN]` | `beatjump_size_double`/`_halve` | CC rel | Via JS |
| BJ Push | `[ChannelN]` | beatjump_size cycle | Note | Via JS |
| Loop Enc Turn | `[ChannelN]` | `loop_double`/`loop_halve` | CC rel | Via JS |
| Loop Enc Push | `[ChannelN]` | `reloop_toggle` | Note | |
| BPM Fader | `[ChannelN]` | `rate` | CC 14-bit | Motor out: `rate` |
| Position Fader | `[ChannelN]` | `playposition` | CC 14-bit | Motor out: `playposition`. Via JS |
| Scrub Joystick | `[ChannelN]` | `jog` | CC | Via JS. Spring-return, speed ∝ deflection |

### 4.2 Mixer → Mixxx

| Physical Control | Mixxx Group | Mixxx Key | MIDI |
|-----------------|-------------|-----------|------|
| Volume Ch 1-4 | `[ChannelN]` | `volume` | CC |
| Gain Ch 1-4 | `[ChannelN]` | `pregain` | CC |
| EQ High Ch 1-4 | `[EqualizerRack1_[ChannelN]_Effect1]` | `parameter3` | CC |
| EQ Mid Ch 1-4 | `[EqualizerRack1_[ChannelN]_Effect1]` | `parameter2` | CC |
| EQ Low Ch 1-4 | `[EqualizerRack1_[ChannelN]_Effect1]` | `parameter1` | CC |
| Quick FX Ch 1-4 | `[QuickEffectRack1_[ChannelN]]` | `super1` | CC |
| PFL/Cue Ch 1-4 | `[ChannelN]` | `pfl` | Note | LED |
| FX1 Assign Ch 1-4 | `[EffectRack1_EffectUnit1]` | `group_[ChannelN]_enable` | Note | LED |
| FX2 Assign Ch 1-4 | `[EffectRack1_EffectUnit2]` | `group_[ChannelN]_enable` | Note | LED |
| Load Track Ch 1-4 | `[ChannelN]` | `LoadSelectedTrack` | Note | |
| Crossfader | `[Master]` | `crossfader` | CC | Optional |
| Headphone Mix | `[Master]` | `headMix` | CC | |
| Headphone Vol | `[Master]` | `headVolume` | CC | |
| Master Vol | `[Master]` | `volume` | CC | |

### 4.3 Sampler → Mixxx

| Physical Control | Mixxx Group | Mixxx Key | MIDI |
|-----------------|-------------|-----------|------|
| Sample 1 | `[Sampler1]` | `cue_gotoandplay` | Note | LED: `track_loaded` |
| Sample 2 | `[Sampler2]` | `cue_gotoandplay` | Note | LED |
| Sample 3 | `[Sampler3]` | `cue_gotoandplay` | Note | LED |
| Sample 4 | `[Sampler4]` | `cue_gotoandplay` | Note | LED |

### 4.4 Effects → Mixxx

Per FX unit (`[EffectRack1_EffectUnitN]`):

| Physical Control | Mixxx Group | Mixxx Key | MIDI |
|-----------------|-------------|-----------|------|
| Knob 1-3 | `[..._EffectN]` | `meta` | CC |
| Dry/Wet | `[..._EffectUnitN]` | `mix` | CC |
| Enable 1-3 | `[..._EffectN]` | `enabled` | Note | LED |
| Select | `[..._EffectUnitN]` | `focused_effect` | Note | Via JS |

### 4.5 Library → Mixxx

| Physical Control | Mixxx Group | Mixxx Key | MIDI |
|-----------------|-------------|-----------|------|
| Browse Turn | `[Library]` | `MoveVertical` | CC rel |
| Browse Push | `[Library]` | `GoToItem` | Note |
| Fullscreen | `[Master]` | `maximize_library` | Note |
| Back | `[Library]` | `MoveLeft` | Note |
| Preview | `[PreviewDeck1]` | `LoadSelectedTrackAndPlay` | Note |

---

## 5. Microcontroller & Hardware Architecture

### 5.1 Microcontroller: Teensy 4.1

**NXP i.MX RT1062 — ARM Cortex-M7 @ 600 MHz**

| Feature | Specification |
|---------|--------------|
| CPU | ARM Cortex-M7 @ 600 MHz |
| RAM | 1024 KB (512K tightly coupled) |
| Flash | 8 MB (7.75 MB usable) |
| Digital GPIO | 55 pins |
| Analog inputs | 18 channels (2× 12-bit ADC) |
| I2S interfaces | 2× native SAI (SAI1 + SAI2) |
| I2C buses | 3× native (Wire, Wire1, Wire2) |
| SPI buses | 3× native (SPI, SPI1, SPI2) |
| USB | Native USB with composite device support (MIDI + Audio) |
| Quad timer | 4× (hardware quadrature encoder counting) |
| PWM | FlexPWM (multiple channels) + quad timer PWM |
| SD card | Built-in SDIO slot |
| Cost | ~$30 |

### Why Single Teensy 4.1 Instead of Dual RP2040 Pico

| Factor | Dual Pico (v0.2) | Single Teensy 4.1 (v0.3) |
|--------|-----------------|--------------------------|
| MCU count | 2 | 1 |
| USB hub needed | Yes (FE1.1s) | No |
| Composite MIDI+Audio | No (separate USB endpoints) | Yes (single USB cable) |
| Language | Rust / embassy-rs | C++ / Teensyduino |
| CPU power | 2× 133 MHz Cortex-M0+ | 1× 600 MHz Cortex-M7 |
| Native I2S | No (PIO-based) | 2× hardware SAI |
| Native encoder HW | No (PIO or polling) | Quad timer hardware decode |
| ADC channels | 3 (need 3× external MUX) | 18 (need 3 MUXes — pins A4-A7 used by I2C/I2S) |
| GPIO count | 26 per Pico | 55 total |
| Firmware projects | 2 separate | 1 unified |
| Cost | ~$8 + $1.50 hub = ~$10 | ~$30 |
| Bootloader | Open (UF2) | Proprietary (HalfKay) |

**Trade-offs accepted:**
- **No Rust**: Teensy uses C++ with Arduino/Teensyduino. This is well-proven for audio + MIDI.
- **Higher cost**: +$20 for the MCU, but saves ~$1.50 on USB hub and simplifies assembly.
- **Proprietary bootloader**: HalfKay is closed-source but well-established and reliable.

**Benefits gained:**
- **Single USB cable for everything**: Composite USB device presents both MIDI and Audio to the host.
- **No USB hub**: Eliminates a component and potential failure point.
- **Native I2S**: Two hardware SAI interfaces drive the DACs directly — no PIO hacking.
- **More ADC channels**: 18 native channels. However, A4-A7 are committed to I2C/I2S, so 3 MUXes are used to avoid pin conflicts.
- **Hardware encoder decode**: Quad timers count encoder edges in hardware — no CPU overhead.
- **Teensy Audio Library**: Mature, battle-tested I2S + USB Audio stack.
- **One firmware**: Everything in a single project — simpler development and debugging.

### 5.2 Input Architecture

#### Analog Inputs

| Source | Count | Bits | Method |
|--------|-------|------|--------|
| Rotary pots (EQ, FX, mixer) | 31 | 12 | 3× CD74HC4067 MUX → Teensy ADC |
| Channel volume faders | 4-5 | 12 | MUX #3 → Teensy ADC |
| Motorized fader pots | 8 | 16 | 2× ADS1115 external ADC (I2C) |
| Scrub joysticks | 4 | 12 | MUX #3 → Teensy ADC |
| **Total** | **~48** | | |

**MUX configuration (3× CD74HC4067):**
- All pot/fader/joystick inputs are multiplexed — no direct Teensy ADC pins are used.
  This avoids pin conflicts: Teensy pins A4-A7 (18-21) are committed to I2C (SDA/SCL)
  and I2S1 (LRCLK/BCLK), so they cannot serve as analog inputs.
- MUX #1 (A0, 16ch): Gains (4) + EQ High/Mid/Low (12) = 16 channels
- MUX #2 (A1, 16ch): Quick FX (4) + FX1 knobs/DW (4) + FX2 knobs/DW (4) + HP Mix + Master Vol + HP Vol + spare = 16 channels
- MUX #3 (A2, 16ch): Volume faders (4) + crossfader (1) + joysticks (4) + 7 spare = 16 channels
- All 3 MUXes share 4 address lines (S0-S3); each has its own COM→ADC pin
- 2× ADS1115 (4-ch each, 16-bit) = 8 channels for motorized fader position (I2C)

#### Digital Inputs

| Source | Count | Method |
|--------|-------|--------|
| Buttons (all sections) | 93 | Button matrix via 2× MCP23017 (I2C) |
| Toggle switches | 5 | Direct GPIO or MCP23017 |
| Rotary encoders (A/B pins) | 9×2 = 18 | Direct GPIO (quad timer HW decode where possible) |
| **Total** | **~116** | |

**Button matrix**: 10 rows × 10 columns on MCP23017 expanders = 100 positions (93 used + 7 spare).

#### Displays

- 4× 2-digit 7-segment displays, driven by 2× MAX7219 (SPI) — each MAX7219 drives up to 8 digits.

#### LED Output

- ~60 LEDs for button indicators
- Driven by 8× 74HC595 shift registers (8 outputs each = 64 LEDs), daisy-chained on SPI bus

### 5.3 Motor Control

- 4× DRV8833 dual H-bridge motor drivers = 8 motor channels
- PWM from PCA9685 (16-channel I2C PWM driver) → DRV8833 inputs
- Motor kill switch: MOSFET on motor power rail, controlled by hardware toggle

### 5.4 Button Matrix Wiring & Anti-Ghosting

#### Overview

The button matrix uses a **10 rows × 10 columns** scanning architecture via two MCP23017 I2C port expanders. Each button is placed at a row-column intersection with a protection diode to prevent ghosting (false key presses when multiple buttons are pressed).

**Scanning method (active low):**
- Port A (outputs): Drive one column low at a time; all others high
- Port B (inputs): Read which rows are pulled low (indicating pressed buttons)
- Firmware sequentially activates each column and samples all row bits

#### MCP23017 IC Distribution

| IC | Address | Port A (Outputs) | Port B (Inputs) | Purpose |
|----|---------|------------------|-----------------|---------|
| MCP23017 #1 | 0x20 | Columns 0-7 (bits 0-7) | Rows 0-7 (bits 0-7) | Left/bottom matrix section |
| MCP23017 #2 | 0x21 | Columns 8-9 (bits 0-1) | Rows 8-9 (bits 0-1) | Right/top matrix section |

**I2C addressing:**
- Both ICs on same Wire bus (I2C standard, 100 kHz or 400 kHz)
- Address pins A0-A2 on both chips grounded → base address 0x20 (IC #1) and 0x21 (IC #2)
- Pull-ups: 4.7 kΩ on SDA/SCL (typically on Teensy breakout)

#### Button Connection Wiring

**Standard button cell at intersection (row R, column C):**

```
Column C driver (MCP23017 Port A, bit C)
    │
    ├─── [1N4148 diode, cathode to row] ──┬──── [Momentary button] ──┬──── GND
    │                                     │                          │
    └─────────────────────────────────────┴────-- MCP23017 Port B input (row R, with internal pull-up)
```

Or equivalently, diode can be oriented with anode toward column (most common):

```
Column C driver
    ├─── [Momentary button] ──┬──── [1N4148 diode, anode to row] ──┬──── GND
    │                         │                                    │
    └─────────────────────────┴────── MCP23017 Port B (row R pull-up)
```

**Why the diode?**
- Prevents ghosting: when multiple buttons are pressed, the diode at each intersection ensures current only flows through the intended button pair (column + row combination)
- Reverse bias protection: diode blocks unwanted current paths through unpressed buttons in the same row/column

#### Complete Button Wiring Reference

**Legend:**
- **Column IC Pin:** MCP23017 Port A pin that drives the column (active low)
- **Row IC Pin:** MCP23017 Port B pin that senses the row (with internal 100kΩ pull-up)
- **Diode placement:** Between column pin and button, button and row pin (see orientation note below)

**DECK 1** (Columns 0-4, Rows 0-4)

| Button Name | Column | Row | Column IC Pin | Row IC Pin | Matrix Coords |
|-------------|--------|-----|---------------|-----------|------------|
| Play | 0 | 0 | MCP#1 RA0 | MCP#1 RB0 | (0,0) |
| Cue | 1 | 0 | MCP#1 RA1 | MCP#1 RB0 | (1,0) |
| Sync | 2 | 0 | MCP#1 RA2 | MCP#1 RB0 | (2,0) |
| Keylock | 3 | 0 | MCP#1 RA3 | MCP#1 RB0 | (3,0) |
| Quantize | 4 | 0 | MCP#1 RA4 | MCP#1 RB0 | (4,0) |
| Loop In | 0 | 1 | MCP#1 RA0 | MCP#1 RB1 | (0,1) |
| Loop Out | 1 | 1 | MCP#1 RA1 | MCP#1 RB1 | (1,1) |
| Loop Deactivate | 2 | 1 | MCP#1 RA2 | MCP#1 RB1 | (2,1) |
| Loop Encoder Push | 3 | 1 | MCP#1 RA3 | MCP#1 RB1 | (3,1) |
| Beat Jump Encoder Push | 4 | 1 | MCP#1 RA4 | MCP#1 RB1 | (4,1) |
| Hotcue 1 | 0 | 2 | MCP#1 RA0 | MCP#1 RB2 | (0,2) |
| Hotcue 2 | 1 | 2 | MCP#1 RA1 | MCP#1 RB2 | (1,2) |
| Hotcue 3 | 2 | 2 | MCP#1 RA2 | MCP#1 RB2 | (2,2) |
| Hotcue 4 | 3 | 2 | MCP#1 RA3 | MCP#1 RB2 | (3,2) |
| Hotcue 5 | 4 | 2 | MCP#1 RA4 | MCP#1 RB2 | (4,2) |
| Hotcue 6 | 0 | 3 | MCP#1 RA0 | MCP#1 RB3 | (0,3) |
| Hotcue 7 | 1 | 3 | MCP#1 RA1 | MCP#1 RB3 | (1,3) |
| Hotcue 8 | 2 | 3 | MCP#1 RA2 | MCP#1 RB3 | (2,3) |
| Shift | 3 | 3 | MCP#1 RA3 | MCP#1 RB3 | (3,3) |
| PFL | 4 | 3 | MCP#1 RA4 | MCP#1 RB3 | (4,3) |
| FX1 Assign | 0 | 4 | MCP#1 RA0 | MCP#1 RB4 | (0,4) |
| FX2 Assign | 1 | 4 | MCP#1 RA1 | MCP#1 RB4 | (1,4) |
| Load Track | 2 | 4 | MCP#1 RA2 | MCP#1 RB4 | (2,4) |
| Sampler Trigger 1 | 3 | 4 | MCP#1 RA3 | MCP#1 RB4 | (3,4) |

**DECK 2** (Columns 5-9, Rows 0-4)

| Button Name | Column | Row | Column IC Pin | Row IC Pin | Matrix Coords |
|-------------|--------|-----|---------------|-----------|------------|
| Play | 5 | 0 | MCP#1 RA5 | MCP#1 RB0 | (5,0) |
| Cue | 6 | 0 | MCP#1 RA6 | MCP#1 RB0 | (6,0) |
| Sync | 7 | 0 | MCP#1 RA7 | MCP#1 RB0 | (7,0) |
| Keylock | 8 | 0 | MCP#2 RA0 | MCP#1 RB0 | (8,0) |
| Quantize | 9 | 0 | MCP#2 RA1 | MCP#1 RB0 | (9,0) |
| Loop In | 5 | 1 | MCP#1 RA5 | MCP#1 RB1 | (5,1) |
| Loop Out | 6 | 1 | MCP#1 RA6 | MCP#1 RB1 | (6,1) |
| Loop Deactivate | 7 | 1 | MCP#1 RA7 | MCP#1 RB1 | (7,1) |
| Loop Encoder Push | 8 | 1 | MCP#2 RA0 | MCP#1 RB1 | (8,1) |
| Beat Jump Encoder Push | 9 | 1 | MCP#2 RA1 | MCP#1 RB1 | (9,1) |
| Hotcue 1 | 5 | 2 | MCP#1 RA5 | MCP#1 RB2 | (5,2) |
| Hotcue 2 | 6 | 2 | MCP#1 RA6 | MCP#1 RB2 | (6,2) |
| Hotcue 3 | 7 | 2 | MCP#1 RA7 | MCP#1 RB2 | (7,2) |
| Hotcue 4 | 8 | 2 | MCP#2 RA0 | MCP#1 RB2 | (8,2) |
| Hotcue 5 | 9 | 2 | MCP#2 RA1 | MCP#1 RB2 | (9,2) |
| Hotcue 6 | 5 | 3 | MCP#1 RA5 | MCP#1 RB3 | (5,3) |
| Hotcue 7 | 6 | 3 | MCP#1 RA6 | MCP#1 RB3 | (6,3) |
| Hotcue 8 | 7 | 3 | MCP#1 RA7 | MCP#1 RB3 | (7,3) |
| Shift | 8 | 3 | MCP#2 RA0 | MCP#1 RB3 | (8,3) |
| PFL | 9 | 3 | MCP#2 RA1 | MCP#1 RB3 | (9,3) |
| FX1 Assign | 5 | 4 | MCP#1 RA5 | MCP#1 RB4 | (5,4) |
| FX2 Assign | 6 | 4 | MCP#1 RA6 | MCP#1 RB4 | (6,4) |
| Load Track | 7 | 4 | MCP#1 RA7 | MCP#1 RB4 | (7,4) |
| Sampler Trigger 2 | 8 | 4 | MCP#2 RA0 | MCP#1 RB4 | (8,4) |

**DECK 3** (Columns 0-4, Rows 5-9)

| Button Name | Column | Row | Column IC Pin | Row IC Pin | Matrix Coords |
|-------------|--------|-----|---------------|-----------|------------|
| Play | 0 | 5 | MCP#1 RA0 | MCP#1 RB5 | (0,5) |
| Cue | 1 | 5 | MCP#1 RA1 | MCP#1 RB5 | (1,5) |
| Sync | 2 | 5 | MCP#1 RA2 | MCP#1 RB5 | (2,5) |
| Keylock | 3 | 5 | MCP#1 RA3 | MCP#1 RB5 | (3,5) |
| Quantize | 4 | 5 | MCP#1 RA4 | MCP#1 RB5 | (4,5) |
| Loop In | 0 | 6 | MCP#1 RA0 | MCP#1 RB6 | (0,6) |
| Loop Out | 1 | 6 | MCP#1 RA1 | MCP#1 RB6 | (1,6) |
| Loop Deactivate | 2 | 6 | MCP#1 RA2 | MCP#1 RB6 | (2,6) |
| Loop Encoder Push | 3 | 6 | MCP#1 RA3 | MCP#1 RB6 | (3,6) |
| Beat Jump Encoder Push | 4 | 6 | MCP#1 RA4 | MCP#1 RB6 | (4,6) |
| Hotcue 1 | 0 | 7 | MCP#1 RA0 | MCP#1 RB7 | (0,7) |
| Hotcue 2 | 1 | 7 | MCP#1 RA1 | MCP#1 RB7 | (1,7) |
| Hotcue 3 | 2 | 7 | MCP#1 RA2 | MCP#1 RB7 | (2,7) |
| Hotcue 4 | 3 | 7 | MCP#1 RA3 | MCP#1 RB7 | (3,7) |
| Hotcue 5 | 4 | 7 | MCP#1 RA4 | MCP#1 RB7 | (4,7) |
| Hotcue 6 | 0 | 8 | MCP#1 RA0 | MCP#2 RB0 | (0,8) |
| Hotcue 7 | 1 | 8 | MCP#1 RA1 | MCP#2 RB0 | (1,8) |
| Hotcue 8 | 2 | 8 | MCP#1 RA2 | MCP#2 RB0 | (2,8) |
| Shift | 3 | 8 | MCP#1 RA3 | MCP#2 RB0 | (3,8) |
| PFL | 4 | 8 | MCP#1 RA4 | MCP#2 RB0 | (4,8) |
| FX1 Assign | 0 | 9 | MCP#1 RA0 | MCP#2 RB1 | (0,9) |
| FX2 Assign | 1 | 9 | MCP#1 RA1 | MCP#2 RB1 | (1,9) |
| Load Track | 2 | 9 | MCP#1 RA2 | MCP#2 RB1 | (2,9) |
| Sampler Trigger 3 | 3 | 9 | MCP#1 RA3 | MCP#2 RB1 | (3,9) |

**DECK 4** (Columns 5-9, Rows 5-9)

| Button Name | Column | Row | Column IC Pin | Row IC Pin | Matrix Coords |
|-------------|--------|-----|---------------|-----------|------------|
| Play | 5 | 5 | MCP#1 RA5 | MCP#1 RB5 | (5,5) |
| Cue | 6 | 5 | MCP#1 RA6 | MCP#1 RB5 | (6,5) |
| Sync | 7 | 5 | MCP#1 RA7 | MCP#1 RB5 | (7,5) |
| Keylock | 8 | 5 | MCP#2 RA0 | MCP#1 RB5 | (8,5) |
| Quantize | 9 | 5 | MCP#2 RA1 | MCP#1 RB5 | (9,5) |
| Loop In | 5 | 6 | MCP#1 RA5 | MCP#1 RB6 | (5,6) |
| Loop Out | 6 | 6 | MCP#1 RA6 | MCP#1 RB6 | (6,6) |
| Loop Deactivate | 7 | 6 | MCP#1 RA7 | MCP#1 RB6 | (7,6) |
| Loop Encoder Push | 8 | 6 | MCP#2 RA0 | MCP#1 RB6 | (8,6) |
| Beat Jump Encoder Push | 9 | 6 | MCP#2 RA1 | MCP#1 RB6 | (9,6) |
| Hotcue 1 | 5 | 7 | MCP#1 RA5 | MCP#1 RB7 | (5,7) |
| Hotcue 2 | 6 | 7 | MCP#1 RA6 | MCP#1 RB7 | (6,7) |
| Hotcue 3 | 7 | 7 | MCP#1 RA7 | MCP#1 RB7 | (7,7) |
| Hotcue 4 | 8 | 7 | MCP#2 RA0 | MCP#1 RB7 | (8,7) |
| Hotcue 5 | 9 | 7 | MCP#2 RA1 | MCP#1 RB7 | (9,7) |
| Hotcue 6 | 5 | 8 | MCP#1 RA5 | MCP#2 RB0 | (5,8) |
| Hotcue 7 | 6 | 8 | MCP#1 RA6 | MCP#2 RB0 | (6,8) |
| Hotcue 8 | 7 | 8 | MCP#1 RA7 | MCP#2 RB0 | (7,8) |
| Shift | 8 | 8 | MCP#2 RA0 | MCP#2 RB0 | (8,8) |
| PFL | 9 | 8 | MCP#2 RA1 | MCP#2 RB0 | (9,8) |
| FX1 Assign | 5 | 9 | MCP#1 RA5 | MCP#2 RB1 | (5,9) |
| FX2 Assign | 6 | 9 | MCP#1 RA6 | MCP#2 RB1 | (6,9) |
| Load Track | 7 | 9 | MCP#1 RA7 | MCP#2 RB1 | (7,9) |
| Sampler Trigger 4 | 8 | 9 | MCP#2 RA0 | MCP#2 RB1 | (8,9) |

#### Diode Specifications & Placement

**Component: 1N4148 Fast Switching Diode**
- Forward voltage drop: ~0.65V @ 10 mA
- Fast reverse recovery: ~4 ns
- Current rating: 200 mA (adequate for button current ~1-5 mA)
- Cost: $0.01-0.02 per unit (purchase 100 to allow spares)
- Total needed: 93 diodes (per active buttons) + 7 spares = 100 units

**Diode wiring for each button:**

For each button in the tables above, use the Column IC Pin and Row IC Pin to wire the diode:

```
MCP23017 IC
│
Port A pin (Column IC Pin from table) ──[Button switch]──[1N4148 diode]──┬──── GND
                                                                         │
                                                   Port B pin (Row IC Pin from table)
                                                   (internally pulled up to 3.3V via 100kΩ)
```

**Specific wiring example (Deck 1 Play button):**
- From table: Column IC Pin = MCP#1 RA0, Row IC Pin = MCP#1 RB0
- Wire: MCP23017 #1 pin RA0 ──[Button]──[1N4148]──→ MCP23017 #1 pin RB0 (with GND on diode cathode end)

**Diode orientation:**
- **Anode** (marked end, often with a ring): points toward Row IC Pin
- **Cathode** (unmarked end): connects to GND
- This ensures the diode blocks reverse current paths that would cause ghosting

**PCB routing:**
- Diodes are mounted in a **100-position grid** on the PCB (10×10 pattern, one per matrix intersection)
- Each diode position corresponds to a (row, column) pair from the tables
- Use standard 0603 or 0805 surface-mount packages for compact layout
- Alternatively, use through-hole diodes with PCB cutouts for button mounting
- Ensure adequate spacing (buttons are typically 16-20mm pitch)

#### Anti-Ghosting Behavior

When **2 buttons in same row pressed** (e.g., Col 0-Row 0 and Col 1-Row 0):
```
Col 0 driver (LOW) ──[Button]──[D0]──→ Row 0 (sensed, pulled low) ✓ Correct: Col 0-Row 0 detected
                                ├─ GND

Col 1 driver (LOW) ──[Button]──[D1]──→ Row 0 (already pulled low by D0)
                                ├─ GND

Without diodes, activating Col 2 would falsely sense Row 0 from capacitive coupling.
Diodes prevent phantom closes via adjacent columns.
```

When **3 buttons forming a "phantom" pattern** (Col 0-Row 0, Col 0-Row 1, Col 1-Row 0):
```
       Col 0 (active)      Col 1 (inactive)
            │                    │
       [Button A]          [Button X]
            │D0                 │DX
       [D0] ──→ Row 0      [DX] ──→ Row 0  (blocked by D0 reverse bias)
       [D1] ──→ Row 1      [D1'] ──→ Row 1 (blocked by D1')

Expected: A & Button at (1,1) pressed
Phantom (without diodes): "Col 1-Row 0" would false-close via Row 0 → Col 0 → Button A path
With diodes: D0 blocks reverse current; phantom cell cannot close.
```

#### I2C Wiring

| Signal | Teensy Pin | MCP23017 #1 | MCP23017 #2 | Notes |
|--------|------------|-------------|------------|-------|
| SDA | 18 (A4) | Pin 12 | Pin 12 | Shared, 4.7kΩ pull-up to 3.3V |
| SCL | 19 (A5) | Pin 13 | Pin 13 | Shared, 4.7kΩ pull-up to 3.3V |
| GND | GND | Pin 9, 10 | Pin 9, 10 | All grounds connected |
| VCC (3.3V) | 3V3 | Pin 16 | Pin 16 | 0.1µF capacitor near each IC |
| A0 | GND | Pin 15 | GND | Address bit 0 (IC #1: addr 0x20) |
| A1 | GND | Pin 14 | 3.3V | Address bit 1 (IC #1: 0x20, IC #2: 0x21) |
| A2 | GND | Pin 13 | GND | Address bit 2 (both 0x20/0x21) |

**Note:** MCP23017 address configuration sets IC #2 address to 0x21 by tying A1 to 3.3V on that IC only.

---

## 6. Audio System

### 6.1 Architecture

The Teensy 4.1 handles USB Audio natively as part of a composite USB device. The i.MX RT1062 has two independent SAI (Synchronous Audio Interface) modules, each capable of driving a separate I2S DAC.

```
Host PC (Mixxx)
    │
    USB Composite Device (MIDI + Audio)
    │
Teensy 4.1
    │
    ├── SAI1 (I2S) ──→ PCM5102A DAC #1 ──→ OPA2134 buffer ──→ Master L/R
    │                                                          ├── RCA jacks
    │                                                          ├── TRS 1/4"
    │                                                          └── Booth (via vol pot)
    │
    ├── SAI2 (I2S) ──→ PCM5102A DAC #2 ──→ TPA6120A2 HP amp ──→ Headphones
    │                                                            ├── 3.5mm TRS
    │                                                            └── 6.35mm TRS
    │
    └── (Future) ADC ←── Mic/Instrument In
```

### 6.2 DAC: PCM5102A

- 32-bit / 384 kHz, I2S input, SNR 112 dB
- Breakout boards available for $2-5
- Target: 48 kHz / 24-bit (96 kHz stretch goal)

### 6.3 Audio Output Buffering

Each DAC output is buffered with an **OPA2134** (dual op-amp, unity-gain buffer):
- Provides low-impedance output for driving long cables
- Isolates DAC from load variations
- One OPA2134 per DAC (L+R = 2 channels per chip)

Booth output: taps master DAC buffer output through a hardware volume pot.

### 6.4 Headphone Amp: TPA6120A2

- Drives 250Ω headphones (DT-660) at ~20 mW
- Powered from ±5V (charge pump from USB 5V, or boost converter)
- Hardware volume pot in analog path before amp

### 6.5 Teensy Audio Library

The Teensy Audio Library provides:
- `AudioOutputI2S` for SAI1 (master output)
- `AudioOutputI2S2` for SAI2 (headphone output)
- `AudioInputUSB` for receiving audio streams from the host
- All handled in an interrupt-driven audio engine at 44.1 kHz or 48 kHz

The USB Audio Class 2.0 composite device presents 4 output channels (2 stereo pairs) to the host. Mixxx sees this as a standard soundcard.

---

## 7. Motorized Faders & Haptic Feedback

### 7.1 Overview

8× motorized 100mm faders (all same size — sourced from Alibaba):
- 4× BPM/Rate faders (vertical)
- 4× Playback Position faders (horizontal)

### 7.2 Haptic Feedback — No Capacitive Sensor

The goal is to provide slight resistance when the fader is software-controlled, and detect user override purely through motor back-EMF and position monitoring — **no additional touch sensor needed**.

#### Detection Method: Back-EMF + Position Monitoring

When the motor is driving the fader to a target position:

1. **Periodically disable the H-bridge** for a brief window (~100 µs every 10 ms)
2. **Measure back-EMF voltage** across the motor terminals via ADC
   - If the fader is stationary at target: back-EMF ≈ 0
   - If the user is pushing the fader: back-EMF shows voltage from forced motor movement
3. **Position delta monitoring**: Compare expected position change (from motor drive) against actual position change
   - If actual change ≫ expected → user is moving the fader
   - If actual change opposes motor direction → user is definitely overriding

#### Haptic Resistance Behavior

```
┌───────────────────────────────────────────────────────┐
│ Motorized Fader State Machine                         │
├───────────────────────────────────────────────────────┤
│                                                       │
│  [SOFTWARE_TRACKING]                                  │
│  Motor drives fader to target with PID control.       │
│  Provides mild holding torque at target position      │
│  (low PWM duty, ~15-20%) so user feels resistance.    │
│      │                                                │
│      │ User force detected (back-EMF or position Δ)   │
│      ▼                                                │
│  [USER_OVERRIDE]                                      │
│  Motor stops driving. Fader position is sent to       │
│  Mixxx. User has full control.                        │
│      │                                                │
│      │ No movement for 200ms + user force gone         │
│      ▼                                                │
│  [RETURNING]                                          │
│  Motor gently returns fader to software target        │
│  with reduced PWM (smooth return, not snap).          │
│      │                                                │
│      │ Target reached                                 │
│      ▼                                                │
│  [SOFTWARE_TRACKING] (loop)                           │
│                                                       │
└───────────────────────────────────────────────────────┘
```

#### Implementation on DRV8833

The DRV8833 has a decay mode (fast/slow) that affects back-EMF measurement:
- Use **fast decay** mode during measurement windows (both outputs low → motor coasts)
- Read back-EMF on the motor terminal via a voltage divider to ADC
- Add Schottky diodes for flyback protection (already built into DRV8833)

### 7.3 Motor Kill Switch

- Physical toggle switch on the controller surface
- Connected to a **P-channel MOSFET** (e.g., IRF9540) on the motor 5V power rail
- When switch is OFF: MOSFET cuts power to all DRV8833 motor drivers
- Faders still function as normal potentiometers (position sensing unaffected)
- Read by firmware via GPIO to suppress motor-related MIDI output processing

### 7.4 Playback Position Fader

- During playback: motor slowly moves fader right (0.33 mm/sec for a 5-minute track)
- User grab: motor disengages, position sent to Mixxx for seeking
- User release: motor re-engages, returns to current playback position

### 7.5 BPM Rate Fader

- Reflects current rate/BPM setting
- On sync: motor jumps fader to synced position
- User override: changes rate, motor holds at new position

### 7.6 Calibration

1. Hold both Shift buttons simultaneously
2. All motorized faders drive to minimum → firmware records ADC min values
3. User presses any button → faders drive to maximum → firmware records ADC max values
4. User presses button again → values stored in Teensy EEPROM
5. Normal operation resumes with calibrated range

---

## 8. Power & USB Architecture

### 8.1 Single USB Connection

```
USB-C Port (host laptop)
    │
    USB 2.0 High-Speed (480 Mbps)
    │
Teensy 4.1
    (Composite USB Device: MIDI + Audio Class 2.0)
    │
    ├── USB MIDI Interface → control data
    └── USB Audio Interface → 4 output channels (2 stereo pairs)
```

No USB hub needed. The Teensy presents a single composite USB device with both MIDI and Audio interfaces. The host OS sees one USB device with two functions.

**External USB-A ports** (optional): If still desired, a small passive hub IC can be added, but this is no longer required for the core architecture.

### 8.2 Power Distribution

```
USB-C 5V input (from host or powered hub)
    │
    ├── Teensy 4.1 VUSB (5V) → onboard 3.3V regulator → MCU core
    │
    ├── 3.3V LDO (AMS1117-3.3) → DACs (PCM5102A × 2, isolated analog)
    │
    ├── 5V → Motor kill switch (P-MOSFET) → DRV8833 × 4
    │         └── Capacitor bank: 4× 2200µF electrolytic
    │
    ├── 5V → TPA6120A2 headphone amp (or ±5V via charge pump)
    │
    └── 5V → PCA9685, MAX7219, 74HC595, MCP23017 (logic)
```

### 8.3 Grounding

- Separate analog and digital ground planes
- Star ground at USB-C input
- Ferrite beads (600Ω @ 100MHz) between motor power and audio power
- Motor ground returns routed away from DAC ground

---

## 9. Physical Layout

### 9.1 Layout (DJ-Facing = Bottom)

The DJ stands at the bottom edge. Decks are stacked (1 over 3 on left, 2 over 4 on right). Center column: Global at top, Library, FX1/FX2 side-by-side, Mixer at bottom, Samplers at very bottom.

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                                                                   BACK PANEL  │
│  [USB-C] [Master RCA] [Booth RCA] [Master TRS] [Booth TRS]                   │
│  [HP 3.5mm] [HP 6.35mm] [Mic XLR/TRS]                                        │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ┌──── DECK 1 ────┐  ┌─── GLOBAL/LIB ──┐  ┌──── DECK 2 ────┐                │
│  │ POS ═══════════ │  │    [SHFT L]      │  │ POS ═══════════ │                │
│  │ JS [◄───▌───►] │  │    [SHFT R]      │  │ JS [◄───▌───►] │                │
│  │ BPM ═══════════ │  │   [MOTOR OFF]    │  │ BPM ═══════════ │                │
│  │                 │  │                  │  │                 │                │
│  │ BJ[enc] LP[enc] │  │                  │  │ BJ[enc] LP[enc] │                │
│  │ [LI][LO][LD]    │  │  ┌──LIBRARY──┐  │  │ [LI][LO][LD]    │                │
│  │ [88] ← 7-seg    │  │  │ [BK][FS]  │  │  │ [88] ← 7-seg    │                │
│  │                 │  │  │ [PR]      │  │  │                 │                │
│  │ HC/ROLL: 1234   │  │  │ (BROWSE)  │  │  │ HC/ROLL: 1234   │                │
│  │          5678   │  │  └───────────┘  │  │          5678   │                │
│  │ [mode toggle]   │  │                  │  │ [mode toggle]   │                │
│  │                 │  │                  │  │                 │                │
│  │ [PLAY][CUE ]   │  │                  │  │ [PLAY][CUE ]   │                │
│  │ [SYNC][KYLK]   │  │                  │  │ [SYNC][KYLK]   │                │
│  │ [QNTZ]         │  │                  │  │ [QNTZ]         │                │
│  └─────────────────┘  │                  │  └─────────────────┘                │
│                        │                  │                                     │
│  ┌──── DECK 3 ────┐  │  ┌─FX1──┐┌─FX2──┐│  ┌──── DECK 4 ────┐                │
│  │ POS ═══════════ │  │  │(K1) ││(K1) ││  │ POS ═══════════ │                │
│  │ JS [◄───▌───►] │  │  │(K2) ││(K2) ││  │ JS [◄───▌───►] │                │
│  │ BPM ═══════════ │  │  │(K3) ││(K3) ││  │ BPM ═══════════ │                │
│  │                 │  │  │(DW) ││(DW) ││  │                 │                │
│  │ BJ[enc] LP[enc] │  │  │[E1] ││[E1] ││  │ BJ[enc] LP[enc] │                │
│  │ [LI][LO][LD]    │  │  │[E2] ││[E2] ││  │ [LI][LO][LD]    │                │
│  │ [88] ← 7-seg    │  │  │[E3] ││[E3] ││  │ [88] ← 7-seg    │                │
│  │                 │  │  │[SE] ││[SE] ││  │                 │                │
│  │ HC/ROLL: 1234   │  │  └─────┘└─────┘│  │ HC/ROLL: 1234   │                │
│  │          5678   │  │                  │  │          5678   │                │
│  │ [mode toggle]   │  │                  │  │ [mode toggle]   │                │
│  │                 │  │                  │  │                 │                │
│  │ [PLAY][CUE ]   │  │                  │  │ [PLAY][CUE ]   │                │
│  │ [SYNC][KYLK]   │  │                  │  │ [SYNC][KYLK]   │                │
│  │ [QNTZ]         │  │                  │  │ [QNTZ]         │                │
│  └─────────────────┘  │                  │  └─────────────────┘                │
│                        │                  │                                     │
│  ┌────────────────────── MIXER ──────────────────────────┐                     │
│  │ [LD1]  [LD2]  [LD3]  [LD4]                            │  ← Load track btns │
│  │                                                        │                     │
│  │ (GN1) (GN2) (GN3) (GN4)                              │  ← Gain knobs       │
│  │ (HI1) (HI2) (HI3) (HI4)                              │  ← EQ High          │
│  │ (MD1) (MD2) (MD3) (MD4)                              │  ← EQ Mid           │
│  │ (LO1) (LO2) (LO3) (LO4)                              │  ← EQ Low           │
│  │ (QF1) (QF2) (QF3) (QF4)                              │  ← Quick FX         │
│  │                                                        │                     │
│  │ [PF1][F1a][F2a]  [PF2][F1b][F2b]  ...x4              │  ← PFL, FX assign   │
│  │                                                        │                     │
│  │ ═══  ═══  ═══  ═══                                    │  ← Volume faders    │
│  │ V1   V2   V3   V4                                    │                     │
│  │                                                        │                     │
│  │ (HP VOL) (HP MIX) (MASTER VOL)                        │                     │
│  │ ═══════ XFADER ═══════ (optional)                     │                     │
│  │                                                        │                     │
│  │            [S1][S2][S3][S4]                            │  ← Sampler triggers │
│  └────────────────────────────────────────────────────────┘                     │
│                                                                                │
├──── DJ STANDS HERE (BOTTOM EDGE) ──────────────────────────────────────────────┤
└────────────────────────────────────────────────────────────────────────────────┘

Legend:
  (XX)  = Rotary knob/pot        [XX]  = Button (LED where applicable)
  ═══   = Fader (motorized where noted)
  enc   = Rotary encoder         [88]  = 2-digit 7-segment display
  POS   = Playback position motorized fader (horizontal)
  BPM   = BPM/Rate motorized fader
  JS    = Scrub joystick (1-axis, spring-return)
  HC    = Hotcue buttons
  BJ    = Beat jump encoder      LP    = Loop encoder
  LI/LO/LD = Loop In/Out/Deactivate
  K1-3  = FX knobs   DW = Dry/Wet   E1-3 = FX enable   SE = FX select
  GN    = Gain       HI/MD/LO = EQ   QF = Quick FX
  PF    = PFL/Cue    F1/F2 = FX assign   LD = Load track
  S1-4  = Sampler trigger buttons
```

### 9.2 Dimensions (Estimated)

- **Width**: ~600mm (decks + center column)
- **Depth**: ~450mm (stacked decks + mixer)
- **Height**: ~45mm
- **Weight**: ~2.5 kg

---

## 10. Wiring Diagram

### 10.1 System-Level Wiring

```
                         ┌──────────────────────────────────────┐
                         │         USB-C CONNECTOR              │
                         │         (Power + Data)               │
                         └───────────┬──────────────────────────┘
                                     │ 5V + D+/D-
                                     │
                              ┌──────┴──────┐
                              │ TEENSY 4.1  │
                              │ i.MX RT1062 │
                              │ Cortex-M7   │
                              │ @ 600 MHz   │
                              │             │
                              │ USB Composite│
                              │ MIDI + Audio │
                              └──┬─┬─┬─┬─┬──┘
                    I2C ─────────┘ │ │ │ └────── SPI
                    I2S ───────────┘ │ └──────── GPIO/ADC
                    PWM ─────────────┘
                       │    │    │    │    │
          ┌────────────┘    │    │    │    └────────────┐
          │                 │    │    │                  │
          ▼                 ▼    │    ▼                  ▼
     ┌────────┐       ┌──────┐  │  ┌─────┐       ┌───────────┐
     │2×MCP   │       │2×ADS │  │  │2×MAX│       │2×PCM5102A │
     │23017   │       │1115  │  │  │7219 │       │   DAC     │
     │I/O Exp │       │ADC   │  │  │7-seg│       │           │
     │(I2C)   │       │(I2C) │  │  │(SPI)│       │ SAI1:Mastr│
     └───┬────┘       └──┬───┘  │  └─┬──┘       │ SAI2:HP   │
         │                │     │    │           └──┬──┬─────┘
         │                │     │    │              │  │
         ▼                ▼     │    ▼              ▼  ▼
     ┌────────┐      ┌──────┐  │  ┌────┐      ┌──────┐┌────────┐
     │Button  │      │8×Mot.│  │  │4×  │      │OPA   ││TPA6120 │
     │Matrix  │      │Fader │  │  │7seg│      │2134  ││A2 HP   │
     │10×10   │      │Pots  │  │  │Disp│      │Buffer││Amp     │
     └────────┘      └──────┘  │  └────┘      └──┬───┘└──┬─────┘
                               │                  │       │
     ┌────────────┐            │                  ▼       ▼
     │PCA9685     │            │            ┌────────┐┌───────┐
     │16-ch PWM   │            │            │Master  ││HP Out │
     │(I2C)       │            │            │RCA/TRS ││3.5/6.3│
     └──┬─────────┘            │            │Booth   ││mm     │
        │                      │            └────────┘└───────┘
        ▼                      │
     ┌──────────┐              │
     │4×DRV8833 │              │     ┌──────────────┐
     │Motor     │              │     │8× 74HC595    │
     │Drivers   │              │     │Shift Regs    │
     └──┬───────┘              │     │(SPI)         │
        │                      │     └──┬───────────┘
        ▼                      │        │
     ┌──────────┐              │        ▼
     │Motor Kill│              │     ┌─────┐
     │Switch    │              │     │60+  │
     │(P-MOSFET)│              │     │LEDs │
     └──────────┘              │     └─────┘
                               │
                        ┌──────┴──────┐
                        │Analog inputs│
                        │3×CD74HC4067 │
                        │MUX (all     │
                        │inputs muxed)│
                        └─────────────┘
```

### 10.2 I2C Bus (Wire — pins 18/19)

All I2C devices share one I2C bus with different addresses:

```
Teensy 4.1 Wire (Pin 18 = SDA, Pin 19 = SCL)
    │
    ├── MCP23017 #1 (addr 0x20) — Button matrix rows/cols (bank A)
    ├── MCP23017 #2 (addr 0x21) — Button matrix rows/cols (bank B)
    ├── ADS1115 #1  (addr 0x48) — Motor fader pots 1-4
    ├── ADS1115 #2  (addr 0x49) — Motor fader pots 5-8
    └── PCA9685     (addr 0x40) — 16-ch PWM for motor drivers
```

### 10.3 SPI Bus (SPI — pins 11/13)

```
Teensy 4.1 SPI (Pin 13=SCK, Pin 11=MOSI, Pin 10=CS_SR, Pin 9=CS_7SEG)
    │
    ├── 74HC595 chain (8 ICs daisy-chained, latch on CS_SR)
    │   └── 64 LED outputs (directly driving LEDs via 220Ω resistors)
    │
    └── MAX7219 chain (2 ICs daisy-chained, CS_7SEG)
        └── 4× 2-digit 7-segment displays (8 digits total)
```

### 10.4 Analog MUX → ADC

```
Teensy 4.1 ADC channels:

MUX-based (3× CD74HC4067, all sharing address lines):
    Pin A0 (14) ← CD74HC4067 #1 COM (pots 0-15: gains, EQ)
    Pin A1 (15) ← CD74HC4067 #2 COM (pots 16-31: QFX, FX, mixer knobs)
    Pin A2 (16) ← CD74HC4067 #3 COM (volumes, crossfader, joysticks)

MUX Address (shared, active for all 3 MUXes):
    Pin 2  → S0
    Pin 5  → S1
    Pin 6  → S2
    Pin 8  → S3

Note: All pot/fader/joystick analog inputs go through MUXes.
      No direct ADC pins are used because A4-A7 (pins 18-21) are
      committed to I2C (SDA/SCL) and I2S1 (LRCLK/BCLK).
      This design frees pins 22-36 for 9 dedicated encoder pairs.
```

### 10.5 Motor Driver Wiring

```
PCA9685 (I2C PWM)              DRV8833 #1 (Deck 1)
    Ch0 (PWM) ──────────────→ AIN1 (motor 1 fwd)
    Ch1 (PWM) ──────────────→ AIN2 (motor 1 rev)
    Ch2 (PWM) ──────────────→ BIN1 (motor 2 fwd)
    Ch3 (PWM) ──────────────→ BIN2 (motor 2 rev)

PCA9685                        DRV8833 #2 (Deck 2)
    Ch4 ────────────────────→ AIN1
    Ch5 ────────────────────→ AIN2
    Ch6 ────────────────────→ BIN1
    Ch7 ────────────────────→ BIN2

PCA9685                        DRV8833 #3 (Deck 3)
    Ch8  ───────────────────→ AIN1
    Ch9  ───────────────────→ AIN2
    Ch10 ───────────────────→ BIN1
    Ch11 ───────────────────→ BIN2

PCA9685                        DRV8833 #4 (Deck 4)
    Ch12 ───────────────────→ AIN1
    Ch13 ───────────────────→ AIN2
    Ch14 ───────────────────→ BIN1
    Ch15 ───────────────────→ BIN2

Motor Power (5V from USB):
    5V ──→ [Motor Kill Switch / P-MOSFET] ──→ DRV8833 VM pins (all 4)
                                               └── 4× 2200µF caps
    GND ──→ DRV8833 GND pins (all 4)

Each DRV8833:
    VM   ← 5V (switched)
    GND  ← GND
    AIN1 ← PCA9685 PWM
    AIN2 ← PCA9685 PWM
    BIN1 ← PCA9685 PWM
    BIN2 ← PCA9685 PWM
    AOUT1/AOUT2 ──→ Motor A terminals
    BOUT1/BOUT2 ──→ Motor B terminals
    nSLEEP ← 3.3V (always enabled; kill switch on power rail)
    nFAULT → (optional: read by Teensy GPIO for fault detection)

Motor potentiometer wipers:
    → ADS1115 analog inputs (0-3.3V, via voltage divider if needed)
```

### 10.6 Audio Wiring (I2S via SAI1/SAI2)

```
Teensy 4.1 I2S:
    Pin 21 (BCLK1)  ──→ PCM5102A #1 BCK   (SAI1 = Master DAC)
    Pin 20 (LRCLK1) ──→ PCM5102A #1 LRCK
    Pin 7  (TX1)     ──→ PCM5102A #1 DIN

    Pin 4  (BCLK2)   ──→ PCM5102A #2 BCK   (SAI2 = Headphone DAC)
    Pin 3  (LRCLK2)  ──→ PCM5102A #2 LRCK
    Pin 2  (TX2)      ──→ PCM5102A #2 DIN

    NOTE: Pins 2-5 conflict with MUX address pins. See Section 11
    for the actual pin assignment — SAI2 uses alternate pins via
    Teensy pin muxing, or the MUX address lines are reassigned.
    Final assignment in pin table below resolves all conflicts.

PCM5102A #1 (Master):
    VCC  ← 3.3V (LDO, isolated analog)
    GND  ← AGND
    BCK  ← Teensy BCLK1
    LRCK ← Teensy LRCLK1
    DIN  ← Teensy TX1
    SCK  ← GND (auto clock mode)
    FMT  ← GND (I2S format)
    XSMT ← 3.3V (unmute)
    FLT  ← GND (normal latency)
    DEMP ← GND (de-emphasis off)
    VOUTL ──→ OPA2134 IN+ (ch A)
    VOUTR ──→ OPA2134 IN+ (ch B)

OPA2134 (Master Buffer):
    V+   ← 5V
    V-   ← GND (single supply with virtual ground at 2.5V, or ±5V)
    Ch A IN+  ← PCM5102A VOUTL
    Ch A IN-  ← Ch A OUT (unity gain)
    Ch A OUT  ──→ Master L (RCA + TRS)
               ──→ Booth L (via 10kΩ volume pot)
    Ch B IN+  ← PCM5102A VOUTR
    Ch B IN-  ← Ch B OUT
    Ch B OUT  ──→ Master R (RCA + TRS)
               ──→ Booth R (via 10kΩ volume pot)

PCM5102A #2 (Headphone):
    Same pinout as #1, connected to SAI2 I2S bus
    VOUTL/R ──→ TPA6120A2 inputs

TPA6120A2 (Headphone Amp):
    V+   ← +5V (or +9V via boost)
    V-   ← -5V (charge pump ICL7660 or similar)
    INL  ← PCM5102A #2 VOUTL
    INR  ← PCM5102A #2 VOUTR
    OUTL ──→ HP volume pot wiper L ──→ 3.5mm TRS tip
    OUTR ──→ HP volume pot wiper R ──→ 3.5mm TRS ring
                                   ──→ 6.35mm TRS
    GND  ← AGND
```

---

## 11. Pin Assignment Tables

### 11.1 Teensy 4.1 — Complete Pin Assignment

The Teensy 4.1 has 42 header pins (0-41). All are assigned below with **zero conflicts**. Pins A4-A7 (18-21) are committed to I2C and I2S1, so all pot/fader/joystick inputs are multiplexed through 3× CD74HC4067. This frees pins 22-36 exclusively for 9 encoder pairs.

**I2S / Audio Pins (fixed by hardware):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| 7 | I2S1 TX (data out) | PCM5102A #1 DIN (Master) |
| 20 | I2S1 LRCLK | PCM5102A #1 LRCK |
| 21 | I2S1 BCLK | PCM5102A #1 BCK |
| 32 | I2S2 TX (data out) | PCM5102A #2 DIN (Headphone) |
| 4 | I2S2 BCLK | PCM5102A #2 BCK |
| 3 | I2S2 LRCLK | PCM5102A #2 LRCK |

**I2C Bus (Wire):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| 18 | SDA0 (Wire) | MCP23017 ×2, ADS1115 ×2, PCA9685 |
| 19 | SCL0 (Wire) | MCP23017 ×2, ADS1115 ×2, PCA9685 |

**SPI Bus (SPI):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| 13 | SCK (SPI) | MAX7219 + 74HC595 clock |
| 11 | MOSI (SPI) | MAX7219 + 74HC595 data |
| 10 | CS_SR | 74HC595 latch (LED shift registers) |
| 9 | CS_7SEG | MAX7219 load (7-segment displays) |

**MUX Address Lines (shared by all 3 MUXes):**

| Pin | Function | Connected To |
|-----|----------|-------------|
| 2 | MUX S0 | CD74HC4067 ×3 address bit 0 |
| 5 | MUX S1 | CD74HC4067 ×3 address bit 1 |
| 6 | MUX S2 | CD74HC4067 ×3 address bit 2 |
| 8 | MUX S3 | CD74HC4067 ×3 address bit 3 |

**MUX COM → ADC Inputs:**

| Pin | Function | Connected To |
|-----|----------|-------------|
| A0 (14) | ADC MUX #1 | CD74HC4067 #1 COM (gains, EQ) |
| A1 (15) | ADC MUX #2 | CD74HC4067 #2 COM (QFX, FX, mixer pots) |
| A2 (16) | ADC MUX #3 | CD74HC4067 #3 COM (volumes, crossfader, joysticks) |

**Encoder Pins (9 encoders × 2 pins = 18):**

| Pins | Function | Connected To |
|------|----------|-------------|
| 0, 1 | Encoder 1 (Deck1 BeatJump) | EC11 encoder A/B |
| 12, 17 | Encoder 2 (Deck1 Loop) | EC11 encoder A/B |
| 22, 23 | Encoder 3 (Deck2 BeatJump) | EC11 encoder A/B |
| 24, 25 | Encoder 4 (Deck2 Loop) | EC11 encoder A/B |
| 26, 27 | Encoder 5 (Deck3 BeatJump) | EC11 encoder A/B |
| 28, 29 | Encoder 6 (Deck3 Loop) | EC11 encoder A/B |
| 30, 31 | Encoder 7 (Deck4 BeatJump) | EC11 encoder A/B |
| 33, 34 | Encoder 8 (Deck4 Loop) | EC11 encoder A/B |
| 35, 36 | Encoder 9 (Browse) | EC11 encoder A/B |

**Motor Kill + Spare:**

| Pin | Function | Connected To |
|-----|----------|-------------|
| 37 | Motor Kill Switch | GPIO input (reads toggle switch state) |
| 38 | Spare (A14) | — |
| 39 | Spare (A15) | — |
| 40 | Spare (A16) | — |
| 41 | Spare (A17) | — |

**Pin conflict verification:**
- Pins 18-19 (A4-A5): I2C only — NOT used as ADC.
- Pins 20-21 (A6-A7): I2S1 only — NOT used as ADC.
- Pins 22-27 (A8-A13): encoder GPIO — NOT used as ADC.
- All analog inputs route through MUXes to A0, A1, A2 — no overlap with any peripheral.
- MUX address lines (2, 5, 6, 8) do not conflict with I2S2 (3, 4, 32).

### 11.2 MCP23017 #1 (Addr 0x20) — Button Matrix Bank A

| Pin | Port | Function | Connected To |
|-----|------|----------|-------------|
| GPA0 | A0 | Matrix Col 0 (output, active low) | Button col wire |
| GPA1 | A1 | Matrix Col 1 | Button col wire |
| GPA2 | A2 | Matrix Col 2 | Button col wire |
| GPA3 | A3 | Matrix Col 3 | Button col wire |
| GPA4 | A4 | Matrix Col 4 | Button col wire |
| GPA5 | A5 | Matrix Col 5 | Button col wire |
| GPA6 | A6 | Matrix Col 6 | Button col wire |
| GPA7 | A7 | Matrix Col 7 | Button col wire |
| GPB0 | B0 | Matrix Row 0 (input, pull-up) | Button row wire |
| GPB1 | B1 | Matrix Row 1 | Button row wire |
| GPB2 | B2 | Matrix Row 2 | Button row wire |
| GPB3 | B3 | Matrix Row 3 | Button row wire |
| GPB4 | B4 | Matrix Row 4 | Button row wire |
| GPB5 | B5 | Matrix Row 5 | Button row wire |
| GPB6 | B6 | Matrix Row 6 | Button row wire |
| GPB7 | B7 | Matrix Row 7 | Button row wire |
| INTA | — | Interrupt A | Teensy pin (optional) |
| SDA | — | I2C data | Teensy pin 18 |
| SCL | — | I2C clock | Teensy pin 19 |
| A0-A2 | — | Address = 000 | GND (addr 0x20) |

### 11.3 MCP23017 #2 (Addr 0x21) — Button Matrix Bank B

| Pin | Port | Function | Connected To |
|-----|------|----------|-------------|
| GPA0-7 | A0-A7 | Matrix Col 8-9 + Toggle switches | Buttons/toggles |
| GPB0-7 | B0-B7 | Matrix Row 8-9 + spare | Button row wire |
| A0-A2 | — | Address = 001 | A0 → 3.3V (addr 0x21) |

### 11.4 ADS1115 #1 (Addr 0x48) — Motor Fader Pots 1-4

| Pin | Function | Connected To |
|-----|----------|-------------|
| AIN0 | Analog In 0 | Deck 1 BPM fader pot wiper |
| AIN1 | Analog In 1 | Deck 1 Position fader pot wiper |
| AIN2 | Analog In 2 | Deck 2 BPM fader pot wiper |
| AIN3 | Analog In 3 | Deck 2 Position fader pot wiper |
| SDA | I2C data | Teensy pin 18 |
| SCL | I2C clock | Teensy pin 19 |
| ADDR | Address | GND (0x48) |
| VDD | Power | 3.3V |

### 11.5 ADS1115 #2 (Addr 0x49) — Motor Fader Pots 5-8

| Pin | Function | Connected To |
|-----|----------|-------------|
| AIN0 | Analog In 0 | Deck 3 BPM fader pot wiper |
| AIN1 | Analog In 1 | Deck 3 Position fader pot wiper |
| AIN2 | Analog In 2 | Deck 4 BPM fader pot wiper |
| AIN3 | Analog In 3 | Deck 4 Position fader pot wiper |
| ADDR | Address | VDD (0x49) |

### 11.6 PCA9685 (Addr 0x40) — Motor PWM

| Channel | Function | Connected To |
|---------|----------|-------------|
| Ch 0 | Deck 1 BPM motor fwd | DRV8833 #1 AIN1 |
| Ch 1 | Deck 1 BPM motor rev | DRV8833 #1 AIN2 |
| Ch 2 | Deck 1 Pos motor fwd | DRV8833 #1 BIN1 |
| Ch 3 | Deck 1 Pos motor rev | DRV8833 #1 BIN2 |
| Ch 4 | Deck 2 BPM motor fwd | DRV8833 #2 AIN1 |
| Ch 5 | Deck 2 BPM motor rev | DRV8833 #2 AIN2 |
| Ch 6 | Deck 2 Pos motor fwd | DRV8833 #2 BIN1 |
| Ch 7 | Deck 2 Pos motor rev | DRV8833 #2 BIN2 |
| Ch 8 | Deck 3 BPM motor fwd | DRV8833 #3 AIN1 |
| Ch 9 | Deck 3 BPM motor rev | DRV8833 #3 AIN2 |
| Ch 10 | Deck 3 Pos motor fwd | DRV8833 #3 BIN1 |
| Ch 11 | Deck 3 Pos motor rev | DRV8833 #3 BIN2 |
| Ch 12 | Deck 4 BPM motor fwd | DRV8833 #4 AIN1 |
| Ch 13 | Deck 4 BPM motor rev | DRV8833 #4 AIN2 |
| Ch 14 | Deck 4 Pos motor fwd | DRV8833 #4 BIN1 |
| Ch 15 | Deck 4 Pos motor rev | DRV8833 #4 BIN2 |

### 11.7 CD74HC4067 MUX Channel Assignments

**MUX #1 (ADC via Teensy A0, pin 14):**

| Ch | Analog Input |
|----|-------------|
| 0 | Deck 1 Gain pot |
| 1 | Deck 2 Gain pot |
| 2 | Deck 3 Gain pot |
| 3 | Deck 4 Gain pot |
| 4 | Ch1 EQ High |
| 5 | Ch2 EQ High |
| 6 | Ch3 EQ High |
| 7 | Ch4 EQ High |
| 8 | Ch1 EQ Mid |
| 9 | Ch2 EQ Mid |
| 10 | Ch3 EQ Mid |
| 11 | Ch4 EQ Mid |
| 12 | Ch1 EQ Low |
| 13 | Ch2 EQ Low |
| 14 | Ch3 EQ Low |
| 15 | Ch4 EQ Low |

**MUX #2 (ADC via Teensy A1, pin 15):**

| Ch | Analog Input |
|----|-------------|
| 0 | Ch1 Quick FX |
| 1 | Ch2 Quick FX |
| 2 | Ch3 Quick FX |
| 3 | Ch4 Quick FX |
| 4 | FX1 Knob 1 |
| 5 | FX1 Knob 2 |
| 6 | FX1 Knob 3 |
| 7 | FX1 Dry/Wet |
| 8 | FX2 Knob 1 |
| 9 | FX2 Knob 2 |
| 10 | FX2 Knob 3 |
| 11 | FX2 Dry/Wet |
| 12 | Headphone Mix pot |
| 13 | Master Volume pot |
| 14 | Headphone Volume pot |
| 15 | Spare |

**MUX #3 (ADC via Teensy A2, pin 16):**

| Ch | Analog Input |
|----|-------------|
| 0 | Ch1 Volume fader |
| 1 | Ch2 Volume fader |
| 2 | Ch3 Volume fader |
| 3 | Ch4 Volume fader |
| 4 | Crossfader |
| 5 | Deck 1 Scrub Joystick |
| 6 | Deck 2 Scrub Joystick |
| 7 | Deck 3 Scrub Joystick |
| 8 | Deck 4 Scrub Joystick |
| 9-15 | Spare (7 channels) |

---

## 12. Bill of Materials

### 12.1 Microcontrollers & ICs

| Component | Qty | Unit Cost | Total | Notes |
|-----------|-----|-----------|-------|-------|
| Teensy 4.1 | 1 | $29.25 | $29.25 | Main MCU (MIDI + Audio) |
| MCP23017 I2C I/O Expander (DIP/SOIC) | 2 | $1.80 | $3.60 | Button matrix |
| ADS1115 16-bit ADC (breakout) | 2 | $3.50 | $7.00 | Motor fader position |
| PCA9685 16-ch PWM (breakout) | 1 | $3.00 | $3.00 | Motor PWM generation |
| CD74HC4067 16-ch MUX (breakout) | 3 | $1.00 | $3.00 | Analog input MUX |
| DRV8833 Dual H-Bridge (breakout) | 4 | $2.50 | $10.00 | Motor drivers |
| PCM5102A DAC (breakout) | 2 | $4.00 | $8.00 | Audio output |
| MAX7219 LED Driver (breakout) | 2 | $1.50 | $3.00 | 7-segment displays |
| 74HC595 Shift Register (DIP) | 8 | $0.30 | $2.40 | LED output |
| OPA2134 Dual Op-Amp (DIP) | 1 | $4.00 | $4.00 | Master audio buffer |
| TPA6120A2 Headphone Amp | 1 | $6.00 | $6.00 | HP driver |
| ICL7660 Charge Pump | 1 | $1.00 | $1.00 | -5V for HP amp |
| AMS1117-3.3 LDO Regulator | 1 | $0.30 | $0.30 | 3.3V for DACs (isolated) |
| IRF9540 P-Channel MOSFET | 1 | $0.80 | $0.80 | Motor kill switch |

### 12.2 Electromechanical

| Component | Qty | Unit Cost | Total | Notes |
|-----------|-----|-----------|-------|-------|
| Motorized Slide Pot 100mm (B10K) | 8 | $8.00 | $64.00 | Alibaba sourced |
| Standard Slide Pot 60mm (B10K) | 5 | $2.00 | $10.00 | Volume + crossfader |
| Rotary Potentiometer B10K | 31 | $0.60 | $18.60 | EQ, FX, gain, mixer |
| Rotary Encoder w/ Push (EC11) | 9 | $1.00 | $9.00 | BeatJump, Loop, Browse |
| Tactile Push Button (6mm, LED) | 93 | $0.30 | $27.90 | All buttons |
| Toggle Switch (SPDT, mini) | 5 | $0.50 | $2.50 | HC/Roll mode ×4, Motor kill |
| 1-Axis Analog Joystick (spring-return) | 4 | $2.50 | $10.00 | Deck scrub joysticks |
| 2-Digit 7-Segment Display (common cathode) | 4 | $0.80 | $3.20 | Loop/BJ size display |
| Knob Cap (D-shaft, 6mm) | 31 | $0.40 | $12.40 | For rotary pots |
| Fader Knob Cap | 13 | $0.80 | $10.40 | For slide pots |

### 12.3 Connectors

| Component | Qty | Unit Cost | Total | Notes |
|-----------|-----|-----------|-------|-------|
| USB-C Receptacle + Breakout | 1 | $2.50 | $2.50 | Main power/data |
| RCA Jack Pair (red/white) | 2 | $1.50 | $3.00 | Master + Booth |
| TRS 1/4" Jack (6.35mm) | 2 | $1.50 | $3.00 | Master + Booth |
| TRS 3.5mm Jack | 1 | $0.50 | $0.50 | Headphone |
| TRS 6.35mm Jack | 1 | $1.00 | $1.00 | Headphone |
| XLR/TRS Combo Jack (Neutrik) | 1 | $5.00 | $5.00 | Future mic input |
| 2.54mm Pin Headers (40-pin) | 10 | $0.30 | $3.00 | Board interconnects |
| JST-XH Connectors (2-6 pin) | 20 | $0.20 | $4.00 | Internal wiring |
| Ribbon Cable (20-conductor) | 2m | $1.50 | $3.00 | Internal bus wiring |

### 12.4 Passive Components

| Component | Qty | Unit Cost | Total | Notes |
|-----------|-----|-----------|-------|-------|
| **Resistors** | | | | |
| 220Ω 1/4W (LED current limiting) | 70 | $0.02 | $1.40 | For 74HC595 LED outputs |
| 10kΩ 1/4W (pull-up/pull-down) | 30 | $0.02 | $0.60 | I2C pull-ups, MUX, buttons |
| 4.7kΩ 1/4W (I2C pull-up) | 2 | $0.02 | $0.04 | I2C bus SDA/SCL |
| 100kΩ 1/4W (voltage dividers) | 16 | $0.02 | $0.32 | Back-EMF dividers |
| 47kΩ 1/4W (voltage dividers) | 8 | $0.02 | $0.16 | Back-EMF dividers |
| 1kΩ 1/4W (general purpose) | 10 | $0.02 | $0.20 | Gate resistors, misc |
| 10Ω 1/4W (ferrite alternative) | 4 | $0.02 | $0.08 | Power rail filtering |
| **Capacitors** | | | | |
| 100nF (0.1µF) ceramic | 40 | $0.03 | $1.20 | Decoupling caps for every IC |
| 10µF electrolytic | 10 | $0.05 | $0.50 | Power supply filtering |
| 2200µF 10V electrolytic | 4 | $0.40 | $1.60 | Motor surge current bank |
| 1µF ceramic | 4 | $0.05 | $0.20 | DAC output filtering |
| 10nF ceramic | 8 | $0.03 | $0.24 | High-frequency filtering |
| 22pF ceramic | 4 | $0.03 | $0.12 | Crystal load caps (if needed) |
| **Diodes** | | | | |
| 1N4148 Signal Diode | 100 | $0.02 | $2.00 | Button matrix anti-ghosting |
| 1N5819 Schottky Diode | 8 | $0.05 | $0.40 | Motor flyback (backup) |
| **Inductors / Ferrites** | | | | |
| Ferrite Bead 600Ω@100MHz | 4 | $0.10 | $0.40 | Motor/audio power isolation |

### 12.5 PCB & Enclosure

| Component | Qty | Unit Cost | Total | Notes |
|-----------|-----|-----------|-------|-------|
| Custom PCB (JLCPCB, 2-layer) | 1 | $15.00 | $15.00 | Single main board |
| Acrylic Sheet (3mm, 600×450mm) | 2 | $10.00 | $20.00 | Top panel + base |
| 3D Printed Enclosure Walls | 1 | $15.00 | $15.00 | PLA/PETG |
| Standoffs, M3 screws, nuts | 1 set | $5.00 | $5.00 | Assembly hardware |
| Rubber feet | 4 | $0.50 | $2.00 | Anti-slip |
| Wire (22 AWG, assorted) | 1 spool | $5.00 | $5.00 | Internal wiring |
| Heat shrink tubing | 1 pack | $3.00 | $3.00 | Wire insulation |
| Solder | 1 roll | $5.00 | $5.00 | Assembly |

### 12.6 Cost Summary

| Category | Total |
|----------|-------|
| ICs & MCUs | $81.35 |
| Electromechanical | $167.00 |
| Connectors | $25.00 |
| Passive Components | $9.46 |
| PCB & Enclosure | $70.00 |
| **Grand Total** | **~$353** |

*Note: Prices are approximate. The Teensy 4.1 costs ~$20 more than dual Picos, but eliminates the USB hub ($1.50) and one PCB ($15). Net cost is roughly equivalent. Does not include shipping or tools.*

---

## 13. Firmware Architecture

### 13.1 Unified Firmware — C++ / Teensyduino

The entire controller runs on a single Teensy 4.1 with a monolithic firmware. The Teensyduino core provides USB composite device support (MIDI + Audio), the Teensy Audio Library handles I2S streaming, and all I/O scanning runs in the main loop or timer interrupts.

```
┌─────────────────────────────────────────────────────────────────┐
│                    Teensy 4.1 Firmware                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌────────────────────────────┐   ┌────────────────────────┐   │
│  │  USB Composite Device      │   │  Teensy Audio Library   │   │
│  │  (Teensyduino core)        │   │                        │   │
│  │                            │   │  AudioInputUSB (host→) │   │
│  │  USB MIDI  ←→ Host MIDI    │   │  AudioOutputI2S (SAI1) │   │
│  │  USB Audio ←→ Host Audio   │   │  AudioOutputI2S2(SAI2) │   │
│  └────────────────────────────┘   └────────────────────────┘   │
│                                                                 │
│  ┌──────────────┐    ┌────────────────────┐                    │
│  │ Analog Scan   │    │  Motor Control     │                    │
│  │ (3× MUX→ADC)  │    │  (PID + Haptic)    │                    │
│  │ 48 MUX ch      │    │  8 faders          │                    │
│  │ ~1 kHz rate   │    │  PCA9685 PWM out   │                    │
│  └──────────────┘    │  Back-EMF detect   │                    │
│                       └────────────────────┘                    │
│                                                                 │
│  ┌──────────────┐    ┌────────────────────┐                    │
│  │ Button Matrix │    │  LED Output        │                    │
│  │ (MCP23017)    │    │  74HC595 (SPI)     │                    │
│  │ 10×10 scan    │    │  64 outputs        │                    │
│  │ ~500 Hz       │    │                    │                    │
│  └──────────────┘    └────────────────────┘                    │
│                                                                 │
│  ┌──────────────┐    ┌────────────────────┐                    │
│  │ Encoder Read  │    │  7-Seg Display     │                    │
│  │ (GPIO ISR /   │    │  MAX7219 (SPI)     │                    │
│  │  quad timer)  │    │  4× 2-digit        │                    │
│  │ 9 encoders    │    │                    │                    │
│  └──────────────┘    └────────────────────┘                    │
│                                                                 │
│  ┌──────────────┐                                              │
│  │ Calibration   │                                              │
│  │ EEPROM store  │                                              │
│  └──────────────┘                                              │
└─────────────────────────────────────────────────────────────────┘
```

### 13.2 USB Device Type

Set in the Arduino IDE / PlatformIO build flags:

```
USB Type: "Serial + MIDI + Audio"
```

This creates a composite USB device with:
- USB MIDI (1 IN + 1 OUT cable)
- USB Audio (4 output channels = 2 stereo pairs, 48 kHz / 16-bit)
- USB Serial (for debug logging)

### 13.3 Firmware Modules

| Module | File | Purpose |
|--------|------|---------|
| Main | `BrevvDeck.ino` | Setup, main loop, task scheduling |
| Analog | `analog.h/cpp` | MUX scanning + direct ADC reading |
| Buttons | `buttons.h/cpp` | MCP23017 button matrix scanning |
| Encoders | `encoders.h/cpp` | Rotary encoder reading (ISR or polling) |
| Motors | `motors.h/cpp` | PID control, haptic state machine, ADS1115 reading |
| LEDs | `leds.h/cpp` | 74HC595 shift register output |
| Display | `display.h/cpp` | MAX7219 7-segment display |
| MidiMap | `midi_map.h` | Channel, note, CC constants |
| Audio | `audio.h/cpp` | Teensy Audio Library setup |
| Config | `config.h` | Pin assignments, tuning parameters |

See `brevvdeck-firmware/` directory for complete firmware source files.

---

## 14. Mixxx Mapping Strategy

### 14.1 XML-First Approach

MIDI CC/Note assignments are chosen so most controls map directly in XML without JavaScript.

**Per-deck MIDI assignment (channels 1-4):**

| MIDI Msg | Number | Mixxx Control |
|----------|--------|---------------|
| Note 0x00 | Play | `play` |
| Note 0x01 | Cue | `cue_default` |
| Note 0x02 | Sync | `sync_enabled` |
| Note 0x03 | Keylock | `keylock` |
| Note 0x04 | Quantize | `quantize` |
| Note 0x05 | Loop In | `loop_in` |
| Note 0x06 | Loop Out | `loop_out` |
| Note 0x07 | Loop Deactivate | `reloop_toggle` |
| Note 0x10-0x17 | Hotcue/Roll 1-8 | Script-bound (mode-dependent) |
| Note 0x21 | BJ Encoder Push | Script-bound (cycle BJ size) |
| Note 0x30 | Shift | Script-bound |
| CC 0x00 | Rate MSB | `rate` (14-bit with CC 0x20) |
| CC 0x01 | Position MSB | Script-bound (seek) |
| CC 0x02 | Scrub Joystick | Script-bound (variable-speed scrub) |
| CC 0x10 | BeatJump Encoder | Script-bound (relative) |
| CC 0x11 | Loop Encoder | Script-bound (relative) |
| CC 0x20 | Rate LSB | `rate` (14-bit pair) |
| CC 0x21 | Position LSB | Script-bound |

**Mixer MIDI (channel 5):**

| MIDI Msg | Number | Mixxx Control |
|----------|--------|---------------|
| CC 0x00-0x03 | Volume Ch1-4 | `volume` |
| CC 0x04-0x07 | EQ High Ch1-4 | `parameter3` |
| CC 0x08-0x0B | EQ Mid Ch1-4 | `parameter2` |
| CC 0x0C-0x0F | EQ Low Ch1-4 | `parameter1` |
| CC 0x10-0x13 | Quick FX Ch1-4 | `super1` |
| CC 0x14 | HP Mix | `headMix` |
| CC 0x15 | HP Volume | `headVolume` |
| CC 0x16 | Master Volume | `volume` |
| CC 0x17 | Crossfader | `crossfader` |
| CC 0x18-0x1B | Gain Ch1-4 | `pregain` |
| Note 0x00-0x03 | PFL Ch1-4 | `pfl` |
| Note 0x04-0x07 | FX1 Assign Ch1-4 | `group_[ChannelN]_enable` |
| Note 0x08-0x0B | FX2 Assign Ch1-4 | `group_[ChannelN]_enable` |
| Note 0x0C-0x0F | Load Ch1-4 | `LoadSelectedTrack` |

**Sampler MIDI (channel 8):**

| MIDI Msg | Number | Mixxx Control |
|----------|--------|---------------|
| Note 0x10-0x13 | Sampler 1-4 | Script-bound |

### 14.2 JavaScript Responsibilities

- Beat jump encoder: normal → jump fwd/back; shift+turn → halve/double jump size
- Loop encoder: rotate → halve/double loop size
- Hotcue/Roll button mode switching (reads hardware toggle state via CC)
- Position fader 14-bit seeking
- Joystick variable-speed scrub (dead zone, proportional speed, shift multiplier)
- 7-segment display value updates (via MIDI CC output)
- FX focus cycling
- Sampler trigger logic
- Shift state management

### 14.3 Output Mapping

XML outputs send control values back to firmware for:
- LED states (play, cue, sync, keylock, quantize, loop, PFL, FX assign, FX enable, hotcue, sampler)
- Motor targets (rate, playposition — 7-bit CC, firmware interpolates)
- 7-segment display data (beatjump_size and loop size via CC to firmware)

---

## 15. Open Questions & Future Work

### Resolved in v0.3
- [x] Motorized fader touch detection → back-EMF + position monitoring (no cap sensor)
- [x] BPM fader size → 100mm (same as position, sourced from Alibaba)
- [x] Hotcue/roll dual mode → hardware toggle switch per deck
- [x] Single vs dual MCU → single Teensy 4.1 (composite MIDI + Audio)
- [x] USB Audio on RP2040 → no longer relevant; Teensy Audio Library handles it natively
- [x] Browse encoder GPIO → Teensy has enough pins; dedicated GPIO pair (39/40)
- [x] Rust USB Audio → switched to C++ / Teensyduino (mature composite USB + audio)

### Open
- [ ] **Touch strip**: SoftPot (resistive) vs capacitive (Azoteq IQS5xx)? → **Resolved in v0.4: replaced with 1-axis spring-return analog joysticks.** Joystick left/right controls variable-speed track scrubbing; dead zone around center prevents drift. Cost savings ~$18 vs SoftPot strips.
- [ ] **Back-EMF measurement**: Needs testing to validate fader detection threshold.
- [ ] **7-segment display update**: Best way to send display data from Mixxx? CC output, or firmware-side calculation from beatjump_size/loop values?
- [ ] **Power budget**: 8 motors + 2 DACs + HP amp. Need to verify USB-C 5V/3A is sufficient. Single MCU helps (no hub overhead).
- [ ] **Teensy USB Audio latency**: Test round-trip latency with Mixxx. Teensy Audio Library defaults to 128-sample blocks (~2.9ms at 44.1kHz).
- [ ] **I2S2 pin conflict resolution**: ~~Verify that the pin assignments in Section 11 work with Teensy's pin mux for SAI2 + MUX address lines.~~ → **Resolved in v0.4.** MUX address lines are on pins 2, 5, 6, 8; I2S2 is on pins 3, 4, 32. No conflicts.

### Future (V2+)
- [ ] OLED screens for track info
- [ ] RGB LED rings around encoders
- [ ] Balanced XLR outputs
- [ ] Microphone input with preamp
- [ ] DVS timecode input
- [ ] Battery with USB-C PD charging
- [ ] 96 kHz audio support
- [ ] Modular/detachable deck sections
- [ ] USB-A passthrough ports (optional small hub IC)

---

*BrevvDeck Design Document v0.4 — Fixed pin conflicts (3× MUX, zero ADC overlap), replaced touch strips with 1-axis scrub joysticks.*
