// BrevvDeck Firmware — Main Entry Point
// Target: PJRC Teensy 4.1 (i.MX RT1062, Cortex-M7 @ 600 MHz)
// Framework: Arduino (Teensyduino)
// USB Mode: Serial + MIDI + Audio  ← must match platformio.ini build_flags
//
// This file wires together all firmware modules. It contains two functions:
//
//   setup()  — runs once on power-on or reset. Initialises peripherals,
//               registers MIDI input callbacks, resets timers.
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
//   Motor    PID:   8 ADS1115 reads + PWM out = ~200 µs  every 500 µs
//   MIDI receive:   callback dispatch         = ~1 µs     every iteration

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "midi_map.h"
#include "analog.h"
#include "buttons.h"
#include "encoders.h"
#include "motors.h"

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
static elapsedMicros sinceMotorScan;

// ==========================================================================
// MIDI Input — Motor Target Tracking
// ==========================================================================
// Mixxx sends the current playback position and rate as 14-bit MIDI CC
// pairs to tell the firmware where to move each motorized fader.
//
// 14-bit MIDI CC protocol (standard MIDI high-resolution):
//   Step 1: Send CC (MSB number) with the upper 7 bits of the value.
//   Step 2: Send CC (LSB number = MSB + 0x20) with the lower 7 bits.
//   The firmware buffers the MSB and acts when the LSB arrives.
//
// Fader index convention (matches NUM_FADERS = 8 in config.h):
//   0-3: Rate/pitch faders for Decks 1-4
//   4-7: Playback-position faders for Decks 1-4

static uint8_t rateMsb[4]     = {0};
static uint8_t positionMsb[4] = {0};

static void onControlChange(uint8_t channel, uint8_t cc, uint8_t value) {
    // Only handle messages on Deck channels 1-4.
    if (channel < MidiCh::DECK1 || channel > MidiCh::DECK4) return;
    int deck = channel - 1;  // Convert 1-based MIDI channel to 0-based index.

    switch (cc) {
        case DeckCC::RATE_MSB:
            rateMsb[deck] = value;
            break;

        case DeckCC::RATE_LSB: {
            // Combine buffered MSB with this LSB into a 14-bit value (0-16383).
            uint16_t target = ((uint16_t)rateMsb[deck] << 7) | value;
            motorsSetTarget(deck, target);
            LOG("Rate   deck=%d  target=%u", deck, target);
            break;
        }

        case DeckCC::POSITION_MSB:
            positionMsb[deck] = value;
            break;

        case DeckCC::POSITION_LSB: {
            // Position faders use index deck+4 (4-7) to distinguish from
            // rate faders (0-3) inside the motors module.
            uint16_t target = ((uint16_t)positionMsb[deck] << 7) | value;
            motorsSetTarget(deck + 4, target);
            LOG("Posit. deck=%d  target=%u", deck, target);
            break;
        }

        default:
            break;
    }
}

// ==========================================================================
// Motor Kill Switch
// ==========================================================================
// PIN_MOTOR_KILL (pin 37) is a physical toggle switch wired to 3.3 V.
// HIGH = kill engaged (motors will not move even if PID requests it).
// LOW  = normal operation.
//
// Software debounce: only act after the signal has been stable for
// KILL_DEBOUNCE_MS milliseconds to ignore contact bounce on toggle.

static constexpr uint32_t KILL_DEBOUNCE_MS = 20;
static bool     lastKillState    = false;
static bool     pendingKillState = false;
static uint32_t killChangeTime   = 0;

static void checkMotorKill() {
    bool current = (digitalRead(PIN_MOTOR_KILL) == HIGH);

    if (current != pendingKillState) {
        // State changed — start debounce timer.
        pendingKillState = current;
        killChangeTime   = millis();
    }

    if (pendingKillState != lastKillState &&
        (millis() - killChangeTime) >= KILL_DEBOUNCE_MS)
    {
        lastKillState = pendingKillState;
        motorsEnableKill(lastKillState);
        LOG("Motor kill: %s", lastKillState ? "ON" : "OFF");
    }
}

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
    motorsInit();    LOG("motorsInit  done (8 faders, PID)");

    // Motor kill switch — floating-low input; pulled high by toggle switch.
    pinMode(PIN_MOTOR_KILL, INPUT);
    lastKillState    = (digitalRead(PIN_MOTOR_KILL) == HIGH);
    pendingKillState = lastKillState;
    motorsEnableKill(lastKillState);
    LOG("Motor kill switch initial state: %s", lastKillState ? "ON" : "OFF");

    // Register MIDI input callback.
    // usbMIDI.read() (called in loop) dispatches to this function.
    usbMIDI.setHandleControlChange(onControlChange);

    // Reset all scan timers so the first iteration fires each module
    // immediately rather than waiting for the first interval to elapse.
    sinceAnalogScan  = 0;
    sinceButtonScan  = 0;
    sinceEncoderScan = 0;
    sinceMotorScan   = 0;

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

    // --- Motor PID update (2 kHz) -----------------------------------
    // Reads ADS1115 fader positions (I2C), runs PID loop, writes PWM
    // output via PCA9685 (I2C). Tight timing matters here for smooth
    // motor response — 500 µs gives the PID loop good resolution.
    if (sinceMotorScan >= MOTOR_SCAN_INTERVAL_US) {
        sinceMotorScan = 0;
        motorsScan();
    }

    // --- MIDI input processing --------------------------------------
    // usbMIDI.read() checks the USB receive buffer and, if a complete
    // message is available, dispatches it to the registered handler
    // (onControlChange above). The while loop drains the full buffer
    // in case multiple messages arrived since the last iteration.
    while (usbMIDI.read()) {}

    // --- Motor kill switch ------------------------------------------
    // Checked every loop iteration — the debounce logic inside ensures
    // it only acts after the signal is stable.
    checkMotorKill();
}
