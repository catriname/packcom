// ============================================================================
//  fileops.cpp
// ============================================================================
#include "fileops.h"
#include "keys.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

// ===========================================================================
//  CaptureFile
// ===========================================================================

CaptureFile::CaptureFile(PackComConfig& cfg) : cfg_(cfg) {}

CaptureFile::~CaptureFile() { close_file(); }

bool CaptureFile::open_file(const std::string& path, bool overwrite) {
    const char* mode = overwrite ? "w" : "a";
    file_ = ::fopen(path.c_str(), mode);
    return file_ != nullptr;
}

void CaptureFile::close_file() {
    if (file_) { ::fclose(file_); file_ = nullptr; }
    active_   = false;
    filename_ = "";
}

bool CaptureFile::toggle(Terminal& term) {
    if (active_) {
        // Turning OFF
        close_file();
        term.write_status("Capture Mode  OFF");
        return false;
    }

    // Turning ON – prompt for filename
    term.write_status("Capture Mode");
    Platform::goto_xy(20, 4);
    Platform::set_color(term.cfg_.colors.foreground, term.cfg_.colors.background);
    Platform::puts_raw("\" Enter Filename for Capture Save: ");

    std::string fname;
    int k;
    while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
        if (k == KEY_BACKSPACE || k == 127) {
            if (!fname.empty()) {
                fname.pop_back();
                Platform::putch('\b'); Platform::putch(' '); Platform::putch('\b');
            }
        } else if (k == KEY_ESC) {
            Platform::reset_color();
            return false;
        } else if (k >= 32 && k < 127) {
            fname += (char)k;
            Platform::putch((char)k);
        }
    }

    if (fname.empty() || fname.find_first_not_of(" ") == std::string::npos) {
        Platform::goto_xy(21, 4);
        Platform::puts_raw(" *ERROR* Bad Filename");
        Platform::sleep_ms(1500);
        Platform::reset_color();
        return false;
    }

    bool overwrite = false;
    if (Platform::file_exists(fname)) {
        Platform::goto_xy(21, 4);
        Platform::puts_raw("! Write over existing file (N/y) ? ");
        k = Platform::getch_raw();
        overwrite = (k == 'y' || k == 'Y');
    }

    // Ask for timestamps
    Platform::goto_xy(22, 4);
    Platform::puts_raw("7 Generate Date/Time Stamps during capture mode (Y/n) ? ");
    k = Platform::getch_raw();
    timestamps_ = !(k == 'n' || k == 'N');

    if (!open_file(fname, overwrite)) {
        Platform::goto_xy(23, 4);
        Platform::puts_raw(" *ERROR* Bad Filename  Continue? y/n ");
        k = Platform::getch_raw();
        Platform::reset_color();
        return false;
    }

    filename_ = fname;
    active_   = true;
    term.write_status("Capture Mode  ON  -> " + fname);
    Platform::reset_color();
    return true;
}

void CaptureFile::write(const std::string& data) {
    if (!active_ || !file_) return;
    if (timestamps_) {
        std::string ts = "[" + Platform::current_date_str() + " "
                             + Platform::current_time_str() + "] ";
        ::fwrite(ts.data(), 1, ts.size(), file_);
    }
    ::fwrite(data.data(), 1, data.size(), file_);
    ::fflush(file_);
}

// ===========================================================================
//  AsciiTransmit
// ===========================================================================

AsciiTransmit::AsciiTransmit(PackComConfig& cfg, Serial& serial, Terminal& term)
    : cfg_(cfg), serial_(serial), term_(term) {}

