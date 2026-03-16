// ============================================================================
//  serial.cpp
// ============================================================================
#include "serial.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

Serial::Serial(const CommParams& p) : params_(p) {}

Serial::~Serial() { close(); }

bool Serial::open() {
    if (is_open()) close();
    handle_ = Platform::serial_open(params_.com_port);
    if (handle_ == Platform::INVALID_SERIAL) return false;
    if (!reconfigure(params_)) { close(); return false; }
    Platform::serial_set_dtr(handle_, true);
    running_ = true;
    io_thread_ = std::thread(&Serial::io_loop, this);
    return true;
}

void Serial::close() {
    running_ = false;
    if (io_thread_.joinable()) io_thread_.join();
    if (is_open()) {
        Platform::serial_set_dtr(handle_, false);
        Platform::serial_close(handle_);
        handle_ = Platform::INVALID_SERIAL;
    }
}

bool Serial::reconfigure(const CommParams& p) {
    params_ = p;
    if (!is_open()) return false;
    return Platform::serial_configure(handle_, p.baud_rate, p.data_bits,
                                      p.stop_bits, parity_char());
}

char Serial::parity_char() const {
    switch (params_.parity) {
        case Parity::Even: return 'E';
        case Parity::Odd:  return 'O';
        default:           return 'N';
    }
}

bool Serial::carrier_detect() const {
    if (!is_open()) return false;
    return Platform::serial_carrier_detect(handle_);
}

bool Serial::clear_to_send() const {
    // CTS is implicit in modern setups; we read it from modem status
    // For simplicity we assume CTS follows CD unless overridden
    return carrier_detect();
}

void Serial::set_dtr(bool on) {
    if (is_open()) Platform::serial_set_dtr(handle_, on);
}

void Serial::send_break() {
    if (is_open()) Platform::serial_send_break(handle_);
}

bool Serial::read_byte(uint8_t& out) {
    if (rx_fill_.load() == 0) return false;
    out = rx_buf_[rx_head_];
    rx_head_ = (rx_head_ + 1) % RX_BUF;
    rx_fill_.fetch_sub(1);
    return true;
}

int Serial::bytes_available() const {
    return rx_fill_.load();
}

bool Serial::write_byte(uint8_t b, int delay_ms) {
    if (!is_open()) return false;
    // spin-wait if tx ring full (rare at human typing speeds)
    auto deadline = Platform::millis() + 2000;
    while (((tx_tail_ + 1) % TX_BUF) == tx_head_) {
        if (Platform::millis() > deadline) return false;
        Platform::sleep_ms(1);
    }
    tx_buf_[tx_tail_] = b;
    tx_tail_ = (tx_tail_ + 1) % TX_BUF;
    if (delay_ms > 0) Platform::sleep_ms(delay_ms);
    return true;
}

bool Serial::write_str(const std::string& s, int byte_delay_ms, int crlf_delay_ms) {
    for (char c : s) {
        if (!write_byte((uint8_t)c, byte_delay_ms)) return false;
        if ((c == '\r' || c == '\n') && crlf_delay_ms > 0)
            Platform::sleep_ms(crlf_delay_ms);
    }
    return true;
}

bool Serial::wait_drain(int timeout_ms) {
    auto deadline = Platform::millis() + timeout_ms;
    while (tx_head_ != tx_tail_) {
        if (Platform::millis() > deadline) return false;
        Platform::sleep_ms(5);
    }
    return true;
}

// Background I/O thread: drains tx ring → serial, fills rx ring ← serial
void Serial::io_loop() {
    while (running_.load()) {
        // TX
        while (tx_head_ != tx_tail_) {
            uint8_t b = tx_buf_[tx_head_];
            int w = Platform::serial_write(handle_, &b, 1);
            if (w == 1)
                tx_head_ = (tx_head_ + 1) % TX_BUF;
            else
                break;
        }
        // RX
        {
            uint8_t tmp[64];
            int r = Platform::serial_read(handle_, tmp, sizeof(tmp));
            for (int i = 0; i < r; ++i) {
                if (rx_fill_.load() < RX_BUF) {
                    rx_buf_[rx_tail_] = tmp[i];
                    rx_tail_ = (rx_tail_ + 1) % RX_BUF;
                    rx_fill_.fetch_add(1);
                }
                // else overflow: silently drop (matches original behaviour)
            }
        }
        Platform::sleep_ms(1);  // ~1 ms polling interval
    }
}

std::string Serial::diagnostics() const {
    // Mirrors "Display_Comm_Port_Info" output recovered from overlay .001
    char buf[1024];
    (void)0; // port string folded into snprintf below
    ::snprintf(buf, sizeof(buf),
        " base_com_addr      COM%d\n"
        " sout_buf_size      %d\n"
        " sin_buf_size       %d\n"
        " sin_buf_fill_cnt   %d\n"
        " Data_Terminal_Ready %s\n"
        " Clear_To_Send       %s\n"
        " Carrier_Detect      %s\n",
        params_.com_port,
        TX_BUF, RX_BUF,
        rx_fill_.load(),
        "TRUE",           // DTR always asserted when open
        clear_to_send() ? "TRUE" : "FALSE",
        carrier_detect() ? "TRUE" : "FALSE"
    );
    return buf;
}
