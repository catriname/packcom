// ============================================================================
//  config.cpp
// ============================================================================
#include "config.h"
#include "keys.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sstream>

Config::Config(PackComConfig& cfg) : cfg_(cfg) {}

// ===========================================================================
//  File I/O  –  simple binary/text config  (original was binary .CNF)
//  We use a human-readable key=value format for portability.
// ===========================================================================

bool Config::load(const std::string& path) {
    FILE* f = ::fopen(path.c_str(), "r");
    if (!f) return false;

    char line[256];
    while (::fgets(line, sizeof(line), f)) {
        // strip newline
        size_t len = ::strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) line[--len] = 0;

        char* eq = ::strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* key = line;
        const char* val = eq + 1;

        // Comm params
        if      (::strcmp(key,"baud")      == 0) cfg_.comm.baud_rate   = ::atoi(val);
        else if (::strcmp(key,"data_bits") == 0) cfg_.comm.data_bits   = ::atoi(val);
        else if (::strcmp(key,"stop_bits") == 0) cfg_.comm.stop_bits   = ::atoi(val);
        else if (::strcmp(key,"parity")    == 0) {
            if (val[0]=='E'||val[0]=='e') cfg_.comm.parity = Parity::Even;
            else if(val[0]=='O'||val[0]=='o') cfg_.comm.parity = Parity::Odd;
            else cfg_.comm.parity = Parity::None;
        }
        else if (::strcmp(key,"com_port")  == 0) cfg_.comm.com_port    = ::atoi(val);

        // General
        else if (::strcmp(key,"scrollback")       == 0) cfg_.scrollback_lines   = ::atoi(val);
        else if (::strcmp(key,"dos_mem_kb")        == 0) cfg_.dos_mem_kb         = ::atoi(val);
        else if (::strcmp(key,"incoming_bell")     == 0) cfg_.incoming_bell      = ::atoi(val)!=0;
        else if (::strcmp(key,"elim_blank")        == 0) cfg_.eliminate_blank_lines= ::atoi(val)!=0;
        else if (::strcmp(key,"time_mgmt_min")     == 0) cfg_.time_mgmt_minutes  = ::atoi(val);
        else if (::strcmp(key,"display_time")      == 0) cfg_.display_time       = ::atoi(val)!=0;
        else if (::strcmp(key,"byte_delay_ms")     == 0) cfg_.byte_delay_ms      = ::atoi(val);
        else if (::strcmp(key,"crlf_delay_ms")     == 0) cfg_.crlf_delay_ms      = ::atoi(val);
        else if (::strcmp(key,"dos_prog")          == 0) cfg_.default_dos_prog   = val;
        else if (::strcmp(key,"set_tnc_datetime")  == 0) cfg_.set_tnc_datetime   = ::atoi(val)!=0;
        else if (::strcmp(key,"overlay_path")      == 0) cfg_.overlay_path       = val;
        else if (::strcmp(key,"stream_switch")     == 0) cfg_.stream_switch_char = val[0];
        else if (::strcmp(key,"carrier_detect_cd") == 0) cfg_.carrier_detect_cd  = ::atoi(val)!=0;
        else if (::strcmp(key,"break_cmd_mode")    == 0) cfg_.break_cmd_mode     = ::atoi(val)!=0;

        // Colors
        else if (::strcmp(key,"clr_fg")  == 0) cfg_.colors.foreground = (Color)::atoi(val);
        else if (::strcmp(key,"clr_bg")  == 0) cfg_.colors.background = (Color)::atoi(val);
        else if (::strcmp(key,"clr_rfg") == 0) cfg_.colors.rev_fg     = (Color)::atoi(val);
        else if (::strcmp(key,"clr_rbg") == 0) cfg_.colors.rev_bg     = (Color)::atoi(val);
        else if (::strcmp(key,"clr_hfg") == 0) cfg_.colors.hi_fg      = (Color)::atoi(val);
        else if (::strcmp(key,"clr_bdr") == 0) cfg_.colors.border     = (Color)::atoi(val);

        // SFX
        else if (::strcmp(key,"kw_scan")     == 0) cfg_.sfx.keyword_scan_on    = ::atoi(val)!=0;
        else if (::strcmp(key,"sfx_on")      == 0) cfg_.sfx.special_effects_on = ::atoi(val)!=0;
        else if (::strcmp(key,"echo_fg")     == 0) cfg_.sfx.echo_colors.fg     = (Color)::atoi(val);
        else if (::strcmp(key,"echo_bg")     == 0) cfg_.sfx.echo_colors.bg     = (Color)::atoi(val);
        else if (::strcmp(key,"nhdr_fg")     == 0) cfg_.sfx.nonheader_colors.fg= (Color)::atoi(val);
        else if (::strcmp(key,"nhdr_bg")     == 0) cfg_.sfx.nonheader_colors.bg= (Color)::atoi(val);
        else {
            // Keywords: kw0_word, kw0_fg, kw0_bg, kw0_efx  etc.
            for (int ki = 0; ki < MAX_KEYWORDS; ++ki) {
                char kprefix[16]; ::snprintf(kprefix, sizeof(kprefix), "kw%d_", ki);
                if (::strncmp(key, kprefix, ::strlen(kprefix)) == 0) {
                    const char* sub = key + ::strlen(kprefix);
                    if (::strcmp(sub,"word") == 0) cfg_.sfx.keywords[ki].word = val;
                    if (::strcmp(sub,"fg")   == 0) cfg_.sfx.keywords[ki].fg   = (Color)::atoi(val);
                    if (::strcmp(sub,"bg")   == 0) cfg_.sfx.keywords[ki].bg   = (Color)::atoi(val);
                    if (::strcmp(sub,"efx")  == 0) cfg_.sfx.keywords[ki].effects_on = ::atoi(val)!=0;
                }
            }
        }
    }
    ::fclose(f);
    return true;
}

