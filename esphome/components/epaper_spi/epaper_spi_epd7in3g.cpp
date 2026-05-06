#include "epaper_spi_epd7in3g.h"

#include "colorconv.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static const char *const TAG = "epaper_spi.epd7in3g";

void EPaperEpd7In3G::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);
    return;
  }
  const uint8_t p =
      color_to_bwyr(color, UINT8_C(0), UINT8_C(1), UINT8_C(2), UINT8_C(3));
  const unsigned pu = static_cast<unsigned>(p);
  const uint8_t packed = static_cast<uint8_t>(pu << 6 | pu << 4 | pu << 2 | pu);
  this->buffer_.fill(packed);
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
  this->x_low_ = 0;
  this->y_low_ = 0;
}

void EPaperEpd7In3G::clear() { this->fill(COLOR_ON); }

void HOT EPaperEpd7In3G::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;
  const uint8_t p =
      color_to_bwyr(color, UINT8_C(0), UINT8_C(1), UINT8_C(2), UINT8_C(3));
  const uint16_t row_bytes = this->width_ / 4;
  const uint32_t idx = static_cast<uint32_t>(y) * row_bytes + static_cast<uint32_t>(x >> 2);
  const uint8_t shift = static_cast<uint8_t>((3 - (x & 3)) * 2);
  const auto orig = this->buffer_[idx];
  this->buffer_[idx] = static_cast<uint8_t>(
      (orig & static_cast<uint8_t>(~(0x03U << shift))) | static_cast<uint8_t>(static_cast<unsigned>(p) << shift));
}

// State machine calls power_on after transfer; epd7in3g expects booster soft-start before RAM write.
void EPaperEpd7In3G::power_on() {}

bool HOT EPaperEpd7In3G::transfer_data() {
  if (this->xfer_phase_ == 0) {
    ESP_LOGV(TAG, "Power on");
    this->command(this->CMD_BOOSTER_SOFTSTART);
    this->xfer_phase_ = 1;
    return false;
  }
  if (this->xfer_phase_ == 1) {
    if (!this->is_idle_()) {
      App.feed_wdt();
      return false;
    }
    delay(200);  // NOLINT
    this->xfer_phase_ = 2;
    return false;
  }

  const uint32_t start_time = App.get_loop_component_start_time();
  const size_t buffer_length = this->buffer_length_;
  if (this->current_data_index_ == 0) {
    ESP_LOGV(TAG, "Sending data to the display");
    this->command(this->CMD_TRANSFER);
  }

  size_t buf_idx = 0;
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  while (this->current_data_index_ != buffer_length) {
    bytes_to_send[buf_idx++] = this->buffer_[this->current_data_index_++];

    if (buf_idx == sizeof bytes_to_send) {
      this->start_data_();
      this->write_array(bytes_to_send, buf_idx);
      this->disable();
      ESP_LOGV(TAG, "Wrote %u bytes at %ums", static_cast<unsigned>(buf_idx),
               static_cast<unsigned>(millis()));
      buf_idx = 0;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        return false;
      }
    }
  }
  if (buf_idx != 0) {
    this->start_data_();
    this->write_array(bytes_to_send, buf_idx);
    this->disable();
  }
  this->current_data_index_ = 0;
  this->xfer_phase_ = 0;
  return true;
}

void EPaperEpd7In3G::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh");
  (void) partial;
  this->cmd_data(this->CMD_REFRESH, {this->DATA_REFRESH});
}

void EPaperEpd7In3G::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->cmd_data(this->CMD_POWEROFF, {this->DATA_POWEROFF});
}

void EPaperEpd7In3G::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(this->CMD_DEEPSLEEP, {this->DATA_DEEPSLEEP_KEY});
}

}  // namespace esphome::epaper_spi
