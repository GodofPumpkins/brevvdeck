// BrevvDeck — Custom 4-Deck DJ Controller Script for Mixxx (v0.5)
//
// Handles logic that cannot be expressed purely in the XML mapping:
//   - Beat jump encoder (relative rotation → fwd/back jumps; shift+rotate → halve/double size)
//   - Beat jump encoder push (cycle through beat jump size list)
//   - Loop encoder (relative rotation → loop halve/double)
//   - Scrub joystick (1-axis spring-return joystick for variable-speed scrubbing)
//   - Hotcue/Roll dual-mode buttons (hardware toggle selects hotcue vs slip-roll)
//   - Shift button state management (global left/right + per-deck)
//   - Library browse encoder (scroll + shift=waveform zoom)
//   - FX select/focus cycling
//   - Sampler trigger buttons (play if loaded, load if empty)
//   - 7-segment display update (loop size + beat jump size via CC output)
//
// Rate and playback-position faders use soft takeover via XML — no script needed.
//
// MIDI Channel Assignment:
//   Ch 1-4 (0x90-0x93 / 0xB0-0xB3) = Decks 1-4
//   Ch 5   (0x94 / 0xB4)            = Mixer
//   Ch 6   (0x95 / 0xB5)            = FX Unit 1
//   Ch 7   (0x96 / 0xB6)            = FX Unit 2
//   Ch 8   (0x97 / 0xB7)            = Library / Global / Sampler

const BrevvDeck = {};

// ============================================================
// Configuration
// ============================================================

// Beat jump sizes to cycle through (in beats)
BrevvDeck.BEAT_JUMP_SIZES = [0.5, 1, 2, 4, 8, 16, 32];

// Slip-roll beat sizes corresponding to buttons 1-8
// Buttons 0x10-0x17 map to these loop roll sizes (in beats)
BrevvDeck.ROLL_SIZES = [0.125, 0.25, 0.5, 1, 2, 4, 8, 16];

// Touch strip sensitivity: how much nudge per CC step
BrevvDeck.TOUCH_STRIP_SENSITIVITY = 0.5;

// Joystick scrub configuration
BrevvDeck.JOYSTICK_DEADZONE = 5;       // CC units from center (64 ± 5 = no scrub)
BrevvDeck.JOYSTICK_SCRUB_SPEED = 3;    // Max jog value at full deflection

// Relative encoder center value (firmware sends values around 0x40)
// 0x3F and below = counter-clockwise, 0x41 and above = clockwise
BrevvDeck.ENCODER_CENTER = 0x40;

// 7-segment display CC numbers (sent on deck channel)
// Firmware interprets these to drive the 2-digit MAX7219 displays
BrevvDeck.DISPLAY_CC = 0x30;

// ============================================================
// State
// ============================================================

// Per-deck state
BrevvDeck.deckState = {};
for (let i = 1; i <= 4; i++) {
    BrevvDeck.deckState[i] = {
        shifted: false,
        beatJumpSizeIndex: 3,  // Start at 4 beats (index into BEAT_JUMP_SIZES)
        rollMode: false,       // false = hotcue mode, true = slip-roll mode
                               // (set by firmware when it reads the hardware toggle)
    };
}

// Global shift state (left shift affects decks 1/3, right shift affects decks 2/4)
BrevvDeck.shiftLeftActive = false;
BrevvDeck.shiftRightActive = false;

// Connections for output (cleanup on shutdown)
BrevvDeck.connections = [];

// ============================================================
// Utility Functions
// ============================================================

// Get deck number from MIDI channel (Ch1=Deck1, Ch2=Deck2, etc.)
BrevvDeck.deckFromChannel = function(channel) {
    return channel + 1; // MIDI channel 0-3 → deck 1-4
};

// Get the [ChannelN] group for a MIDI channel
BrevvDeck.groupFromChannel = function(channel) {
    return "[Channel" + (channel + 1) + "]";
};

