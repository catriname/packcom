#pragma once
// ============================================================================
//  serial.h  –  interrupt-driven-style serial I/O with ring buffers
//  Mirrors the original Pascal variables:
//    sout_store_ptr, sout_read_ptr, sout_buf_size
//    sin_read_ptr,   sin_buf_size,  sin_buf_fill_cnt
//    async_intr_handler, IRQ_vector_ofs/seg
//    Data_Terminal_Ready, Clear_To_Send, Carrier_Detect
// ============================================================================
#include "platform.h"
#include "packcom.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <deque>

class Serial {
public:
    // -----------------------------------------------------------------------
    explicit Serial(const CommParams& p);
    ~Serial();

    // Open / close the port
    bool open();
    void close();
    bool is_open() const { return handle_ != Platform::INVALID_SERIAL; }

    // Reconfigure on-the-fly (requires re-open in practice)
    bool reconfigure(const CommParams& p);

    // Modem signals
    bool carrier_detect() const;
    bool clear_to_send()  const;
    void set_dtr(bool on);

    // Send BREAK (~250 ms)
    void send_break();

    // Non-blocking read from input ring buffer; returns '\0' if empty
    bool read_byte(uint8_t& out);

    // How many bytes are waiting in input ring buffer
    int  bytes_available() const;

    // Write one byte; blocks briefly if output buffer full
    bool write_byte(uint8_t b, int delay_ms = 0);

    // Write a string (each char optionally followed by byte_delay_ms)
    bool write_str(const std::string& s, int byte_delay_ms = 0, int crlf_delay_ms = 0);

    // Wait until the output buffer drains (or timeout_ms elapses)
    bool wait_drain(int timeout_ms = 5000);

    // Diagnostics string (mirrors Display_Comm_Port_Info output)
    std::string diagnostics() const;

    const CommParams& params() const { return params_; }

private:
    CommParams params_;
    Platform::SerialHandle handle_ = Platform::INVALID_SERIAL;

    // Ring buffers
    static constexpr int RX_BUF = SIN_BUF_SIZE;
    static constexpr int TX_BUF = SOUT_BUF_SIZE;

    std::array<uint8_t, RX_BUF> rx_buf_;
    std::array<uint8_t, TX_BUF> tx_buf_;

    std::atomic<int> rx_head_{0}, rx_tail_{0};   // sin ring
    std::atomic<int> tx_head_{0}, tx_tail_{0};   // sout ring
    std::atomic<int> rx_fill_{0};                // sin_buf_fill_cnt

    mutable std::mutex mutex_;
    std::thread io_thread_;
    std::atomic<bool> running_{false};

    void io_loop();
    char parity_char() const;
};
