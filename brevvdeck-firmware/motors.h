// BrevvDeck Firmware — Motorized Fader Control
//
// 4× motorized faders (rate/pitch sliders), each with:
//   - ADS1115 16-bit ADC (I2C) for precise position reading
//   - PCA9685 12-bit PWM (I2C) for DRV8833 motor driver control
//   - PID control loop tracking target positions from host
//   - 3-state haptic machine: IDLE → SEEKING → HOLD → IDLE
//   - Touch detection (capacitive sense on fader rail) to pause motor
//   - 14-bit MIDI output when user overrides motor position

#pragma once

#include <Arduino.h>

void motorsInit();
void motorsScan();
void motorsSetTarget(uint8_t deck, uint16_t target14bit);
void motorsEnableKill(bool kill);
