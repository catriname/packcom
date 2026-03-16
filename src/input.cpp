// ============================================================================
//  input.cpp
// ============================================================================
#include "input.h"
#include "keys.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

InputManager::InputManager(PackComConfig& cfg, SendFn on_send)
    : cfg_(cfg), on_send_(on_send) {}

// ---------------------------------------------------------------------------
// Macro file  (binary format: each entry = 1-byte length + text)
// We use simple text format for portability
// ---------------------------------------------------------------------------

bool InputManager::load_macros(const std::string& path) {
    FILE* f = ::fopen(path.c_str(), "r");
    if (!f) return false;
    char line[512];
    int idx = 0;
    while (::fgets(line, sizeof(line), f) && idx < NUM_MACRO_KEYS) {
        size_t len = ::strlen(line);
        while (len > 0 && (line[len-1]=='\r'||line[len-1]=='\n')) line[--len]=0;
        // Format: LABEL\tTEXT
        char* tab = ::strchr(line, '\t');
        if (tab) {
            *tab = 0;
            macros_[idx].label = line;
            macros_[idx].text  = tab+1;
        } else {
            macros_[idx].label = line;
            macros_[idx].text  = "";
        }
        ++idx;
    }
    ::fclose(f);
    return true;
}

bool InputManager::save_macros(const std::string& path) const {
    FILE* f = ::fopen(path.c_str(), "w");
    if (!f) return false;
    for (auto& m : macros_)
        ::fprintf(f, "%s\t%s\n", m.label.c_str(), m.text.c_str());
    ::fclose(f);
    return true;
}

// ---------------------------------------------------------------------------
// Macro expansion
// ] = CR, @x = command char (sends text as TNC cmd), ~ = 500ms delay, ; = comment
// ---------------------------------------------------------------------------

void InputManager::expand_and_send(const std::string& macro_text) {
    std::string out;
    bool in_comment = false;
    for (size_t i = 0; i < macro_text.size() && !in_comment; ++i) {
        char c = macro_text[i];
        if (c == ']') {
            out += '\r';
        } else if (c == '~') {
            if (on_send_ && !out.empty()) { on_send_(out); out.clear(); }
            Platform::sleep_ms(500);
        } else if (c == ';') {
            in_comment = true;  // rest of string is a comment
        } else if (c == '@' && i+1 < macro_text.size()) {
            // @x means send as TNC command; for now just include the char
            out += macro_text[++i];
        } else {
            out += c;
        }
    }
    if (on_send_ && !out.empty()) on_send_(out);
}

// ---------------------------------------------------------------------------
// Key processing
// ---------------------------------------------------------------------------

int InputManager::macro_index(int key) {
    // Map F-keys and their shifted/ctrl/alt variants to macro slots
    if (key >= KEY_F1 && key >= KEY_F1 - 10) return -(key - KEY_F1);
    return -1;
}

void InputManager::insert_char(char c) {
    if (insert_mode_) {
        line_.insert(line_.begin() + cursor_, c);
    } else {
        if (cursor_ < (int)line_.size())
            line_[cursor_] = c;
        else
            line_ += c;
    }
    ++cursor_;
}

void InputManager::commit_line() {
    if (on_send_) on_send_(line_);
    if (!line_.empty()) {
        history_.push_front(line_);
        if ((int)history_.size() > 50) history_.pop_back();
    }
    line_.clear();
    cursor_     = 0;
    history_pos_= -1;
}