long AsciiTransmit::run() {
    Platform::clear_screen();
    Platform::goto_xy(3, 4);
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    Platform::puts_raw(" Enter Filename to Transmit: ");

    std::string fname;
    int k;
    while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
        if (k == KEY_BACKSPACE || k == 127) {
            if (!fname.empty()) {
                fname.pop_back();
                Platform::putch('\b'); Platform::putch(' '); Platform::putch('\b');
            }
        } else if (k == KEY_ESC) {
            Platform::reset_color();
            return -1;
        } else if (k >= 32 && k < 127) {
            fname += (char)k;
            Platform::putch((char)k);
        }
    }

    if (!Platform::file_exists(fname)) {
        Platform::goto_xy(5, 4);
        Platform::puts_raw(" Cannot find file: " + fname);
        Platform::sleep_ms(2000);
        Platform::reset_color();
        return -1;
    }

    long total = Platform::file_size(fname);
    FILE* f = ::fopen(fname.c_str(), "rb");
    if (!f) {
        Platform::goto_xy(5, 4);
        Platform::puts_raw(" Cannot find file: " + fname);
        Platform::sleep_ms(2000);
        Platform::reset_color();
        return -1;
    }

    Platform::goto_xy(5, 4);
    char hdr[256];
    ::snprintf(hdr, sizeof(hdr),
        "#s.Transmit ASCII File, <Any Key> to PAUSE/CANCEL  [%s]  (%ldK total)",
        fname.c_str(), total / 1024);
    Platform::puts_raw(hdr);

    long bytes_sent = 0;
    bool paused     = false;
    bool cancelled  = false;
    uint8_t c;

    while (::fread(&c, 1, 1, f) == 1) {
        // Check for keypress: pause or cancel
        if (Platform::kbhit()) {
            k = Platform::getch_raw();
            if (paused) {
                paused = false;
            } else {
                paused = true;
                Platform::goto_xy(7, 4);
                Platform::puts_raw("   -- PAUSED -- Any key to continue, ESC to cancel --");
            }
            if (k == KEY_ESC) { cancelled = true; break; }
        }

        if (!paused) {
            serial_.write_byte(c, cfg_.byte_delay_ms);
            if (c == '\r' || c == '\n') {
                if (cfg_.crlf_delay_ms > 0) Platform::sleep_ms(cfg_.crlf_delay_ms);
            }
            ++bytes_sent;

            // Progress display
            if (bytes_sent % 128 == 0) {
                Platform::goto_xy(8, 4);
                char prog[80];
                ::snprintf(prog, sizeof(prog),
                    "K  Sent so far, %ldK is Total Size of %s",
                    bytes_sent / 1024, fname.c_str());
                Platform::puts_raw(prog);
            }
        } else {
            Platform::sleep_ms(10);
        }
    }

    ::fclose(f);

    // Wait for output buffer to drain (matches original "Waiting for buffer to drain...")
    if (!cancelled) {
        Platform::goto_xy(10, 4);
        Platform::puts_raw(" Waiting for buffer to drain...");
        serial_.wait_drain(10000);
    }

    Platform::reset_color();
    return cancelled ? -1 : bytes_sent;
}

// ===========================================================================
//  FileViewer
// ===========================================================================

FileViewer::FileViewer(PackComConfig& cfg, Terminal& term)
    : cfg_(cfg), term_(term) {}

void FileViewer::run() {
    Platform::clear_screen();
    Platform::goto_xy(3, 4);
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);

    // Check if file viewer window has room (mirrors "Veiw File Windows ran out of room")
    if (term_.window_depth() >= Terminal::MAX_WINDOW_DEPTH - 1) {
        Platform::goto_xy(3, 4);
        Platform::puts_raw("# Veiw File Windows ran out of room ");
        Platform::sleep_ms(2000);
        Platform::reset_color();
        return;
    }

    Platform::puts_raw(" Enter Filename to View: ");

    std::string fname;
    int k;
    while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
        if (k == KEY_BACKSPACE || k == 127) {
            if (!fname.empty()) {
                fname.pop_back();
                Platform::putch('\b'); Platform::putch(' '); Platform::putch('\b');
            }
        } else if (k == KEY_ESC) { Platform::reset_color(); return; }
        else if (k >= 32 && k < 127) { fname += (char)k; Platform::putch((char)k); }
    }

    if (!Platform::file_exists(fname)) {
        Platform::goto_xy(5, 4);
        Platform::puts_raw(" Cannot find file: " + fname);
        Platform::sleep_ms(2000);
        Platform::reset_color();
        return;
    }

    view_file(fname, false);
    Platform::reset_color();
}