// Check if shift is active for a given deck
BrevvDeck.isShifted = function(deckNum) {
    if (deckNum === 1 || deckNum === 3) {
        return BrevvDeck.shiftLeftActive;
    }
    return BrevvDeck.shiftRightActive;
};

// Decode relative encoder value: returns signed integer
// Firmware sends 0x41 for CW, 0x3F for CCW (or further from center for faster turns)
BrevvDeck.decodeRelative = function(value) {
    if (value === BrevvDeck.ENCODER_CENTER) {
        return 0;
    }
    return value - BrevvDeck.ENCODER_CENTER;
};

// Send a value to the 7-segment display for a deck.
// The firmware maps the CC value to a 2-digit display (0-99).
// We encode the displayed number as a MIDI CC value (0-127).
BrevvDeck.updateDisplay = function(deckNum, displayValue) {
    const channel = deckNum - 1; // deck 1 → ch 0
    // Clamp to 0-99 for 2-digit display
    const clamped = Math.max(0, Math.min(99, Math.round(displayValue)));
    midi.sendShortMsg(0xB0 + channel, BrevvDeck.DISPLAY_CC, clamped);
};

// Compute the display value for the current deck state.
// Normal: show current loop size in beats (if a loop is set)
// Shift held: show current beat jump size
BrevvDeck.refreshDisplay = function(deckNum) {
    const group = "[Channel" + deckNum + "]";
    const state = BrevvDeck.deckState[deckNum];

    if (BrevvDeck.isShifted(deckNum)) {
        // Show beat jump size
        const bjSize = BrevvDeck.BEAT_JUMP_SIZES[state.beatJumpSizeIndex];
        // For fractional sizes (0.5), display as 0. For integer sizes, display directly.
        BrevvDeck.updateDisplay(deckNum, bjSize >= 1 ? bjSize : 0);
    } else {
        // Show loop size in beats (beatloop_size control)
        const loopSize = engine.getValue(group, "beatloop_size");
        BrevvDeck.updateDisplay(deckNum, loopSize >= 1 ? loopSize : 0);
    }
};

// ============================================================
// Initialization & Shutdown
// ============================================================

BrevvDeck.init = function() {
    // Enable soft takeover for all non-motorized analog controls
    for (let deck = 1; deck <= 4; deck++) {
        const group = "[Channel" + deck + "]";

        engine.softTakeover(group, "rate", true);
        engine.softTakeover(group, "volume", true);
        engine.softTakeover(group, "pregain", true);
        engine.softTakeover(group, "playposition", true);

        // Initialize beat jump size
        const state = BrevvDeck.deckState[deck];
        engine.setValue(group, "beatjump_size",
            BrevvDeck.BEAT_JUMP_SIZES[state.beatJumpSizeIndex]);
    }

    // Soft takeover for EQ and effect knobs
    for (let ch = 1; ch <= 4; ch++) {
        const eqGroup = "[EqualizerRack1_[Channel" + ch + "]_Effect1]";
        engine.softTakeover(eqGroup, "parameter1", true);
        engine.softTakeover(eqGroup, "parameter2", true);
        engine.softTakeover(eqGroup, "parameter3", true);

        const qfxGroup = "[QuickEffectRack1_[Channel" + ch + "]]";
        engine.softTakeover(qfxGroup, "super1", true);
    }

    // Soft takeover for FX knobs
    for (let unit = 1; unit <= 2; unit++) {
        for (let fx = 1; fx <= 3; fx++) {
            const fxGroup = "[EffectRack1_EffectUnit" + unit + "_Effect" + fx + "]";
            engine.softTakeover(fxGroup, "meta", true);
        }
        const unitGroup = "[EffectRack1_EffectUnit" + unit + "]";
        engine.softTakeover(unitGroup, "mix", true);
    }

    // Soft takeover for master controls
    engine.softTakeover("[Master]", "headMix", true);
    engine.softTakeover("[Master]", "headVolume", true);
    engine.softTakeover("[Master]", "volume", true);
    engine.softTakeover("[Master]", "crossfader", true);

    // Set up display update connections: whenever loop size or shift changes,
    // refresh the 7-segment display
    for (let deck = 1; deck <= 4; deck++) {
        const group = "[Channel" + deck + "]";
        const d = deck; // capture for closure
        const conn = engine.makeConnection(group, "beatloop_size", function() {
            BrevvDeck.refreshDisplay(d);
        });
        if (conn) {
            BrevvDeck.connections.push(conn);
        }
        // Initial display update
        BrevvDeck.refreshDisplay(deck);
    }

    print("BrevvDeck: Initialized (v0.5)");
};

