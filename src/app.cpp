// ============================================================================
//  app.cpp  –  PackComApp implementation
// ============================================================================
#include "app.h"
#include "keys.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

// ===========================================================================
//  Construction / destruction
// ===========================================================================

PackComApp::PackComApp() {}

PackComApp::~PackComApp() {
    if (serial_ && serial_->is_open()) serial_->close();
    Platform::console_restore();
}

// ===========================================================================
//  Entry point
// ===========================================================================

int PackComApp::run(int argc, char* argv[]) {
    // 1. Parse command-line flags
    bool force_init = false;
    for (int i = 1; i < argc; ++i) {
        if (::strcmp(argv[i], "/I") == 0 || ::strcmp(argv[i], "-I") == 0)
            force_init = true;
    }

    // 2. Init console raw mode
    Platform::console_init();

    // 3. Check overlay files exist (cosmetic check – we're not a real overlay system)
    check_overlay_files();

    // 4. Load or re-init configuration
    config_ = std::make_unique<Config>(cfg_);
    if (!force_init) {
        config_->load(CFG_FILE);
    } else {
        // Reset to defaults and save
        cfg_ = PackComConfig{};
        config_->save(CFG_FILE);
    }

    // 5. Init all subsystems
    init_subsystems();

    // 6. Display version banner
    display_version();

    // 7. Open serial port
    if (!open_serial()) {
        term_->write_status("WARNING: Could not open COM" +
                            std::to_string(cfg_.comm.com_port) +
                            " – running in offline mode.");
    }

    // 8. Load macros
    input_->load_macros(KEY_FILE);

    // 9. Main event loop
    main_loop();

    // 10. Save configuration on exit
    config_->save(CFG_FILE);

    Platform::console_restore();
    Platform::clear_screen();
    Platform::puts_raw("End of Program.\n");
    return 0;
}

// ===========================================================================
//  Initialisation helpers
// ===========================================================================

void PackComApp::check_overlay_files() {
    // Original fatal error if overlays missing:
    //   "One or more PackCom Overlay files were not found!"
    // Since we are the C++ port (no overlays), we silently skip.
    // But we mimic the check for correctness / documentation.
    //
    // In the original: PACKCOM.000 and PACKCOM.001 were loaded from
    // cfg_.overlay_path or the current directory.
    (void)OVERLAY_0;
    (void)OVERLAY_1;
}

void PackComApp::init_subsystems() {
    term_      = std::make_unique<Terminal>(cfg_);
    term_->init();

    capture_   = std::make_unique<CaptureFile>(cfg_);
    dir_mgr_   = std::make_unique<DirManager>(cfg_, *term_);
    dos_shell_ = std::make_unique<DosShell>(cfg_, *term_);

    serial_    = std::make_unique<Serial>(cfg_.comm);

    input_ = std::make_unique<InputManager>(cfg_,
        [this](const std::string& line) { send_line(line); });
}

bool PackComApp::open_serial() {
    if (!serial_->open()) return false;
    return true;
}

void PackComApp::display_version() {
    // Mirrors "Display_Version" recovered from overlay .000
    Platform::clear_screen();
    Platform::set_color(cfg_.colors.rev_fg, cfg_.colors.rev_bg);

    int rows, cols;
    Platform::get_terminal_size(rows, cols);

    auto centre = [&](const std::string& s) {
        int pad = (cols - (int)s.size()) / 2;
        return std::string(std::max(0, pad), ' ') + s;
    };

    Platform::goto_xy(5, 0);
    Platform::puts_raw(centre(" PackCom by Jim Wright (" + std::string(PACKCOM_AUTHOR) + ")"));
    Platform::goto_xy(7, 0);
    Platform::puts_raw(centre("   Version: " + std::string(PACKCOM_VERSION)));
    Platform::goto_xy(8, 0);
    Platform::puts_raw(centre("   Date:    " + std::string(PACKCOM_DATE)));
    Platform::reset_color();

    Platform::goto_xy(11, 0);
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    Platform::puts_raw(centre("  Press ANY key to continue..."));
    Platform::reset_color();
    Platform::getch_raw();

    Platform::clear_screen();
    term_->refresh_status_bar();
}

// ===========================================================================
//  Main event loop
// ===========================================================================