void FileViewer::view_file(const std::string& path, bool split_screen) {
    // Load all lines into memory
    std::vector<std::string> lines;
    FILE* f = ::fopen(path.c_str(), "r");
    if (!f) return;

    char buf[1024];
    while (::fgets(buf, sizeof(buf), f)) {
        size_t len = ::strlen(buf);
        while (len > 0 && (buf[len-1]=='\r'||buf[len-1]=='\n')) buf[--len]=0;
        lines.emplace_back(buf);
    }
    ::fclose(f);

    long total_size = Platform::file_size(path);
    int total_lines = (int)lines.size();

    int rows, cols;
    Platform::get_terminal_size(rows, cols);

    int view_top    = split_screen ? rows / 2 : 1;
    int view_height = split_screen ? (rows / 2 - 2) : (rows - 3);
    int top_line    = 0;
    bool done       = false;

    auto repaint = [&]() {
        for (int r = 0; r < view_height; ++r) {
            Platform::goto_xy(view_top + r, 0);
            int idx = top_line + r;
            if (idx < total_lines) {
                std::string line = lines[idx];
                if ((int)line.size() > cols) line = line.substr(0, cols);
                else line.resize(cols, ' ');
                Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
                Platform::puts_raw(line);
                Platform::reset_color();
            } else {
                Platform::puts_raw(std::string(cols, ' '));
            }
        }
        draw_view_status(path,
            std::min((long)(top_line + view_height), (long)total_lines) * 80,
            total_size, top_line, total_lines, split_screen);
    };

    repaint();

    while (!done) {
        int k = Platform::getch_raw();
        switch (k) {
            case KEY_UP:
                if (top_line > 0) { --top_line; repaint(); }
                break;
            case KEY_DOWN:
                if (top_line + view_height < total_lines) { ++top_line; repaint(); }
                break;
            case KEY_PGUP:
                top_line = std::max(0, top_line - view_height);
                repaint();
                break;
            case KEY_PGDN:
                top_line = std::min(total_lines - view_height, top_line + view_height);
                if (top_line < 0) top_line = 0;
                repaint();
                break;
            case KEY_END:
            case KEY_ESC:
                done = true;
                break;
            case KEY_ALT_V:
                // Toggle split screen
                view_file(path, !split_screen);
                return;
            default:
                // Any other key pauses (matches "<Any Key> to Pause or <End> to Exit")
                break;
        }
    }
}

void FileViewer::draw_view_status(const std::string& /*path*/, long viewed, long total,
                                   int cur_line, int /*total_lines*/, bool split) const {
    int rows, cols;
    Platform::get_terminal_size(rows, cols);
    int status_row = split ? (rows / 2 - 1) : (rows - 2);

    Platform::goto_xy(status_row, 0);
    Platform::set_color(cfg_.colors.rev_fg, cfg_.colors.rev_bg);

    char buf[160];
    if (!split) {
        ::snprintf(buf, sizeof(buf),
            "View [%d], <Any Key> to Pause or <End> to Exit  "
            "%ldK  Viewed so far, %ldK Total ]",
            cur_line, viewed/1024, total/1024);
    } else {
        ::snprintf(buf, sizeof(buf),
            "View [%d]  [ <alt-V> to Toggle Split Screen ]  "
            "%ldK  Viewed so far, %ldK Total ]",
            cur_line, viewed/1024, total/1024);
    }
    std::string s(buf);
    s.resize(cols, ' ');
    Platform::puts_raw(s);
    Platform::reset_color();
}

