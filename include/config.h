#pragma once
// ============================================================================
//  config.h  –  configuration management
//  Handles PACKCOM.CNF read/write and the full configuration menu system
//  (recovered from overlay .000 and .001 strings).
// ============================================================================
#include "packcom.h"
#include "terminal.h"
#include <string>

class Config {
public:
    explicit Config(PackComConfig& cfg);

    // Load from file; returns false if file not found (uses defaults)
    bool load(const std::string& path = CFG_FILE);

    // Save to file; returns false on error
    bool save(const std::string& path = CFG_FILE) const;

    // Show the main configuration menu (options 1-22 recovered from binary)
    // Returns true if comm params changed (ALT-P required)
    bool run_menu(Terminal& term);

    // Sub-menus
    void menu_color_config(Terminal& term);          // Option 12
    void menu_special_effects(Terminal& term);       // ALT-A
    void menu_echo_colors(Terminal& term);           //   sub-option 1
    void menu_nonheader_colors(Terminal& term);      //   sub-option 2
    void menu_keywords(Terminal& term);              //   sub-option 3
    void menu_comm_params(Terminal& term);           // ALT-P real-time change

    const PackComConfig& get() const { return cfg_; }
    PackComConfig&       get()       { return cfg_; }

private:
    PackComConfig& cfg_;

    // Helper: display a color picker  (Home/End = fg, Up/Down = bg)
    void color_picker(const std::string& title, Color& fg, Color& bg, Terminal& term);

    // Helper: numeric input field
    int  prompt_int(const std::string& prompt, int current, int lo, int hi, Terminal& term);

    // Helper: string input field
    std::string prompt_str(const std::string& prompt, const std::string& current, Terminal& term);

    // Helper: yes/no toggle
    bool prompt_yesno(const std::string& prompt, bool current, Terminal& term);

    // Helper: draw header for config menus
    void draw_menu_header(const std::string& title, Terminal& term) const;
};