void PackComApp::main_loop() {
    last_time_mgmt_ms_ = Platform::millis();

    while (running_) {
        // --- 1. Drain serial RX buffer ---
        process_rx();

        // --- 2. Poll keyboard ---
        if (Platform::kbhit()) {
            int key = Platform::getch_raw();

            // ALT-key commands
            if (key <= KEY_ALT_A && key >= KEY_ALT_X) {
                handle_alt_key(key);
            } else if (key == KEY_HOME) {
                do_toggle_auto_update();
            } else if (term_->in_scrollback_mode()) {
                // In scrollback mode: navigation keys only
                switch (key) {
                    case KEY_UP:      term_->scroll_up(1);   break;
                    case KEY_DOWN:    term_->scroll_down(1); break;
                    case KEY_PGUP:    term_->scroll_up(term_->scrollback().line_count() > 0 ? 20 : 1); break;
                    case KEY_PGDN:    term_->scroll_down(20); break;
                    case KEY_END:
                    case KEY_ESC:     term_->exit_scrollback_mode(); break;
                    default:          break;
                }
            } else {
                // Pass to line editor
                input_->process_key(key);
                // Redraw input area
                int rows, cols;
                Platform::get_terminal_size(rows, cols);
                input_->draw_input_line(rows - 3, cols);
            }
        }

        // --- 3. Periodic time management ---
        check_time_management();

        Platform::sleep_ms(5);
    }
}

// ===========================================================================
//  Serial RX
// ===========================================================================

void PackComApp::process_rx() {
    if (!serial_->is_open()) return;

    std::string chunk;
    uint8_t b;
    // Read up to 256 bytes per iteration to avoid starving the keyboard
    for (int i = 0; i < 256 && serial_->read_byte(b); ++i) {
        chunk += (char)b;
    }

    if (chunk.empty()) return;

    // Write to terminal (handles coloring, scroll-back, keyword scan)
    term_->write_rx(chunk);

    // Write to capture file if active
    if (capture_->is_active()) capture_->write(chunk);

    // Printer passthrough (Ctrl-PrtSc toggle)
    if (printer_on_) {
        // On POSIX: /dev/lp0 or lpr; on Windows: LPT1.
        // Simple approach: write to stdout and rely on OS printer capture.
        ::fwrite(chunk.data(), 1, chunk.size(), stdout);
    }

    // Refresh display if in auto-update mode
    if (cfg_.display_time) {
        term_->refresh_status_bar();
    }
}

// ===========================================================================
//  Send a line to serial
// ===========================================================================

void PackComApp::send_line(const std::string& line) {
    if (!serial_->is_open()) {
        // Offline: just echo to terminal
        term_->write_echo(line + "\r\n");
        return;
    }

    // In half-duplex: echo to screen ourselves
    if (duplex_ == DuplexMode::Half) {
        term_->write_echo(line + "\r\n");
    }

    // TNC time-management: if configured, set TNC date/time on first connect
    if (cfg_.set_tnc_datetime) {
        static bool tnc_time_set = false;
        if (!tnc_time_set && serial_->carrier_detect()) {
            std::string dt_cmd = "DAYTIME " + Platform::current_date_str()
                               + " " + Platform::current_time_str() + "\r";
            serial_->write_str(dt_cmd, cfg_.byte_delay_ms, cfg_.crlf_delay_ms);
            tnc_time_set = true;
        }
    }

    serial_->write_str(line + "\r", cfg_.byte_delay_ms, cfg_.crlf_delay_ms);
}

// ===========================================================================
//  ALT-key dispatcher
// ===========================================================================

void PackComApp::handle_alt_key(int key) {
    switch (key) {
        case KEY_ALT_H: do_help();             break;
        case KEY_ALT_C: do_capture();          break;
        case KEY_ALT_I: do_config();           break;
        case KEY_ALT_P: do_comm_parms();       break;
        case KEY_ALT_X: do_exit();             break;
        case KEY_ALT_V: do_view();             break;
        case KEY_ALT_T: do_transmit_ascii();   break;
        case KEY_ALT_Y: do_yapp();             break;
        case KEY_ALT_D: do_directory();        break;
        case KEY_ALT_N: do_change_dir();       break;
        case KEY_ALT_E: do_dos_command();      break;
        case KEY_ALT_S: do_scrollback_menu();  break;
        case KEY_ALT_K: do_kill_file();        break;
        case KEY_ALT_M: do_macros();           break;
        case KEY_ALT_Q: do_toggle_input_mode(); break;
        case KEY_ALT_F: do_toggle_duplex();    break;
        case KEY_ALT_B: do_send_break();       break;
        case KEY_ALT_A: do_alarms();           break;
        case KEY_ALT_O: do_other_commands();   break;
        default: break;
    }
    // Repaint screen after returning from any modal command
    term_->repaint();
}

