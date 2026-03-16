// ============================================================================
//  terminal.cpp
// ============================================================================
#include "terminal.h"
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <sstream>

// ===========================================================================
//  ScrollBack
// ===========================================================================

ScrollBack::ScrollBack(int max_lines) : max_lines_(max_lines) {}

void ScrollBack::set_max_lines(int n) {
    max_lines_ = n;
    while ((int)lines_.size() > max_lines_) lines_.pop_front();
}

void ScrollBack::push(const TermLine& line) {
    lines_.push_back(line);
    while ((int)lines_.size() > max_lines_) lines_.pop_front();
}

int ScrollBack::search(const std::string& needle, int start_line) const {
    std::string lower_needle = needle;
    for (char& c : lower_needle) c = (char)std::tolower((unsigned char)c);

    for (int i = start_line; i < (int)lines_.size(); ++i) {
        std::string hay = lines_[i].text;
        for (char& c : hay) c = (char)std::tolower((unsigned char)c);
        if (hay.find(lower_needle) != std::string::npos) return i;
    }
    return -1;
}

// ===========================================================================
//  Terminal
// ===========================================================================

Terminal::Terminal(PackComConfig& cfg)
    : cfg_(cfg),
      scrollback_(cfg.scrollback_lines)
{}

Terminal::~Terminal() { shutdown(); }

void Terminal::init() {
    Platform::get_terminal_size(term_rows_, term_cols_);
    Platform::clear_screen();
    refresh_status_bar();
}

void Terminal::shutdown() {
    Platform::reset_color();
}

bool Terminal::push_window(const Window& w) {
    if ((int)win_stack_.size() >= MAX_WINDOW_DEPTH) {
        // Original message: " Windows nested too deep "
        write_status(" Windows nested too deep ");
        return false;
    }
    win_stack_.push_back(w);
    if (w.has_border) draw_window_border(w);
    return true;
}

void Terminal::pop_window() {
    if (!win_stack_.empty()) win_stack_.pop_back();
    repaint();
}

const Window& Terminal::current_window() const {
    static Window root = {1, 0, 23, 79, "", CLR_BLUE, false};  // default full-screen minus status rows
    if (win_stack_.empty()) return root;
    return win_stack_.back();
}

// ---------------------------------------------------------------------------
// classify_line – echo detection + keyword coloring
// ---------------------------------------------------------------------------
bool Terminal::classify_line(const std::string& text,
                              const std::string& last_sent,
                              Color& fg, Color& bg,
                              bool& is_echo, bool& keyword_hit) const {
    is_echo     = false;
    keyword_hit = false;

    // Echo detection: packet matches last sent
    if (!last_sent.empty() && text.find(last_sent) != std::string::npos) {
        fg       = cfg_.sfx.echo_colors.fg;
        bg       = cfg_.sfx.echo_colors.bg;
        is_echo  = true;
        return true;
    }

    // Keyword scan
    if (cfg_.sfx.keyword_scan_on) {
        std::string lower = text;
        for (char& c : lower) c = (char)std::tolower((unsigned char)c);
        for (auto& kw : cfg_.sfx.keywords) {
            if (kw.word.empty()) continue;
            std::string lkw = kw.word;
            for (char& c : lkw) c = (char)std::tolower((unsigned char)c);
            if (lower.find(lkw) != std::string::npos) {
                fg          = kw.fg;
                bg          = kw.bg;
                keyword_hit = true;
                if (kw.effects_on && cfg_.sfx.special_effects_on)
                    Platform::beep();
                return true;
            }
        }
    }

    // Header detection: AX.25 headers contain '>' or ':' patterns
    bool looks_like_header = (text.find('>') != std::string::npos &&
                               text.find(':') != std::string::npos);
    if (!looks_like_header) {
        fg = cfg_.sfx.nonheader_colors.fg;
        bg = cfg_.sfx.nonheader_colors.bg;
        return true;
    }

    // Default header colour
    fg = cfg_.colors.foreground;
    bg = cfg_.colors.background;
    return false;
}

// ---------------------------------------------------------------------------
// write_rx  –  data received from TNC/radio
// ---------------------------------------------------------------------------
void Terminal::write_rx(const std::string& text) {
    if (text.empty()) return;

    Color fg, bg;
    bool is_echo, keyword_hit;
    classify_line(text, last_sent_, fg, bg, is_echo, keyword_hit);

    // Eliminate blank lines if configured
    if (cfg_.eliminate_blank_lines) {
        bool all_blank = true;
        for (char c : text) if (c != ' ' && c != '\t' && c != '\r' && c != '\n') { all_blank = false; break; }
        if (all_blank) return;
    }

    // Bell on incoming data if enabled
    if (cfg_.incoming_bell && !text.empty() && text.find('\007') != std::string::npos)
        Platform::beep();

    TermLine line;
    line.text       = text;
    line.fg         = fg;
    line.bg         = bg;
    line.is_echo    = is_echo;
    line.keyword_hit= keyword_hit;
    scrollback_.push(line);

    if (auto_update_ && !scrollback_mode_) {
        Platform::set_color(fg, bg);
        Platform::puts_raw(text);
        Platform::reset_color();
    }
}

