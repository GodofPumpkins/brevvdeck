// BrevvDeck Firmware — Rotary Encoder Reading (Implementation)
//
// Uses the PJRC Encoder library which automatically selects the best
// decoding method (hardware quad timer, interrupt-driven, or polling)
// based on which pins are used. On Teensy 4.1, all digital pins support
// interrupts, so all encoders get interrupt-driven quadrature decoding.

#include "encoders.h"
#include "config.h"
#include "midi_map.h"
#include <Encoder.h>

// -----------------------------------------------------------------------
// Encoder instances
// -----------------------------------------------------------------------

// The Encoder library tracks position in hardware/interrupts automatically.
// We read the accumulated position change each scan cycle and send MIDI.

static Encoder encoderObjects[NUM_ENCODERS] = {
    Encoder(ENCODER_PINS[0].pinA, ENCODER_PINS[0].pinB),  // Deck1 BeatJump (0, 1)
    Encoder(ENCODER_PINS[1].pinA, ENCODER_PINS[1].pinB),  // Deck1 Loop (12, 17)
    Encoder(ENCODER_PINS[2].pinA, ENCODER_PINS[2].pinB),  // Deck2 BeatJump (22, 23)
    Encoder(ENCODER_PINS[3].pinA, ENCODER_PINS[3].pinB),  // Deck2 Loop (24, 25)
    Encoder(ENCODER_PINS[4].pinA, ENCODER_PINS[4].pinB),  // Deck3 BeatJump (26, 27)
    Encoder(ENCODER_PINS[5].pinA, ENCODER_PINS[5].pinB),  // Deck3 Loop (28, 29)
    Encoder(ENCODER_PINS[6].pinA, ENCODER_PINS[6].pinB),  // Deck4 BeatJump (30, 31)
    Encoder(ENCODER_PINS[7].pinA, ENCODER_PINS[7].pinB),  // Deck4 Loop (33, 34)
    Encoder(ENCODER_PINS[8].pinA, ENCODER_PINS[8].pinB),  // Browse (35, 36)
};

// -----------------------------------------------------------------------
// Encoder → MIDI mapping
// -----------------------------------------------------------------------

struct EncoderMapping {
    uint8_t midiChannel;  // 1-based
    uint8_t ccNumber;     // CC for rotation
};

static const EncoderMapping ENCODER_MAP[NUM_ENCODERS] = {
    {MidiCh::DECK1,   DeckCC::BEATJUMP_ENC},  // 0: Deck1 BeatJump
    {MidiCh::DECK1,   DeckCC::LOOP_ENC},       // 1: Deck1 Loop
    {MidiCh::DECK2,   DeckCC::BEATJUMP_ENC},  // 2: Deck2 BeatJump
    {MidiCh::DECK2,   DeckCC::LOOP_ENC},       // 3: Deck2 Loop
    {MidiCh::DECK3,   DeckCC::BEATJUMP_ENC},  // 4: Deck3 BeatJump
    {MidiCh::DECK3,   DeckCC::LOOP_ENC},       // 5: Deck3 Loop
    {MidiCh::DECK4,   DeckCC::BEATJUMP_ENC},  // 6: Deck4 BeatJump
    {MidiCh::DECK4,   DeckCC::LOOP_ENC},       // 7: Deck4 Loop
    {MidiCh::LIBRARY, LibCC::BROWSE_ENC},      // 8: Browse
};

// Previous position per encoder (to compute delta)
static long prevPosition[NUM_ENCODERS] = {0};

// -----------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------

void encodersInit() {
    // The Encoder library configures pins in its constructor.
    // Reset all positions to zero.
    for (int i = 0; i < NUM_ENCODERS; i++) {
        encoderObjects[i].write(0);
        prevPosition[i] = 0;
    }
}

// -----------------------------------------------------------------------
// Send accumulated encoder deltas as relative MIDI CC
// Called from loop() at ENCODER_SEND_INTERVAL_US
// -----------------------------------------------------------------------

void encodersSendMidi() {
    for (int i = 0; i < NUM_ENCODERS; i++) {
        long pos = encoderObjects[i].read();
        long delta = pos - prevPosition[i];

        if (delta != 0) {
            prevPosition[i] = pos;

            // Most EC11 encoders produce 4 edges per detent.
            // Divide by 4 to get one step per detent.
            int steps = delta / 4;
            if (steps == 0 && delta != 0) {
                // Fractional detent — don't send yet, accumulate.
                // But keep prevPosition at the last detent boundary.
                prevPosition[i] = pos - (delta % 4);
                continue;
            }

            // Clamp to reasonable range
            if (steps > 10) steps = 10;
            if (steps < -10) steps = -10;

            // Relative CC: center (0x40) + delta
            uint8_t ccValue = static_cast<uint8_t>(
                constrain(ENCODER_CENTER + steps, 0, 127)
            );

            usbMIDI.sendControlChange(
                ENCODER_MAP[i].ccNumber,
                ccValue,
                ENCODER_MAP[i].midiChannel
            );
        }
    }
}
