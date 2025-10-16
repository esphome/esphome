#pragma once

#include <stdint.h>

namespace esphome {
namespace vs1053 {

static constexpr const uint8_t SCI_CMD_READ = 0x03;   // Serial command read
static constexpr const uint8_t SCI_CMD_WRITE = 0x02;  // Serial command write

static constexpr const uint8_t SCI_REG_MODE = 0x00;        // Mode control
static constexpr const uint8_t SCI_REG_STATUS = 0x01;      // Status of VS1053b
static constexpr const uint8_t SCI_REG_BASS = 0x02;        // Built-in bass/treble control
static constexpr const uint8_t SCI_REG_CLOCKF = 0x03;      // Clock frequency + multiplier
static constexpr const uint8_t SCI_REG_DECODETIME = 0x04;  // Decode time in seconds
static constexpr const uint8_t SCI_REG_AUDATA = 0x05;      // Misc. audio data
static constexpr const uint8_t SCI_REG_WRAM = 0x06;        // RAM write/read
static constexpr const uint8_t SCI_REG_WRAMADDR = 0x07;    // Base address for RAM write/read
static constexpr const uint8_t SCI_REG_HDAT0 = 0x08;       // Stream header data 0
static constexpr const uint8_t SCI_REG_HDAT1 = 0x09;       // Stream header data 1
static constexpr const uint8_t SCI_REG_AIADDR = 0x0A;      // Start address of application
static constexpr const uint8_t SCI_REG_VOLUME = 0x0B;      // Volume control
static constexpr const uint8_t SCI_REG_AICTRL0 = 0x0C;     // Application control register 0
static constexpr const uint8_t SCI_REG_AICTRL1 = 0x0D;     // Application control register 1
static constexpr const uint8_t SCI_REG_AICTRL2 = 0x0E;     // Application control register 2
static constexpr const uint8_t SCI_REG_AICTRL3 = 0x0F;     // Application control register 3

static constexpr const uint16_t MODE_SM_RESET = 0x0004;     // Soft reset
static constexpr const uint16_t MODE_SM_CANCEL = 0x0008;    // Cancel decoding current file
static constexpr const uint16_t MODE_SM_TESTS = 0x0020;     // Allow SDI tests
static constexpr const uint16_t MODE_SM_SDINEW = 0x0800;    // VS1002 native SPI modes

static constexpr const uint16_t EXTRA_PARAMETER_VERSION = 0x0003;
static constexpr const uint16_t EXTRA_PARAMETER_ADDR = 0x1E02;

static constexpr const uint16_t PARAMETER_END_FILL_BYTE_ADDR = 0x1E06;

}  // namespace vs1053
}  // namespace esphome