void Terminal::write_echo(const std::string& text) {
    last_sent_ = text;
    // In full-duplex the TNC echoes; in half-duplex we echo locally
    TermLine line;
    line.text    = text;
    line.fg      = cfg_.sfx.echo_colors.fg;
    line.bg      = cfg_.sfx.echo_colors.bg;
    line.is_echo = true;
    scrollback_.push(line);
}

void Terminal::write(const std::string& text, Color fg, Color bg) {
    Platform::set_color(fg, bg);
    Platform::puts_raw(text);
    Platform::reset_color();
}

void Terminal::write_status(const std::string& msg) {
    Platform::goto_xy(term_rows_ - 1, 0);
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    // Pad to full width
    std::string padded = msg;
    if ((int)padded.size() < term_cols_)
        padded.append(term_cols_ - padded.size(), ' ');
    else
        padded = padded.substr(0, term_cols_);
    Platform::puts_raw(padded);
    Platform::reset_color();
}

// ---------------------------------------------------------------------------
// Scroll-back mode
// ---------------------------------------------------------------------------

void Terminal::enter_scrollback_mode() {
    scrollback_mode_ = true;
    scroll_pos_      = std::max(0, scrollback_.line_count() - (term_rows_ - 3));
    repaint();
}

void Terminal::exit_scrollback_mode() {
    scrollback_mode_ = false;
    repaint();
}

void Terminal::scroll_up(int lines) {
    scroll_pos_ = std::max(0, scroll_pos_ - lines);
    repaint();
}

void Terminal::scroll_down(int lines) {
    int max_pos = std::max(0, scrollback_.line_count() - (term_rows_ - 3));
    scroll_pos_ = std::min(max_pos, scroll_pos_ + lines);
    repaint();
}

void Terminal::scroll_to(int line) {
    scroll_pos_ = std::max(0, std::min(line, scrollback_.line_count() - 1));
    repaint();
}

void Terminal::search_scrollback(const std::string& needle, int from_line) {
    int found = scrollback_.search(needle, from_line);
    if (found >= 0) {
        scroll_to(found);
        write_status("Searching for \"" + needle + "\" beginning at line " +
                     std::to_string(from_line));
    } else {
        write_status("Search Argument NOT FOUND, press ENTER to continue");
    }
}

// ---------------------------------------------------------------------------
// Split screen toggle
// ---------------------------------------------------------------------------