bool Config::save(const std::string& path) const {
    FILE* f = ::fopen(path.c_str(), "w");
    if (!f) return false;

    ::fprintf(f, "baud=%d\n",          cfg_.comm.baud_rate);
    ::fprintf(f, "data_bits=%d\n",     cfg_.comm.data_bits);
    ::fprintf(f, "stop_bits=%d\n",     cfg_.comm.stop_bits);
    char pc = 'N';
    if (cfg_.comm.parity == Parity::Even) pc = 'E';
    if (cfg_.comm.parity == Parity::Odd)  pc = 'O';
    ::fprintf(f, "parity=%c\n",        pc);
    ::fprintf(f, "com_port=%d\n",      cfg_.comm.com_port);
    ::fprintf(f, "scrollback=%d\n",    cfg_.scrollback_lines);
    ::fprintf(f, "dos_mem_kb=%d\n",    cfg_.dos_mem_kb);
    ::fprintf(f, "incoming_bell=%d\n", cfg_.incoming_bell?1:0);
    ::fprintf(f, "elim_blank=%d\n",    cfg_.eliminate_blank_lines?1:0);
    ::fprintf(f, "time_mgmt_min=%d\n", cfg_.time_mgmt_minutes);
    ::fprintf(f, "display_time=%d\n",  cfg_.display_time?1:0);
    ::fprintf(f, "byte_delay_ms=%d\n", cfg_.byte_delay_ms);
    ::fprintf(f, "crlf_delay_ms=%d\n", cfg_.crlf_delay_ms);
    ::fprintf(f, "dos_prog=%s\n",      cfg_.default_dos_prog.c_str());
    ::fprintf(f, "set_tnc_datetime=%d\n",  cfg_.set_tnc_datetime?1:0);
    ::fprintf(f, "overlay_path=%s\n",  cfg_.overlay_path.c_str());
    ::fprintf(f, "stream_switch=%c\n", cfg_.stream_switch_char);
    ::fprintf(f, "carrier_detect_cd=%d\n", cfg_.carrier_detect_cd?1:0);
    ::fprintf(f, "break_cmd_mode=%d\n",    cfg_.break_cmd_mode?1:0);

    ::fprintf(f, "clr_fg=%d\n",  cfg_.colors.foreground);
    ::fprintf(f, "clr_bg=%d\n",  cfg_.colors.background);
    ::fprintf(f, "clr_rfg=%d\n", cfg_.colors.rev_fg);
    ::fprintf(f, "clr_rbg=%d\n", cfg_.colors.rev_bg);
    ::fprintf(f, "clr_hfg=%d\n", cfg_.colors.hi_fg);
    ::fprintf(f, "clr_bdr=%d\n", cfg_.colors.border);

    ::fprintf(f, "kw_scan=%d\n", cfg_.sfx.keyword_scan_on?1:0);
    ::fprintf(f, "sfx_on=%d\n",  cfg_.sfx.special_effects_on?1:0);
    ::fprintf(f, "echo_fg=%d\n", cfg_.sfx.echo_colors.fg);
    ::fprintf(f, "echo_bg=%d\n", cfg_.sfx.echo_colors.bg);
    ::fprintf(f, "nhdr_fg=%d\n", cfg_.sfx.nonheader_colors.fg);
    ::fprintf(f, "nhdr_bg=%d\n", cfg_.sfx.nonheader_colors.bg);

    for (int ki = 0; ki < MAX_KEYWORDS; ++ki) {
        ::fprintf(f, "kw%d_word=%s\n", ki, cfg_.sfx.keywords[ki].word.c_str());
        ::fprintf(f, "kw%d_fg=%d\n",   ki, cfg_.sfx.keywords[ki].fg);
        ::fprintf(f, "kw%d_bg=%d\n",   ki, cfg_.sfx.keywords[ki].bg);
        ::fprintf(f, "kw%d_efx=%d\n",  ki, cfg_.sfx.keywords[ki].effects_on?1:0);
    }
    ::fclose(f);
    return true;
}

