// BrevvDeck Firmware — MIDI Channel, Note, and CC Constants
//
// These MUST stay in sync with:
//   - BrevvDeck.midi.xml   (Mixxx XML mapping)
//   - BrevvDeck-scripts.js (Mixxx controller script)
//   - brevvdeck-design-document.md (Section 4 & 14)

#pragma once

#include <Arduino.h>

// ==========================================================================
// MIDI Channels (1-indexed for usbMIDI, 0-indexed in raw bytes)
// ==========================================================================
// usbMIDI.sendNoteOn() uses 1-based channels.
// Raw status bytes use 0-based: 0x90 | channel.

namespace MidiCh {
    constexpr uint8_t DECK1   = 1;
    constexpr uint8_t DECK2   = 2;
    constexpr uint8_t DECK3   = 3;
    constexpr uint8_t DECK4   = 4;
    constexpr uint8_t MIXER   = 5;
    constexpr uint8_t FX1     = 6;
    constexpr uint8_t FX2     = 7;
    constexpr uint8_t LIBRARY = 8;
}

// ==========================================================================
// Deck Note Numbers (same for all 4 decks, differentiated by channel)
// ==========================================================================

namespace DeckNote {
    constexpr uint8_t PLAY           = 0x00;
    constexpr uint8_t CUE            = 0x01;
    constexpr uint8_t SYNC           = 0x02;
    constexpr uint8_t KEYLOCK        = 0x03;
    constexpr uint8_t QUANTIZE       = 0x04;
    constexpr uint8_t LOOP_IN        = 0x05;
    constexpr uint8_t LOOP_OUT       = 0x06;
    constexpr uint8_t LOOP_DEACTIVATE = 0x07;

    // Hotcue/Roll buttons: 0x10 + (button index 0-7)
    constexpr uint8_t HOTCUE_BASE    = 0x10;

    constexpr uint8_t LOOP_ENC_PUSH  = 0x20;
    constexpr uint8_t BJ_ENC_PUSH    = 0x21;

    constexpr uint8_t SHIFT          = 0x30;
}

// ==========================================================================
// Deck CC Numbers
// ==========================================================================

namespace DeckCC {
    constexpr uint8_t RATE_MSB       = 0x00;  // 14-bit pair with RATE_LSB
    constexpr uint8_t POSITION_MSB   = 0x01;  // 14-bit pair with POSITION_LSB
    constexpr uint8_t JOYSTICK       = 0x02;
    constexpr uint8_t ROLL_MODE      = 0x03;  // Firmware → host: toggle state

    constexpr uint8_t BEATJUMP_ENC   = 0x10;  // Relative encoder
    constexpr uint8_t LOOP_ENC       = 0x11;  // Relative encoder

    constexpr uint8_t RATE_LSB       = 0x20;  // 14-bit LSB for rate
    constexpr uint8_t POSITION_LSB   = 0x21;  // 14-bit LSB for position

    constexpr uint8_t DISPLAY        = 0x30;  // Output: host → firmware display
}

// ==========================================================================
// Mixer Note Numbers (Channel 5)
// ==========================================================================

namespace MixerNote {
    // PFL buttons: 0x00 + (ch 0-3)
    constexpr uint8_t PFL_BASE       = 0x00;
    // FX1 assign: 0x04 + (ch 0-3)
    constexpr uint8_t FX1_ASSIGN_BASE = 0x04;
    // FX2 assign: 0x08 + (ch 0-3)
    constexpr uint8_t FX2_ASSIGN_BASE = 0x08;
    // Load track: 0x0C + (ch 0-3)
    constexpr uint8_t LOAD_BASE      = 0x0C;
}

// ==========================================================================
// Mixer CC Numbers (Channel 5)
// ==========================================================================

namespace MixerCC {
    // Volume: 0x00 + (ch 0-3)
    constexpr uint8_t VOL_BASE       = 0x00;
    // EQ High: 0x04 + (ch 0-3)
    constexpr uint8_t EQ_HI_BASE    = 0x04;
    // EQ Mid: 0x08 + (ch 0-3)
    constexpr uint8_t EQ_MID_BASE   = 0x08;
    // EQ Low: 0x0C + (ch 0-3)
    constexpr uint8_t EQ_LO_BASE    = 0x0C;
    // Quick FX: 0x10 + (ch 0-3)
    constexpr uint8_t QFX_BASE      = 0x10;

    constexpr uint8_t HP_MIX        = 0x14;
    constexpr uint8_t HP_VOL        = 0x15;
    constexpr uint8_t MASTER_VOL    = 0x16;
    constexpr uint8_t CROSSFADER    = 0x17;

    // Gain: 0x18 + (ch 0-3)
    constexpr uint8_t GAIN_BASE     = 0x18;
}

// ==========================================================================
// FX Note/CC Numbers (Channels 6-7)
// ==========================================================================

namespace FxNote {
    // Effect enable 1-3: 0x00-0x02
    constexpr uint8_t ENABLE_BASE    = 0x00;
    // Effect select
    constexpr uint8_t SELECT         = 0x03;
}

namespace FxCC {
    // Effect knobs 1-3 meta: 0x00-0x02
    constexpr uint8_t KNOB_BASE      = 0x00;
    // Dry/Wet
    constexpr uint8_t DRY_WET        = 0x03;
}

// ==========================================================================
// Library / Sampler (Channel 8)
// ==========================================================================

namespace LibNote {
    constexpr uint8_t BROWSE_PUSH    = 0x00;
    constexpr uint8_t FULLSCREEN     = 0x01;
    constexpr uint8_t BACK           = 0x02;
    constexpr uint8_t PREVIEW        = 0x03;

    // Sampler buttons: 0x14 + (sampler 0-3)
    constexpr uint8_t SAMPLER_BASE   = 0x14;
}

namespace LibCC {
    constexpr uint8_t BROWSE_ENC     = 0x00;  // Relative encoder
}

// ==========================================================================
// Helper: Deck index (0-3) → MIDI channel (1-4)
// ==========================================================================

inline uint8_t deckChannel(int deckIdx) {
    return static_cast<uint8_t>(deckIdx + 1);
}
