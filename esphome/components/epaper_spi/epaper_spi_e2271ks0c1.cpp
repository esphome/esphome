#include "epaper_spi_e2271ks0c1.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.e2271ks0c1";

bool EPaperE2271KS0C1::is_idle_() const {
  if (this->busy_pin_ == nullptr) {
    ESP_LOGD(TAG, "No busy pin configured, assuming idle");
    return true;
  }
  bool pin = this->busy_pin_->digital_read();
  ESP_LOGD(TAG, "Busy pin read: %d", pin);
  return pin;  // HIGH = idle (active-low busy pin)
}

static inline uint8_t encode_temp_tsset(float temp_c, bool fast) {
  int t = static_cast<int>(lroundf(temp_c));
  uint8_t ts = static_cast<uint8_t>(t & 0xFF);  // 2's complement via cast
  if (fast)
    ts |= 0x40;
  return ts;
}

bool EPaperE2271KS0C1::reset() {
  // Hardware reset: double-toggle sequence per datasheet
  // Only do full reset on first update; skip on subsequent updates
  if (this->reset_pin_ == nullptr) {
    return true;
  }

  if (!this->initialized_) {
    ESP_LOGD(TAG, "Hardware reset sequence (first update)");
    this->reset_pin_->digital_write(false);
    delay(50);
    this->reset_pin_->digital_write(true);
    delay(50);
    this->reset_pin_->digital_write(false);
    delay(50);
    this->reset_pin_->digital_write(true);
    delay(50);
    this->initialized_ = true;
  }

  return true;
}

bool EPaperE2271KS0C1::transfer_data() {
  ESP_LOGD(TAG, "transfer_data() starting");

  // Determine if this is a fast (partial) update or full update
  // update_count_ == 0 means full update, otherwise partial
  // (base class manages update_count_ and increments after refresh_screen)
  const bool partial = this->update_count_ != 0;
  const bool fast = partial;
  ESP_LOGD(TAG, "Update mode: %s (count=%u, full_every=%u)", fast ? "FAST" : "FULL", this->update_count_,
           this->full_update_every_);

  // Soft reset only on full updates (causes flash if done every time)
  if (!partial) {
    uint8_t reset_data = 0x0E;
    this->cmd_data(ADDR_PSR, &reset_data, 1);
    delay(50);
  }

  // Build TX buffer with 90-degree clockwise rotation
  // Source buffer: 264 wide x 176 tall (as reported to ESPHome)
  // Panel expects: 176 wide x 264 tall
  // Rotation: source(x,y) -> dest(src_height-1-y, x)
  const int src_width = this->width_;
  const int src_height = this->height_;
  const int dst_width = this->height_;
  const int dst_height = this->width_;
  const size_t n = static_cast<size_t>(dst_width * dst_height / 8);  // 5808 bytes

  if (this->tx_.size() != n) {
    this->tx_.assign(n, 0x00);
    this->prev_.assign(n, 0x00);
  }

  // Clear tx buffer to white (0 = white on panel)
  std::fill(this->tx_.begin(), this->tx_.end(), 0x00);

  // Rotate and copy pixels from ESPHome buffer to panel format
  // COLOR_ON (1 in buffer) = black on screen
  // COLOR_OFF (0 in buffer) = white on screen
  for (int sy = 0; sy < src_height; sy++) {
    for (int sx = 0; sx < src_width; sx++) {
      // Get source pixel
      int src_byte = sy * (src_width / 8) + sx / 8;
      int src_bit = 7 - (sx % 8);
      bool pixel_on = (this->buffer_[src_byte] >> src_bit) & 1;

      // Calculate destination position (90 deg clockwise)
      int dx = src_height - 1 - sy;
      int dy = sx;

      // Set destination pixel: 1 = black on panel
      int dst_byte = dy * (dst_width / 8) + dx / 8;
      int dst_bit = 7 - (dx % 8);
      if (pixel_on) {
        this->tx_[dst_byte] |= (1 << dst_bit);
      }
    }
  }

  // Temperature and panel configuration
  uint8_t ts = encode_temp_tsset(this->temperature_c_, fast);
  this->cmd_data(ADDR_INPUT_TEMP, &ts, 1);

  uint8_t at = 0x02;
  this->cmd_data(ADDR_ACTIVE_TEMP, &at, 1);

  // PSR (Panel Settings Register) - different settings for fast vs full update
  if (fast) {
    // PSR with fast update flags
    uint8_t fast_psr[2] = {static_cast<uint8_t>(psr_[0] | 0x10), static_cast<uint8_t>(psr_[1] | 0x02)};
    this->cmd_data(ADDR_PSR, fast_psr, 2);

    // Set border to white BEFORE frame data (send twice per datasheet)
    uint8_t border_setting = 0x27;
    this->cmd_data(ADDR_VCOM_CDI, &border_setting, 1);
    this->cmd_data(ADDR_VCOM_CDI, &border_setting, 1);

    // Fast update: Frame 1 = PREVIOUS image, Frame 2 = NEW image
    this->command(ADDR_FRAME1);
    this->start_data_();
    this->write_array(this->prev_.data(), n);
    this->end_data_();

    this->command(ADDR_FRAME2);
    this->start_data_();
    this->write_array(this->tx_.data(), n);
    this->end_data_();

    // Set CDI AFTER frame data (send twice per datasheet)
    uint8_t vcom_setting = 0x07;
    this->cmd_data(ADDR_VCOM_CDI, &vcom_setting, 1);
    this->cmd_data(ADDR_VCOM_CDI, &vcom_setting, 1);
  } else {
    // Normal/full update
    this->cmd_data(ADDR_PSR, psr_, 2);

    // Full update: Frame 1 = NEW image, Frame 2 = zeros
    this->command(ADDR_FRAME1);
    this->start_data_();
    this->write_array(this->tx_.data(), n);
    this->end_data_();

    this->command(ADDR_FRAME2);
    this->start_data_();
    for (size_t i = 0; i < n; i++)
      this->write_byte(0x00);
    this->end_data_();
  }

  ESP_LOGD(TAG, "transfer_data() complete");
  return true;
}

void EPaperE2271KS0C1::power_on() {
  ESP_LOGD(TAG, "power_on()");
  // Send power on command (twice per datasheet)
  this->command(ADDR_PWR_ON);
  this->command(ADDR_PWR_ON);
}

void EPaperE2271KS0C1::refresh_screen(bool partial) {
  ESP_LOGD(TAG, "refresh_screen(partial=%s)", partial ? "true" : "false");
  // Send refresh command (twice per datasheet)
  this->command(ADDR_REFRESH);
  this->command(ADDR_REFRESH);

  // Store current frame for next fast update
  this->prev_ = this->tx_;
}

void EPaperE2271KS0C1::power_off() {
  ESP_LOGD(TAG, "power_off()");
  // Send power off command (twice per datasheet)
  this->command(ADDR_PWR_OFF);
  this->command(ADDR_PWR_OFF);
}

void EPaperE2271KS0C1::deep_sleep() {
  // Optional: enter deep sleep mode for minimum power consumption
  // For now, just log completion - display is already powered off
  ESP_LOGD(TAG, "deep_sleep() - update cycle complete");
}

}  // namespace esphome::epaper_spi
