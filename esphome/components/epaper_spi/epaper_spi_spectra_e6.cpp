#include "epaper_spi_spectra_e6.h"
#include "colorconv.h"

#include <utility>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {
ESPHOME_LOG_TAG(TAG, "epaper_spi.6c");

// Native hardware color codes for this panel's 4-bit color values.
enum E6Color : uint8_t {
  BLACK = 0,
  WHITE = 1,
  YELLOW = 2,
  RED = 3,
  BLUE = 5,
  GREEN = 6,
};

uint8_t EPaperSpectraE6::color_to_native(Color color) {
  return color_to_bwyrgb<uint8_t>(color, BLACK, WHITE, YELLOW, RED, GREEN, BLUE);
}

void EPaperSpectraE6::update() {
  // A normal update is always a full refresh. Drop a stale partial request only
  // when idle: clearing it mid-transfer would turn an in-flight partial into a
  // full transfer.
  if (this->state_ == EPaperState::IDLE) {
    this->partial_active_ = false;
  }
  EPaperBase::update();
}

bool EPaperSpectraE6::partial_update(int x, int y, int w, int h) {
  if (this->state_ != EPaperState::IDLE) {
    ESP_LOGW(TAG, "Partial update rejected: display busy");
    return false;
  }
  if (!this->full_baseline_done_) {
    ESP_LOGW(TAG, "Partial update rejected: show a full update first");
    return false;
  }
  if (w <= 0 || h <= 0) {
    return false;
  }
  // Clip the logical rectangle to the canvas (rotation-aware size).
  const int log_w = this->get_width();
  const int log_h = this->get_height();
  int x0 = x < 0 ? 0 : x;
  int y0 = y < 0 ? 0 : y;
  int x1 = x + w > log_w ? log_w : x + w;
  int y1 = y + h > log_h ? log_h : y + h;
  if (x1 <= x0 || y1 <= y0) {
    return false;
  }
  // Map to native (physical) pixels with the same affine transform as drawing.
  int px = x0;
  int py = y0;
  int pw = x1 - x0;
  int ph = y1 - y0;
  if (this->effective_transform_ & SWAP_XY) {
    std::swap(px, py);
    std::swap(pw, ph);
  }
  if (this->effective_transform_ & MIRROR_X) {
    px = this->width_ - px - pw;
  }
  if (this->effective_transform_ & MIRROR_Y) {
    py = this->height_ - py - ph;
  }
  // Widen physical X outward to the 4-pixel controller granularity.
  int ax0 = px & ~3;
  int ax1 = (px + pw + 3) & ~3;
  if (ax0 < 0) {
    ax0 = 0;
  }
  if (ax1 > this->width_) {
    ax1 = this->width_;
  }
  if (py < 0) {
    ph += py;
    py = 0;
  }
  if (py + ph > this->height_) {
    ph = this->height_ - py;
  }
  if (ax1 <= ax0 || ph <= 0) {
    return false;
  }
  this->partial_x_ = (uint16_t) ax0;
  this->partial_y_ = (uint16_t) py;
  this->partial_w_ = (uint16_t) (ax1 - ax0);
  this->partial_h_ = (uint16_t) ph;
  this->partial_active_ = true;
  ESP_LOGD(TAG, "Partial update %dx%d at (%d,%d)", (int) this->partial_w_, (int) this->partial_h_,
           (int) this->partial_x_, (int) this->partial_y_);
  EPaperBase::update();
  return true;
}

void EPaperSpectraE6::send_partial_window_() {
  const uint16_t x_start = this->partial_x_;
  const uint16_t x_end = (uint16_t) (this->partial_x_ + this->partial_w_ - 1);
  const uint16_t y_start = this->partial_y_;
  // y_end is inclusive (Good Display port geometry); GxEPD2 sends y + h here
  // per its "controller issue (one more)" comment.
  const uint16_t y_end = (uint16_t) (this->partial_y_ + this->partial_h_ - 1);
  const uint8_t payload[9] = {(uint8_t) (x_start >> 8), (uint8_t) (x_start & 0xFF), (uint8_t) (x_end >> 8),
                              (uint8_t) (x_end & 0xFF), (uint8_t) (y_start >> 8),   (uint8_t) (y_start & 0xFF),
                              (uint8_t) (y_end >> 8),   (uint8_t) (y_end & 0xFF),   0x01};
  this->cmd_data(0x83, payload, sizeof(payload));
}

bool EPaperSpectraE6::transfer_data() {
  if (!this->partial_active_) {
    return EPaper4bpp::transfer_data();
  }
  const uint32_t start_time = App.get_loop_component_start_time();
  const size_t row_bytes = (size_t) this->partial_w_ / 2;
  const size_t total = row_bytes * (size_t) this->partial_h_;
  const size_t full_row_bytes = (size_t) this->width_ / 2;
  const size_t base = (size_t) this->partial_y_ * full_row_bytes + (size_t) this->partial_x_ / 2;
  if (this->current_data_index_ == 0) {
    this->send_partial_window_();
    this->command(CMD_TRANSFER_DATA);
  }
  size_t buf_idx = 0;
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  while (this->current_data_index_ != total) {
    const size_t idx = this->current_data_index_++;
    const size_t row = idx / row_bytes;
    const size_t col = idx % row_bytes;
    bytes_to_send[buf_idx++] = this->buffer_[base + row * full_row_bytes + col];
    if (buf_idx == sizeof(bytes_to_send)) {
      this->start_data_();
      this->write_array(bytes_to_send, buf_idx);
      this->disable();
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
  return true;
}

void EPaperSpectraE6::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->command(0x04);
}

void EPaperSpectraE6::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->cmd_data(0x02, {0x00});
}

void EPaperSpectraE6::refresh_screen(bool) {
  if (this->partial_active_) {
    ESP_LOGV(TAG, "Partial refresh");
    this->send_partial_window_();
    // Float the border/CDI for a region refresh.
    this->cmd_data(0x50, {0xFF});
    this->cmd_data(0x12, {0x00});
    // Allow BUSY_N to assert before the base busy-wait (vendor port uses 20 ms).
    this->next_delay_ = 20;
    // Partial refreshes must not consume the shared full_update_every cadence.
    if (this->full_update_every_ > 1) {
      this->update_count_ = (uint8_t) ((this->update_count_ + this->full_update_every_ - 1) % this->full_update_every_);
    }
    this->partial_active_ = false;
    return;
  }
  ESP_LOGV(TAG, "Refresh");
  this->cmd_data(0x12, {0x00});
  this->full_baseline_done_ = true;
}

void EPaperSpectraE6::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x07, {0xA5});
}
}  // namespace esphome::epaper_spi