// ===========================================================================
//  Individual ALT-key handlers
// ===========================================================================

void PackComApp::do_help() {
    // ALT-H: show comm buffer help page (overlay .001 text)
    term_->draw_comm_buffer_help();
    Platform::getch_raw();
}

void PackComApp::do_capture() {
    // ALT-C: toggle capture mode
    capture_->toggle(*term_);
}

void PackComApp::do_config() {
    // ALT-I: configuration menu
    bool comm_changed = config_->run_menu(*term_);
    if (comm_changed && serial_->is_open()) {
        // Re-apply comm parameters
        serial_->reconfigure(cfg_.comm);
        term_->write_status("Comm parameters updated.  Press ALT-P to reconnect.");
    }
}

void PackComApp::do_comm_parms() {
    // ALT-P: real-time comm parms change, then reconnect serial
    config_->menu_comm_params(*term_);
    if (serial_->is_open()) {
        serial_->close();
    }
    if (!serial_->open()) {
        term_->write_status("Could not reopen COM" + std::to_string(cfg_.comm.com_port));
    } else {
        term_->write_status("COM" + std::to_string(cfg_.comm.com_port) +
                            " reopened at " + std::to_string(cfg_.comm.baud_rate) + " baud.");
    }
}

void PackComApp::do_exit() {
    // ALT-X: confirm and exit
    Platform::clear_screen();
    Platform::goto_xy(12, 30);
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    Platform::puts_raw("Exit?");
    Platform::goto_xy(13, 20);
    Platform::puts_raw(" Exit PackCom (N/y) ? ");
    Platform::reset_color();

    int k = Platform::getch_raw();
    if (k == 'y' || k == 'Y') {
        running_ = false;
    }
}

void PackComApp::do_view() {
    // ALT-V: view file or toggle split screen
    if (term_->split_screen()) {
        term_->toggle_split_screen();
    } else {
        FileViewer viewer(cfg_, *term_);
        viewer.run();
    }
}

void PackComApp::do_transmit_ascii() {
    // ALT-T: ASCII file transmit with XON/XOFF flow
    AsciiTransmit tx(cfg_, *serial_, *term_);
    tx.run();
}

void PackComApp::do_yapp() {
    // ALT-Y: YAPP file transfer protocol menu
    YappTransfer yapp(*serial_, *term_);
    yapp.run_menu();
}

void PackComApp::do_directory() {
    // ALT-D: show disk directory
    dir_mgr_->show_directory();
}

void PackComApp::do_change_dir() {
    // ALT-N: change directory / drive
    dir_mgr_->change_dir_interactive();
}

void PackComApp::do_dos_command() {
    // ALT-E: execute external program / command processor
    dos_shell_->run();
}

void PackComApp::do_scrollback_menu() {
    // ALT-S: scroll-back buffer processing menu
    run_scrollback_interactive();
}

void PackComApp::do_kill_file() {
    // ALT-K: kill (delete) a file
    dir_mgr_->kill_file();
}

void PackComApp::do_macros() {
    // ALT-M: edit function-key macro definitions
    input_->run_macro_menu();
}

void PackComApp::do_toggle_input_mode() {
    // ALT-Q: toggle Line/Char keyboard mode
    InputMode new_mode = (input_->mode() == InputMode::Line)
                       ? InputMode::Char : InputMode::Line;
    input_->set_mode(new_mode);
    term_->write_status(new_mode == InputMode::Line
        ? "  Key Board Input (LINE MODE)  "
        : "  Key Board Input (CHAR MODE)  ");
}

void PackComApp::do_toggle_duplex() {
    // ALT-F: toggle Full/Half duplex
    duplex_ = (duplex_ == DuplexMode::Full) ? DuplexMode::Half : DuplexMode::Full;
    input_->set_duplex(duplex_);
    term_->write_status(duplex_ == DuplexMode::Full
        ? "Full Duplex" : "Half Duplex");
}

void PackComApp::do_send_break() {
    // ALT-B: send BREAK signal (optionally enters TNC command mode)
    if (serial_->is_open()) {
        serial_->send_break();
        if (cfg_.break_cmd_mode) {
            // After a BREAK, the TNC should enter command mode
            term_->write_status("BREAK sent – TNC entering CMD mode.");
        } else {
            term_->write_status("BREAK sent.");
        }
    }
}