// ===========================================================================
//  Helpers
// ===========================================================================

void Config::draw_menu_header(const std::string& title, Terminal& /*term*/) const {
    Platform::clear_screen();
    Platform::set_color(cfg_.colors.rev_fg, cfg_.colors.rev_bg);
    std::string hdr = "  " + title;
    hdr.resize(80, ' ');
    Platform::goto_xy(0, 0); Platform::puts_raw(hdr);
    Platform::reset_color();
    Platform::goto_xy(1, 0);
    Platform::puts_raw("? Options with \"X\" do NOT take effect until PackCom is restarted");
    Platform::goto_xy(2, 0);
    Platform::puts_raw("A Options with \"P\" do NOT take effect until after an ALT-P Command");
    Platform::goto_xy(3, 0);
    Platform::puts_raw("* All other options take effect immediately");
    Platform::goto_xy(4, 0);
    Platform::puts_raw(std::string(80, '-'));
}

int Config::prompt_int(const std::string& prompt, int current, int lo, int hi, Terminal& /*term*/) {
    Platform::puts_raw(prompt + " [" + std::to_string(current) + "]: ");
    std::string buf = std::to_string(current);
    // Simple line edit
    int k;
    while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
        if (k == KEY_BACKSPACE || k == 127) {
            if (!buf.empty()) { buf.pop_back(); Platform::putch('\b'); Platform::putch(' '); Platform::putch('\b'); }
        } else if (k >= '0' && k <= '9') {
            buf += (char)k; Platform::putch((char)k);
        }
    }
    int v = buf.empty() ? current : ::atoi(buf.c_str());
    return std::max(lo, std::min(hi, v));
}

std::string Config::prompt_str(const std::string& prompt, const std::string& current, Terminal& /*term*/) {
    Platform::puts_raw(prompt + " [" + current + "]: ");
    std::string buf = current;
    int k;
    while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
        if (k == KEY_BACKSPACE || k == 127) {
            if (!buf.empty()) { buf.pop_back(); Platform::putch('\b'); Platform::putch(' '); Platform::putch('\b'); }
        } else if (k >= 32 && k < 127) {
            buf += (char)k; Platform::putch((char)k);
        }
    }
    return buf;
}

bool Config::prompt_yesno(const std::string& prompt, bool current, Terminal& /*term*/) {
    Platform::puts_raw(prompt + " (" + (current ? "Y/n" : "y/N") + "): ");
    int k = Platform::getch_raw();
    if (k == 'y' || k == 'Y') return true;
    if (k == 'n' || k == 'N') return false;
    return current;
}

