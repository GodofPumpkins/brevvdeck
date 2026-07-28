// BrevvDeck Firmware — Analog Input Scanning (Implementation)
//
// All pot/fader/joystick inputs are read through 3× CD74HC4067 MUXes.
// No direct ADC pins — avoids conflicts with I2C (A4/A5) and I2S (A6/A7).
// Rate and position faders for all 4 decks are included here (7-bit CC).
// Soft takeover for rate/position is handled by Mixxx via the XML mapping.

#include "analog.h"
#include "config.h"
#include "midi_map.h"

// -----------------------------------------------------------------------
// MUX channel → MIDI mapping
// -----------------------------------------------------------------------

struct AnalogMapping {
    uint8_t midiChannel;  // 1-based (for usbMIDI), 0 = skip
    uint8_t ccNumber;
};

// MUX #1 channel assignments (A0, 16ch): Gains and EQ
static const AnalogMapping MUX1_MAP[16] = {
    {MidiCh::MIXER, MixerCC::GAIN_BASE + 0},  // Ch0: Gain Ch1
    {MidiCh::MIXER, MixerCC::GAIN_BASE + 1},  // Ch1: Gain Ch2
    {MidiCh::MIXER, MixerCC::GAIN_BASE + 2},  // Ch2: Gain Ch3
    {MidiCh::MIXER, MixerCC::GAIN_BASE + 3},  // Ch3: Gain Ch4
    {MidiCh::MIXER, MixerCC::EQ_HI_BASE + 0}, // Ch4: EQ High Ch1
    {MidiCh::MIXER, MixerCC::EQ_HI_BASE + 1}, // Ch5: EQ High Ch2
    {MidiCh::MIXER, MixerCC::EQ_HI_BASE + 2}, // Ch6: EQ High Ch3
    {MidiCh::MIXER, MixerCC::EQ_HI_BASE + 3}, // Ch7: EQ High Ch4
    {MidiCh::MIXER, MixerCC::EQ_MID_BASE + 0},// Ch8: EQ Mid Ch1
    {MidiCh::MIXER, MixerCC::EQ_MID_BASE + 1},// Ch9: EQ Mid Ch2
    {MidiCh::MIXER, MixerCC::EQ_MID_BASE + 2},// Ch10: EQ Mid Ch3
    {MidiCh::MIXER, MixerCC::EQ_MID_BASE + 3},// Ch11: EQ Mid Ch4
    {MidiCh::MIXER, MixerCC::EQ_LO_BASE + 0}, // Ch12: EQ Low Ch1
    {MidiCh::MIXER, MixerCC::EQ_LO_BASE + 1}, // Ch13: EQ Low Ch2
    {MidiCh::MIXER, MixerCC::EQ_LO_BASE + 2}, // Ch14: EQ Low Ch3
    {MidiCh::MIXER, MixerCC::EQ_LO_BASE + 3}, // Ch15: EQ Low Ch4
};

// MUX #2 channel assignments (A1, 16ch): Quick FX, FX knobs, mixer pots, Deck4 position fader
static const AnalogMapping MUX2_MAP[16] = {
    {MidiCh::MIXER, MixerCC::QFX_BASE + 0},      // Ch0: Quick FX Ch1
    {MidiCh::MIXER, MixerCC::QFX_BASE + 1},      // Ch1: Quick FX Ch2
    {MidiCh::MIXER, MixerCC::QFX_BASE + 2},      // Ch2: Quick FX Ch3
    {MidiCh::MIXER, MixerCC::QFX_BASE + 3},      // Ch3: Quick FX Ch4
    {MidiCh::FX1,   FxCC::KNOB_BASE + 0},        // Ch4: FX1 Knob 1
    {MidiCh::FX1,   FxCC::KNOB_BASE + 1},        // Ch5: FX1 Knob 2
    {MidiCh::FX1,   FxCC::KNOB_BASE + 2},        // Ch6: FX1 Knob 3
    {MidiCh::FX1,   FxCC::DRY_WET},              // Ch7: FX1 Dry/Wet
    {MidiCh::FX2,   FxCC::KNOB_BASE + 0},        // Ch8: FX2 Knob 1
    {MidiCh::FX2,   FxCC::KNOB_BASE + 1},        // Ch9: FX2 Knob 2
    {MidiCh::FX2,   FxCC::KNOB_BASE + 2},        // Ch10: FX2 Knob 3
    {MidiCh::FX2,   FxCC::DRY_WET},              // Ch11: FX2 Dry/Wet
    {MidiCh::MIXER, MixerCC::HP_MIX},            // Ch12: HP Mix
    {MidiCh::MIXER, MixerCC::MASTER_VOL},        // Ch13: Master Volume
    {MidiCh::MIXER, MixerCC::HP_VOL},            // Ch14: HP Volume
    {MidiCh::DECK4, DeckCC::POSITION_MSB},       // Ch15: Deck 4 Position fader
};