void Terminal::toggle_split_screen() {
    split_screen_ = !split_screen_;
    repaint();
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

void Terminal::refresh_status_bar() {
    Platform::goto_xy(0, 0);
    Platform::set_color(cfg_.colors.rev_fg, cfg_.colors.rev_bg);
    std::string bar = "     Alt:  H=Help, C=Capture, I=Config, P=Comm Parms, X=Exit.";
    if (cfg_.display_time)
        bar += "  " + Platform::current_time_str();
    if ((int)bar.size() < term_cols_)
        bar.append(term_cols_ - bar.size(), ' ');
    else
        bar = bar.substr(0, term_cols_);
    Platform::puts_raw(bar);
    Platform::reset_color();
}

// ---------------------------------------------------------------------------
// Repaint
// ---------------------------------------------------------------------------

void Terminal::repaint() {
    Platform::clear_screen();
    refresh_status_bar();

    int display_rows = term_rows_ - 2;   // leave 1 status top, 1 bottom
    int start_line   = scrollback_mode_ ? scroll_pos_
                                        : std::max(0, scrollback_.line_count() - display_rows);

    for (int i = 0; i < display_rows; ++i) {
        int idx = start_line + i;
        if (idx < 0 || idx >= scrollback_.line_count()) {
            Platform::goto_xy(1 + i, 0);
            Platform::puts_raw(std::string(term_cols_, ' '));
        } else {
            draw_line_at(1 + i, scrollback_[idx]);
        }
    }

    // Bottom status row
    if (scrollback_mode_) {
        std::string sb_status = "View [";
        sb_status += std::to_string(scroll_pos_) + "-" +
                     std::to_string(scroll_pos_ + display_rows) + "], " +
                     "<Any Key> to Pause or <End> to Exit";
        write_status(sb_status);
    } else {
        std::string mode_str = "  Key Board Input (LINE MODE)  ";
        write_status(mode_str);
    }
}

void Terminal::draw_line_at(int screen_row, const TermLine& line, int col_offset) {
    Platform::goto_xy(screen_row, col_offset);
    Platform::set_color(line.fg, line.bg);
    std::string display = line.text;
    // Clip to terminal width
    if ((int)display.size() > term_cols_ - col_offset)
        display = display.substr(0, term_cols_ - col_offset);
    Platform::puts_raw(display);
    Platform::reset_color();
}

// ---------------------------------------------------------------------------
// Window borders
// ---------------------------------------------------------------------------

void Terminal::draw_window_border(const Window& w) {
    Platform::set_color(w.border_color, cfg_.colors.background);

    // Top border
    Platform::goto_xy(w.top, w.left);
    Platform::putch('+');
    for (int c = w.left+1; c < w.right; ++c) Platform::putch('-');
    Platform::putch('+');

    // Title
    if (!w.title.empty()) {
        int title_col = w.left + (w.right - w.left - (int)w.title.size()) / 2;
        if (title_col < w.left+1) title_col = w.left+1;
        Platform::goto_xy(w.top, title_col);
        Platform::puts_raw(w.title);
    }

    // Sides
    for (int r = w.top+1; r < w.bottom; ++r) {
        Platform::goto_xy(r, w.left);  Platform::putch('|');
        Platform::goto_xy(r, w.right); Platform::putch('|');
    }

    // Bottom
    Platform::goto_xy(w.bottom, w.left);
    Platform::putch('+');
    for (int c = w.left+1; c < w.right; ++c) Platform::putch('-');
    Platform::putch('+');

    Platform::reset_color();
}

// ---------------------------------------------------------------------------
// Help screens  (text recovered verbatim from binary strings)
// ---------------------------------------------------------------------------

void Terminal::draw_comm_buffer_help() {
    Platform::clear_screen();
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    const char* lines[] = {
        "   1 = Help menu for Communication Buffer.",
        "   2 = Help menu for Keyboard Edit.",
        "   X = EXit and return to DOS.",
        "   C = Toggle capture mode ON/OFF.",
        "   D = Display the disk directory.",
        "   N = New directory and/or drive.",
        "   E = Execute DOS Command.",
        "   S = Scroll Back Buffer Processing.",
        "   V = View file or Toggle View file.",
        "   K = Kill file.   M = Macro key defs.",
        "   H = Help menu.   I = Change config.",
        "   P = Change communication parameters.",
        "   T = Transmit using ASCII XON/XOFF.",
        "   Y = Transmit using YAPP protocal.",
        "   A = Alarms and Special Effects.",
        "   Q = Toggle Keyboard Mode (Line/Char).",
        "   F = Toggle between FULL/HALF duplex.",
        "   B = Send BREAK.  O = Other Commands.",
        "   ^PrtSc = Toggle printer.",
        "   (Home) = Toggle Auto Update of Communication Buffer.",
        nullptr
    };
    int row = 2;
    for (int i = 0; lines[i]; ++i)
        { Platform::goto_xy(row++, 2); Platform::puts_raw(lines[i]); }
    Platform::goto_xy(row+1, 2); Platform::puts_raw("  Press ANY key to continue...");
    Platform::reset_color();
}

void Terminal::draw_keyboard_edit_help() {
    Platform::clear_screen();
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    const char* lines[] = {
        " (Up Arrow)   = Scroll Back One Line.",
        " (Down Arrow) = Scroll Forward One Line.",
        " (Page Up)    = Scroll Back One Page.",
        " (Page Down)  = Scroll Forward One Page.",
        " (Left Arrow)       = Move Cursor to Left.",
        " (Right Arrow)      = Move Cursor to Right.",
        " (Ins)              = Toggle Insert Mode.",
        " (Del)              = Delete Current Character.",
        " (Back Space)       = Delete Previous Character.",
        " (Cntl Home)        = Beginning of Line.",
        " (End)              = End of Line.",
        " (Cntl Left Arrow)  = Tab Cursor Left.",
        " (Cntl Right Arrow) = Tab Cursor Right.",
        " (Cntl End)         = Erase from Cursor to End of Line.",
        " (Cntl Page Up)     = Scroll Keyboard Buffer Backward.",
        " (Cntl Page Down)   = Scroll Keyboard Buffer Forward.",
        nullptr
    };
    int row = 2;
    for (int i = 0; lines[i]; ++i)
        { Platform::goto_xy(row++, 2); Platform::puts_raw(lines[i]); }
    Platform::goto_xy(row+1, 2); Platform::puts_raw("  Press ANY key to continue...");
    Platform::reset_color();
}

void Terminal::draw_directory(const std::string& mask) {
    std::vector<std::string> names;
    std::vector<long> sizes;
    uint64_t free_bytes = Platform::list_directory(mask, names, sizes);

    Platform::clear_screen();
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    Platform::goto_xy(1, 2);
    Platform::puts_raw("                Dir Mask: " + mask);

    int row = 3;
    for (int i = 0; i < (int)names.size() && row < 22; ++i, ++row) {
        Platform::goto_xy(row, 4);
        char line[80];
        ::snprintf(line, sizeof(line), "%-20s  %8ld", names[i].c_str(), sizes[i]);
        Platform::puts_raw(line);
    }

    char free_str[64];
    ::snprintf(free_str, sizeof(free_str), "   Bytes Available: %llu",
               (unsigned long long)free_bytes);
    Platform::goto_xy(23, 2);
    Platform::puts_raw(free_str);
    Platform::goto_xy(24, 2);
    Platform::puts_raw("  Press ANY key to continue...");
    Platform::reset_color();
}