// ===========================================================================
//  Main configuration menu
//  Options 1-22 recovered verbatim from overlay .000 strings
// ===========================================================================

bool Config::run_menu(Terminal& term) {
    bool comm_changed = false;
    bool done = false;

    while (!done) {
        draw_menu_header("PackCom Configuration", term);

        // Page 1: options 1-12
        const char* page1[] = {
            "P 1. Number of Stopbits",
            "P 2. Number of Databits",
            "P 3. Parity Type",
            "P 4. Baud Rate",
            "P 5. Communication Port",
            "  6. ...........DUMMY PARM............",
            "X 7. Scroll Back Buffer Size",
            "  8. Incomming BELL (cntl-G)",
            "  9. Eliminate Blank Lines",
            " 10. Time Management Every ? Minutes",
            " 11. Display Time",
            " 12. Change Color (takes effect as screen is rewritten)",
            nullptr
        };
        int row = 5;
        for (int i = 0; page1[i]; ++i) {
            Platform::goto_xy(row++, 2);
            Platform::puts_raw(page1[i]);
        }

        // Show current values
        Platform::goto_xy( 5, 50); Platform::puts_raw(std::to_string(cfg_.comm.stop_bits));
        Platform::goto_xy( 6, 50); Platform::puts_raw(std::to_string(cfg_.comm.data_bits));
        Platform::goto_xy( 7, 50); Platform::puts_raw(cfg_.comm.parity==Parity::Even?"Even":
                                                       cfg_.comm.parity==Parity::Odd?"Odd":"None");
        Platform::goto_xy( 8, 50); Platform::puts_raw(std::to_string(cfg_.comm.baud_rate));
        Platform::goto_xy( 9, 50); Platform::puts_raw("COM" + std::to_string(cfg_.comm.com_port));
        Platform::goto_xy(11, 50); Platform::puts_raw(std::to_string(cfg_.scrollback_lines));
        Platform::goto_xy(12, 50); Platform::puts_raw(cfg_.incoming_bell?"ON":"OFF");
        Platform::goto_xy(13, 50); Platform::puts_raw(cfg_.eliminate_blank_lines?"ON":"OFF");
        Platform::goto_xy(14, 50); Platform::puts_raw(std::to_string(cfg_.time_mgmt_minutes));
        Platform::goto_xy(15, 50); Platform::puts_raw(cfg_.display_time?"ON":"OFF");

        Platform::goto_xy(row+1, 2);
        Platform::puts_raw("Enter the number to change or RETURN for more: ");

        std::string sel_str;
        int k;
        while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
            if (k >= '0' && k <= '9') { sel_str += (char)k; Platform::putch((char)k); }
            else if (k == KEY_ESC) { done = true; break; }
        }
        if (done) break;

        int sel = sel_str.empty() ? 0 : ::atoi(sel_str.c_str());
        if (sel == 0) {
            // Page 2
            draw_menu_header("PackCom Configuration (continued)", term);
            const char* page2[] = {
                " 14. 1/1000 Sec. after sending a Byte",
                " 15. 1/1000 Sec. after sending a CRLF",
                "X16. Memory for DOS Programs (KBytes)",
                " 17. Default DOS Program to Execute",
                "X18. Set TNC's Date and Time",
                "X19. Drive/Path to PackCom's Overlays",
                " 20. Switch Stream Char used",
                " 21. Carrier Detect TRUE if Connected?",
                " 22. BREAK puts TNC into CMD mode?",
                nullptr
            };
            row = 5;
            for (int i = 0; page2[i]; ++i) {
                Platform::goto_xy(row++, 2); Platform::puts_raw(page2[i]);
            }
            // Current values
            Platform::goto_xy( 5, 50); Platform::puts_raw(std::to_string(cfg_.byte_delay_ms));
            Platform::goto_xy( 6, 50); Platform::puts_raw(std::to_string(cfg_.crlf_delay_ms));
            Platform::goto_xy( 7, 50); Platform::puts_raw(std::to_string(cfg_.dos_mem_kb));
            Platform::goto_xy( 8, 50); Platform::puts_raw(cfg_.default_dos_prog);
            Platform::goto_xy( 9, 50); Platform::puts_raw(cfg_.set_tnc_datetime?"YES":"NO");
            Platform::goto_xy(10, 50); Platform::puts_raw(cfg_.overlay_path);
            Platform::goto_xy(11, 50); Platform::puts_raw(std::string(1, cfg_.stream_switch_char));
            Platform::goto_xy(12, 50); Platform::puts_raw(cfg_.carrier_detect_cd?"YES":"NO");
            Platform::goto_xy(13, 50); Platform::puts_raw(cfg_.break_cmd_mode?"YES":"NO");

            Platform::goto_xy(row+1, 2);
            Platform::puts_raw("Enter the number to change or RETURN to exit: ");
            sel_str.clear();
            while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
                if (k >= '0' && k <= '9') { sel_str += (char)k; Platform::putch((char)k); }
                else if (k == KEY_ESC) { done = true; break; }
            }
            if (done) break;
            sel = sel_str.empty() ? -1 : ::atoi(sel_str.c_str());
        }

        // Process selection
        Platform::goto_xy(22, 2);
        switch (sel) {
            case 1:
                cfg_.comm.stop_bits = prompt_int("Stop bits (1 or 2)", cfg_.comm.stop_bits, 1, 2, term);
                comm_changed = true; break;
            case 2:
                cfg_.comm.data_bits = prompt_int("Data bits (7 or 8)", cfg_.comm.data_bits, 7, 8, term);
                comm_changed = true; break;
            case 3: {
                Platform::puts_raw("Parity (N)one, (E)ven, (O)dd: ");
                k = Platform::getch_raw();
                if (k=='E'||k=='e') cfg_.comm.parity = Parity::Even;
                else if(k=='O'||k=='o') cfg_.comm.parity = Parity::Odd;
                else cfg_.comm.parity = Parity::None;
                comm_changed = true; break;
            }
            case 4: {
                Platform::puts_raw("Baud Rate 300,1200,2400,4800,9600: ");
                int b = prompt_int("", cfg_.comm.baud_rate, 300, 9600, term);
                // Snap to valid values
                int valid[] = {300,1200,2400,4800,9600};
                int best = 1200;
                for (int v : valid) if (::abs(v-b) < ::abs(best-b)) best = v;
                cfg_.comm.baud_rate = best;
                comm_changed = true; break;
            }
            case 5:
                cfg_.comm.com_port = prompt_int("COM port (1-4)", cfg_.comm.com_port, 1, 4, term);
                comm_changed = true; break;
            case 7:
                cfg_.scrollback_lines = prompt_int("Scrollback lines", cfg_.scrollback_lines, 50, 9999, term);
                break;
            case 8:
                cfg_.incoming_bell = prompt_yesno("Incoming BELL", cfg_.incoming_bell, term); break;
            case 9:
                cfg_.eliminate_blank_lines = prompt_yesno("Eliminate blank lines", cfg_.eliminate_blank_lines, term); break;
            case 10:
                cfg_.time_mgmt_minutes = prompt_int("Time mgmt minutes (0=off)", cfg_.time_mgmt_minutes, 0, 99, term); break;
            case 11:
                cfg_.display_time = prompt_yesno("Display time", cfg_.display_time, term); break;
            case 12:
                menu_color_config(term); break;
            case 14:
                cfg_.byte_delay_ms = prompt_int("Byte delay ms", cfg_.byte_delay_ms, 0, 9999, term); break;
            case 15:
                cfg_.crlf_delay_ms = prompt_int("CRLF delay ms", cfg_.crlf_delay_ms, 0, 9999, term); break;
            case 16:
                cfg_.dos_mem_kb = prompt_int("DOS memory KB", cfg_.dos_mem_kb, 0, 640, term); break;
            case 17:
                cfg_.default_dos_prog = prompt_str("Default DOS program", cfg_.default_dos_prog, term); break;
            case 18:
                cfg_.set_tnc_datetime = prompt_yesno("Set TNC date/time", cfg_.set_tnc_datetime, term); break;
            case 19:
                cfg_.overlay_path = prompt_str("Overlay drive/path", cfg_.overlay_path, term); break;
            case 20: {
                Platform::puts_raw("Stream switch char: ");
                k = Platform::getch_raw();
                if (k > 0 && k < 128) cfg_.stream_switch_char = (char)k;
                break;
            }
            case 21:
                cfg_.carrier_detect_cd = prompt_yesno("CD = connected", cfg_.carrier_detect_cd, term); break;
            case 22:
                cfg_.break_cmd_mode = prompt_yesno("BREAK = CMD mode", cfg_.break_cmd_mode, term); break;
            default:
                done = true; break;
        }
    }

    save();
    return comm_changed;
}

