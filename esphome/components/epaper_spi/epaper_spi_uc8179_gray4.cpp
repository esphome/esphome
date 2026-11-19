#include "epaper_spi_uc8179_gray4.h"

#include <algorithm>
#include <array>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.uc8179_gray4");

void EPaperUC8179Gray4::fill(Color color) {
  if (this->get_clipping().is_set()) {
    // Falls back to the generic per-pixel implementation for the clipped rectangle.
    EPaperBase::fill(color);
    return;
  }
  const uint8_t level = color_to_gray4(color);
  this->buffer_.fill((uint8_t) (level | (level << 2) | (level << 4) | (level << 6)));
  this->mark_all_drawn_();
}

void HOT EPaperUC8179Gray4::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  const uint8_t level = color_to_gray4(color);
  const size_t byte_position = (size_t) y * this->row_width_ + x / 4;
  const uint8_t shift = (uint8_t) (6 - 2 * (x % 4));  // most significant pixel first
  const uint8_t original = this->buffer_[byte_position];
  this->buffer_[byte_position] = (uint8_t) ((original & ~(0x03 << shift)) | (level << shift));
}

namespace {

constexpr uint8_t CMD_PANEL_SETTING = 0x00;
constexpr uint8_t CMD_DUAL_SPI = 0x15;
constexpr uint8_t CMD_LUT_VCOM = 0x20;
constexpr uint8_t CMD_LUT_WW = 0x21;
constexpr uint8_t CMD_LUT_KW = 0x22;
constexpr uint8_t CMD_LUT_WK = 0x23;
constexpr uint8_t CMD_LUT_KK = 0x24;
constexpr uint8_t CMD_TCON = 0x60;
constexpr uint8_t CMD_RESOLUTION = 0x61;
constexpr uint8_t CMD_GATE_SOURCE_START = 0x65;

// A waveform is up to seven steps of six bytes: four voltages of 2 bits each (GND, VDH or VDL),
// how many frames each is held, and how often the step repeats. VDL moves a pixel towards white and
// VDH towards black. The example gives each 15 frames to cross, about three times what it takes at
// room temperature; that margin is kept, as nothing here allows for a cold panel.
constexpr uint8_t FULL = 15;
constexpr uint8_t GAP = 1;
// Frames of VDL which take a black pixel to each grey. Found by eye on one panel: 1 frame is
// nearly black still, 3 is mid grey and 5 is nearly white.
constexpr uint8_t DARK_FRAMES = 2;
constexpr uint8_t LIGHT_FRAMES = 4;
constexpr size_t LUT_SIZE = 42;
constexpr uint8_t GND = 0, VDH = 1, VDL = 2;
constexpr uint8_t levels(uint8_t a, uint8_t b = GND, uint8_t c = GND, uint8_t d = GND) {
  return (uint8_t) (a << 6 | b << 4 | c << 2 | d);
}

using Waveform = std::array<uint8_t, LUT_SIZE>;

// To white, then to black. Every pixel is black after it.
constexpr uint8_t CLEAR_LEVELS = levels(VDL, GND, VDH, GND);

// VCOM stays at its DC level throughout
constexpr Waveform LUT_VCOM PROGMEM = {
    levels(GND), FULL, 0, 0, 0, 1, levels(GND), FULL, GAP, FULL, GAP, 1, levels(GND), FULL, 0, 0, 0, 1,
};

// Towards white for the given number of frames after clearing, and as long towards black before it
constexpr Waveform to_white(uint8_t frames) {
  const uint8_t idle = (uint8_t) (FULL - frames);
  return {
      levels(VDH), frames, idle, 0, 0, 1, CLEAR_LEVELS, FULL, GAP, FULL, GAP, 1, levels(VDL), frames, idle, 0, 0, 1,
  };
}
constexpr Waveform LUT_WHITE PROGMEM = to_white(FULL);
constexpr Waveform LUT_LIGHT PROGMEM = to_white(LIGHT_FRAMES);
constexpr Waveform LUT_DARK PROGMEM = to_white(DARK_FRAMES);

// Black needs no more after clearing: once more to black, and as long towards white before it
constexpr Waveform LUT_BLACK PROGMEM = {
    levels(VDL), FULL, 0, 0, 0, 1, CLEAR_LEVELS, FULL, GAP, FULL, GAP, 1, levels(VDH), FULL, 0, 0, 0, 1,
};

}  // namespace

