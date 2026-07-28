// BrevvDeck Firmware — Button Matrix Scanning (Implementation)

#include "buttons.h"
#include "config.h"
#include "midi_map.h"
#include <Wire.h>

// -----------------------------------------------------------------------
// MCP23017 Registers
// -----------------------------------------------------------------------

static constexpr uint8_t MCP_IODIRA = 0x00;  // Port A direction
static constexpr uint8_t MCP_IODIRB = 0x01;  // Port B direction
static constexpr uint8_t MCP_GPPUB  = 0x0D;  // Port B pull-ups
static constexpr uint8_t MCP_GPIOA  = 0x12;  // Port A output
static constexpr uint8_t MCP_GPIOB  = 0x13;  // Port B input

// -----------------------------------------------------------------------
// I2C Helpers
// -----------------------------------------------------------------------

static void mcpWriteReg(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static uint8_t mcpReadReg(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

// -----------------------------------------------------------------------
// Button Matrix → MIDI Mapping
// -----------------------------------------------------------------------
// Returns true if the position maps to a valid button.
// Sets midiChannel (1-based) and noteNumber.

struct ButtonMapping {
    uint8_t midiChannel;
    uint8_t noteNumber;
    bool valid;
};

static ButtonMapping getButtonMapping(int col, int row) {
    ButtonMapping m = {0, 0, false};

    // Determine which deck based on grid quadrant:
    //   Cols 0-4, Rows 0-4 → Deck 1
    //   Cols 5-9, Rows 0-4 → Deck 2
    //   Cols 0-4, Rows 5-9 → Deck 3
    //   Cols 5-9, Rows 5-9 → Deck 4
    uint8_t deckCh;
    int localCol, localRow;

    if (col < 5 && row < 5) {
        deckCh = MidiCh::DECK1; localCol = col; localRow = row;
    } else if (col >= 5 && row < 5) {
        deckCh = MidiCh::DECK2; localCol = col - 5; localRow = row;
    } else if (col < 5 && row >= 5) {
        deckCh = MidiCh::DECK3; localCol = col; localRow = row - 5;
    } else {
        deckCh = MidiCh::DECK4; localCol = col - 5; localRow = row - 5;
    }

    // Within each 5×5 block:
    //   Row 0: Play, Cue, Sync, Keylock, Quantize
    //   Row 1: Loop In, Loop Out, Loop Deact, Loop Enc Push, BJ Enc Push
    //   Row 2: Hotcue 1-5
    //   Row 3: Hotcue 6-8, Shift, PFL (mixer channel)
    //   Row 4: FX1 Assign, FX2 Assign, Load Track, Sampler
    switch (localRow) {
        case 0:
            switch (localCol) {
                case 0: m = {deckCh, DeckNote::PLAY, true}; break;
                case 1: m = {deckCh, DeckNote::CUE, true}; break;
                case 2: m = {deckCh, DeckNote::SYNC, true}; break;
                case 3: m = {deckCh, DeckNote::KEYLOCK, true}; break;
                case 4: m = {deckCh, DeckNote::QUANTIZE, true}; break;
            }
            break;
        case 1:
            switch (localCol) {
                case 0: m = {deckCh, DeckNote::LOOP_IN, true}; break;
                case 1: m = {deckCh, DeckNote::LOOP_OUT, true}; break;
                case 2: m = {deckCh, DeckNote::LOOP_DEACTIVATE, true}; break;
                case 3: m = {deckCh, DeckNote::LOOP_ENC_PUSH, true}; break;
                case 4: m = {deckCh, DeckNote::BJ_ENC_PUSH, true}; break;
            }
            break;
        case 2:
            if (localCol < 5) {
                m = {deckCh, static_cast<uint8_t>(DeckNote::HOTCUE_BASE + localCol), true};
            }
            break;
        case 3:
            switch (localCol) {
                case 0: m = {deckCh, static_cast<uint8_t>(DeckNote::HOTCUE_BASE + 5), true}; break;
                case 1: m = {deckCh, static_cast<uint8_t>(DeckNote::HOTCUE_BASE + 6), true}; break;
                case 2: m = {deckCh, static_cast<uint8_t>(DeckNote::HOTCUE_BASE + 7), true}; break;
                case 3: m = {deckCh, DeckNote::SHIFT, true}; break;
                case 4: {
                    // PFL button — on mixer channel, note = deck index
                    uint8_t deckIdx = deckCh - 1;  // 0-3
                    m = {MidiCh::MIXER, static_cast<uint8_t>(MixerNote::PFL_BASE + deckIdx), true};
                    break;
                }
            }
            break;
        case 4: {
            uint8_t deckIdx = deckCh - 1;
            switch (localCol) {
                case 0: m = {MidiCh::MIXER, static_cast<uint8_t>(MixerNote::FX1_ASSIGN_BASE + deckIdx), true}; break;
                case 1: m = {MidiCh::MIXER, static_cast<uint8_t>(MixerNote::FX2_ASSIGN_BASE + deckIdx), true}; break;
                case 2: m = {MidiCh::MIXER, static_cast<uint8_t>(MixerNote::LOAD_BASE + deckIdx), true}; break;
                case 3: m = {MidiCh::LIBRARY, static_cast<uint8_t>(LibNote::SAMPLER_BASE + deckIdx), true}; break;
            }
            break;
        }
    }

    return m;
}

// -----------------------------------------------------------------------
// State
// -----------------------------------------------------------------------

// Previous button states: true = not pressed (pull-up default)
static bool prevState[10][10];

// -----------------------------------------------------------------------
// Initialization
// -----------------------------------------------------------------------

void buttonsInit() {
    // Initialize previous state (all not pressed)
    for (int c = 0; c < 10; c++) {
        for (int r = 0; r < 10; r++) {
            prevState[c][r] = true;
        }
    }

    // Configure MCP23017 #1: Port A = outputs (columns), Port B = inputs (rows)
    mcpWriteReg(MCP23017_ADDR_1, MCP_IODIRA, 0x00);  // All outputs
    mcpWriteReg(MCP23017_ADDR_1, MCP_IODIRB, 0xFF);  // All inputs
    mcpWriteReg(MCP23017_ADDR_1, MCP_GPPUB,  0xFF);  // Pull-ups on inputs
    mcpWriteReg(MCP23017_ADDR_1, MCP_GPIOA,  0xFF);  // All columns high (inactive)

    // Configure MCP23017 #2
    mcpWriteReg(MCP23017_ADDR_2, MCP_IODIRA, 0x00);
    mcpWriteReg(MCP23017_ADDR_2, MCP_IODIRB, 0xFF);
    mcpWriteReg(MCP23017_ADDR_2, MCP_GPPUB,  0xFF);
    mcpWriteReg(MCP23017_ADDR_2, MCP_GPIOA,  0xFF);
}

// -----------------------------------------------------------------------
// Scan function — called from loop() at BUTTON_SCAN_INTERVAL_US
// -----------------------------------------------------------------------

void buttonsScan() {
    for (int col = 0; col < 10; col++) {
        // Determine which MCP23017 owns this column
        uint8_t addr;
        uint8_t bit;
        if (col < 8) {
            addr = MCP23017_ADDR_1;
            bit = col;
        } else {
            addr = MCP23017_ADDR_2;
            bit = col - 8;
        }

        // Drive one column low (active), all others high
        mcpWriteReg(addr, MCP_GPIOA, ~(1 << bit));
        delayMicroseconds(10);  // Settling time

        // Read rows from both MCP23017s
        uint8_t rows1 = mcpReadReg(MCP23017_ADDR_1, MCP_GPIOB);
        uint8_t rows2 = mcpReadReg(MCP23017_ADDR_2, MCP_GPIOB);

        // Deactivate column
        mcpWriteReg(addr, MCP_GPIOA, 0xFF);

        // Check each row for state changes
        for (int row = 0; row < 10; row++) {
            bool isPressed;
            if (row < 8) {
                isPressed = ((rows1 & (1 << row)) == 0);  // Active low
            } else {
                isPressed = ((rows2 & (1 << (row - 8))) == 0);
            }

            bool wasPressed = !prevState[col][row];

            if (isPressed != wasPressed) {
                prevState[col][row] = !isPressed;

                ButtonMapping mapping = getButtonMapping(col, row);
                if (mapping.valid) {
                    if (isPressed) {
                        usbMIDI.sendNoteOn(mapping.noteNumber, 127, mapping.midiChannel);
                    } else {
                        usbMIDI.sendNoteOff(mapping.noteNumber, 0, mapping.midiChannel);
                    }
                }
            }
        }
    }
}
