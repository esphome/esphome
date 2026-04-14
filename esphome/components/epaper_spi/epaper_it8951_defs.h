#pragma once

#include <cstdint>

namespace esphome {
namespace epaper_spi {

struct IT8951DevInfo {
  uint16_t panel_width{0};
  uint16_t panel_height{0};
  uint16_t img_buf_addr_l{0};
  uint16_t img_buf_addr_h{0};
  uint16_t fw_version[8]{};
  uint16_t lut_version[8]{};
};

// --- IT8951 Command defines ---

// Packet types
static constexpr uint16_t IT8951_PACKET_TYPE_CMD = 0x6000;
static constexpr uint16_t IT8951_PACKET_TYPE_WRITE = 0x0000;
static constexpr uint16_t IT8951_PACKET_TYPE_READ = 0x1000;

// Built in I80 Command Code
static constexpr uint16_t IT8951_TCON_SYS_RUN = 0x0001;
static constexpr uint16_t IT8951_TCON_STANDBY = 0x0002;
static constexpr uint16_t IT8951_TCON_SLEEP = 0x0003;
static constexpr uint16_t IT8951_TCON_REG_RD = 0x0010;
static constexpr uint16_t IT8951_TCON_REG_WR = 0x0011;

static constexpr uint16_t IT8951_TCON_MEM_BST_RD_T = 0x0012;
static constexpr uint16_t IT8951_TCON_MEM_BST_RD_S = 0x0013;
static constexpr uint16_t IT8951_TCON_MEM_BST_WR = 0x0014;
static constexpr uint16_t IT8951_TCON_MEM_BST_END = 0x0015;

static constexpr uint16_t IT8951_TCON_LD_IMG = 0x0020;
static constexpr uint16_t IT8951_TCON_LD_IMG_AREA = 0x0021;
static constexpr uint16_t IT8951_TCON_LD_IMG_END = 0x0022;

// I80 User defined command code
static constexpr uint16_t IT8951_I80_CMD_DPY_AREA = 0x0034;
static constexpr uint16_t IT8951_I80_CMD_GET_DEV_INFO = 0x0302;
static constexpr uint16_t IT8951_I80_CMD_DPY_BUF_AREA = 0x0037;
static constexpr uint16_t IT8951_I80_CMD_VCOM = 0x0039;
static constexpr uint16_t IT8951_I80_CMD_VCOM_READ = 0x0000;
static constexpr uint16_t IT8951_I80_CMD_VCOM_WRITE = 0x0001;

// --- IT8951 Mode defines ---

// Pixel mode (Bit per Pixel)
static constexpr uint8_t IT8951_2BPP = 0;
static constexpr uint8_t IT8951_3BPP = 1;
static constexpr uint8_t IT8951_4BPP = 2;
static constexpr uint8_t IT8951_8BPP = 3;

// Endian Type
static constexpr uint8_t IT8951_LDIMG_L_ENDIAN = 0;
static constexpr uint8_t IT8951_LDIMG_B_ENDIAN = 1;

// Default VCOM value (mV)
static constexpr uint16_t IT8951_DEFAULT_VCOM = 2300;

// SPI probe frequency for IT8951 for configuration only.
static constexpr uint32_t IT8951_SPI_PROBE_FREQUENCY = 1'000'000;

/*-----------------------------------------------------------------------
 Refresh mode description
------------------------------------------------------------------------
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

// Update modes matching IT8951 driver options
enum UpdateModeE {
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

// --- IT8951 Register defines ---

// Register Base Address
static constexpr uint16_t IT8951_DISPLAY_REG_BASE = 0x1000;

// Base Address of Basic LUT Registers
static constexpr uint16_t IT8951_LUT0EWHR = IT8951_DISPLAY_REG_BASE + 0x00;   // LUT0 Engine Width Height
static constexpr uint16_t IT8951_LUT0XYR = IT8951_DISPLAY_REG_BASE + 0x40;    // LUT0 XY
static constexpr uint16_t IT8951_LUT0BADDR = IT8951_DISPLAY_REG_BASE + 0x80;  // LUT0 Base Address
static constexpr uint16_t IT8951_LUT0MFN = IT8951_DISPLAY_REG_BASE + 0xC0;    // LUT0 Mode and Frame number
static constexpr uint16_t IT8951_LUT01AF = IT8951_DISPLAY_REG_BASE + 0x114;   // LUT0 and LUT1 Active Flag

// Update Parameter Setting Registers
static constexpr uint16_t IT8951_UP0SR = IT8951_DISPLAY_REG_BASE + 0x134;      // Update Parameter0 Setting
static constexpr uint16_t IT8951_UP1SR = IT8951_DISPLAY_REG_BASE + 0x138;      // Update Parameter1 Setting
static constexpr uint16_t IT8951_LUT0ABFRV = IT8951_DISPLAY_REG_BASE + 0x13C;  // LUT0 Alpha blend and Fill rect
static constexpr uint16_t IT8951_UPBBADDR = IT8951_DISPLAY_REG_BASE + 0x17C;   // Update Buffer Base Address
static constexpr uint16_t IT8951_LUT0IMXY = IT8951_DISPLAY_REG_BASE + 0x180;   // LUT0 Image buffer X/Y offset
static constexpr uint16_t IT8951_LUTAFSR = IT8951_DISPLAY_REG_BASE + 0x224;    // LUT Status (All LUT Engines)
static constexpr uint16_t IT8951_BGVR = IT8951_DISPLAY_REG_BASE + 0x250;       // Bitmap (1bpp) color table

// System Registers
static constexpr uint16_t IT8951_I80CPCR = 0x0004;

// Memory Converter Registers
static constexpr uint16_t IT8951_MCSR_BASE_ADDR = 0x0200;
static constexpr uint16_t IT8951_MCSR = IT8951_MCSR_BASE_ADDR + 0x0000;
static constexpr uint16_t IT8951_LISAR = IT8951_MCSR_BASE_ADDR + 0x0008;

}  // namespace epaper_spi
}  // namespace esphome