// MUX #3 channel assignments (A2, 16ch): Volumes, crossfader, joysticks, rate faders, position faders 1-3
static const AnalogMapping MUX3_MAP[16] = {
    {MidiCh::MIXER, MixerCC::VOL_BASE + 0},      // Ch0: Volume Ch1
    {MidiCh::MIXER, MixerCC::VOL_BASE + 1},      // Ch1: Volume Ch2
    {MidiCh::MIXER, MixerCC::VOL_BASE + 2},      // Ch2: Volume Ch3
    {MidiCh::MIXER, MixerCC::VOL_BASE + 3},      // Ch3: Volume Ch4
    {MidiCh::MIXER, MixerCC::CROSSFADER},        // Ch4: Crossfader
    {MidiCh::DECK1, DeckCC::JOYSTICK},           // Ch5: Joystick Deck 1
    {MidiCh::DECK2, DeckCC::JOYSTICK},           // Ch6: Joystick Deck 2
    {MidiCh::DECK3, DeckCC::JOYSTICK},           // Ch7: Joystick Deck 3
    {MidiCh::DECK4, DeckCC::JOYSTICK},           // Ch8: Joystick Deck 4
    {MidiCh::DECK1, DeckCC::RATE_MSB},           // Ch9:  Deck 1 Rate fader
    {MidiCh::DECK2, DeckCC::RATE_MSB},           // Ch10: Deck 2 Rate fader
    {MidiCh::DECK3, DeckCC::RATE_MSB},           // Ch11: Deck 3 Rate fader
    {MidiCh::DECK4, DeckCC::RATE_MSB},           // Ch12: Deck 4 Rate fader
    {MidiCh::DECK1, DeckCC::POSITION_MSB},       // Ch13: Deck 1 Position fader
    {MidiCh::DECK2, DeckCC::POSITION_MSB},       // Ch14: Deck 2 Position fader
    {MidiCh::DECK3, DeckCC::POSITION_MSB},       // Ch15: Deck 3 Position fader
};
// Note: Deck 4 Position fader is on MUX #2 Ch15

// -----------------------------------------------------------------------
// State — previous 7-bit values for change detection
// -----------------------------------------------------------------------

static uint8_t prevMux1[16] = {0};
static uint8_t prevMux2[16] = {0};
static uint8_t prevMux3[16] = {0};

// -----------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------

void analogInit() {
    // Configure MUX address pins as outputs
    pinMode(PIN_MUX_S0, OUTPUT);
    pinMode(PIN_MUX_S1, OUTPUT);
    pinMode(PIN_MUX_S2, OUTPUT);
    pinMode(PIN_MUX_S3, OUTPUT);

    // Set ADC resolution to 12 bits (Teensy 4.1 supports 10 or 12)
    analogReadResolution(12);
}

// -----------------------------------------------------------------------
// Set MUX address lines (selects which of 16 channels is active)
// -----------------------------------------------------------------------

static void setMuxChannel(uint8_t ch) {
    digitalWriteFast(PIN_MUX_S0, ch & 0x01);
    digitalWriteFast(PIN_MUX_S1, (ch >> 1) & 0x01);
    digitalWriteFast(PIN_MUX_S2, (ch >> 2) & 0x01);
    digitalWriteFast(PIN_MUX_S3, (ch >> 3) & 0x01);
}

// -----------------------------------------------------------------------
// Send CC if value changed beyond threshold
// -----------------------------------------------------------------------

static void sendIfChanged(uint8_t* prev, uint8_t newVal, uint8_t midiCh, uint8_t cc) {
    uint8_t diff = (newVal > *prev) ? (newVal - *prev) : (*prev - newVal);
    if (diff >= CC_THRESHOLD) {
        *prev = newVal;
        usbMIDI.sendControlChange(cc, newVal, midiCh);
    }
}

// -----------------------------------------------------------------------
// Read one MUX and send changes for all 16 channels
// -----------------------------------------------------------------------

static void scanOneMux(uint8_t comPin, const AnalogMapping* map, uint8_t* prev) {
    for (uint8_t ch = 0; ch < 16; ch++) {
        setMuxChannel(ch);
        delayMicroseconds(5);  // MUX settling time

        uint16_t raw = analogRead(comPin);
        uint8_t val = raw >> 5;  // 12-bit → 7-bit

        if (map[ch].midiChannel != 0) {
            sendIfChanged(&prev[ch], val, map[ch].midiChannel, map[ch].ccNumber);
        }
    }
}

// -----------------------------------------------------------------------
// Main scan function — called from loop() at ANALOG_SCAN_INTERVAL_US
// -----------------------------------------------------------------------

void analogScan() {
    scanOneMux(PIN_MUX1_COM, MUX1_MAP, prevMux1);
    scanOneMux(PIN_MUX2_COM, MUX2_MAP, prevMux2);
    scanOneMux(PIN_MUX3_COM, MUX3_MAP, prevMux3);
}