void EPaperUC8179Gray4::send_waveform_(uint8_t command, const uint8_t *waveform) {
  uint8_t data[LUT_SIZE];
  progmem_memcpy(data, waveform, LUT_SIZE);
  this->cmd_data(command, data, LUT_SIZE);
}

// The panel settings which follow power-on in the EPD_7in5_V2_old example, and the waveforms.
// The border takes the K-to-W waveform, so that is the one given to white.
void EPaperUC8179Gray4::send_waveforms_() {
  this->cmd_data(CMD_PANEL_SETTING, {0x3F});  // black/white mode, waveforms from registers
  this->cmd_data(CMD_RESOLUTION, {(uint8_t) (this->width_ >> 8), (uint8_t) this->width_, (uint8_t) (this->height_ >> 8),
                                  (uint8_t) this->height_});
  this->cmd_data(CMD_DUAL_SPI, {0x00});
  this->cmd_data(CMD_CDI, {0x10, 0x00});
  this->cmd_data(CMD_TCON, {0x22});
  this->cmd_data(CMD_GATE_SOURCE_START, {0x00, 0x00, 0x00, 0x00});
  this->send_waveform_(CMD_LUT_VCOM, LUT_VCOM.data());
  this->send_waveform_(CMD_LUT_WW, LUT_LIGHT.data());
  this->send_waveform_(CMD_LUT_KW, LUT_WHITE.data());
  this->send_waveform_(CMD_LUT_WK, LUT_DARK.data());
  this->send_waveform_(CMD_LUT_KK, LUT_BLACK.data());
}

// A pixel's two bits, DTM2 then DTM1, choose its waveform: 01 white (K-to-W), 00 light grey
// (W-to-W), 10 dark grey (W-to-K) and 11 black (K-to-K). So DTM2 is the level's high bit inverted,
// and DTM1 is set where the two bits of the level are equal.
bool HOT EPaperUC8179Gray4::transfer_data() {
  const uint32_t start_time = millis();
  const size_t plane_length = this->buffer_length_ / 2;
  if (this->current_data_index_ == 0) {
    ESP_LOGV(TAG, "Four-level refresh");
    this->send_waveforms_();
  }

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  while (this->current_data_index_ < 2 * plane_length) {
    const bool high_bit = this->current_data_index_ >= plane_length;
    const size_t offset = high_bit ? plane_length : 0;
    if (this->current_data_index_ == offset) {
      this->command(high_bit ? CMD_DTM2 : CMD_DTM1);
    }
    this->start_data_();
    while (this->current_data_index_ < offset + plane_length) {
      const size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, offset + plane_length - this->current_data_index_);
      const size_t source = (this->current_data_index_ - offset) * 2;
      for (size_t i = 0; i < bytes_to_copy; i++) {
        const uint8_t first = this->buffer_[source + 2 * i], second = this->buffer_[source + 2 * i + 1];
        const uint8_t high = gray4_plane_byte(first, second, true);
        bytes_to_send[i] = (uint8_t) ~(high_bit ? high : high ^ gray4_plane_byte(first, second, false));
      }
      this->write_array(bytes_to_send, bytes_to_copy);
      this->current_data_index_ += bytes_to_copy;
      if (millis() - start_time > MAX_TRANSFER_TIME) {
        this->disable();
        return false;
      }
    }
    this->disable();
  }

  this->current_data_index_ = 0;
  return true;
}

}  // namespace esphome::epaper_spi
