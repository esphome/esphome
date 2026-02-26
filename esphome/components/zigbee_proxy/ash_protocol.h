#pragma once

#include <cstdint>
#include <cstddef>

namespace esphome::zigbee_proxy {

// ASH Protocol Constants
static constexpr uint8_t ASH_FLAG_BYTE = 0x7E;        // Frame delimiter
static constexpr uint8_t ASH_ESCAPE_BYTE = 0x7D;      // Escape/substitution byte
static constexpr uint8_t ASH_XOR_BYTE = 0x20;         // XOR mask for escaped bytes
static constexpr uint8_t ASH_SUBSTITUTE_BYTE = 0x18;  // Substitution for invalid bytes

// Reserved bytes that must be escaped
static constexpr uint8_t ASH_RESERVED_BYTES[] = {0x7E, 0x7D, 0x11, 0x13, 0x93, 0xA3};

// Buffer size configuration
#ifdef ZIGBEE_PROXY_BUFFER_SIZE
static constexpr size_t MAX_ASH_FRAME_SIZE = ZIGBEE_PROXY_BUFFER_SIZE;
#else
#ifdef USE_ESP8266
static constexpr size_t MAX_ASH_FRAME_SIZE = 512;  // Limited RAM on ESP8266
#else
static constexpr size_t MAX_ASH_FRAME_SIZE = 1024;  // Full buffer on ESP32/RP2040
#endif
#endif

// Protocol limits
static constexpr uint8_t ASH_MAX_SEQUENCE = 7;       // 3-bit sequence number (0-7)
static constexpr uint8_t ASH_TX_WINDOW_SIZE = 1;     // Only 1 unacknowledged frame allowed
static constexpr uint8_t ASH_MAX_RETRIES = 5;        // Maximum retransmission attempts
static constexpr uint16_t ASH_CRC_INIT = 0xFFFF;     // CRC-CCITT initial value
static constexpr uint32_t ASH_RESET_TIMEOUT = 3000;  // RST/RSTACK timeout in milliseconds

// IEEE address size
static constexpr size_t ZIGBEE_IEEE_ADDR_SIZE = 8;  // 64-bit IEEE address

// ASH Frame Types (encoded in control byte)
// DATA format:    0ffrPPPP - bit 7=0, bits 6-4=frmNum, bit 3=reTx, bits 2-0=ackNum
// ACK/NAK format: 10XnrPPP - bit 5 distinguishes ACK(0) from NAK(1)
enum class AshFrameType : uint8_t {
  DATA = 0x00,    // Data frame (bit 7 = 0)
  ACK = 0x80,     // Acknowledge frame (100nrPPP, bit 5 = 0)
  NAK = 0xA0,     // Negative acknowledge (101nrPPP, bit 5 = 1)
  RST = 0xC0,     // Reset request (bits 7-6 = 11, bits 2-0 = 000)
  RSTACK = 0xC1,  // Reset acknowledgment (bits 7-6 = 11, bits 2-0 = 001)
  ERROR = 0xC2,   // Error indication (bits 7-6 = 11, bits 2-0 = 010)
};

// ASH Connection State
enum class AshState : uint8_t {
  DISCONNECTED,  // Initial state, no connection
  CONNECTING,    // Sent RST, waiting for RSTACK
  CONNECTED,     // Normal operation
  FAILED,        // Too many errors/timeouts, requires reset
};

// Frame Parsing State Machine
enum class ParsingState : uint8_t {
  WAIT_FLAG_START,  // Looking for frame start FLAG (0x7E)
  WAIT_CONTROL,     // Reading control byte
  WAIT_DATA,        // Reading data payload
  WAIT_CRC_HIGH,    // Reading CRC high byte
  WAIT_CRC_LOW,     // Reading CRC low byte
  WAIT_FLAG_END,    // Expecting end FLAG (0x7E)
};

// Bootloader detection states
enum class BootloaderState : uint8_t {
  NORMAL,    // Normal operation
  DETECTED,  // Bootloader mode detected
  MENU,      // In bootloader menu
};

// EZSP Error Codes (from ERROR frame)
enum class EzspError : uint8_t {
  VERSION_NOT_SET = 0x00,
  RESET_UNKNOWN = 0x01,
  RESET_EXTERNAL = 0x02,
  RESET_POWER_ON = 0x03,
  RESET_WATCHDOG = 0x04,
  RESET_ASSERT = 0x05,
  RESET_BOOTLOADER = 0x06,
  RESET_SOFTWARE = 0x07,
  EXCEEDED_MAXIMUM_ACK_TIMEOUT_COUNT = 0x51,
};

}  // namespace esphome::zigbee_proxy
