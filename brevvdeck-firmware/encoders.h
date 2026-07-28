// BrevvDeck Firmware — Rotary Encoder Reading
//
// 9 rotary encoders read via GPIO with interrupt-driven quadrature decoding.
// Each encoder sends relative MIDI CC messages (center = 0x40, ±delta).
//
// Teensy 4.1's Cortex-M7 handles interrupts fast enough that GPIO-based
// quadrature decoding is reliable even at high rotation speeds.
// For even higher accuracy, the Encoder library (PJRC) uses hardware timers
// where available.

#pragma once

#include <Arduino.h>

void encodersInit();
void encodersSendMidi();
