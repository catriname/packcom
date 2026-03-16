// ============================================================================
//  platform.cpp  –  OS abstraction implementation
// ============================================================================
#include "platform.h"
#include "keys.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cassert>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

#ifdef PACKCOM_WINDOWS
#  include <windows.h>
#  include <direct.h>
#  include <io.h>
#else
#  include <termios.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <sys/select.h>
#  include <sys/statvfs.h>
#  include <dirent.h>
#  include <glob.h>
#  include <errno.h>
#  include <signal.h>
#endif

namespace Platform {

// ===========================================================================
//  Console
// ===========================================================================

#ifdef PACKCOM_POSIX
static struct termios g_orig_termios;
static bool g_raw_mode = false;
#endif

void console_init() {
#ifdef PACKCOM_POSIX
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~OPOST;
    raw.c_cflag |=  CS8;
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 1;   // 100 ms timeout
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    g_raw_mode = true;
#endif
    // Windows: handled naturally by conio.h
}

void console_restore() {
#ifdef PACKCOM_POSIX
    if (g_raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_raw_mode = false;
    }
#endif
}

bool kbhit() {
#ifdef PACKCOM_POSIX
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};
    return select(1, &fds, nullptr, nullptr, &tv) > 0;
#else
    return ::_kbhit() != 0;
#endif
}

// ANSI escape → internal KeyCode mapping for POSIX
static int decode_escape_sequence() {
    // We already consumed ESC; peek at what follows
    char seq[8] = {};
    int  n = 0;
#ifdef PACKCOM_POSIX
    // Set a very short timeout to distinguish lone ESC from sequences
    struct timeval tv = {0, 50000};  // 50 ms
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    if (select(1, &fds, nullptr, nullptr, &tv) <= 0)
        return KEY_ESC;  // lone ESC
    seq[n++] = (char)::read(STDIN_FILENO, seq, 1);  // consume '['
    if (seq[0] != '[' && seq[0] != 'O') return KEY_ESC;

    FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds); tv = {0, 50000};
    if (select(1, &fds, nullptr, nullptr, &tv) <= 0) return KEY_ESC;
    ::read(STDIN_FILENO, seq + 1, 1);

    // CSI sequences
    if (seq[0] == '[') {
        switch (seq[1]) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
            case '2': ::read(STDIN_FILENO, seq+2, 1); return KEY_INS;
            case '3': ::read(STDIN_FILENO, seq+2, 1); return KEY_DEL;
            case '5': ::read(STDIN_FILENO, seq+2, 1); return KEY_PGUP;
            case '6': ::read(STDIN_FILENO, seq+2, 1); return KEY_PGDN;
        }
    }
    // SS3 sequences (xterm)
    if (seq[0] == 'O') {
        switch (seq[1]) {
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
            case 'P': return KEY_F1;
            case 'Q': return KEY_F2;
            case 'R': return KEY_F3;
            case 'S': return KEY_F4;
        }
    }
#endif
    return KEY_UNKNOWN;
}

int getch_raw() {
#ifdef PACKCOM_POSIX
    uint8_t c = 0;
    int r = ::read(STDIN_FILENO, &c, 1);
    if (r <= 0) return -1;
    if (c == 27) return decode_escape_sequence();
    // Alt-key: if next byte arrives quickly after ESC substitute above,
    // real alt combos are handled differently on real terminals.
    // For now map Ctrl+letter for ALT simulation (Alt-H etc. via Meta).
    return (int)c;
#else
    int c = ::_getch();
    if (c == 0 || c == 0xE0) {
        int ext = ::_getch();
        // Map Windows scan codes to our KeyCode enum
        switch (ext) {
            case 72: return KEY_UP;
            case 80: return KEY_DOWN;
            case 75: return KEY_LEFT;
            case 77: return KEY_RIGHT;
            case 71: return KEY_HOME;
            case 79: return KEY_END;
            case 73: return KEY_PGUP;
            case 81: return KEY_PGDN;
            case 82: return KEY_INS;
            case 83: return KEY_DEL;
            case 119: return KEY_CTRL_HOME;
            case 117: return KEY_CTRL_END;
            case 115: return KEY_CTRL_LEFT;
            case 116: return KEY_CTRL_RIGHT;
            case 134: return KEY_CTRL_PGUP;
            case 118: return KEY_CTRL_PGDN;
            // Alt keys (scan code 0 prefix)
            case 35: return KEY_ALT_H;
            case 46: return KEY_ALT_C;
            case 23: return KEY_ALT_I;
            case 25: return KEY_ALT_P;
            case 45: return KEY_ALT_X;
            case 47: return KEY_ALT_V;
            case 20: return KEY_ALT_T;
            case 21: return KEY_ALT_Y;
            case 32: return KEY_ALT_D;
            case 49: return KEY_ALT_N;
            case 18: return KEY_ALT_E;
            case 31: return KEY_ALT_S;
            case 37: return KEY_ALT_K;
            case 50: return KEY_ALT_M;
            case 16: return KEY_ALT_Q;
            case 33: return KEY_ALT_F;
            case 48: return KEY_ALT_B;
            case 24: return KEY_ALT_O;
            case 30: return KEY_ALT_A;
            default: return KEY_UNKNOWN;
        }
    }
    return c;
#endif
}

