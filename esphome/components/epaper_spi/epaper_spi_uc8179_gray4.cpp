#include "epaper_spi_uc8179_gray4.h"

#include <algorithm>
#include <array>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

ESPHOME_LOG_TAG(TAG, "epaper_spi.uc8179_gray4");

void EPaperUC8179Gray4::setup() {
  EPaperUC8179::setup();
  // What each pixel was last driven to, for the black-and-white updates in between
  if (this->is_using_partial_update_())
    this->init_sent_frame_(this->buffer_length_);
}

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
constexpr uint8_t CMD_POWER_SETTING = 0x01;
constexpr uint8_t CMD_BOOSTER_SOFT_START = 0x06;
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

void EPaperUC8179Gray4::send_resolution_() {
  this->cmd_data(CMD_RESOLUTION, {(uint8_t) (this->width_ >> 8), (uint8_t) this->width_, (uint8_t) (this->height_ >> 8),
                                  (uint8_t) this->height_});
}

// The panel settings which follow power-on in the EPD_7in5_V2_old example, and the waveforms.
// The border takes the K-to-W waveform, so that is the one given to white.
void EPaperUC8179Gray4::send_waveforms_() {
  this->cmd_data(CMD_PANEL_SETTING, {0x3F});  // black/white mode, waveforms from registers
  this->send_resolution_();
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

// The panel's own four-level waveforms, as Waveshare's EPD_7in5_V2 example selects them: by
// forcing the temperature the OTP holds them under
void EPaperUC8179Gray4::select_stored_waveforms_() {
  this->cmd_data(CMD_BOOSTER_SOFT_START, {0x27, 0x27, 0x18, 0x17});
  this->cmd_data(CMD_CCSET, {0x02});
  this->cmd_data(CMD_TSSET, {0x5F});
}

// A black-and-white update needs the copy of what the panel shows; without it every update is a
// four-level one
bool EPaperUC8179Gray4::initialise(bool partial) {
  this->mono_push_ = partial && this->sent_valid_;
  if (!this->mono_push_)
    return EPaperUC8179::initialise(partial);
  // As the black-and-white model sets the panel up, with its waveforms from the OTP. Keep this the
  // same as UC8179.get_init_sequence() in models/uc8179.py.
  this->cmd_data(CMD_POWER_SETTING, {0x07, 0x07, 0x3F, 0x3F});
  this->cmd_data(CMD_BOOSTER_SOFT_START, {0x17, 0x17, 0x28, 0x17});
  this->cmd_data(CMD_PANEL_SETTING, {0x1F});
  this->send_resolution_();
  this->cmd_data(CMD_DUAL_SPI, {0x00});
  this->cmd_data(CMD_CDI, {0x10, 0x07});
  this->cmd_data(CMD_TCON, {0x22});
  this->command(CMD_POWER_ON);
  this->next_delay_ = 100;
  return true;
}

bool HOT EPaperUC8179Gray4::transfer_data() {
  return this->mono_push_ ? this->transfer_mono_() : this->transfer_gray_();
}

// A pixel's two bits, DTM2 then DTM1, choose its waveform.
//
// With the panel's own waveforms, as in the example: DTM2 is the level's high bit and DTM1 its
// low bit, both inverted, so white is 00 and black 11. The example's Python driver sends a lighter
// grey as level 2 and a darker one as level 1, as here.
//
// With the driver's waveforms: 01 white (K-to-W), 00 light grey (W-to-W), 10 dark grey (W-to-K)
// and 11 black (K-to-K). So DTM2 is the level's high bit inverted, and DTM1 is set where the two
// bits of the level are equal.
bool HOT EPaperUC8179Gray4::transfer_gray_() {
  const uint32_t start_time = millis();
  const size_t plane_length = this->buffer_length_ / 2;
  if (this->current_data_index_ == 0) {
    ESP_LOGV(TAG, "Four-level refresh");
    // A black-and-white update leaves the controller in partial mode, and without a reset pin
    // nothing else takes it out
    this->command(CMD_PARTIAL_OUT);
    if (this->driver_waveforms_) {
      this->send_waveforms_();
    } else {
      this->select_stored_waveforms_();
    }
  }

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  uint8_t levels[2 * MAX_TRANSFER_SIZE];
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
        const uint8_t high = gray4_plane_byte(first, second, true), low = gray4_plane_byte(first, second, false);
        if (high_bit) {
          bytes_to_send[i] = (uint8_t) ~high;
        } else {
          bytes_to_send[i] = (uint8_t) ~(this->driver_waveforms_ ? high ^ low : low);
        }
        levels[2 * i] = first;
        levels[2 * i + 1] = second;
      }
      // Every pixel is driven to its level, so that is what the panel shows from here on
      if (!high_bit)
        this->record_sent_(source, levels, 2 * bytes_to_copy);
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

namespace {

// One byte of each plane for a black-and-white update, from the levels eight pixels show and the
// levels they should have. A pixel whose level is unchanged, or which already shows the black or
// white its new level is nearer, gets the same bit in both planes and is left alone. Any other is
// told it goes from the opposite colour to that one. The shown levels are updated to match.
struct MonoPlanes {
  uint8_t old_bits;
  uint8_t new_bits;
};

MonoPlanes mono_planes(uint8_t shown[2], const uint8_t wanted[2]) {
  MonoPlanes planes{};
  for (uint8_t half = 0; half < 2; half++) {
    for (uint8_t shift = 6;; shift -= 2) {
      const uint8_t have = (shown[half] >> shift) & 0x03, want = (wanted[half] >> shift) & 0x03;
      const bool white = want >= 2;
      const uint8_t target = white ? 3 : 0;
      if (have == want || have == target) {
        const uint8_t bit = have >> 1;
        planes.old_bits = (uint8_t) (planes.old_bits << 1 | bit);
        planes.new_bits = (uint8_t) (planes.new_bits << 1 | bit);
      } else {
        planes.old_bits = (uint8_t) (planes.old_bits << 1 | !white);
        planes.new_bits = (uint8_t) (planes.new_bits << 1 | white);
        shown[half] = (uint8_t) ((shown[half] & ~(0x03 << shift)) | (target << shift));
      }
      if (shift == 0)
        break;
    }
  }
  return planes;
}

}  // namespace

// DTM1 is the image the panel is told it shows and DTM2 the new one, a set bit being white. The
// copy of the shown levels is updated as the second plane goes out, as both are worked out from it.
bool HOT EPaperUC8179Gray4::transfer_mono_() {
  const uint32_t start_time = millis();
  const size_t plane_length = this->buffer_length_ / 2;
  if (this->current_data_index_ == 0) {
    ESP_LOGV(TAG, "Black-and-white partial refresh");
    // Flipped data polarity and a floating border, the partial waveform by its forced
    // temperature, and partial mode with the window set to the full screen
    this->cmd_data(CMD_CDI, {0xA9, 0x07});
    this->cmd_data(CMD_CCSET, {0x02});
    this->cmd_data(CMD_TSSET, {0x6E});
    this->set_partial_window_(0, this->width_ - 1, 0, this->height_ - 1);
  }

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  while (this->current_data_index_ < 2 * plane_length) {
    const bool new_plane = this->current_data_index_ >= plane_length;
    const size_t offset = new_plane ? plane_length : 0;
    if (this->current_data_index_ == offset) {
      this->command(new_plane ? CMD_DTM2 : CMD_DTM1);
    }
    this->start_data_();
    while (this->current_data_index_ < offset + plane_length) {
      const size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, offset + plane_length - this->current_data_index_);
      const size_t source = (this->current_data_index_ - offset) * 2;
      for (size_t i = 0; i < bytes_to_copy; i++) {
        const size_t index = source + 2 * i;
        uint8_t shown[2] = {this->sent_[index], this->sent_[index + 1]};
        const uint8_t wanted[2] = {this->buffer_[index], this->buffer_[index + 1]};
        if (shown[0] == wanted[0] && shown[1] == wanted[1]) {
          bytes_to_send[i] = gray4_plane_byte(shown[0], shown[1], true);
          continue;
        }
        const MonoPlanes planes = mono_planes(shown, wanted);
        bytes_to_send[i] = new_plane ? planes.new_bits : planes.old_bits;
        if (new_plane) {
          this->sent_[index] = shown[0];
          this->sent_[index + 1] = shown[1];
        }
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
