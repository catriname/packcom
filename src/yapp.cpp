// ============================================================================
//  yapp.cpp  –  YAPP file transfer state machine
// ============================================================================
#include "yapp.h"
#include "keys.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>

YappTransfer::YappTransfer(Serial& serial, Terminal& term)
    : serial_(serial), term_(term) {}

// ---------------------------------------------------------------------------
// Menu  (recovered verbatim from .001 overlay strings)
// ---------------------------------------------------------------------------
void YappTransfer::run_menu() {
    Platform::clear_screen();
    Platform::set_color(CLR_LIGHTGRAY, CLR_BLACK);
    Platform::goto_xy(5, 20);
    Platform::puts_raw("YAPP File Transfer Protocol");
    Platform::goto_xy(7, 24);
    Platform::puts_raw("  R.  Receive File");
    Platform::goto_xy(8, 24);
    Platform::puts_raw("  S.  Send File");
    Platform::goto_xy(10, 20);
    Platform::puts_raw("  Enter the option desired");
    Platform::goto_xy(11, 20);
    Platform::puts_raw("         or RETURN to exit: ");
    Platform::reset_color();

    while (true) {
        int k = Platform::getch_raw();
        if (k == 'r' || k == 'R') {
            // Prompt for receive filename
            Platform::goto_xy(14, 4);
            Platform::puts_raw("! Enter filename to Recieve Data: ");
            std::string fname;
            char c;
            while ((c = (char)Platform::getch_raw()) != '\r' && c != '\n')
                fname += c;
            if (!fname.empty()) receive(fname);
            break;
        } else if (k == 's' || k == 'S') {
            Platform::goto_xy(14, 4);
            Platform::puts_raw(" Enter Filename to Upload: ");
            std::string fname;
            char c;
            while ((c = (char)Platform::getch_raw()) != '\r' && c != '\n')
                fname += c;
            if (!fname.empty()) {
                if (!Platform::file_exists(fname)) {
                    Platform::goto_xy(16, 4);
                    Platform::puts_raw(" Cannot find file: " + fname);
                    Platform::sleep_ms(2000);
                } else {
                    send(fname);
                }
            }
            break;
        } else if (k == KEY_ENTER || k == KEY_ESC) {
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Packet helpers
// ---------------------------------------------------------------------------

uint8_t YappTransfer::checksum(YappPacket type, const uint8_t* data, uint8_t len) {
    uint8_t cs = (uint8_t)type + len;
    for (int i = 0; i < len; ++i) cs += data[i];
    return cs;
}

bool YappTransfer::send_packet(YappPacket type, const uint8_t* data, uint8_t len) {
    uint8_t pkt[3 + 255];
    pkt[0] = SOH;
    pkt[1] = (uint8_t)type;
    pkt[2] = len;
    if (data && len > 0) std::memcpy(pkt + 3, data, len);
    pkt[3 + len] = checksum(type, data ? data : (uint8_t*)"", len);
    return serial_.write_str(std::string((char*)pkt, 4 + len)) ;
}

bool YappTransfer::recv_packet(YappPacket& type, std::vector<uint8_t>& data, int timeout_ms) {
    auto deadline = Platform::millis() + timeout_ms;
    data.clear();

    // Wait for SOH
    while (true) {
        if (Platform::millis() > deadline) return false;
        uint8_t b;
        if (serial_.read_byte(b) && b == SOH) break;
        Platform::sleep_ms(1);
    }

    // type
    {
        auto t2 = Platform::millis() + 2000;
        uint8_t b;
        while (!serial_.read_byte(b)) {
            if (Platform::millis() > t2) return false;
            Platform::sleep_ms(1);
        }
        type = (YappPacket)b;
    }

    // length
    uint8_t len;
    {
        auto t2 = Platform::millis() + 2000;
        uint8_t b;
        while (!serial_.read_byte(b)) {
            if (Platform::millis() > t2) return false;
            Platform::sleep_ms(1);
        }
        len = b;
    }

    // data bytes
    for (int i = 0; i < len; ++i) {
        auto t2 = Platform::millis() + 2000;
        uint8_t b;
        while (!serial_.read_byte(b)) {
            if (Platform::millis() > t2) return false;
            Platform::sleep_ms(1);
        }
        data.push_back(b);
    }

    // checksum
    uint8_t rx_cs;
    {
        auto t2 = Platform::millis() + 2000;
        uint8_t b;
        while (!serial_.read_byte(b)) {
            if (Platform::millis() > t2) return false;
            Platform::sleep_ms(1);
        }
        rx_cs = b;
    }

    uint8_t calc_cs = checksum(type, data.empty() ? nullptr : data.data(), len);
    return rx_cs == calc_cs;
}

void YappTransfer::set_state(YappState s) {
    state_ = s;
}

bool YappTransfer::check_abort() {
    if (Platform::kbhit()) {
        int k = Platform::getch_raw();
        if (k == KEY_ESC) { aborted_ = true; return true; }
    }
    return false;
}

void YappTransfer::show_status(const char* status_msg, long bytes_done, long total,
                                const std::string& header, ProgressFn fn) {
    // Draw the YAPP status window  (mirrors the original on-screen layout)
    Platform::goto_xy(10, 2); Platform::set_color(CLR_CYAN, CLR_BLACK);
    Platform::puts_raw("STATE:  ");
    Platform::puts_raw(YAPP_STATE_NAMES[(int)state_]);
    Platform::goto_xy(11, 2);
    Platform::puts_raw(status_msg);
    if (!header.empty()) {
        Platform::goto_xy(12, 2);
        Platform::puts_raw("Header: " + header);
    }
    Platform::goto_xy(13, 2);
    char buf[80];
    if (total > 0)
        ::snprintf(buf, sizeof(buf), "Bytes: %ld / %ld", bytes_done, total);
    else
        ::snprintf(buf, sizeof(buf), "      %ld bytes", bytes_done);
    Platform::puts_raw(buf);
    Platform::goto_xy(15, 2);
    Platform::puts_raw("        Press <ESC> to Abort            ");
    Platform::reset_color();
    if (fn) fn(bytes_done, total, YAPP_STATE_NAMES[(int)state_]);
}

// ---------------------------------------------------------------------------
// SEND
// ---------------------------------------------------------------------------

bool YappTransfer::send(const std::string& filename, ProgressFn on_progress) {
    aborted_ = false;

    long total = Platform::file_size(filename);
    FILE* f    = ::fopen(filename.c_str(), "rb");
    if (!f) {
        term_.write_status(" Cannot find file: " + filename);
        return false;
    }

    Platform::clear_screen();
    Platform::goto_xy(3, 10);
    Platform::set_color(CLR_WHITE, CLR_BLACK);
    Platform::puts_raw("YAPP File Transfer - SENDING");
    Platform::reset_color();

    // ---- SendInit: wait for RR (Receive Ready) ----
    set_state(YappState::SendInit);
    show_status(YAPP_STATUS[0], 0, total, "", on_progress);

    int retries = 0;
    bool got_rr = false;
    while (!got_rr && retries < MAX_RETRY && !aborted_) {
        // Send SI (Send Init)
        send_packet(YappPacket::SI, nullptr, 0);
        YappPacket pkt; std::vector<uint8_t> data;
        if (recv_packet(pkt, data, TIMEOUT_MS) && pkt == YappPacket::RR)
            got_rr = true;
        else
            ++retries;
        if (check_abort()) break;
    }
    if (!got_rr || aborted_) {
        ::fclose(f);
        set_state(YappState::SndABORT);
        show_status(YAPP_STATUS[13], 0, total, "", on_progress);
        Platform::sleep_ms(2000);
        return false;
    }

    // ---- SendHeader ----
    set_state(YappState::SendHeader);
    // Header packet: null-terminated filename + 4-byte file size
    std::string base = filename;
    auto slash = base.rfind('/');
    if (slash != std::string::npos) base = base.substr(slash+1);
#ifdef _WIN32
    auto bslash = base.rfind('\\');
    if (bslash != std::string::npos) base = base.substr(bslash+1);
#endif
    std::vector<uint8_t> hdr_data(base.begin(), base.end());
    hdr_data.push_back(0);
    uint8_t sz_bytes[4];
    sz_bytes[0] = (uint8_t)(total & 0xFF);
    sz_bytes[1] = (uint8_t)((total >> 8)  & 0xFF);
    sz_bytes[2] = (uint8_t)((total >> 16) & 0xFF);
    sz_bytes[3] = (uint8_t)((total >> 24) & 0xFF);
    hdr_data.insert(hdr_data.end(), sz_bytes, sz_bytes+4);

    show_status(YAPP_STATUS[1], 0, total, base, on_progress);
    retries = 0;
    bool hdr_acked = false;
    while (!hdr_acked && retries < MAX_RETRY && !aborted_) {
        send_packet(YappPacket::HD, hdr_data.data(), (uint8_t)hdr_data.size());
        YappPacket pkt; std::vector<uint8_t> data;
        if (recv_packet(pkt, data, TIMEOUT_MS) && pkt == YappPacket::RF)
            hdr_acked = true;
        else
            ++retries;
        if (check_abort()) break;
    }
    if (!hdr_acked || aborted_) { ::fclose(f); return false; }

    // ---- SendData ----
    set_state(YappState::SendData);
    long bytes_sent = 0;
    uint8_t block[BLOCK_SIZE];
    bool data_ok = true;
    while (!::feof(f) && !aborted_) {
        int n = (int)::fread(block, 1, BLOCK_SIZE, f);
        if (n <= 0) break;
        retries = 0;
        bool acked = false;
        while (!acked && retries < MAX_RETRY) {
            send_packet(YappPacket::DT, block, (uint8_t)n);
            YappPacket pkt; std::vector<uint8_t> pdata;
            if (recv_packet(pkt, pdata, TIMEOUT_MS) && pkt == YappPacket::RR)
                acked = true;
            else
                ++retries;
            if (check_abort()) { data_ok = false; break; }
        }
        if (!data_ok || retries >= MAX_RETRY) break;
        bytes_sent += n;
        show_status(YAPP_STATUS[2], bytes_sent, total, base, on_progress);
    }
    ::fclose(f);

    if (aborted_ || !data_ok) {
        send_packet(YappPacket::CA, nullptr, 0);
        show_status(YAPP_STATUS[14], bytes_sent, total, base, on_progress);
        Platform::sleep_ms(2000);
        return false;
    }

    // ---- SendEof ----
    set_state(YappState::SendEof);
    show_status(YAPP_STATUS[4], bytes_sent, total, base, on_progress);
    retries = 0;
    bool eof_acked = false;
    while (!eof_acked && retries < MAX_RETRY) {
        send_packet(YappPacket::EF, nullptr, 0);
        YappPacket pkt; std::vector<uint8_t> pdata;
        if (recv_packet(pkt, pdata, TIMEOUT_MS) && pkt == YappPacket::RR)
            eof_acked = true;
        else ++retries;
    }

    // ---- SendEOT ----
    set_state(YappState::SendEOT);
    show_status(YAPP_STATUS[5], bytes_sent, total, base, on_progress);
    retries = 0;
    bool eot_acked = false;
    while (!eot_acked && retries < MAX_RETRY) {
        send_packet(YappPacket::ET, nullptr, 0);
        YappPacket pkt; std::vector<uint8_t> pdata;
        if (recv_packet(pkt, pdata, TIMEOUT_MS) && pkt == YappPacket::RR)
            eot_acked = true;
        else ++retries;
    }

    show_status(YAPP_STATUS[12], bytes_sent, total, base, on_progress);  // Returning to terminal
    set_state(YappState::Start);
    Platform::sleep_ms(1500);
    return true;
}

// ---------------------------------------------------------------------------
// RECEIVE
// ---------------------------------------------------------------------------

bool YappTransfer::receive(const std::string& save_path, ProgressFn on_progress) {
    aborted_ = false;

    Platform::clear_screen();
    Platform::goto_xy(3, 10);
    Platform::set_color(CLR_WHITE, CLR_BLACK);
    Platform::puts_raw("YAPP File Transfer - RECEIVING");
    Platform::reset_color();

    // ---- RcvWait: wait for SI ----
    set_state(YappState::RcvWait);
    show_status(YAPP_STATUS[6], 0, 0, "", on_progress);

    // Send RR to signal ready
    send_packet(YappPacket::RR, nullptr, 0);

    YappPacket pkt; std::vector<uint8_t> data;
    if (!recv_packet(pkt, data, TIMEOUT_MS * 3) || aborted_) {
        show_status(YAPP_STATUS[17], 0, 0, "", on_progress);
        Platform::sleep_ms(2000);
        return false;
    }
    if (pkt != YappPacket::SI) {
        show_status(YAPP_STATUS[3], 0, 0, "", on_progress); // UNEXPECTED
        Platform::sleep_ms(2000);
        return false;
    }

    // ---- RcvHeader ----
    set_state(YappState::RcvHeader);
    show_status(YAPP_STATUS[7], 0, 0, "", on_progress);
    send_packet(YappPacket::RF, nullptr, 0);

    if (!recv_packet(pkt, data, TIMEOUT_MS) || pkt != YappPacket::HD) {
        show_status(YAPP_STATUS[3], 0, 0, "", on_progress);
        Platform::sleep_ms(2000);
        return false;
    }

    // Parse header: null-terminated name + 4-byte size
    std::string rcv_name;
    size_t i = 0;
    while (i < data.size() && data[i] != 0) rcv_name += (char)data[i++];
    ++i;  // skip null
    long expected_size = 0;
    if (i + 4 <= data.size()) {
        expected_size = data[i] | (data[i+1]<<8) | (data[i+2]<<16) | (data[i+3]<<24);
    }

    // Determine save path
    std::string out_path = save_path;
    if (Platform::file_exists(out_path)) {
        Platform::goto_xy(17, 4);
        Platform::puts_raw("\"  Write over existing file (N/y) ? ");
        int k = Platform::getch_raw();
        if (k != 'y' && k != 'Y') {
            send_packet(YappPacket::CA, nullptr, 0);
            return false;
        }
    }

    // Check disk space
    std::vector<std::string> dummy_n; std::vector<long> dummy_s;
    uint64_t free_bytes = Platform::list_directory("*", dummy_n, dummy_s);
    if (expected_size > 0 && (long)free_bytes < expected_size) {
        show_status(YAPP_STATUS[18], 0, expected_size, rcv_name, on_progress);  // NOT ENOUGH ROOM
        send_packet(YappPacket::CA, nullptr, 0);
        Platform::sleep_ms(2000);
        return false;
    }

    // Open output file
    FILE* f = ::fopen(out_path.c_str(), "wb");
    if (!f) {
        show_status(" *ERROR* Bad Filename", 0, expected_size, rcv_name, on_progress);
        send_packet(YappPacket::CA, nullptr, 0);
        Platform::sleep_ms(2000);
        return false;
    }

    Platform::goto_xy(16, 4);
    Platform::puts_raw(" Opening \"" + rcv_name + "\" to Recieve file.");
    send_packet(YappPacket::RR, nullptr, 0);

    // ---- RcvData ----
    set_state(YappState::RcvData);
    long bytes_rcvd = 0;
    bool rcv_ok     = true;

    while (!aborted_) {
        if (!recv_packet(pkt, data, TIMEOUT_MS)) {
            // Timeout
            show_status(YAPP_STATUS[15], bytes_rcvd, expected_size, rcv_name, on_progress);
            rcv_ok = false;
            break;
        }
        if (pkt == YappPacket::DT) {
            ::fwrite(data.data(), 1, data.size(), f);
            bytes_rcvd += (long)data.size();
            show_status(YAPP_STATUS[8], bytes_rcvd, expected_size, rcv_name, on_progress);
            send_packet(YappPacket::RR, nullptr, 0);
        } else if (pkt == YappPacket::EF) {
            send_packet(YappPacket::RR, nullptr, 0);
            break;
        } else if (pkt == YappPacket::CA || pkt == YappPacket::CN) {
            show_status(YAPP_STATUS[14], bytes_rcvd, expected_size, rcv_name, on_progress);
            rcv_ok = false;
            break;
        } else {
            show_status(YAPP_STATUS[3], bytes_rcvd, expected_size, rcv_name, on_progress);
        }
        if (check_abort()) { rcv_ok = false; break; }
    }
    ::fclose(f);

    if (!rcv_ok || aborted_) {
        if (aborted_) send_packet(YappPacket::CA, nullptr, 0);
        Platform::file_delete(out_path);
        show_status(YAPP_STATUS[16], bytes_rcvd, expected_size, rcv_name, on_progress);
        Platform::sleep_ms(2000);
        return false;
    }

    // Wait for EOT
    recv_packet(pkt, data, TIMEOUT_MS);
    send_packet(YappPacket::RR, nullptr, 0);

    show_status(YAPP_STATUS[9], bytes_rcvd, expected_size, rcv_name, on_progress);  // FILE RECEIVED SUCCESSFULLY
    Platform::sleep_ms(2000);
    return true;
}
