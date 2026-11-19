#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "esphome/components/canbus/canbus.h"

namespace esphome::mcp251xfd {

// Values are the oscillator frequency in MHz.
enum class CanClock : uint8_t { CAN_CLOCK_4MHZ = 4, CAN_CLOCK_20MHZ = 20, CAN_CLOCK_40MHZ = 40 };

enum class CanMode : uint8_t {
  CAN_MODE_NORMAL = 0,
  CAN_MODE_LOOPBACK = 2,
  CAN_MODE_LISTENONLY = 3,
  CAN_MODE_CONFIGURATION = 4,
  CAN_MODE_CLASSIC = 6,
};

static constexpr uint8_t INSTRUCTION_RESET = 0x00;
static constexpr uint8_t INSTRUCTION_WRITE = 0x20;
static constexpr uint8_t INSTRUCTION_READ_CRC = 0xB0;

static constexpr uint16_t REG_CON = 0x000;
static constexpr uint16_t REG_NBTCFG = 0x004;
static constexpr uint16_t REG_INT = 0x01C;
static constexpr uint16_t REG_TREC = 0x034;
// FIFO 1 transmits. FIFO 2 receives. TXQ and TEF are disabled.
static constexpr uint16_t REG_TX_FIFO_CON = 0x05C;
static constexpr uint16_t REG_TX_FIFO_STA = 0x060;
static constexpr uint16_t REG_RX_FIFO_CON = 0x068;
static constexpr uint16_t REG_RX_FIFO_STA = 0x06C;
static constexpr uint16_t REG_FLTCON = 0x1D0;
static constexpr uint16_t REG_FLTOBJ0 = 0x1F0;
static constexpr uint16_t REG_OSC = 0xE00;

static constexpr uint16_t RAM_START = 0x400;
static constexpr uint16_t RAM_SIZE = 2048;
// FIFO objects are two header words and the payload, no timestamp. CLASSIC uses 8-byte payloads.
static constexpr size_t MESSAGE_HEADER_SIZE = 8;
static constexpr size_t MESSAGE_SIZE = MESSAGE_HEADER_SIZE + 64;
static constexpr size_t CLASSIC_MESSAGE_SIZE = MESSAGE_HEADER_SIZE + 8;
// The CON to TDC block written in setup is the largest register write.
static constexpr size_t MAX_REGISTER_WRITE_SIZE = 16;
// Largest object this build sends, and largest read: the header with 8 data bytes or the rest of a payload
static constexpr size_t MAX_MESSAGE_SIZE = MESSAGE_HEADER_SIZE + canbus::CAN_FRAME_DATA_LENGTH;
static constexpr size_t MAX_READ_SIZE = std::max<size_t>(MESSAGE_HEADER_SIZE + canbus::CAN_MAX_DATA_LENGTH,
                                                         canbus::CAN_FRAME_DATA_LENGTH - canbus::CAN_MAX_DATA_LENGTH);
static constexpr uint32_t TX_FIFO_DEPTH = 8;
static constexpr uint32_t RX_FIFO_DEPTH = 20;
static constexpr uint32_t CLASSIC_RX_FIFO_DEPTH = 32;
static_assert((TX_FIFO_DEPTH + RX_FIFO_DEPTH) * MESSAGE_SIZE <= RAM_SIZE);
static_assert((TX_FIFO_DEPTH + CLASSIC_RX_FIFO_DEPTH) * CLASSIC_MESSAGE_SIZE <= RAM_SIZE);

static constexpr uint32_t CON_ISO_CRC_ENABLE = 1UL << 5;
static constexpr uint8_t CON_MODE_MASK = 0x07;
static constexpr uint8_t CON_MODE_SHIFT = 5;
static constexpr uint8_t INT_SYSTEM_ERROR = 1U << 4;     // SERRIF in byte 1 of INT.
static constexpr uint8_t INT_MODE_CHANGE = 1U << 3;      // MODIF in byte 0 of INT.
static constexpr uint8_t INT_CAN_ERROR = 1U << 5;        // CERRIF in byte 1 of INT.
static constexpr uint8_t INT_INVALID_MESSAGE = 1U << 7;  // IVMIF in byte 1 of INT.
static constexpr uint8_t INT_RX_OVERFLOW = 1U << 3;      // RXOVIF in byte 1 of INT.
static constexpr uint8_t INT_RX_ENABLE = 1U << 1;        // RXIE in byte 2 of INT.
static constexpr uint32_t FIFO_PAYLOAD_8 = 0;
static constexpr uint32_t FIFO_PAYLOAD_64 = 7UL << 29;
static constexpr uint32_t FIFO_TX_ENABLE = 1UL << 7;
static constexpr uint8_t FIFO_RX_NOT_EMPTY_ENABLE = 1U << 0;
static constexpr uint8_t FIFO_RX_OVERFLOW_ENABLE = 1U << 3;
static constexpr uint8_t FIFO_NOT_FULL_NOT_EMPTY = 1U << 0;
static constexpr uint8_t FIFO_RX_OVERFLOW = 1U << 3;
static constexpr uint8_t FIFO_INCREMENT = 1U << 0;  // Byte 1 of FIFOCON.
static constexpr uint8_t FIFO_TX_REQUEST = 1U << 1;
static constexpr uint8_t FIFO_RESET = 1U << 2;
static constexpr uint8_t FILTER_ENABLE_RX_FIFO = 0x80 | 2;
static constexpr uint8_t OSC_PLL_ENABLE = 1U << 0;
static constexpr uint8_t OSC_CLKODIV_10 = 3U << 5;  // Reset value: CLKO is SYSCLK / 10.
static constexpr uint8_t OSC_PLL_READY = 1U << 0;   // Byte 1 of OSC.
static constexpr uint8_t OSC_READY = 1U << 2;
static constexpr uint8_t ERROR_RX_PASSIVE = 1U << 3;  // Byte 2 of TREC.
static constexpr uint8_t ERROR_TX_PASSIVE = 1U << 4;
static constexpr uint8_t ERROR_BUS_OFF = 1U << 5;

static constexpr uint8_t MESSAGE_DLC_MASK = 0x0F;
static constexpr uint8_t MESSAGE_IDE = 1U << 4;
static constexpr uint8_t MESSAGE_RTR = 1U << 5;
static constexpr uint8_t MESSAGE_BRS = 1U << 6;
static constexpr uint8_t MESSAGE_FDF = 1U << 7;
static constexpr uint8_t MESSAGE_ESI = 1U << 0;  // Byte 5 of the message header.
static constexpr uint32_t EXTENDED_ID_LOW_MASK = 0x3FFFF;

static constexpr uint32_t FAULT_LOG_INTERVAL_MS = 5000;
static constexpr uint32_t MODE_RECOVERY_INTERVAL_MS = 5000;

}  // namespace esphome::mcp251xfd