BrevvDeck.shutdown = function() {
    // Disconnect all output connections
    for (const conn of BrevvDeck.connections) {
        conn.disconnect();
    }
    BrevvDeck.connections = [];

    // Turn off all LEDs by sending Note Off to all used note numbers
    for (let ch = 0; ch < 4; ch++) {
        const noteOff = 0x80 + ch;
        // Transport + Quantize + Loop LEDs (0x00-0x07)
        for (let note = 0x00; note <= 0x07; note++) {
            midi.sendShortMsg(noteOff, note, 0x00);
        }
        // Hotcue/Roll LEDs (0x10-0x17)
        for (let hc = 0x10; hc <= 0x17; hc++) {
            midi.sendShortMsg(noteOff, hc, 0x00);
        }
        // Clear displays
        midi.sendShortMsg(0xB0 + ch, BrevvDeck.DISPLAY_CC, 0x00);
    }

    // Mixer LEDs (Ch 5)
    for (let mn = 0x00; mn <= 0x0F; mn++) {
        midi.sendShortMsg(0x84, mn, 0x00);
    }

    // FX LEDs (Ch 6, 7)
    for (let fn = 0x00; fn <= 0x03; fn++) {
        midi.sendShortMsg(0x85, fn, 0x00);
        midi.sendShortMsg(0x86, fn, 0x00);
    }

    // Sampler LEDs (Ch 8)
    for (let sn = 0x14; sn <= 0x17; sn++) {
        midi.sendShortMsg(0x87, sn, 0x00);
    }

    print("BrevvDeck: Shutdown — all LEDs off");
};

// ============================================================
// Deck Controls — Script-Bound Handlers
// ============================================================

// --- Hotcue/Roll Dual-Mode Buttons ---
// Mode is determined by a hardware toggle switch per deck.
// The firmware sends CC 0x03 with value 0 (hotcue) or 127 (roll) when the toggle changes.
// In hotcue mode: normal=activate, shift=clear
// In roll mode: press=activate beatlooproll, release=deactivate (slip mode)
BrevvDeck.hotcueRollButton = function(channel, control, value, status, group) {
    const deckNum = BrevvDeck.deckFromChannel(channel);
    const state = BrevvDeck.deckState[deckNum];
    const buttonIndex = control - 0x10; // 0x10 → 0, 0x11 → 1, etc.

    if (buttonIndex < 0 || buttonIndex > 7) {
        return;
    }

    if (state.rollMode) {
        // Slip-roll mode: activate on press, deactivate on release
        const rollSize = BrevvDeck.ROLL_SIZES[buttonIndex];
        if (value > 0) {
            engine.setValue(group, "beatlooproll_" + rollSize + "_activate", 1);
        } else {
            // Release deactivates the loop roll (slip mode returns to original position)
            engine.setValue(group, "beatlooproll_" + rollSize + "_activate", 0);
        }
    } else {
        // Hotcue mode
        if (value === 0) {
            return; // Ignore note-off in hotcue mode
        }
        const hotcueNum = buttonIndex + 1;

        if (BrevvDeck.isShifted(deckNum)) {
            engine.setValue(group, "hotcue_" + hotcueNum + "_clear", 1);
        } else {
            engine.setValue(group, "hotcue_" + hotcueNum + "_activate", 1);
        }
    }
};