void putch(char c) { ::fputc(c, stdout); ::fflush(stdout); }

void puts_raw(const std::string& s) {
    ::fwrite(s.data(), 1, s.size(), stdout);
    ::fflush(stdout);
}

void clear_screen() {
#ifdef PACKCOM_POSIX
    puts_raw("\033[2J\033[H");
#else
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD origin = {0,0};
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(h, &csbi);
    DWORD written;
    DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(h, ' ', cells, origin, &written);
    FillConsoleOutputAttribute(h, csbi.wAttributes, cells, origin, &written);
    SetConsoleCursorPosition(h, origin);
#endif
}

void goto_xy(int row, int col) {
#ifdef PACKCOM_POSIX
    char buf[32];
    ::snprintf(buf, sizeof(buf), "\033[%d;%dH", row+1, col+1);
    puts_raw(buf);
#else
    COORD pos = { (SHORT)col, (SHORT)row };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
#endif
}

void get_terminal_size(int& rows, int& cols) {
    rows = 25; cols = 80;  // defaults
#ifdef PACKCOM_POSIX
    struct winsize ws;
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        rows = ws.ws_row;
        cols = ws.ws_col;
    }
#else
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        cols = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
        rows = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
    }
#endif
}

// ANSI colour table: maps 0-15 to ANSI 256-colour codes
static const int ANSI_FG[16] = {30,34,32,36,31,35,33,37,90,94,92,96,91,95,93,97};
static const int ANSI_BG[16] = {40,44,42,46,41,45,43,47,100,104,102,106,101,105,103,107};

void set_color(int fg, int bg) {
#ifdef PACKCOM_POSIX
    char buf[32];
    ::snprintf(buf, sizeof(buf), "\033[%d;%dm", ANSI_FG[fg & 15], ANSI_BG[bg & 15]);
    puts_raw(buf);
#else
    WORD attr = (WORD)((bg & 0xF) << 4) | (fg & 0xF);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), attr);
#endif
}

void reset_color() {
#ifdef PACKCOM_POSIX
    puts_raw("\033[0m");
#else
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
#endif
}

void beep() {
#ifdef PACKCOM_POSIX
    ::fputc('\007', stdout); ::fflush(stdout);
#else
    ::MessageBeep(MB_OK);
#endif
}

// ===========================================================================
//  Serial port
// ===========================================================================

#ifdef PACKCOM_POSIX
static const char* serial_dev(int portNum) {
    switch (portNum) {
        case 1: return "/dev/ttyS0";
        case 2: return "/dev/ttyS1";
        case 3: return "/dev/ttyS2";
        case 4: return "/dev/ttyS3";
        default: return "/dev/ttyS0";
    }
}
#endif

