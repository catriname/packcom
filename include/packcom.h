#pragma once
// ============================================================================
//  PackCom - Packet Radio Terminal Program
//  Original: PackCom v0.75 by Jim Wright (WB4ZJV), 01/28/1987
//  Reconstructed in portable C++ from compiled Turbo Pascal binaries
//  Target: C++17, POSIX / Windows cross-platform
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <functional>

// ---------------------------------------------------------------------------
// Version info (recovered from binary strings)
// ---------------------------------------------------------------------------
constexpr const char* PACKCOM_VERSION   = "0.75 Dev.";
constexpr const char* PACKCOM_DATE      = "01/28/1987";
constexpr const char* PACKCOM_AUTHOR    = "Jim Wright (WB4ZJV)";

// ---------------------------------------------------------------------------
// File names
// ---------------------------------------------------------------------------
constexpr const char* CFG_FILE          = "PACKCOM.CNF";
constexpr const char* KEY_FILE          = "PACKCOM.KEY";
constexpr const char* WORD_FILE         = "PACKCOM.WRD";
constexpr const char* OVERLAY_0         = "packcom.000";
constexpr const char* OVERLAY_1         = "packcom.001";

// ---------------------------------------------------------------------------
// Serial / comm constants
// ---------------------------------------------------------------------------
constexpr int  MAX_COM_PORTS    = 4;
constexpr int  SIN_BUF_SIZE     = 4096;   // serial input  ring buffer
constexpr int  SOUT_BUF_SIZE    = 4096;   // serial output ring buffer
constexpr int  SCROLLBACK_DEFAULT = 200;  // lines

// ---------------------------------------------------------------------------
// Colour indices  (matching original Borland palette 0-15)
// ---------------------------------------------------------------------------
enum Color : uint8_t {
    CLR_BLACK = 0, CLR_BLUE, CLR_GREEN, CLR_CYAN,
    CLR_RED, CLR_MAGENTA, CLR_BROWN, CLR_LIGHTGRAY,
    CLR_DARKGRAY, CLR_LIGHTBLUE, CLR_LIGHTGREEN, CLR_LIGHTCYAN,
    CLR_LIGHTRED, CLR_LIGHTMAGENTA, CLR_YELLOW, CLR_WHITE
};

constexpr const char* COLOR_NAMES[16] = {
    "Black       ", "Blue        ", "Green       ", "Cyan        ",
    "Red         ", "Magenta     ", "Brown       ", "LightGray   ",
    "DarkGray    ", "LightBlue   ", "lightGreen  ", "LightCyan   ",
    "LightRed    ", "LightMagenta", "Yellow      ", "White       "
};

// ---------------------------------------------------------------------------
// Colour scheme (6 slots recovered from config menu)
// ---------------------------------------------------------------------------
struct ColorConfig {
    Color foreground     = CLR_LIGHTGRAY;
    Color background     = CLR_BLACK;
    Color rev_fg         = CLR_BLACK;
    Color rev_bg         = CLR_LIGHTGRAY;
    Color hi_fg          = CLR_WHITE;
    Color border         = CLR_BLUE;
};

// ---------------------------------------------------------------------------
// Keyboard/Digi echo colour pair  (for echo-matched packet highlighting)
// ---------------------------------------------------------------------------
struct EchoColors {
    Color fg = CLR_YELLOW;
    Color bg = CLR_BLACK;
};

// ---------------------------------------------------------------------------
// Non-header packet colour pair
// ---------------------------------------------------------------------------
struct NonHeaderColors {
    Color fg = CLR_LIGHTCYAN;
    Color bg = CLR_BLACK;
};

// ---------------------------------------------------------------------------
// Keyword entry for packet scanning / special effects
// ---------------------------------------------------------------------------
constexpr int MAX_KEYWORDS = 10;
struct KeywordEntry {
    std::string word;
    Color fg          = CLR_LIGHTRED;
    Color bg          = CLR_BLACK;
    bool  effects_on  = true;   // audio bell / visual effect
};

// ---------------------------------------------------------------------------
// Special-effects / keyword-scan configuration
// ---------------------------------------------------------------------------
struct SpecialEffectsConfig {
    bool keyword_scan_on    = true;
    bool special_effects_on = true;
    EchoColors      echo_colors;
    NonHeaderColors nonheader_colors;
    std::array<KeywordEntry, MAX_KEYWORDS> keywords;
};

// ---------------------------------------------------------------------------
// Parity enumeration
// ---------------------------------------------------------------------------
enum class Parity { None, Even, Odd };

// ---------------------------------------------------------------------------
// Communication parameters  (P-flagged: need ALT-P to apply)
// ---------------------------------------------------------------------------
struct CommParams {
    int    baud_rate   = 1200;      // 300,1200,2400,4800,9600
    int    data_bits   = 8;         // 7 or 8
    int    stop_bits   = 1;         // 1 or 2
    Parity parity      = Parity::None;
    int    com_port    = 2;         // 1-4  (COM1-COM4)
};

// ---------------------------------------------------------------------------
// Alarm / notification sound type
// ---------------------------------------------------------------------------
enum class AlarmType { None, Bell, Alarm, Lance };
constexpr const char* ALARM_NAMES[4] = { "NONE ", "BELL ", "ALARM", "LANCE" };