// ===========================================================================
//  Color configuration (option 12)  –  recovered verbatim from .000 overlay
// ===========================================================================

void Config::menu_color_config(Terminal& term) {
    bool done = false;
    while (!done) {
        Platform::clear_screen();
        Platform::set_color(cfg_.colors.rev_fg, cfg_.colors.rev_bg);
        Platform::goto_xy(1, 28); Platform::puts_raw("COLOR CONFIGURATION");
        Platform::reset_color();
        Platform::goto_xy(3, 4);
        Platform::puts_raw("Press <Number Key> to Change Corresponding Color,");
        Platform::goto_xy(4, 4); Platform::puts_raw("Press <ENTER> to Exit.");

        auto show_color = [&](int row, int num, const std::string& label, Color fg, Color bg) {
            Platform::goto_xy(row, 4);
            Platform::puts_raw("  " + std::to_string(num) + ")  " + label + "  ");
            Platform::set_color(fg, bg);
            Platform::puts_raw(COLOR_NAMES[fg]);
            Platform::reset_color();
            Platform::puts_raw(" on ");
            Platform::set_color(fg, bg);
            Platform::puts_raw(COLOR_NAMES[bg]);
            Platform::reset_color();
        };

        show_color(6,  1, "Foreground Color is",        cfg_.colors.foreground, cfg_.colors.background);
        show_color(7,  2, "Background Color is",        cfg_.colors.foreground, cfg_.colors.background);
        show_color(8,  3, "Reverse Image Foreground is",cfg_.colors.rev_fg,     cfg_.colors.rev_bg);
        show_color(9,  4, "Reverse Image Background is",cfg_.colors.rev_fg,     cfg_.colors.rev_bg);
        show_color(10, 5, "High Intensity Foreground is",cfg_.colors.hi_fg,     cfg_.colors.background);
        show_color(11, 6, "Border Color is",            cfg_.colors.border,     cfg_.colors.background);

        int k = Platform::getch_raw();
        switch (k) {
            case '1': color_picker("Foreground", cfg_.colors.foreground, cfg_.colors.background, term); break;
            case '2': color_picker("Background", cfg_.colors.background, cfg_.colors.foreground, term); break;
            case '3': color_picker("Rev FG",     cfg_.colors.rev_fg,     cfg_.colors.rev_bg,     term); break;
            case '4': color_picker("Rev BG",     cfg_.colors.rev_bg,     cfg_.colors.rev_fg,     term); break;
            case '5': color_picker("Hi FG",      cfg_.colors.hi_fg,      cfg_.colors.background, term); break;
            case '6': color_picker("Border",     cfg_.colors.border,     cfg_.colors.background, term); break;
            case KEY_ENTER: done = true; break;
            default: break;
        }
    }
    save();
}