// --- Hotcue/Roll Mode Toggle ---
// Firmware sends CC 0x03 on deck channel when hardware toggle changes.
// value 0 = hotcue mode, value 127 = roll mode.
BrevvDeck.rollModeToggle = function(channel, control, value, status, group) {
    const deckNum = BrevvDeck.deckFromChannel(channel);
    BrevvDeck.deckState[deckNum].rollMode = (value > 63);
};

// --- Beat Jump Encoder ---
// Normal rotate: jump forward (CW) or backward (CCW)
// Shift + rotate: halve (CCW) or double (CW) the beat jump size
BrevvDeck.beatJumpEncoder = function(channel, control, value, status, group) {
    const direction = BrevvDeck.decodeRelative(value);
    if (direction === 0) {
        return;
    }

    const deckNum = BrevvDeck.deckFromChannel(channel);

    if (BrevvDeck.isShifted(deckNum)) {
        // Shift + rotate: change beat jump size
        const state = BrevvDeck.deckState[deckNum];
        if (direction > 0 && state.beatJumpSizeIndex < BrevvDeck.BEAT_JUMP_SIZES.length - 1) {
            state.beatJumpSizeIndex++;
        } else if (direction < 0 && state.beatJumpSizeIndex > 0) {
            state.beatJumpSizeIndex--;
        }
        const newSize = BrevvDeck.BEAT_JUMP_SIZES[state.beatJumpSizeIndex];
        engine.setValue(group, "beatjump_size", newSize);
        // Update 7-segment display to show new beat jump size
        BrevvDeck.updateDisplay(deckNum, newSize >= 1 ? newSize : 0);
    } else {
        // Normal rotate: jump
        if (direction > 0) {
            engine.setValue(group, "beatjump_forward", 1);
        } else {
            engine.setValue(group, "beatjump_backward", 1);
        }
    }
};

// --- Beat Jump Size Cycle ---
// Push the beat jump encoder to cycle through sizes.
// Normal push: double. Shift+push: halve.
BrevvDeck.beatJumpSizeCycle = function(channel, control, value, status, group) {
    if (value === 0) {
        return; // Ignore release
    }

    const deckNum = BrevvDeck.deckFromChannel(channel);
    const state = BrevvDeck.deckState[deckNum];

    if (BrevvDeck.isShifted(deckNum)) {
        if (state.beatJumpSizeIndex > 0) {
            state.beatJumpSizeIndex--;
        }
    } else if (state.beatJumpSizeIndex < BrevvDeck.BEAT_JUMP_SIZES.length - 1) {
        state.beatJumpSizeIndex++;
    }

    const newSize = BrevvDeck.BEAT_JUMP_SIZES[state.beatJumpSizeIndex];
    engine.setValue(group, "beatjump_size", newSize);
    BrevvDeck.updateDisplay(deckNum, newSize >= 1 ? newSize : 0);
};

// --- Loop Encoder ---
// Rotate CW: double loop size. Rotate CCW: halve loop size.
BrevvDeck.loopEncoder = function(channel, control, value, status, group) {
    const direction = BrevvDeck.decodeRelative(value);
    if (direction === 0) {
        return;
    }

    if (direction > 0) {
        engine.setValue(group, "loop_double", 1);
    } else {
        engine.setValue(group, "loop_halve", 1);
    }
};

