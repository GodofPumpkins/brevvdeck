// BrevvDeck Firmware — Main Entry Point
// Target: PJRC Teensy 4.1 (i.MX RT1062, Cortex-M7 @ 600 MHz)
// Framework: Arduino (Teensyduino)
// USB Mode: Serial + MIDI + Audio  ← must match platformio.ini build_flags
//
// This file wires together all firmware modules. It contains two functions:
//
//   setup()  — runs once on power-on or reset. Initialises peripherals,
//               resets timers.
//
//   loop()   — runs forever after setup() returns. Dispatches each module's
//               scan/update function at its own independent rate using
//               elapsedMicros timers (Teensy-specific, backed by ARM cycle
//               counter — zero overhead, no interrupt contention).
//
// Timing budgets (worst-case per loop iteration):
//   Analog   scan:  48 MUX channels × ~15 µs = ~720 µs  every 1000 µs
//   Button   scan:  I2C read × 2 expanders   = ~80 µs   every 2000 µs
//   Encoder  poll:  9 reads from SRAM cache   = ~5 µs    every 1000 µs

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "midi_map.h"
#include "analog.h"
#include "buttons.h"
#include "encoders.h"

// ==========================================================================
// Debug Logging
// ==========================================================================
// Build with -D BREVVDECK_DEBUG (see platformio.ini) to enable.
// Without the flag every LOG() call compiles to nothing — zero cost.
// With the flag, output appears on the USB serial port (Serial Monitor).

#ifdef BREVVDECK_DEBUG
  #define LOG(fmt, ...) Serial.printf("[%6lu ms] " fmt "\n", millis(), ##__VA_ARGS__)
#else
  #define LOG(fmt, ...)
#endif

// ==========================================================================
// Scan Timers
// ==========================================================================
// elapsedMicros is a Teensy built-in type. It increments automatically in
// the background using the ARM DWT cycle counter — no ISR, no overhead.
// Assign 0 to reset. Compare against an interval in microseconds.

static elapsedMicros sinceAnalogScan;
static elapsedMicros sinceButtonScan;
static elapsedMicros sinceEncoderScan;

// ==========================================================================
// setup() — runs once at power-on / reset
// ==========================================================================

void setup() {
#ifdef BREVVDECK_DEBUG
    Serial.begin(115200);
    // Give a USB serial terminal time to connect before the first log line.
    delay(600);
    Serial.println("=== BrevvDeck v0.4 boot ===");
#endif

    // I2C bus — Wire must start before any module that uses it
    // (buttons, motors both use I2C peripherals).
    Wire.begin();
    Wire.setClock(400000);  // 400 kHz fast-mode; all I2C devices support it.
    LOG("I2C started at 400 kHz");

    // Initialise hardware modules in dependency order.
    // Each *Init() function configures its pins and peripheral registers.
    analogInit();    LOG("analogInit  done (3x MUX, 12-bit ADC)");
    buttonsInit();   LOG("buttonsInit done (2x MCP23017)");
    encodersInit();  LOG("encodersInit done (9 encoders)");

    // Reset all scan timers so the first iteration fires each module
    // immediately rather than waiting for the first interval to elapse.
    sinceAnalogScan  = 0;
    sinceButtonScan  = 0;
    sinceEncoderScan = 0;

    LOG("Boot complete. Entering main loop.");
}

// ==========================================================================
// loop() — runs forever
// ==========================================================================
// Each module has its own interval. The elapsed timer approach means modules
// never block each other: if the analog scan runs slightly long, the button
// scan just fires on the next iteration instead of being delayed by a sleep.

void loop() {
    // --- Analog MUX scan (1 kHz) -----------------------------------
    // Reads all 48 MUX channels (3 × 16), converts to 7-bit CC,
    // and sends USB MIDI CC for any channel that changed.
    if (sinceAnalogScan >= ANALOG_SCAN_INTERVAL_US) {
        sinceAnalogScan = 0;
        analogScan();
    }

    // --- Button matrix scan (500 Hz) --------------------------------
    // Reads 2× MCP23017 via I2C. Sends MIDI Note On/Off for any button
    // that changed state since the last scan.
    if (sinceButtonScan >= BUTTON_SCAN_INTERVAL_US) {
        sinceButtonScan = 0;
        buttonsScan();
    }

    // --- Encoder poll (1 kHz) ---------------------------------------
    // The PJRC Encoder library accumulates position in ISRs; this call
    // reads the accumulated delta and sends a relative MIDI CC message.
    // Low cost — no I2C, just reads a cached counter in SRAM.
    if (sinceEncoderScan >= ENCODER_SCAN_INTERVAL_US) {
        sinceEncoderScan = 0;
        encodersSendMidi();
    }

    // --- MIDI input processing --------------------------------------
    // usbMIDI.read() checks the USB receive buffer and dispatches
    // any complete messages (e.g. LED outputs from Mixxx).
    while (usbMIDI.read()) {}
}
