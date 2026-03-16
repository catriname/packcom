#pragma once
// ============================================================================
//  platform.h  –  thin OS abstraction layer
//  Wraps POSIX (Linux/macOS) and Win32 differences so the rest of the code
//  stays portable.
// ============================================================================

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
#  define PACKCOM_WINDOWS
#  include <windows.h>
#  include <conio.h>
#else
#  ifndef PACKCOM_POSIX
#    define PACKCOM_POSIX
#  endif
#  include <termios.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <sys/select.h>
#endif

namespace Platform {

// ---------------------------------------------------------------------------
// Console / terminal
// ---------------------------------------------------------------------------

/// Initialise the console for raw character I/O (no echo, no line-buffering).
void console_init();

/// Restore the console to its original state.
void console_restore();

/// Return true if a key is waiting in stdin.
bool kbhit();

/// Read one raw key code.  Returns -1 on error.
/// Extended keys (arrows, F-keys, etc.) are returned as negative values
/// using the encoding defined in keys.h.
int  getch_raw();

/// Write a character to stdout.
void putch(char c);

/// Write a string to stdout.
void puts_raw(const std::string& s);

/// Clear the screen.
void clear_screen();

/// Move cursor to (row, col), 0-based.
void goto_xy(int row, int col);

/// Get current terminal dimensions.
void get_terminal_size(int& rows, int& cols);

/// Set foreground + background colour (ANSI on POSIX, SetConsoleTextAttribute on Win32).
void set_color(int fg, int bg);

/// Reset colour to default.
void reset_color();

/// Ring the terminal bell.
void beep();

// ---------------------------------------------------------------------------
// Serial port
// ---------------------------------------------------------------------------

#ifdef PACKCOM_WINDOWS
using SerialHandle = HANDLE;
constexpr SerialHandle INVALID_SERIAL = INVALID_HANDLE_VALUE;
#else
using SerialHandle = int;
constexpr SerialHandle INVALID_SERIAL = -1;
#endif

/// Open a serial port.  portNum is 1-based (1 = COM1 / /dev/ttyS0).
/// Returns INVALID_SERIAL on failure.
SerialHandle serial_open(int portNum);

/// Close a serial port handle.
void serial_close(SerialHandle h);

/// Configure baud, data bits, stop bits, parity.
bool serial_configure(SerialHandle h, int baud, int data_bits, int stop_bits, char parity);

/// Non-blocking read; returns bytes actually read (0 if none available).
int  serial_read(SerialHandle h, uint8_t* buf, int maxlen);

/// Write bytes; returns bytes written, or -1 on error.
int  serial_write(SerialHandle h, const uint8_t* buf, int len);

/// Check modem status: returns true if Carrier Detect is asserted.
bool serial_carrier_detect(SerialHandle h);

/// Assert / de-assert Data Terminal Ready.
void serial_set_dtr(SerialHandle h, bool on);

/// Send a serial BREAK signal (~250 ms).
void serial_send_break(SerialHandle h);

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

/// Current wall-clock time as "HH:MM:SS".
std::string current_time_str();

/// Current date as "MM/DD/YYYY".
std::string current_date_str();

/// Sleep for ms milliseconds.
void sleep_ms(int ms);

/// Return monotonic millisecond counter.
uint64_t millis();

// ---------------------------------------------------------------------------
// File / directory helpers
// ---------------------------------------------------------------------------

/// Return true if a file exists.
bool file_exists(const std::string& path);

/// Return file size in bytes, or -1 if not found.
long file_size(const std::string& path);

/// Delete a file; returns true on success.
bool file_delete(const std::string& path);

/// Return current working directory string.
std::string get_cwd();

/// Change directory; returns true on success.
bool change_dir(const std::string& path);

/// Change drive letter (Windows-only, no-op on POSIX).
bool change_drive(char letter);

/// List files matching a mask (e.g. "*.*") in directory.
/// Fills names with filenames, sizes with file sizes.
/// Returns free bytes on the current drive.
uint64_t list_directory(const std::string& mask,
                        std::vector<std::string>& names,
                        std::vector<long>& sizes);

/// Get the COMSPEC environment variable (path to command interpreter).
std::string get_comspec();

/// Execute an external program/command; returns exit code.
int  exec_command(const std::string& cmd);

} // namespace Platform
