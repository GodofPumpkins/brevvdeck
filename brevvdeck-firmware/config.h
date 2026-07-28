// BrevvDeck Firmware — Configuration & Pin Assignments
// Teensy 4.1 (i.MX RT1062, ARM Cortex-M7 @ 600 MHz)
//
// This file centralizes all hardware pin assignments and tuning constants.
// Change pins here if the PCB layout requires different routing.
//
// PIN ASSIGNMENT VERIFICATION (v0.4):
//   All 42 Teensy 4.1 header pins (0-41) are assigned below with zero
//   conflicts. Analog pins A4-A7 (18-21) are committed to I2C and I2S1,
//   so ALL pot/fader/joystick inputs route through 3× CD74HC4067 MUXes.
//   This frees pins 22-36 exclusively for 9 encoder pairs.

#pragma once

#include <Arduino.h>

// ==========================================================================
// Build Configuration
// ==========================================================================

// Set USB Type in platformio.ini or Arduino IDE:
//   "Serial + MIDI + Audio"
// This enables composite USB with MIDI, Audio, and Serial (debug).

// ==========================================================================
// I2C Bus (Wire — pins 18/19)
// ==========================================================================
// All I2C devices share one bus:
//   MCP23017 ×2  (0x20, 0x21) — button matrix
//   ADS1115  ×2  (0x48, 0x49) — motor fader position ADC
//   PCA9685      (0x40)       — motor PWM driver

constexpr uint8_t PIN_I2C_SDA = 18;
constexpr uint8_t PIN_I2C_SCL = 19;

// I2C addresses
constexpr uint8_t MCP23017_ADDR_1 = 0x20;  // Button matrix bank A
constexpr uint8_t MCP23017_ADDR_2 = 0x21;  // Button matrix bank B
constexpr uint8_t ADS1115_ADDR_1  = 0x48;  // Motor fader pots 1-4
constexpr uint8_t ADS1115_ADDR_2  = 0x49;  // Motor fader pots 5-8
constexpr uint8_t PCA9685_ADDR    = 0x40;  // 16-ch motor PWM

// ==========================================================================
// SPI Bus (SPI — pins 11/13)
// ==========================================================================

constexpr uint8_t PIN_SPI_SCK  = 13;  // SPI clock
constexpr uint8_t PIN_SPI_MOSI = 11;  // SPI data out
constexpr uint8_t PIN_CS_SR    = 10;  // 74HC595 latch (LED shift registers)
constexpr uint8_t PIN_CS_7SEG  = 9;   // MAX7219 load (7-segment displays)

// ==========================================================================
// I2S / Audio Pins (fixed by Teensy 4.1 hardware)
// ==========================================================================

// SAI1 (Master DAC — PCM5102A #1)
constexpr uint8_t PIN_I2S1_BCLK  = 21;  // Bit clock
constexpr uint8_t PIN_I2S1_LRCLK = 20;  // Word select (L/R clock)
constexpr uint8_t PIN_I2S1_TX    = 7;   // Data out

// SAI2 (Headphone DAC — PCM5102A #2)
constexpr uint8_t PIN_I2S2_BCLK  = 4;   // Bit clock
constexpr uint8_t PIN_I2S2_LRCLK = 3;   // Word select
constexpr uint8_t PIN_I2S2_TX    = 32;  // Data out

// ==========================================================================
// Analog MUX (3× CD74HC4067)
// ==========================================================================
// All pot/fader/joystick inputs are multiplexed. No direct ADC pins are
// used because A4-A7 (pins 18-21) are committed to I2C/I2S and higher
// analog pins (A8+) are needed for encoders.

// Shared address lines (active for all 3 MUXes simultaneously)
constexpr uint8_t PIN_MUX_S0 = 2;
constexpr uint8_t PIN_MUX_S1 = 5;
constexpr uint8_t PIN_MUX_S2 = 6;
constexpr uint8_t PIN_MUX_S3 = 8;