void Config::color_picker(const std::string& title, Color& fg, Color& bg, Terminal& /*term*/) {
    Platform::clear_screen();
    Platform::goto_xy(3, 10);
    Platform::set_color(CLR_WHITE, CLR_BLACK);
    Platform::puts_raw("Set-Up Color Display - " + title);
    Platform::reset_color();
    Platform::goto_xy(5, 4); Platform::puts_raw("Press <Home> or <End> to Modify Foreground Color.");
    Platform::goto_xy(6, 4); Platform::puts_raw("Press <Up Arrow> or <Down Arrow> to Modify Background Color.");
    Platform::goto_xy(7, 4); Platform::puts_raw("Press <Enter> to Exit.");

    bool done = false;
    while (!done) {
        // Show preview
        Platform::goto_xy(10, 20);
        Platform::set_color(fg, bg);
        Platform::puts_raw("  Sample Text  ");
        Platform::reset_color();
        Platform::goto_xy(12, 10);
        Platform::puts_raw("  FG: " + std::string(COLOR_NAMES[fg]) + "  BG: " + std::string(COLOR_NAMES[bg]) + "  ");

        int k = Platform::getch_raw();
        switch (k) {
            case KEY_HOME: fg = (Color)((fg + 1) & 15); break;
            case KEY_END:  fg = (Color)((fg + 15) & 15); break;
            case KEY_UP:   bg = (Color)((bg + 15) & 15); break;
            case KEY_DOWN: bg = (Color)((bg + 1) & 15); break;
            case KEY_ENTER: done = true; break;
            default: break;
        }
    }
}

