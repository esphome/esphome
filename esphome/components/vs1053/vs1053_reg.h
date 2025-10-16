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

static constexpr const uint16_t GPIO_DDR = 0xC017;    // Direction
static constexpr const uint16_t GPIO_IDATA = 0xC018;  // Values read from pins
static constexpr const uint16_t GPIO_ODATA = 0xC019;  // Values set to the pins

static constexpr const uint16_t INT_ENABLE = 0xC01A;  // Interrupt enable

static constexpr const uint16_t MODE_SM_DIFF = 0x0001;      // Differential, 0: normal in-phase audio, 1: left channel inverted
static constexpr const uint16_t MODE_SM_LAYER12 = 0x0002;   // Allow MPEG layers I & II
static constexpr const uint16_t MODE_SM_RESET = 0x0004;     // Soft reset
static constexpr const uint16_t MODE_SM_CANCEL = 0x0008;    // Cancel decoding current file
static constexpr const uint16_t MODE_SM_EARSPKLO = 0x0010;  // EarSpeaker low setting
static constexpr const uint16_t MODE_SM_TESTS = 0x0020;     // Allow SDI tests
static constexpr const uint16_t MODE_SM_STREAM = 0x0040;    // Stream mode
static constexpr const uint16_t MODE_SM_SDINEW = 0x0800;    // VS1002 native SPI modes
static constexpr const uint16_t MODE_SM_ADPCM = 0x1000;     // PCM/ADPCM recording active
static constexpr const uint16_t MODE_SM_LINE1 = 0x4000;     // MIC/LINE1 selector, 0: MICP, 1: LINE1
static constexpr const uint16_t MODE_SM_CLKRANGE = 0x8000;  // Input clock range, 0: 12..13 MHz, 1: 24..26 MHz

static constexpr const uint16_t EXTRA_PARAMETER_VERSION = 0x0003;
static constexpr const uint16_t EXTRA_PARAMETER_ADDR = 0x1E02;

static constexpr const uint16_t PARAMETER_END_FILL_BYTE_ADDR = 0x1E06;

typedef struct extra_parameters_t {
  uint16_t version;         // 0x1E02 - Structure version
  uint16_t config1;         // 0x1E03 - PPSs RRRR: PS mode, SBR mode, Reverb
  uint16_t play_speed;      // 0x1E04 - 0,1 = Normal speed; 2 = Twice; 3 = Three times, etc.
  uint16_t byte_rate;       // 0x1E05 - Average byte rate
  uint16_t end_fill_byte;   // 0x1E06 - Byte value to send after file sent
  uint16_t reserved[16];    // 0x1E07–0x1E15 - File byte offsets
  uint32_t jump_points[8];  // 0x1E16–0x1E25 - File byte offsets
  uint16_t latest_jump;     // 0x1E26 - Index of the last updated jump point
  uint32_t position_ms;     // 0x1E27–0x1E28 - Play position, if known (WMA, Ogg Vorbis)
  int16_t resync;           // 0x1E29 - >0 for automatic M4A, ADIF, WMA resyncs
  union {
    struct {
      uint32_t cur_packet_size;  // 0x1E2A - Current packet size
      uint32_t packet_size;      // 0x1E2B - Packet size
    } wma;
    struct {
      uint16_t sce_found_mask;     // 0x1E2A - SCEs found since last clear
      uint16_t cpe_found_mask;     // 0x1E2B - CPEs found since last clear
      uint16_t lfe_found_mask;     // 0x1E2C - LFEs found since last clear
      uint16_t play_select;        // 0x1E2D - 0 = First any, initialized at AAC init
      int16_t dyn_compress;        // 0x1E2E - -8192 = 1.0, initialized at AAC init
      int16_t dyn_boost;           // 0x1E2F - 8192 = 1.0, initialized at AAC init
      uint16_t sbr_and_ps_status;  // 0x1E30 - 1 = SBR, 2 = Upsample, 4 = PS, 8 = PS active
    } aac;
    struct {
      uint32_t bytes_left;  // 0x1E2A - Bytes left
    } midi;
    struct {
      int16_t gain;  // 0x1E2A - Proposed gain offset in 0.5 dB steps, default = -12
    } vorbis;
  } codec;
} extra_parameters_t;

}  // namespace vs1053
}  // namespace esphome
