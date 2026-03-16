#pragma once
// ============================================================================
//  fileops.h  –  file operation subsystems
//  Covers: capture mode, ASCII transmit (XON/XOFF), file view, kill file,
//  directory listing, change drive/dir, DOS shell-out.
//  All prompts and messages recovered verbatim from overlay .000 / .001.
// ============================================================================
#include "packcom.h"
#include "platform.h"
#include "serial.h"
#include "terminal.h"
#include <string>
#include <fstream>

// ---------------------------------------------------------------------------
// CaptureFile  –  writes received data to a file (ALT-C toggle)
// ---------------------------------------------------------------------------
class CaptureFile {
public:
    explicit CaptureFile(PackComConfig& cfg);
    ~CaptureFile();

    // Toggle capture on/off.  Prompts for filename if turning on.
    // Returns the new state (true = capturing).
    bool toggle(Terminal& term);

    bool is_active() const { return active_; }
    const std::string& filename() const { return filename_; }

    // Write a chunk of received data to the capture file
    void write(const std::string& data);

private:
    PackComConfig& cfg_;
    bool       active_   = false;
    bool       timestamps_= true;
    std::string filename_;
    FILE*      file_     = nullptr;

    bool open_file(const std::string& path, bool overwrite);
    void close_file();
};

// ---------------------------------------------------------------------------
// AsciiTransmit  –  send a file char-by-char with XON/XOFF flow (ALT-T)
// ---------------------------------------------------------------------------
class AsciiTransmit {
public:
    AsciiTransmit(PackComConfig& cfg, Serial& serial, Terminal& term);

    // Prompt for filename and transmit; returns bytes sent or -1 on cancel.
    long run();

private:
    PackComConfig& cfg_;
    Serial&   serial_;
    Terminal& term_;
};

// ---------------------------------------------------------------------------
// FileViewer  –  view a text file in a split or full-screen window (ALT-V)
// ---------------------------------------------------------------------------
class FileViewer {
public:
    FileViewer(PackComConfig& cfg, Terminal& term);

    // Show file-view menu, open file, run interactive viewer.
    void run();

    // View a specific file (called directly by ALT-V toggle for split mode)
    void view_file(const std::string& path, bool split_screen = false);

private:
    PackComConfig& cfg_;
    Terminal& term_;

    void draw_view_status(const std::string& path, long viewed, long total,
                          int cur_line, int total_lines, bool split) const;
};

// ---------------------------------------------------------------------------
// DirManager  –  directory listing and drive/path navigation (ALT-D / ALT-N)
// ---------------------------------------------------------------------------
class DirManager {
public:
    DirManager(PackComConfig& cfg, Terminal& term);

    // Show directory of current directory with optional mask
    void show_directory();

    // Change directory / drive interactively
    void change_dir_interactive();

    // Kill (delete) a file interactively
    void kill_file();

private:
    PackComConfig& cfg_;
    Terminal& term_;
};

// ---------------------------------------------------------------------------
// DosShell  –  spawn external program or command processor (ALT-E)
// ---------------------------------------------------------------------------
class DosShell {
public:
    DosShell(PackComConfig& cfg, Terminal& term);

    // Run the shell-out subsystem
    void run();

private:
    PackComConfig& cfg_;
    Terminal& term_;

    bool check_memory() const;
};