// ===========================================================================
//  DirManager
// ===========================================================================

DirManager::DirManager(PackComConfig& cfg, Terminal& term)
    : cfg_(cfg), term_(term) {}

void DirManager::show_directory() {
    // Get DTA (directory transfer area) – on POSIX this is just glob
    // Original message: "Unable to get current DTA" / "Cannot reset DTA"
    // We use Platform::list_directory which handles this transparently.

    Platform::clear_screen();
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);

    std::string cwd  = Platform::get_cwd();
    std::string mask = cwd + "/*";

    Platform::goto_xy(1, 4);
    Platform::puts_raw("Disk Directory");
    Platform::goto_xy(2, 4);
    Platform::puts_raw("                Dir Mask: " + mask);

    std::vector<std::string> names;
    std::vector<long> sizes;
    uint64_t free_bytes = Platform::list_directory(mask, names, sizes);

    int row = 4;
    int rows, cols;
    Platform::get_terminal_size(rows, cols);

    for (int i = 0; i < (int)names.size() && row < rows - 3; ++i) {
        if (i > 0 && i % 2 == 0) ++row;
        int col = (i % 2 == 0) ? 4 : 44;
        Platform::goto_xy(row, col);
        char entry[40];
        ::snprintf(entry, sizeof(entry), "%-20s %8ld", names[i].c_str(), sizes[i]);
        Platform::puts_raw(entry);
        if (i % 2 == 1) ++row;
    }

    Platform::goto_xy(rows - 3, 4);
    char free_str[64];
    ::snprintf(free_str, sizeof(free_str), "   Bytes Available: %llu",
               (unsigned long long)free_bytes);
    Platform::puts_raw(free_str);

    Platform::goto_xy(rows - 2, 4);
    Platform::puts_raw("  Press ANY key to continue...");
    Platform::reset_color();
    Platform::getch_raw();
}

void DirManager::change_dir_interactive() {
    Platform::clear_screen();
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    Platform::goto_xy(3, 4);
    Platform::puts_raw("Change Directory\\Drive");
    Platform::goto_xy(5, 4);
    Platform::puts_raw(" Current Drive\\Directory: " + Platform::get_cwd());

    // Change drive
    Platform::goto_xy(7, 4);
    Platform::puts_raw(" Enter New Drive Letter : ");
    int k = Platform::getch_raw();
    if (k != KEY_ENTER && k != '\r' && k >= 'A' && k <= 'z') {
        char letter = (char)(k & ~0x20);  // to uppercase
        Platform::change_drive(letter);
    }

    // Change directory
    Platform::goto_xy(9, 4);
    Platform::puts_raw("     Enter New Directory: ");
    std::string dir;
    while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
        if (k == KEY_BACKSPACE || k == 127) {
            if (!dir.empty()) {
                dir.pop_back();
                Platform::putch('\b'); Platform::putch(' '); Platform::putch('\b');
            }
        } else if (k == KEY_ESC) {
            Platform::reset_color();
            return;
        } else if (k >= 32 && k < 127) {
            dir += (char)k;
            Platform::putch((char)k);
        }
    }

    if (!dir.empty()) {
        if (!Platform::change_dir(dir)) {
            Platform::goto_xy(11, 4);
            Platform::puts_raw(" Can't access that directory!");
            Platform::sleep_ms(2000);
        }
    }
    Platform::reset_color();
}

void DirManager::kill_file() {
    Platform::clear_screen();
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    Platform::goto_xy(5, 4);
    Platform::puts_raw("\tKill File");
    Platform::goto_xy(7, 4);
    Platform::puts_raw(" Enter Filename to Kill: ");

    std::string fname;
    int k;
    while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
        if (k == KEY_BACKSPACE || k == 127) {
            if (!fname.empty()) {
                fname.pop_back();
                Platform::putch('\b'); Platform::putch(' '); Platform::putch('\b');
            }
        } else if (k == KEY_ESC) { Platform::reset_color(); return; }
        else if (k >= 32 && k < 127) { fname += (char)k; Platform::putch((char)k); }
    }

    if (!fname.empty()) {
        if (!Platform::file_delete(fname)) {
            Platform::goto_xy(9, 4);
            Platform::puts_raw(" Cannot kill file: " + fname);
            Platform::sleep_ms(2000);
        }
    }
    Platform::reset_color();
}