// ---------------------------------------------------------------------------
// Full configuration  (written/read as PACKCOM.CNF)
// ---------------------------------------------------------------------------
struct PackComConfig {
    // --- P-flagged (require ALT-P reconnect) ---
    CommParams comm;

    // --- X-flagged (require restart) ---
    int    scrollback_lines     = SCROLLBACK_DEFAULT;
    int    dos_mem_kb           = 0;          // KB for DOS shell child
    bool   set_tnc_datetime     = false;
    std::string overlay_path    = "";         // drive/path for overlay files

    // --- Immediate ---
    bool   incoming_bell        = true;
    bool   eliminate_blank_lines= false;
    int    time_mgmt_minutes    = 0;
    bool   display_time         = false;
    int    byte_delay_ms        = 0;          // 1/1000 sec after each byte
    int    crlf_delay_ms        = 0;          // 1/1000 sec after CRLF
    std::string default_dos_prog= "";
    char   stream_switch_char   = '\x1C';     // default switch-stream char
    bool   carrier_detect_cd    = true;       // CD line = connected?
    bool   break_cmd_mode       = false;      // BREAK → TNC command mode

    // --- Colour ---
    ColorConfig colors;
    SpecialEffectsConfig sfx;
};

// ---------------------------------------------------------------------------
// Keyboard macro entry
// ---------------------------------------------------------------------------
struct MacroKey {
    std::string label;   // e.g. "F1"
    std::string text;    // expansion; ] = CR, @x = CMD, ~ = delay, ; = comment
};

// ---------------------------------------------------------------------------
// Keyboard input mode
// ---------------------------------------------------------------------------
enum class InputMode { Line, Char };

// ---------------------------------------------------------------------------
// Duplex mode
// ---------------------------------------------------------------------------
enum class DuplexMode { Full, Half };

// ---------------------------------------------------------------------------
// YAPP transfer state machine  (state names recovered verbatim from binary)
// ---------------------------------------------------------------------------
enum class YappState {
    Start,
    SendInit,
    SendHeader,
    SendData,
    SendEof,
    SendEOT,
    RcvWait,
    RcvHeader,
    RcvData,
    SndABORT,
    WaitAbtAck,
    RcdABORT
};

constexpr const char* YAPP_STATE_NAMES[] = {
    "Start     ",
    "SendInit  ",
    "SendHeader",
    "SendData  ",
    "SendEof   ",
    "SendEOT   ",
    "RcvWait   ",
    "RcvHeader ",
    "RcvData   ",
    "SndABORT  ",
    "WaitAbtAck",
    "RcdABORT  "
};

// YAPP packet-type byte codes  (recovered from binary)
enum class YappPacket : char {
    UK = 'U', // Unknown / undefined
    RR = 'R', // Receive Ready
    RF,        // Receive File
    SI,        // Send Init
    HD,        // Header
    DT,        // Data
    EF,        // End of File
    ET,        // End of Transfer
    NR,        // Not Ready
    CN,        // Cancel
    CA,        // Cancel Abort
    RI,        // Retry Init
    TX,        // Transmit
    UU,        // Unknown/Unexpected
    TM,        // Timeout
    AF,        // Abort File
    AT         // Abort Transfer
};

// YAPP status messages (recovered verbatim)
constexpr const char* YAPP_STATUS[] = {
    "(Ready to Send - Awaiting Recieve Ready  ",
    "(Sent Header - Awaiting Ready to Rcv File",
    "(Sending Data                            ",
    "(UNEXPECTED Packet Type Recieved         ",
    "(SEND COMPLETE - Awaiting Ack EOF (file) ",
    "(SEND COMPLETE - Awaiting Ack EOT (xfer) ",
    "(Ready to Recieve - Awaiting Send Init   ",
    "(Ready to Recieve Header                 ",
    "(Recieving Data                          ",
    "(FILE RECIEVED SUCESSFULLY AND SAVED     ",
    "(Awaiting EOT (xfer) or Next Header      ",
    "(        Press <ESC> to Abort            ",
    "(Returning to Terminal Mode              ",
    "(Abort Request Sent - Awaiting Ack       ",
    "(Transfer Cancelled by other station     ",
    "(Transfer Cancelled - Timeout            ",
    "(Transfer Cancelled                      ",
    "(Transfer Cancelled - DISCONNECTED       ",
    "(NOT ENOUGH ROOM ON DISK TO CAPTURE FILE "
};

// ---------------------------------------------------------------------------
// DOS error codes  (recovered verbatim from overlay 0 and overlay 1)
// ---------------------------------------------------------------------------
constexpr const char* DOS_ERRORS[] = {
    "invalid function number",
    "file not found",
    "path not found",
    "too many open files (no handles left)",
    "access denied",
    "invalid file handle",
    "memory control blocks destroyed",
    "insufficient memory",
    "invalid memory block address",
    "invalid environment",
    "invalid format",
    "invalid access code",
    "invalid data",
    "unrecognized error",
    "invalid drive was specified",
    "attempted to remove the current directory",
    "not same device",
    "no more files"
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
class Serial;
class Terminal;
class ScrollBack;
class Config;
class YappTransfer;
class CaptureFile;
class MacroManager;