// ===========================================================================
//  Special effects menu  (ALT-A)
// ===========================================================================

void Config::menu_special_effects(Terminal& term) {
    bool done = false;
    while (!done) {
        Platform::clear_screen();
        Platform::set_color(cfg_.colors.rev_fg, cfg_.colors.rev_bg);
        Platform::goto_xy(0, 20); Platform::puts_raw("Special Effects Configuration.");
        Platform::reset_color();

        Platform::goto_xy(3, 4);
        Platform::puts_raw("  1.  Configure Keyboard/Digi Echo Colors.");
        Platform::goto_xy(4, 4);
        Platform::puts_raw("  2.  Configure Non-Header Colors.");
        Platform::goto_xy(5, 4);
        Platform::puts_raw("  3.  Configure Key Word's to Search.");
        Platform::goto_xy(6, 4);
        Platform::puts_raw("  4.  Toggle Key Word Special Effect's (beeps) ON/OFF.  " +
                           std::string(cfg_.sfx.special_effects_on ? "(Currently ON)" : "(Currently OFF)"));
        Platform::goto_xy(7, 4);
        Platform::puts_raw("  5.  Toggle Key Word Special Effect's & Coloring ON/OFF.  " +
                           std::string(cfg_.sfx.keyword_scan_on ? "(Currently ON)" : "(Currently OFF)"));
        Platform::goto_xy(9, 4);
        Platform::puts_raw("Enter an option to change or press <ENTER> to exit: ");

        int k = Platform::getch_raw();
        switch (k) {
            case '1': menu_echo_colors(term);    break;
            case '2': menu_nonheader_colors(term); break;
            case '3': menu_keywords(term);        break;
            case '4': cfg_.sfx.special_effects_on = !cfg_.sfx.special_effects_on; break;
            case '5': cfg_.sfx.keyword_scan_on    = !cfg_.sfx.keyword_scan_on;    break;
            case KEY_ENTER: done = true; break;
            default: break;
        }
    }
    save();
}

void Config::menu_echo_colors(Terminal& term) {
    Platform::clear_screen();
    Platform::goto_xy(2, 4);
    Platform::puts_raw("Configure Keyboard/Digi Echo Colors.");
    Platform::goto_xy(4, 4);
    Platform::puts_raw("  This window is used to update the Colors generated on the monitor");
    Platform::goto_xy(5, 4);
    Platform::puts_raw("  for packets received by PackCom, that match previously sent packets.");
    Platform::goto_xy(6, 4);
    Platform::puts_raw("  Usually these packets come from the TNC echoing the keyboard's");
    Platform::goto_xy(7, 4);
    Platform::puts_raw("  input back to you, or by a Digipeater echoing your outgoing packets");
    color_picker("Keyboard/Digi Echo Colors", cfg_.sfx.echo_colors.fg, cfg_.sfx.echo_colors.bg, term);
}

void Config::menu_nonheader_colors(Terminal& term) {
    Platform::clear_screen();
    Platform::goto_xy(2, 4);
    Platform::puts_raw("Configure Non-Header Colors.");
    Platform::goto_xy(4, 4);
    Platform::puts_raw("  This window is used to update the Colors for non-header packets.");
    color_picker("Non-Header Colors", cfg_.sfx.nonheader_colors.fg, cfg_.sfx.nonheader_colors.bg, term);
}

