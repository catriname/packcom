#pragma once
// ============================================================================
//  keys.h  –  unified key code definitions
//  Regular printable chars return their ASCII value (positive).
//  Extended / special keys return a negative value from this enum.
// ============================================================================

enum KeyCode {
    // Navigation
    KEY_UP          = -1,
    KEY_DOWN        = -2,
    KEY_LEFT        = -3,
    KEY_RIGHT       = -4,
    KEY_HOME        = -5,
    KEY_END         = -6,
    KEY_PGUP        = -7,
    KEY_PGDN        = -8,
    KEY_INS         = -9,
    KEY_DEL         = -10,

    // Control combos
    KEY_CTRL_HOME   = -11,
    KEY_CTRL_END    = -12,
    KEY_CTRL_LEFT   = -13,
    KEY_CTRL_RIGHT  = -14,
    KEY_CTRL_PGUP   = -15,
    KEY_CTRL_PGDN   = -16,

    // Function keys
    KEY_F1          = -21,
    KEY_F2          = -22,
    KEY_F3          = -23,
    KEY_F4          = -24,
    KEY_F5          = -25,
    KEY_F6          = -26,
    KEY_F7          = -27,
    KEY_F8          = -28,
    KEY_F9          = -29,
    KEY_F10         = -30,

    // Alt combos  (recovered from PackCom ALT-key menu)
    KEY_ALT_H       = -41,   // Help
    KEY_ALT_C       = -42,   // Capture toggle
    KEY_ALT_I       = -43,   // Config / Init
    KEY_ALT_P       = -44,   // Comm Parms
    KEY_ALT_X       = -45,   // Exit
    KEY_ALT_V       = -46,   // View file / toggle split screen
    KEY_ALT_T       = -47,   // Transmit ASCII
    KEY_ALT_Y       = -48,   // YAPP transfer
    KEY_ALT_D       = -49,   // Directory
    KEY_ALT_N       = -50,   // New directory/drive
    KEY_ALT_E       = -51,   // Execute DOS command
    KEY_ALT_S       = -52,   // Scroll back
    KEY_ALT_K       = -53,   // Kill file
    KEY_ALT_M       = -54,   // Macro key defs
    KEY_ALT_Q       = -55,   // Toggle keyboard mode (Line/Char)
    KEY_ALT_F       = -56,   // Toggle Full/Half duplex
    KEY_ALT_B       = -57,   // Send BREAK
    KEY_ALT_O       = -58,   // Other commands
    KEY_ALT_A       = -59,   // Alarms and special effects

    // Misc
    KEY_ESC         = 27,    // ASCII ESC (also used as abort)
    KEY_ENTER       = 13,
    KEY_BACKSPACE   = 8,
    KEY_TAB         = 9,

    KEY_UNKNOWN     = -999
};

/// Return a human-readable name for a key code (for help display).
inline const char* key_name(int k) {
    switch (k) {
        case KEY_UP:          return "Up Arrow";
        case KEY_DOWN:        return "Down Arrow";
        case KEY_LEFT:        return "Left Arrow";
        case KEY_RIGHT:       return "Right Arrow";
        case KEY_HOME:        return "Home";
        case KEY_END:         return "End";
        case KEY_PGUP:        return "Page Up";
        case KEY_PGDN:        return "Page Down";
        case KEY_INS:         return "Ins";
        case KEY_DEL:         return "Del";
        case KEY_CTRL_HOME:   return "Ctrl-Home";
        case KEY_CTRL_END:    return "Ctrl-End";
        case KEY_CTRL_LEFT:   return "Ctrl-Left";
        case KEY_CTRL_RIGHT:  return "Ctrl-Right";
        case KEY_CTRL_PGUP:   return "Ctrl-PgUp";
        case KEY_CTRL_PGDN:   return "Ctrl-PgDn";
        case KEY_F1:          return "F1";
        case KEY_ALT_H:       return "Alt-H";
        case KEY_ALT_C:       return "Alt-C";
        case KEY_ALT_I:       return "Alt-I";
        case KEY_ALT_P:       return "Alt-P";
        case KEY_ALT_X:       return "Alt-X";
        case KEY_ALT_V:       return "Alt-V";
        case KEY_ESC:         return "ESC";
        case KEY_ENTER:       return "Enter";
        case KEY_BACKSPACE:   return "BackSpace";
        default:              return "?";
    }
}