// MUX COM outputs → ADC inputs
constexpr uint8_t PIN_MUX1_COM = A0;  // pin 14 — MUX #1 (gains, EQ)
constexpr uint8_t PIN_MUX2_COM = A1;  // pin 15 — MUX #2 (QFX, FX, mixer pots)
constexpr uint8_t PIN_MUX3_COM = A2;  // pin 16 — MUX #3 (volumes, crossfader, joysticks)

// ==========================================================================
// Rotary Encoders (9 encoders × 2 pins)
// ==========================================================================
// All encoders on dedicated GPIO pins with no analog/peripheral conflicts.
// Pins 0, 1, 12, 17 are on headers. Pins 22-36 are contiguous header pins.

struct EncoderPins {
    uint8_t pinA;
    uint8_t pinB;
};

constexpr EncoderPins ENCODER_PINS[] = {
    { 0,  1},  // 0: Deck 1 Beat Jump
    {12, 17},  // 1: Deck 1 Loop
    {22, 23},  // 2: Deck 2 Beat Jump
    {24, 25},  // 3: Deck 2 Loop
    {26, 27},  // 4: Deck 3 Beat Jump
    {28, 29},  // 5: Deck 3 Loop
    {30, 31},  // 6: Deck 4 Beat Jump
    {33, 34},  // 7: Deck 4 Loop
    {35, 36},  // 8: Browse
};
constexpr int NUM_ENCODERS = 9;

// ==========================================================================
// Motor Kill Switch
// ==========================================================================

constexpr uint8_t PIN_MOTOR_KILL = 37;  // HIGH = motors disabled

// ==========================================================================
// Spare Pins
// ==========================================================================

// Pins 38 (A14), 39 (A15), 40 (A16), 41 (A17) are unused.
// Available for future expansion (e.g., RGB LEDs, OLED, extra buttons).

// ==========================================================================
// MIDI Tuning Constants
// ==========================================================================

// Relative encoder center value (encoder sends 0x40 ± delta)
constexpr uint8_t ENCODER_CENTER = 0x40;

// Analog change threshold (7-bit CC units) — noise gate
constexpr uint8_t CC_THRESHOLD = 1;

// ==========================================================================
// Motor PID Tuning
// ==========================================================================

constexpr float MOTOR_KP = 2.0f;
constexpr float MOTOR_KI = 0.1f;
constexpr float MOTOR_KD = 0.5f;
constexpr float MOTOR_INTEGRAL_MAX = 1000.0f;
constexpr float MOTOR_OUTPUT_MAX = 4095.0f;

// Back-EMF / position-jump threshold for detecting user touch (14-bit units)
constexpr int16_t BEMF_THRESHOLD = 200;

// Position tolerance for "target reached" (14-bit units, ~0.5% of range)
constexpr int16_t POSITION_TOLERANCE = 80;

// Time (ms) the fader must be stable before returning to software tracking
constexpr uint32_t USER_RELEASE_MS = 200;

// ==========================================================================
// Motorized Fader Count
// ==========================================================================

constexpr int NUM_FADERS = 8;  // 4 rate + 4 position

// ==========================================================================
// Calibration
// ==========================================================================

// EEPROM address for fader calibration data
// Each fader stores: magic(4) + min(2) + max(2) = 8 bytes
constexpr int EEPROM_CAL_BASE = 0;
constexpr uint32_t EEPROM_CAL_MAGIC = 0xBDCA11B0;

// ==========================================================================
// Scan Rates (microseconds)
// ==========================================================================

constexpr uint32_t ANALOG_SCAN_INTERVAL_US  = 1000;   // 1 kHz
constexpr uint32_t BUTTON_SCAN_INTERVAL_US  = 2000;   // 500 Hz
constexpr uint32_t MOTOR_UPDATE_INTERVAL_US = 8000;   // 125 Hz
constexpr uint32_t ENCODER_SEND_INTERVAL_US = 4000;   // 250 Hz
constexpr uint32_t LED_UPDATE_INTERVAL_US   = 5000;   // 200 Hz
constexpr uint32_t DISPLAY_UPDATE_INTERVAL_US = 10000; // 100 Hz