void Config::menu_keywords(Terminal& term) {
    bool done = false;
    while (!done) {
        Platform::clear_screen();
        Platform::goto_xy(1, 4);
        Platform::puts_raw("Packet Key Word's to Search.");
        Platform::goto_xy(3, 4);
        Platform::puts_raw("  Enter: Keyword Option to Change or Press <Enter> to Exit.");

        for (int i = 0; i < MAX_KEYWORDS; ++i) {
            Platform::goto_xy(5 + i, 4);
            char buf[80];
            ::snprintf(buf, sizeof(buf), "%2d. [%-20s] FG:%-14s BG:%-14s EFX:%s",
                i+1,
                cfg_.sfx.keywords[i].word.c_str(),
                COLOR_NAMES[cfg_.sfx.keywords[i].fg],
                COLOR_NAMES[cfg_.sfx.keywords[i].bg],
                cfg_.sfx.keywords[i].effects_on ? "ON" : "OFF");
            Platform::set_color(cfg_.sfx.keywords[i].fg, cfg_.sfx.keywords[i].bg);
            Platform::puts_raw(buf);
            Platform::reset_color();
        }

        Platform::goto_xy(17, 4);
        Platform::puts_raw("Which One? ");
        std::string sel_str;
        int k;
        while ((k = Platform::getch_raw()) != KEY_ENTER && k != '\r') {
            if (k >= '0' && k <= '9') { sel_str += (char)k; Platform::putch((char)k); }
            else if (k == KEY_ESC) { done = true; break; }
        }
        if (done) break;
        int sel = sel_str.empty() ? 0 : ::atoi(sel_str.c_str()) - 1;
        if (sel < 0 || sel >= MAX_KEYWORDS) { done = true; break; }

        auto& kw = cfg_.sfx.keywords[sel];
        Platform::goto_xy(19, 4);
        Platform::puts_raw("Modify the Keyword and Press <Enter>: ");
        kw.word = prompt_str("", kw.word, term);
        color_picker("Keyword Colors", kw.fg, kw.bg, term);
        Platform::goto_xy(21, 4);
        kw.effects_on = prompt_yesno("Enable effects (beep)", kw.effects_on, term);
    }
    save();
}

// ===========================================================================
//  Real-time comm params change  (ALT-P)
//  "Change Parameters NOW" menu recovered from .001 overlay
// ===========================================================================

void Config::menu_comm_params(Terminal& term) {
    Platform::clear_screen();
    Platform::goto_xy(3, 10);
    Platform::set_color(CLR_WHITE, CLR_BLACK);
    Platform::puts_raw("Change Parameters NOW");
    Platform::reset_color();
    Platform::goto_xy(5, 10);
    Platform::puts_raw("    Current Setting: ");
    Platform::goto_xy(6, 10);
    Platform::puts_raw("       Enter New Parameters.");
    Platform::goto_xy(7, 10);
    Platform::puts_raw("       ---------------------");

    Platform::goto_xy(9, 10);
    Platform::puts_raw("    Baud Rate, 300,1200,2400,4800,9600 : ");
    int b = prompt_int("", cfg_.comm.baud_rate, 300, 9600, term);
    int valid[] = {300,1200,2400,4800,9600,0};
    int best = cfg_.comm.baud_rate;
    for (int i = 0; valid[i]; ++i)
        if (::abs(valid[i]-b) < ::abs(best-b)) best = valid[i];
    cfg_.comm.baud_rate = best;

    Platform::goto_xy(11, 10);
    Platform::puts_raw("    Parity, (N)one, (E)ven, (O)dd      : ");
    int k = Platform::getch_raw();
    if (k=='E'||k=='e') cfg_.comm.parity = Parity::Even;
    else if(k=='O'||k=='o') cfg_.comm.parity = Parity::Odd;
    else cfg_.comm.parity = Parity::None;

    Platform::goto_xy(13, 10);
    Platform::puts_raw("    Data Bits, 7 or 8                  : ");
    cfg_.comm.data_bits = prompt_int("", cfg_.comm.data_bits, 7, 8, term);

    Platform::goto_xy(15, 10);
    Platform::puts_raw("    Stop Bits, 1 or 2                  : ");
    cfg_.comm.stop_bits = prompt_int("", cfg_.comm.stop_bits, 1, 2, term);

    save();
}