SerialHandle serial_open(int portNum) {
#ifdef PACKCOM_POSIX
    const char* dev = serial_dev(portNum);
    int fd = ::open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    return fd;
#else
    char devName[16];
    ::snprintf(devName, sizeof(devName), "\\\\.\\COM%d", portNum);
    HANDLE h = CreateFileA(devName, GENERIC_READ | GENERIC_WRITE,
                           0, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    return h;
#endif
}

void serial_close(SerialHandle h) {
#ifdef PACKCOM_POSIX
    if (h >= 0) ::close(h);
#else
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
#endif
}

bool serial_configure(SerialHandle h, int baud, int data_bits, int stop_bits, char parity) {
#ifdef PACKCOM_POSIX
    struct termios tios = {};
    if (tcgetattr(h, &tios) != 0) return false;
    cfmakeraw(&tios);

    speed_t speed = B9600;
    switch (baud) {
        case 300:    speed = B300;    break;
        case 1200:   speed = B1200;   break;
        case 2400:   speed = B2400;   break;
        case 4800:   speed = B4800;   break;
        case 9600:   speed = B9600;   break;
        default:     speed = B1200;   break;
    }
    cfsetispeed(&tios, speed);
    cfsetospeed(&tios, speed);

    tios.c_cflag &= ~CSIZE;
    switch (data_bits) {
        case 7: tios.c_cflag |= CS7; break;
        default: tios.c_cflag |= CS8; break;
    }

    if (stop_bits == 2) tios.c_cflag |= CSTOPB;
    else                tios.c_cflag &= ~CSTOPB;

    tios.c_iflag &= ~(INPCK | ISTRIP);
    tios.c_cflag &= ~(PARENB | PARODD);
    switch (parity) {
        case 'E': tios.c_cflag |= PARENB;              tios.c_iflag |= INPCK; break;
        case 'O': tios.c_cflag |= PARENB | PARODD;     tios.c_iflag |= INPCK; break;
        default: break;
    }
    tios.c_cflag |= CLOCAL | CREAD;
    tios.c_cc[VMIN]  = 0;
    tios.c_cc[VTIME] = 1;
    return tcsetattr(h, TCSANOW, &tios) == 0;
#else
    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) return false;
    dcb.BaudRate = (DWORD)baud;
    dcb.ByteSize = (BYTE)data_bits;
    dcb.StopBits = (stop_bits == 2) ? TWOSTOPBITS : ONESTOPBIT;
    switch (parity) {
        case 'E': dcb.Parity = EVENPARITY;  dcb.fParity = TRUE; break;
        case 'O': dcb.Parity = ODDPARITY;   dcb.fParity = TRUE; break;
        default:  dcb.Parity = NOPARITY;    dcb.fParity = FALSE; break;
    }
    dcb.fBinary = TRUE;
    if (!SetCommState(h, &dcb)) return false;
    COMMTIMEOUTS ct = {};
    ct.ReadIntervalTimeout         = MAXDWORD;
    ct.ReadTotalTimeoutConstant    = 0;
    ct.ReadTotalTimeoutMultiplier  = 0;
    ct.WriteTotalTimeoutConstant   = 500;
    ct.WriteTotalTimeoutMultiplier = 10;
    return SetCommTimeouts(h, &ct) != 0;
#endif
}

int serial_read(SerialHandle h, uint8_t* buf, int maxlen) {
#ifdef PACKCOM_POSIX
    int r = ::read(h, buf, maxlen);
    return (r < 0 && errno == EAGAIN) ? 0 : r;
#else
    DWORD n = 0;
    ReadFile(h, buf, (DWORD)maxlen, &n, nullptr);
    return (int)n;
#endif
}

int serial_write(SerialHandle h, const uint8_t* buf, int len) {
#ifdef PACKCOM_POSIX
    return (int)::write(h, buf, len);
#else
    DWORD n = 0;
    WriteFile(h, buf, (DWORD)len, &n, nullptr);
    return (int)n;
#endif
}

bool serial_carrier_detect(SerialHandle h) {
#ifdef PACKCOM_POSIX
    int status = 0;
    ::ioctl(h, TIOCMGET, &status);
    return (status & TIOCM_CD) != 0;
#else
    DWORD status = 0;
    GetCommModemStatus(h, &status);
    return (status & MS_RLSD_ON) != 0;
#endif
}

void serial_set_dtr(SerialHandle h, bool on) {
#ifdef PACKCOM_POSIX
    int cmd = on ? TIOCMBIS : TIOCMBIC;
    int flag = TIOCM_DTR;
    ::ioctl(h, cmd, &flag);
#else
    EscapeCommFunction(h, on ? SETDTR : CLRDTR);
#endif
}

