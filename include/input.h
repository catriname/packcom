#pragma once
// ============================================================================
//  input.h  –  keyboard input subsystem
//  Implements the keyboard editor (Line/Char modes), macro keys (PACKCOM.KEY),
//  and the scrollable keyboard input history buffer.
//  Key commands recovered verbatim from .001 overlay binary.
// ============================================================================
#include "packcom.h"
#include "platform.h"
#include "serial.h"
#include <string>
#include <deque>
#include <array>
#include <functional>

// Number of function keys supported for macros
constexpr int NUM_MACRO_KEYS = 40;  // F1-F10, Shift, Ctrl, Alt variants

class InputManager {
public:
    // Callback when user commits a line (presses Enter)
    using SendFn = std::function<void(const std::string&)>;

    InputManager(PackComConfig& cfg, SendFn on_send);

    // Load / save PACKCOM.KEY
    bool load_macros(const std::string& path = KEY_FILE);
    bool save_macros(const std::string& path = KEY_FILE) const;

    // Process one keypress; returns true if the application should continue
    bool process_key(int key);

    // Called each frame to draw the input line at the bottom of the screen
    void draw_input_line(int screen_row, int screen_cols) const;

    // Mode
    void set_mode(InputMode m) { mode_ = m; }
    InputMode mode() const     { return mode_; }

    // Duplex
    void set_duplex(DuplexMode d) { duplex_ = d; }
    DuplexMode duplex() const    { return duplex_; }

    // Macro editor menu
    void run_macro_menu();

    // Insert mode
    bool insert_mode() const { return insert_mode_; }

    // Current line text (for display)
    const std::string& line() const { return line_; }
    int cursor() const { return cursor_; }

private:
    PackComConfig& cfg_;
    SendFn         on_send_;

    InputMode  mode_   = InputMode::Line;
    DuplexMode duplex_ = DuplexMode::Full;
    bool       insert_mode_ = false;

    std::string line_;
    int         cursor_ = 0;

    // Scrollable input history (Ctrl-PgUp / Ctrl-PgDn)
    std::deque<std::string> history_;
    int history_pos_ = -1;

    // Macro table: indexed by a function-key code
    std::array<MacroKey, NUM_MACRO_KEYS> macros_;

    // Expand a macro string:
    // ] = CR, @x = TNC command prefix, ~ = 500ms delay, ; = comment
    void expand_and_send(const std::string& macro_text);

    // Insert a character at cursor
    void insert_char(char c);

    // Commit the current line
    void commit_line();

    static int macro_index(int key);
};
