#pragma once

#include <cstdint>

namespace esphome::it8951 {

struct DevInfo {
  uint16_t panel_width{0};
  uint16_t panel_height{0};
  uint16_t img_buf_addr_l{0};
  uint16_t img_buf_addr_h{0};
  uint16_t fw_version[8]{};
  uint16_t lut_version[8]{};
};

// --- IT8951 SPI packet preambles ---
static constexpr uint16_t PACKET_TYPE_CMD = 0x6000;
static constexpr uint16_t PACKET_TYPE_WRITE = 0x0000;
static constexpr uint16_t PACKET_TYPE_READ = 0x1000;

// --- Built-in I80 commands ---
static constexpr uint16_t TCON_SYS_RUN = 0x0001;
static constexpr uint16_t TCON_STANDBY = 0x0002;
static constexpr uint16_t TCON_SLEEP = 0x0003;
static constexpr uint16_t TCON_REG_RD = 0x0010;
static constexpr uint16_t TCON_REG_WR = 0x0011;

static constexpr uint16_t TCON_LD_IMG = 0x0020;
static constexpr uint16_t TCON_LD_IMG_AREA = 0x0021;
static constexpr uint16_t TCON_LD_IMG_END = 0x0022;

// --- I80 user-defined commands ---
static constexpr uint16_t I80_CMD_DPY_AREA = 0x0034;
static constexpr uint16_t I80_CMD_GET_DEV_INFO = 0x0302;
static constexpr uint16_t I80_CMD_DPY_BUF_AREA = 0x0037;
static constexpr uint16_t I80_CMD_VCOM = 0x0039;
static constexpr uint16_t I80_CMD_VCOM_READ = 0x0000;
static constexpr uint16_t I80_CMD_VCOM_WRITE = 0x0001;

// --- Pixel mode (bits per pixel encoding) ---
static constexpr uint8_t PIXEL_2BPP = 0;
static constexpr uint8_t PIXEL_3BPP = 1;
static constexpr uint8_t PIXEL_4BPP = 2;
static constexpr uint8_t PIXEL_8BPP = 3;

// --- Endian flags for LD_IMG_AREA ---
static constexpr uint8_t LDIMG_L_ENDIAN = 0;
static constexpr uint8_t LDIMG_B_ENDIAN = 1;

// --- SPI probe frequency used for initial controller handshake ---
static constexpr uint32_t SPI_PROBE_FREQUENCY = 1'000'000;

// --- Refresh modes ---
// See IT8951 datasheet for descriptions of the available waveform modes.
enum UpdateMode : uint16_t {
  UPDATE_MODE_INIT = 0,
  UPDATE_MODE_DU = 1,
  UPDATE_MODE_GC16 = 2,
  UPDATE_MODE_GL16 = 3,
  UPDATE_MODE_GLR16 = 4,
  UPDATE_MODE_GLD16 = 5,
  UPDATE_MODE_DU4 = 6,
  UPDATE_MODE_A2 = 7,
  UPDATE_MODE_NONE = 8,
};

// --- Registers ---
static constexpr uint16_t DISPLAY_REG_BASE = 0x1000;
static constexpr uint16_t UP1SR = DISPLAY_REG_BASE + 0x138;
static constexpr uint16_t LUTAFSR = DISPLAY_REG_BASE + 0x224;
static constexpr uint16_t BGVR = DISPLAY_REG_BASE + 0x250;

static constexpr uint16_t I80CPCR = 0x0004;

static constexpr uint16_t MCSR_BASE_ADDR = 0x0200;
static constexpr uint16_t LISAR = MCSR_BASE_ADDR + 0x0008;

// Display orientation flags
static constexpr uint8_t TRANSFORM_NONE = 0;
static constexpr uint8_t TRANSFORM_MIRROR_X = 1;
static constexpr uint8_t TRANSFORM_MIRROR_Y = 2;
static constexpr uint8_t TRANSFORM_SWAP_XY = 4;

}  // namespace esphome::it8951