// --- Scrub Joystick ---
// 1-axis spring-return joystick: center (0x40) = no scrub.
// Left of center = scrub backward, right = scrub forward.
// Speed is proportional to deflection from center.
// Dead zone around center prevents drift from mechanical imprecision.
// Shift + joystick: fast seek mode (larger jog multiplier).
BrevvDeck.joystickScrub = function(channel, control, value, status, group) {
    const deckNum = BrevvDeck.deckFromChannel(channel);
    const deflection = value - 64;  // -64 to +63

    // Dead zone: ignore small deflections near center
    if (Math.abs(deflection) <= BrevvDeck.JOYSTICK_DEADZONE) {
        return;
    }

    // Remove dead zone offset so scrub starts smoothly from zero
    const effectiveDeflection = deflection > 0
        ? deflection - BrevvDeck.JOYSTICK_DEADZONE
        : deflection + BrevvDeck.JOYSTICK_DEADZONE;

    // Max effective deflection = 64 - deadzone
    const maxDeflection = 64 - BrevvDeck.JOYSTICK_DEADZONE;

    if (BrevvDeck.isShifted(deckNum)) {
        // Shift + joystick: fast seek (3× normal speed)
        const seekAmount = (effectiveDeflection / maxDeflection) * BrevvDeck.JOYSTICK_SCRUB_SPEED * 3;
        engine.setValue(group, "jog", seekAmount);
    } else {
        // Normal scrub: proportional to deflection
        const jogValue = (effectiveDeflection / maxDeflection) * BrevvDeck.JOYSTICK_SCRUB_SPEED;
        engine.setValue(group, "jog", jogValue);
    }
};

// --- Per-Deck Shift ---
BrevvDeck.shift = function(channel, control, value, status, group) {
    const deckNum = BrevvDeck.deckFromChannel(channel);
    BrevvDeck.deckState[deckNum].shifted = (value > 0);
};

// ============================================================
// Global Shift Buttons
// ============================================================

BrevvDeck.shiftLeft = function(channel, control, value, status, group) {
    BrevvDeck.shiftLeftActive = (value > 0);
    // Refresh displays for decks 1 and 3 (shift changes what's shown)
    BrevvDeck.refreshDisplay(1);
    BrevvDeck.refreshDisplay(3);
};

BrevvDeck.shiftRight = function(channel, control, value, status, group) {
    BrevvDeck.shiftRightActive = (value > 0);
    // Refresh displays for decks 2 and 4
    BrevvDeck.refreshDisplay(2);
    BrevvDeck.refreshDisplay(4);
};

// ============================================================
// Library Browse Encoder
// ============================================================

BrevvDeck.browseEncoder = function(channel, control, value, status, group) {
    const direction = BrevvDeck.decodeRelative(value);
    if (direction === 0) {
        return;
    }

    if (BrevvDeck.shiftLeftActive || BrevvDeck.shiftRightActive) {
        // Shift + browse = waveform zoom
        if (direction > 0) {
            engine.setValue("[Controls]", "waveform_zoom_up", 1);
        } else {
            engine.setValue("[Controls]", "waveform_zoom_down", 1);
        }
    } else {
        engine.setValue("[Library]", "MoveVertical", direction);
    }
};

// ============================================================
// FX Select/Focus
// ============================================================

// Cycle through the focused effect in an FX unit.
// Press: advance focus (0=none → 1 → 2 → 3 → 0)
BrevvDeck.fxSelect = function(channel, control, value, status, group) {
    if (value === 0) {
        return;
    }

    let unitNum;
    if ((status & 0x0F) === 0x05) {
        unitNum = 1;
    } else {
        unitNum = 2;
    }

    const unitGroup = "[EffectRack1_EffectUnit" + unitNum + "]";
    const currentFocus = engine.getValue(unitGroup, "focused_effect");
    const newFocus = (currentFocus + 1) % 4;
    engine.setValue(unitGroup, "focused_effect", newFocus);
};

// ============================================================
// Sampler Buttons
// ============================================================

// If sampler has a track loaded: play/restart it.
// If sampler is empty: load the currently selected library track.
// Shift + press: eject the sample.
BrevvDeck.samplerButton = function(channel, control, value, status, group) {
    if (value === 0) {
        return; // Ignore release
    }

    const isShifted = BrevvDeck.shiftLeftActive || BrevvDeck.shiftRightActive;
    const trackLoaded = engine.getValue(group, "track_loaded");

    if (isShifted && trackLoaded) {
        // Shift + press on loaded sampler = eject
        engine.setValue(group, "eject", 1);
    } else if (trackLoaded) {
        // Track loaded: play from start
        engine.setValue(group, "cue_gotoandplay", 1);
    } else {
        // No track: load selected track
        engine.setValue(group, "LoadSelectedTrack", 1);
    }
};
