#pragma once
// ============================================================================
//  terminal.h  –  screen windowing, scroll-back buffer, keyword coloring
//  Mirrors the original PackCom windowed display system recovered from the
//  binary: "Windows nested too deep", "Which_Window out of range", etc.
// ============================================================================
#include "packcom.h"
#include "platform.h"
#include <string>
#include <vector>
#include <deque>
#include <functional>

// ---------------------------------------------------------------------------
// A single line of terminal output with colour metadata
// ---------------------------------------------------------------------------
struct TermLine {
    std::string text;
    Color fg = CLR_LIGHTGRAY;
    Color bg = CLR_BLACK;
    bool  is_echo    = false;   // keyboard/digi echo line
    bool  is_header  = false;   // AX.25 header line
    bool  keyword_hit= false;   // matched a keyword
};

// ---------------------------------------------------------------------------
// ScrollBack buffer
// ---------------------------------------------------------------------------
class ScrollBack {
public:
    explicit ScrollBack(int max_lines = SCROLLBACK_DEFAULT);

    void set_max_lines(int n);
    int  max_lines()  const { return max_lines_; }
    int  line_count() const { return (int)lines_.size(); }

    // Append a new line
    void push(const TermLine& line);

    // Random access
    const TermLine& operator[](int idx) const { return lines_[idx]; }

    // Search
    // Returns line index >= start_line where text contains needle,
    // or -1 if not found.
    int search(const std::string& needle, int start_line = 0) const;

    void clear() { lines_.clear(); }

private:
    int max_lines_;
    std::deque<TermLine> lines_;
};

// ---------------------------------------------------------------------------
// Window descriptor (mirrors the Pascal window stack)
// ---------------------------------------------------------------------------
struct Window {
    int top, left, bottom, right;  // 0-based screen co-ordinates
    std::string title;
    Color border_color = CLR_BLUE;
    bool  has_border   = true;
};

// ---------------------------------------------------------------------------
// Terminal  –  the main display engine
// ---------------------------------------------------------------------------
class Terminal {
public:
    static constexpr int MAX_WINDOW_DEPTH = 8;   // "Windows nested too deep"

    explicit Terminal(PackComConfig& cfg);
    ~Terminal();

    void init();
    void shutdown();

    // --- Window stack ---
    bool push_window(const Window& w);  // false = "nested too deep"
    void pop_window();
    const Window& current_window() const;
    int  window_depth() const { return (int)win_stack_.size(); }

    // --- Output ---
    // Write text to the current terminal window, applying keyword coloring
    void write(const std::string& text, Color fg, Color bg);
    // Write from serial with auto color-selection
    void write_rx(const std::string& text);
    // Write user's own keyboard input (echoed locally in half-duplex)
    void write_echo(const std::string& text);
    // Write a status bar message to the bottom row
    void write_status(const std::string& msg);

    // --- Scroll-back ---
    ScrollBack& scrollback() { return scrollback_; }
    void enter_scrollback_mode();
    void exit_scrollback_mode();
    bool in_scrollback_mode() const { return scrollback_mode_; }

    // Scroll back view
    void scroll_up(int lines = 1);
    void scroll_down(int lines = 1);
    void scroll_to(int line);
    int  scroll_pos() const { return scroll_pos_; }

    // Search within scroll-back
    void search_scrollback(const std::string& needle, int from_line);

    // --- Split screen (ALT-V toggle) ---
    void toggle_split_screen();
    bool split_screen() const { return split_screen_; }

    // --- Status / info bar (top row) ---
    void refresh_status_bar();

    // --- Keyword coloring engine ---
    // Returns true + fills fg/bg if line matches a keyword
    bool classify_line(const std::string& text,
                       const std::string& last_sent,
                       Color& fg, Color& bg, bool& is_echo, bool& keyword_hit) const;

    // --- Auto-update mode ---
    void set_auto_update(bool on) { auto_update_ = on; }
    bool auto_update() const      { return auto_update_; }

    // Force full screen repaint
    void repaint();

    // Draw directory listing in a window
    void draw_directory(const std::string& mask);

    // Draw the comm buffer help page
    void draw_comm_buffer_help();

    // Draw the keyboard edit help page
    void draw_keyboard_edit_help();

private:
    PackComConfig& cfg_;
    ScrollBack scrollback_;
    std::vector<Window> win_stack_;

    bool split_screen_   = false;
    bool scrollback_mode_= false;
    bool auto_update_    = true;
    int  scroll_pos_     = 0;       // top line index in scrollback view

    int term_rows_ = 25;
    int term_cols_ = 80;

    // Current line being assembled (handles partial writes)
    std::string current_line_buf_;

    // Last string sent by user (for echo detection)
    std::string last_sent_;

    void draw_window_border(const Window& w);
    void draw_line_at(int screen_row, const TermLine& line, int col_offset = 0);
    void update_last_sent(const std::string& s) { last_sent_ = s; }

    friend class PackComApp;
    friend class CaptureFile;
    friend class DosShell;
    friend class FileViewer;
    friend class DirManager;
};