bool InputManager::process_key(int key) {
    // --- Character mode: send immediately, no editing ---
    if (mode_ == InputMode::Char) {
        if (key > 0 && key < 256) {
            std::string s(1, (char)key);
            if (on_send_) on_send_(s);
        }
        return true;
    }

    // --- Line mode ---
    // Navigation / editing (recovered from binary help screen)
    switch (key) {
        // (Left Arrow) = Move Cursor to Left
        case KEY_LEFT:
            if (cursor_ > 0) --cursor_;
            return true;

        // (Right Arrow) = Move Cursor to Right
        case KEY_RIGHT:
            if (cursor_ < (int)line_.size()) ++cursor_;
            return true;

        // (Cntl Home) = Beginning of Line
        case KEY_CTRL_HOME:
            cursor_ = 0;
            return true;

        // (End) = End of Line
        case KEY_END:
            cursor_ = (int)line_.size();
            return true;

        // (Cntl End) = Erase from Cursor to End of Line
        case KEY_CTRL_END:
            line_.erase(cursor_);
            return true;

        // (Ins) = Toggle Insert Mode
        case KEY_INS:
            insert_mode_ = !insert_mode_;
            return true;

        // (Del) = Delete Current Character
        case KEY_DEL:
            if (cursor_ < (int)line_.size())
                line_.erase(cursor_, 1);
            return true;

        // (Back Space) = Delete Previous Character
        case 127:
            if (cursor_ > 0) {
                line_.erase(--cursor_, 1);
            }
            return true;

        // (Cntl Left Arrow) = Tab Cursor Left
        case KEY_CTRL_LEFT: {
            if (cursor_ <= 0) return true;
            --cursor_;
            while (cursor_ > 0 && line_[cursor_-1] != ' ') --cursor_;
            return true;
        }

        // (Cntl Right Arrow) = Tab Cursor Right
        case KEY_CTRL_RIGHT: {
            int sz = (int)line_.size();
            while (cursor_ < sz && line_[cursor_] != ' ') ++cursor_;
            while (cursor_ < sz && line_[cursor_] == ' ') ++cursor_;
            return true;
        }

        // (Cntl Page Up) = Scroll Keyboard Buffer Backward
        case KEY_CTRL_PGUP:
            if (!history_.empty()) {
                if (history_pos_ < (int)history_.size() - 1) {
                    ++history_pos_;
                    line_   = history_[history_pos_];
                    cursor_ = (int)line_.size();
                }
            }
            return true;

        // (Cntl Page Down) = Scroll Keyboard Buffer Forward
        case KEY_CTRL_PGDN:
            if (history_pos_ > 0) {
                --history_pos_;
                line_   = history_[history_pos_];
                cursor_ = (int)line_.size();
            } else if (history_pos_ == 0) {
                history_pos_ = -1;
                line_.clear();
                cursor_ = 0;
            }
            return true;

        // Enter: commit  (KEY_ENTER == 13 == '\r', use only one)
        case KEY_ENTER:
            commit_line();
            return true;

        // ESC: clear line
        case KEY_ESC:
            line_.clear();
            cursor_ = 0;
            return true;

        default:
            break;
    }

    // Printable character
    if (key >= 32 && key < 127) {
        insert_char((char)key);
        return true;
    }

    // Ctrl characters (pass-through for TNC commands)
    if (key >= 1 && key < 32) {
        insert_char((char)key);
        return true;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Draw the input line at the bottom of the screen
// ---------------------------------------------------------------------------

void InputManager::draw_input_line(int screen_row, int screen_cols) const {
    Platform::goto_xy(screen_row, 0);
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);

    // Mode indicator
    std::string mode_label;
    if (mode_ == InputMode::Line)
        mode_label = "  Key Board Input (LINE MODE)  ";
    else
        mode_label = "  Key Board Input (CHAR MODE)  ";

    Platform::set_color(cfg_.colors.rev_fg, cfg_.colors.rev_bg);
    Platform::puts_raw(mode_label);
    Platform::reset_color();

    Platform::goto_xy(screen_row + 1, 0);
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);

    // Clip line to screen width minus a small margin
    std::string display = line_;
    if ((int)display.size() >= screen_cols) display = display.substr(0, screen_cols - 1);
    Platform::puts_raw(display);
    // Pad rest with spaces
    if ((int)display.size() < screen_cols)
        Platform::puts_raw(std::string(screen_cols - display.size(), ' '));

    // Position cursor
    Platform::goto_xy(screen_row + 1, std::min(cursor_, screen_cols - 1));
    Platform::reset_color();
}

// ---------------------------------------------------------------------------
// Macro editor menu
// ---------------------------------------------------------------------------

void InputManager::run_macro_menu() {
    Platform::clear_screen();
    Platform::goto_xy(1, 2);
    Platform::puts_raw("Function Keys, Use: ] for CR, @x for CMDS, ~ for delay, ; for comment.");
    Platform::goto_xy(2, 2);
    Platform::puts_raw("Enter: C=Chg, F=PgFwd, B=PgBak, Q=Quit.");

    // Labels: Unshifted, Shifted, Ctrl, Alt  (recovered from .001)
    const char* section_names[] = {"< Unshifted >", "< Shifted >", "< Ctrl >", "< Alt >"};
    int page = 0;  // 0-3 for the four sections

    bool done = false;
    while (!done) {
        Platform::goto_xy(3, 2);
        Platform::puts_raw(std::string(76, ' '));
        Platform::goto_xy(3, 2);
        Platform::puts_raw(section_names[page % 4]);

        // Show 10 macros per page section
        int base = (page % 4) * 10;
        for (int i = 0; i < 10 && base+i < NUM_MACRO_KEYS; ++i) {
            Platform::goto_xy(5 + i, 2);
            char buf[80];
            ::snprintf(buf, sizeof(buf), "F%-2d  %-40s",
                i+1, macros_[base+i].text.c_str());
            Platform::puts_raw(buf);
        }

        Platform::goto_xy(17, 2);
        Platform::puts_raw("Enter: C=Chg, F=PgFwd, B=PgBak, Q=Quit.  ");

        int k = Platform::getch_raw();
        switch (k) {
            case 'c': case 'C': {
                Platform::goto_xy(18, 2);
                Platform::puts_raw("Which One? ");
                std::string num_str;
                int c2;
                while ((c2 = Platform::getch_raw()) != KEY_ENTER && c2 != '\r') {
                    if (c2 >= '0' && c2 <= '9') { num_str += (char)c2; Platform::putch((char)c2); }
                }
                int fn = num_str.empty() ? 0 : ::atoi(num_str.c_str()) - 1;
                if (fn >= 0 && fn < 10) {
                    int slot = base + fn;
                    Platform::goto_xy(19, 2);
                    Platform::puts_raw("New text: ");
                    std::string text;
                    char c3;
                    while ((c3 = (char)Platform::getch_raw()) != '\r' && c3 != KEY_ENTER) {
                        if ((unsigned char)c3 >= 32) { text += c3; Platform::putch(c3); }
                        else if (c3 == KEY_BACKSPACE && !text.empty()) {
                            text.pop_back(); Platform::putch('\b'); Platform::putch(' '); Platform::putch('\b');
                        }
                    }
                    macros_[slot].text = text;
                }
                break;
            }
            case 'f': case 'F': page = (page + 1) % 4; break;
            case 'b': case 'B': page = (page + 3) % 4; break;
            case 'q': case 'Q': done = true; break;
            default: break;
        }
    }
    save_macros();
}
