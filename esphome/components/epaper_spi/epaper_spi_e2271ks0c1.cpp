#include "epaper_spi_e2271ks0c1.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.e2271ks0c1";

void EPaperE2271KS0C1::setup() {
  EPaperBase::setup();
  // Hardware reset: double-toggle sequence per datasheet
  if (this->reset_pin_ != nullptr) {
    ESP_LOGV(TAG, "Hardware reset");
    this->reset_pin_->digital_write(false);
    delay(50);
    this->reset_pin_->digital_write(true);
    delay(50);
    this->reset_pin_->digital_write(false);
    delay(50);
    this->reset_pin_->digital_write(true);
    delay(50);
  }
}

bool EPaperE2271KS0C1::is_idle() const {
  if (this->busy_pin_ == nullptr) {
    return true;
  }
  // E2271KS0C1 has active-low busy: HIGH = idle
  return this->busy_pin_->digital_read();
}

static inline uint8_t encode_temp(float temp_c, bool fast) {
  uint8_t ts = static_cast<uint8_t>(static_cast<int>(lroundf(temp_c)) & 0xFF);
  if (fast)
    ts |= 0x40;
  return ts;
}

bool EPaperE2271KS0C1::transfer_data() {
  const bool partial = this->update_count_ != 0;

  // Soft reset only on full updates (non-blocking via state machine delay)
  if (!partial && !this->soft_reset_pending_) {
    ESP_LOGV(TAG, "Transfer data, partial=%s", YESNO(partial));
    uint8_t reset_data = 0x0E;
    this->cmd_data(CMD_PSR, &reset_data, 1);
    this->soft_reset_pending_ = true;
    this->delay_until_ = millis() + 50;
    return false;  // Wait for soft reset delay
  }
  this->soft_reset_pending_ = false;
  ESP_LOGV(TAG, "Transfer data, partial=%s", YESNO(partial));

  // Build rotated buffer (90 degrees clockwise)
  // Source: 264x176, Panel expects: 176x264
  const int src_width = this->width_;
  const int src_height = this->height_;
  const int dst_width = this->height_;
  const int dst_height = this->width_;
  const size_t n = dst_width * dst_height / 8;

  if (this->tx_.size() != n) {
    this->tx_.assign(n, 0x00);
    this->prev_.assign(n, 0x00);
  }

  std::fill(this->tx_.begin(), this->tx_.end(), 0x00);

  // Rotate pixels: source(x,y) -> dest(src_height-1-y, x)
  for (int sy = 0; sy < src_height; sy++) {
    for (int sx = 0; sx < src_width; sx++) {
      int src_byte = sy * (src_width / 8) + sx / 8;
      int src_bit = 7 - (sx % 8);
      if ((this->buffer_[src_byte] >> src_bit) & 1) {
        int dx = src_height - 1 - sy;
        int dy = sx;
        int dst_byte = dy * (dst_width / 8) + dx / 8;
        int dst_bit = 7 - (dx % 8);
        this->tx_[dst_byte] |= (1 << dst_bit);
      }
    }
  }

  // Temperature configuration
  uint8_t ts = encode_temp(this->temperature_c_, partial);
  this->cmd_data(CMD_INPUT_TEMP, &ts, 1);

  uint8_t at = 0x02;
  this->cmd_data(CMD_ACTIVE_TEMP, &at, 1);

  if (partial) {
    // Fast/partial update
    uint8_t fast_psr[2] = {static_cast<uint8_t>(PSR_DEFAULT[0] | 0x10), static_cast<uint8_t>(PSR_DEFAULT[1] | 0x02)};
    this->cmd_data(CMD_PSR, fast_psr, 2);

    uint8_t border = 0x27;
    this->cmd_data(CMD_VCOM_CDI, &border, 1);
    this->cmd_data(CMD_VCOM_CDI, &border, 1);

    // Frame 1 = previous, Frame 2 = new
    this->command(CMD_FRAME1);
    this->start_data_();
    this->write_array(this->prev_.data(), n);
    this->end_data_();

    this->command(CMD_FRAME2);
    this->start_data_();
    this->write_array(this->tx_.data(), n);
    this->end_data_();

    uint8_t vcom = 0x07;
    this->cmd_data(CMD_VCOM_CDI, &vcom, 1);
    this->cmd_data(CMD_VCOM_CDI, &vcom, 1);
  } else {
    // Full update
    this->cmd_data(CMD_PSR, PSR_DEFAULT, 2);

    // Frame 1 = new, Frame 2 = zeros
    this->command(CMD_FRAME1);
    this->start_data_();
    this->write_array(this->tx_.data(), n);
    this->end_data_();

    this->command(CMD_FRAME2);
    this->start_data_();
    for (size_t i = 0; i < n; i++)
      this->write_byte(0x00);
    this->end_data_();
  }

  return true;
}

void EPaperE2271KS0C1::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->command(CMD_PWR_ON);
  this->command(CMD_PWR_ON);
}

void EPaperE2271KS0C1::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh");
  this->command(CMD_REFRESH);
  this->command(CMD_REFRESH);
  // Store current frame for next partial update
  this->prev_ = this->tx_;
}

void EPaperE2271KS0C1::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->command(CMD_PWR_OFF);
  this->command(CMD_PWR_OFF);
}

void EPaperE2271KS0C1::deep_sleep() { ESP_LOGV(TAG, "Deep sleep"); }

}  // namespace esphome::epaper_spi