void PackComApp::do_alarms() {
    // ALT-A: special effects / keyword alarm configuration
    config_->menu_special_effects(*term_);
}

void PackComApp::do_other_commands() {
    // ALT-O: "Other Commands" – comm port diagnostics display
    do_comm_port_info();
}

void PackComApp::do_toggle_auto_update() {
    // Home key: toggle auto-update of communication buffer display
    bool new_val = !term_->auto_update();
    term_->set_auto_update(new_val);
    term_->write_status(new_val
        ? "( Auto Update ON"
        : "( Auto Update OFF (press Home to toggle) ");
}

void PackComApp::do_comm_port_info() {
    // Displays the serial port diagnostic info
    // (mirrors Display_Comm_Port_Info recovered from overlay .001)
    Platform::clear_screen();
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);

    Platform::goto_xy(2, 4);
    Platform::puts_raw("sYIt");  // original label prefix from binary
    Platform::goto_xy(3, 4);
    Platform::puts_raw("Display_Comm_Port_Info");
    Platform::goto_xy(5, 0);
    Platform::puts_raw(serial_->diagnostics());

    Platform::goto_xy(20, 4);
    Platform::puts_raw("  Press RETURN key to continue...");
    Platform::reset_color();

    int k;
    while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r' && k != KEY_ESC) {}
}

// ===========================================================================
//  Scroll-back interactive processing
//  ("Scroll Back Processing" menu recovered from .001 overlay)
// ===========================================================================

void PackComApp::run_scrollback_interactive() {
    Platform::clear_screen();
    Platform::set_color(cfg_.colors.foreground, cfg_.colors.background);
    Platform::goto_xy(5, 4);
    Platform::puts_raw("Scroll Back Processing.");
    Platform::goto_xy(7, 4);
    Platform::puts_raw("  1. View Scroll Back Data beginning with Line Number");
    Platform::goto_xy(8, 4);
    Platform::puts_raw("  2. Scan Scroll Back Data");
    Platform::goto_xy(10, 4);
    Platform::puts_raw("Enter the option desired or RETURN to exit: ");
    Platform::reset_color();

    int k = Platform::getch_raw();
    switch (k) {
        case '1': {
            Platform::goto_xy(12, 4);
            Platform::puts_raw("'Enter beginning line to display >  ");
            std::string num_str;
            while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
                if (k >= '0' && k <= '9') { num_str += (char)k; Platform::putch((char)k); }
            }
            int line_num = num_str.empty() ? 0 : ::atoi(num_str.c_str());
            term_->enter_scrollback_mode();
            term_->scroll_to(line_num);
            break;
        }
        case '2': {
            Platform::goto_xy(12, 4);
            Platform::puts_raw("Enter text to search for >  ");
            std::string needle;
            while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
                if (k >= 32 && k < 127) { needle += (char)k; Platform::putch((char)k); }
                else if ((k == KEY_BACKSPACE || k == 127) && !needle.empty()) {
                    needle.pop_back(); Platform::putch('\b'); Platform::putch(' '); Platform::putch('\b');
                }
            }
            Platform::goto_xy(13, 4);
            Platform::puts_raw("\"Enter beginning line to search >  ");
            std::string start_str;
            while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
                if (k >= '0' && k <= '9') { start_str += (char)k; Platform::putch((char)k); }
            }
            int start_line = start_str.empty() ? 0 : ::atoi(start_str.c_str());
            Platform::goto_xy(15, 4);
            Platform::puts_raw("Searching for \"" + needle +
                               "\" beginning at line number " + std::to_string(start_line));

            int found = term_->scrollback().search(needle, start_line);
            if (found >= 0) {
                term_->enter_scrollback_mode();
                term_->scroll_to(found);
            } else {
                Platform::goto_xy(16, 4);
                Platform::puts_raw("2Search Argument NOT FOUND, press ENTER to continue");
                while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {}
            }
            break;
        }
        default:
            break;
    }
}

// ===========================================================================
//  Time management
// ===========================================================================

void PackComApp::check_time_management() {
    if (cfg_.time_mgmt_minutes <= 0) return;
    uint64_t now = Platform::millis();
    uint64_t interval_ms = (uint64_t)cfg_.time_mgmt_minutes * 60 * 1000;
    if (now - last_time_mgmt_ms_ >= interval_ms) {
        last_time_mgmt_ms_ = now;
        if (cfg_.display_time) {
            term_->refresh_status_bar();
        }
        // Could also set TNC time here if configured
    }
}