void serial_send_break(SerialHandle h) {
#ifdef PACKCOM_POSIX
    tcsendbreak(h, 0);
#else
    SetCommBreak(h);
    sleep_ms(250);
    ClearCommBreak(h);
#endif
}

// ===========================================================================
//  Time helpers
// ===========================================================================

std::string current_time_str() {
    time_t t = ::time(nullptr);
    struct tm* tm_ = ::localtime(&t);
    char buf[16];
    ::strftime(buf, sizeof(buf), "%H:%M:%S", tm_);
    return buf;
}

std::string current_date_str() {
    time_t t = ::time(nullptr);
    struct tm* tm_ = ::localtime(&t);
    char buf[16];
    ::strftime(buf, sizeof(buf), "%m/%d/%Y", tm_);
    return buf;
}

void sleep_ms(int ms) {
#ifdef PACKCOM_POSIX
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    ::nanosleep(&ts, nullptr);
#else
    ::Sleep((DWORD)ms);
#endif
}

uint64_t millis() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

// ===========================================================================
//  File / directory helpers
// ===========================================================================

bool file_exists(const std::string& path) {
#ifdef PACKCOM_POSIX
    return ::access(path.c_str(), F_OK) == 0;
#else
    return ::_access(path.c_str(), 0) == 0;
#endif
}

long file_size(const std::string& path) {
    FILE* f = ::fopen(path.c_str(), "rb");
    if (!f) return -1;
    ::fseek(f, 0, SEEK_END);
    long sz = ::ftell(f);
    ::fclose(f);
    return sz;
}

bool file_delete(const std::string& path) {
    return ::remove(path.c_str()) == 0;
}

std::string get_cwd() {
    char buf[512] = {};
#ifdef PACKCOM_POSIX
    ::getcwd(buf, sizeof(buf)-1);
#else
    ::_getcwd(buf, sizeof(buf)-1);
#endif
    return buf;
}

bool change_dir(const std::string& path) {
#ifdef PACKCOM_POSIX
    return ::chdir(path.c_str()) == 0;
#else
    return ::_chdir(path.c_str()) == 0;
#endif
}

bool change_drive(char letter) {
#ifdef PACKCOM_WINDOWS
    char path[4] = { letter, ':', '\\', 0 };
    return ::_chdir(path) == 0;
#else
    (void)letter;
    return true;  // no-op on POSIX
#endif
}

uint64_t list_directory(const std::string& mask,
                         std::vector<std::string>& names,
                         std::vector<long>& sizes) {
    names.clear();
    sizes.clear();

#ifdef PACKCOM_POSIX
    // Use glob for mask matching
    glob_t gl = {};
    ::glob(mask.c_str(), GLOB_MARK, nullptr, &gl);
    for (size_t i = 0; i < gl.gl_pathc; ++i) {
        std::string n = gl.gl_pathv[i];
        long sz = file_size(n);
        names.push_back(n);
        sizes.push_back(sz);
    }
    ::globfree(&gl);

    // Free bytes
    struct statvfs sv;
    ::statvfs(".", &sv);
    return (uint64_t)sv.f_bavail * sv.f_frsize;
#else
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(mask.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            names.push_back(fd.cFileName);
            ULARGE_INTEGER sz;
            sz.LowPart  = fd.nFileSizeLow;
            sz.HighPart = fd.nFileSizeHigh;
            sizes.push_back((long)sz.QuadPart);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    ULARGE_INTEGER free_bytes;
    GetDiskFreeSpaceExA(".", &free_bytes, nullptr, nullptr);
    return (uint64_t)free_bytes.QuadPart;
#endif
}

std::string get_comspec() {
    const char* cs = ::getenv("COMSPEC");
    if (cs) return cs;
#ifdef PACKCOM_WINDOWS
    return "cmd.exe";
#else
    const char* sh = ::getenv("SHELL");
    return sh ? sh : "/bin/sh";
#endif
}

int exec_command(const std::string& cmd) {
    return ::system(cmd.c_str());
}

} // namespace Platform