// ===========================================================================
//  DosShell
// ===========================================================================

DosShell::DosShell(PackComConfig& cfg, Terminal& term)
    : cfg_(cfg), term_(term) {}

bool DosShell::check_memory() const {
    // On DOS the program allocated a fixed pool; here we simply check
    // whether a non-zero value was configured (matches original logic).
    return cfg_.dos_mem_kb > 0;
}

void DosShell::run() {
    Platform::clear_screen();
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);

    if (!check_memory()) {
        // Matches: "External memory has not been allocated!"
        Platform::goto_xy(5, 4);
        Platform::puts_raw("(External memory has not been allocated! ");
        Platform::goto_xy(6, 4);
        Platform::puts_raw("7 Either an error has occured while allocating memory or");
        Platform::goto_xy(7, 4);
        Platform::puts_raw("L Set the DOS Command Memory Allocation Configuration Parm greater than Zero.");
        Platform::goto_xy(9, 4);
        Platform::puts_raw(" Returning to PackCom ");
        Platform::sleep_ms(2500);
        Platform::reset_color();
        return;
    }

    // Prompt for command  (or accept default program)
    Platform::goto_xy(5, 4);
    char prompt[160];
    ::snprintf(prompt, sizeof(prompt),
        " Program to execute (within %dK allocated):  ", cfg_.dos_mem_kb);
    Platform::puts_raw(prompt);

    // Default program
    std::string prog = cfg_.default_dos_prog;
    if (prog.empty()) prog = Platform::get_comspec();

    Platform::puts_raw(prog);

    // Allow override
    Platform::goto_xy(7, 4);
    Platform::puts_raw("? The above program to be executed can be change now if desired.");
    Platform::goto_xy(8, 4);
    Platform::puts_raw("@ If there is not enough memory allocated to execute the program,");
    Platform::goto_xy(9, 4);
    Platform::puts_raw("A then alter the Configuration Parm for External Memory Allocation");
    Platform::goto_xy(10, 4);
    Platform::puts_raw(" and restart PackCom.");

    Platform::goto_xy(12, 4);
    std::string override_prog;
    Platform::puts_raw("DOS Command being processed:  ");
    int k;
    while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
        if (k == KEY_BACKSPACE || k == 127) {
            if (!override_prog.empty()) {
                override_prog.pop_back();
                Platform::putch('\b'); Platform::putch(' '); Platform::putch('\b');
            }
        } else if (k == KEY_ESC) {
            Platform::goto_xy(15, 4); Platform::puts_raw("Command Cancelled.");
            Platform::sleep_ms(1000);
            Platform::reset_color();
            return;
        } else if (k >= 32 && k < 127) {
            override_prog += (char)k; Platform::putch((char)k);
        }
    }
    if (!override_prog.empty()) prog = override_prog;

    // Restore console before spawning child
    Platform::console_restore();
    Platform::clear_screen();

    // DOS shell info line  (mirrors "DOS has been entered with .K allocated.")
    ::printf("DOS has been entered with %dK allocated.\n", cfg_.dos_mem_kb);
    ::printf("Enter \"EXIT\" to get back to packcom.\n\n");
    ::fflush(stdout);

    Platform::exec_command(prog);

    // Re-init console on return
    Platform::console_init();
    Platform::clear_screen();
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    Platform::goto_xy(5, 4);
    Platform::puts_raw(" Returning to PackCom ");
    Platform::sleep_ms(500);
    Platform::reset_color();
}
