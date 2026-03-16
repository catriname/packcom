#pragma once
// ============================================================================
//  yapp.h  –  YAPP (Yet Another Packet Protocol) file transfer
//  State machine names, packet types, and status strings all recovered
//  verbatim from PACKCOM.001 overlay binary.
//
//  YAPP packet structure:
//    SOH (0x01)  type (1 byte)  length (1 byte)  data (length bytes)  checksum (1 byte)
// ============================================================================
#include "packcom.h"
#include "serial.h"
#include "terminal.h"
#include <string>
#include <fstream>
#include <functional>

class YappTransfer {
public:
    static constexpr uint8_t SOH        = 0x01;
    static constexpr int     BLOCK_SIZE = 128;
    static constexpr int     TIMEOUT_MS = 10000;
    static constexpr int     MAX_RETRY  = 10;

    // Progress callback: (bytes_done, bytes_total, state_name)
    using ProgressFn = std::function<void(long, long, const char*)>;

    YappTransfer(Serial& serial, Terminal& term);

    // Send a file via YAPP.  Returns true on success.
    bool send(const std::string& filename, ProgressFn on_progress = nullptr);

    // Receive a file via YAPP.  Returns true on success.
    bool receive(const std::string& save_path, ProgressFn on_progress = nullptr);

    // Show the YAPP menu and handle user choice
    void run_menu();

    bool was_aborted() const { return aborted_; }

private:
    Serial&   serial_;
    Terminal& term_;
    YappState state_ = YappState::Start;
    bool      aborted_ = false;

    // Low-level packet I/O
    bool send_packet(YappPacket type, const uint8_t* data, uint8_t len);
    bool recv_packet(YappPacket& type, std::vector<uint8_t>& data, int timeout_ms);

    // Checksum
    static uint8_t checksum(YappPacket type, const uint8_t* data, uint8_t len);

    // Update the on-screen status display
    void show_status(const char* status_msg, long bytes_done, long total,
                     const std::string& header, ProgressFn fn);

    // Check for ESC abort
    bool check_abort();

    // State transition
    void set_state(YappState s);
};
