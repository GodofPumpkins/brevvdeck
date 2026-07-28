// BrevvDeck Firmware — Analog Input Scanning
//
// Reads analog inputs from 3× CD74HC4067 16-channel MUXes (48 channels):
//   MUX #1: Gains (4) + EQ High/Mid/Low (12) = 16
//   MUX #2: Quick FX (4) + FX1 knobs (4) + FX2 knobs (4) + mixer pots (3) + Deck4 Pos fader (1) = 16
//   MUX #3: Volume faders (4) + crossfader (1) + joysticks (4) + Rate faders (4) + Pos faders 1-3 (3) = 16
//
// All analog inputs are multiplexed — no direct ADC pins are used.
// This avoids pin conflicts between A4-A7 (I2C/I2S) and encoder GPIO.
//
// Each value is converted to 7-bit MIDI CC (0-127) and sent when it changes
// by more than CC_THRESHOLD (noise gate / hysteresis).

#pragma once

#include <Arduino.h>

void analogInit();
void analogScan();
