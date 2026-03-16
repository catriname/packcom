#pragma once
// ============================================================================
//  app.h  –  top-level application class
//  Owns all subsystems and runs the main event loop.
// ============================================================================
#include "packcom.h"
#include "platform.h"
#include "serial.h"
#include "terminal.h"
#include "input.h"
#include "config.h"
#include "yapp.h"
#include "fileops.h"
#include <memory>
#include <string>

class PackComApp {
public:
    PackComApp();
    ~PackComApp();

    // Entry point – returns 0 on clean exit
    int run(int argc, char* argv[]);

private:
    // --- Core state ---
    PackComConfig cfg_;
    bool          running_      = true;
    bool          printer_on_   = false;    // Ctrl-PrtSc toggle
    DuplexMode    duplex_       = DuplexMode::Full;

    // --- Subsystems ---
    std::unique_ptr<Serial>       serial_;
    std::unique_ptr<Terminal>     term_;
    std::unique_ptr<InputManager> input_;
    std::unique_ptr<Config>       config_;
    std::unique_ptr<CaptureFile>  capture_;
    std::unique_ptr<DirManager>   dir_mgr_;
    std::unique_ptr<DosShell>     dos_shell_;

    // --- Initialisation ---
    void init_subsystems();
    bool open_serial();
    void display_version();
    void check_overlay_files();
    bool parse_args(int argc, char* argv[]);

    // --- Main loop ---
    void main_loop();

    // --- Serial RX processing ---
    void process_rx();

    // --- ALT-key command dispatcher ---
    void handle_alt_key(int key);

    // --- ALT-H  Help ---
    void do_help();

    // --- ALT-C  Capture toggle ---
    void do_capture();

    // --- ALT-I  Config/Init ---
    void do_config();

    // --- ALT-P  Comm parms ---
    void do_comm_parms();

    // --- ALT-X  Exit ---
    void do_exit();

    // --- ALT-V  View file / split-screen toggle ---
    void do_view();

    // --- ALT-T  Transmit ASCII ---
    void do_transmit_ascii();

    // --- ALT-Y  YAPP transfer ---
    void do_yapp();

    // --- ALT-D  Directory ---
    void do_directory();

    // --- ALT-N  New dir/drive ---
    void do_change_dir();

    // --- ALT-E  Execute DOS command ---
    void do_dos_command();

    // --- ALT-S  Scroll-back processing ---
    void do_scrollback_menu();

    // --- ALT-K  Kill file ---
    void do_kill_file();

    // --- ALT-M  Macro key defs ---
    void do_macros();

    // --- ALT-Q  Toggle keyboard mode ---
    void do_toggle_input_mode();

    // --- ALT-F  Toggle Full/Half duplex ---
    void do_toggle_duplex();

    // --- ALT-B  Send BREAK ---
    void do_send_break();

    // --- ALT-A  Alarms / special effects ---
    void do_alarms();

    // --- ALT-O  Other commands ---
    void do_other_commands();

    // --- Home  Toggle auto-update ---
    void do_toggle_auto_update();

    // --- Comm-port debug display ---
    void do_comm_port_info();

    // --- Time management ---
    void check_time_management();
    uint64_t last_time_mgmt_ms_ = 0;

    // --- Send a line to serial port ---
    void send_line(const std::string& line);

    // --- Scrollback interactive mode ---
    void run_scrollback_interactive();
};
