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
// VCOM write selectors. Different IT8951-driven panels accept different
// selector values for the VCOM SET sub-command. Most panels (m5stack-m5paper,
// generic dev kits) accept 0x0001. Some panels — notably the Seeed
// reTerminal E1003 — only respond to selector 0x0002 and silently ignore
// 0x0001, leaving VCOM at its default and making grayscale waveforms
// (GC16/GL16) ineffective even though INIT still works.
static constexpr uint16_t I80_CMD_VCOM_WRITE = 0x0001;
static constexpr uint16_t I80_CMD_VCOM_WRITE_ALT = 0x0002;

// Force temperature command. The IT8951 selects waveform LUTs based on
// panel temperature; if it is left at the controller default, panels with
// auto-temperature disabled (notably the Seeed reTerminal E1003) will
// run waveforms against a mismatched LUT, leaving pixels visually
// unchanged even though the LUT engine completes a full cycle. The
// selector word selects the operation (0x0001 = write); the value word
// is the temperature in degrees Celsius.
static constexpr uint16_t I80_CMD_FORCE_TEMP = 0x0040;
static constexpr uint16_t I80_CMD_FORCE_TEMP_WRITE = 0x0001;
static constexpr int16_t DEFAULT_FORCE_TEMP_C = 25;

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
/*
 INIT The initialization (INIT) mode is
 used to completely erase the display and leave it in the white state. It is
 useful for situations where the display information in memory is not a faithful
 representation of the optical state of the display, for example, after the
 device receives power after it has been fully powered down. This waveform
 switches the display several times and leaves it in the white state.

 DU
 The direct update (DU) is a very fast, non-flashy update. This mode supports
 transitions from any graytone to black or white only. It cannot be used to
 update to any graytone other than black or white. The fast update time for this
 mode makes it useful for response to touch sensor or pen input or menu selection
 indictors.

 GC16
 The grayscale clearing (GC16) mode is used to update the full display and
 provide a high image quality. When GC16 is used with Full Display Update the
 entire display will update as the new image is written. If a Partial Update
 command is used the only pixels with changing graytone values will update. The
 GC16 mode has 16 unique gray levels.

 GL16
 The GL16 waveform is primarily used to update sparse content on a white
 background, such as a page of anti-aliased text, with reduced flash. The
 GL16 waveform has 16 unique gray levels.

 GLR16
 The GLR16 mode is used in conjunction with an image preprocessing algorithm to
 update sparse content on a white background with reduced flash and reduced image
 artifacts. The GLR16 mode supports 16 graytones. If only the even pixel states
 are used (0, 2, 4, … 30), the mode will behave exactly as a traditional GL16
 waveform mode. If a separately-supplied image preprocessing algorithm is used,
 the transitions invoked by the pixel states 29 and 31 are used to improve
 display quality. For the AF waveform, it is assured that the GLR16 waveform data
 will point to the same voltage lists as the GL16 data and does not need to be
 stored in a separate memory.

 GLD16
 The GLD16 mode is used in conjunction with an image preprocessing algorithm to
 update sparse content on a white background with reduced flash and reduced image
 artifacts. It is recommended to be used only with the full display update. The
 GLD16 mode supports 16 graytones. If only the even pixel states are used (0, 2,
 4, … 30), the mode will behave exactly as a traditional GL16 waveform mode. If a
 separately-supplied image preprocessing algorithm is used, the transitions
 invoked by the pixel states 29 and 31 are used to refresh the background with a
 lighter flash compared to GC16 mode following a predetermined pixel map as
 encoded in the waveform file, and reduce image artifacts even more compared to
 the GLR16 mode. For the AF waveform, it is assured that the GLD16 waveform data
 will point to the same voltage lists as the GL16 data and does not need to be
 stored in a separate memory.

 DU4
 The DU4 is a fast update time (similar to DU), non-flashy waveform. This mode
 supports transitions from any gray tone to gray tones 1,6,11,16 represented by
 pixel states [0 10 20 30]. The combination of fast update time and four gray
 tones make it useful for anti-aliased text in menus. There is a moderate
 increase in ghosting compared with GC16.

 A2
 The A2 mode is a fast, non-flash update mode designed for fast paging turning or
 simple black/white animation. This mode supports transitions from and to black
 or white only. It cannot be used to update to any graytone other than black or
 white. The recommended update sequence to transition into repeated A2 updates is
 shown in Figure 1. The use of a white image in the transition from 4-bit to
 1-bit images will reduce ghosting and improve image quality for A2 updates.
 */
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
