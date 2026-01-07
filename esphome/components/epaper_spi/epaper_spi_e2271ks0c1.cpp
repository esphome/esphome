#include "epaper_spi_e2271ks0c1.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.e2271ks0c1";

static inline uint8_t encode_temp(float temp_c, bool fast) {
  uint8_t ts = static_cast<uint8_t>(static_cast<int>(lroundf(temp_c)) & 0xFF);
  if (fast)
    ts |= 0x40;
  return ts;
}

bool HOT EPaperE2271KS0C1::transfer_data() {
  const uint32_t start_time = millis();
  const bool partial = this->update_count_ != 0 && this->temperature_c_ >= 0;
  const size_t buffer_length = this->buffer_length_;

  // Phase 0: Initialization (soft reset for full updates, configure registers)
  if (this->transfer_phase_ == 0) {
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

    // Initialize previous buffer on first use
    if (this->prev_.size() != buffer_length) {
      this->prev_.resize(buffer_length, 0x00);
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
    } else {
      // Full update
      this->cmd_data(CMD_PSR, PSR_DEFAULT, 2);
    }

    // Start Frame 1 transfer
    this->command(CMD_FRAME1);
    this->current_data_index_ = 0;
    this->transfer_phase_ = 1;
  }

  // Phase 1: Transfer Frame 1 data
  if (this->transfer_phase_ == 1) {
    this->start_data_();
    while (this->current_data_index_ < buffer_length) {
      // For partial updates, Frame 1 = previous data; for full updates, Frame 1 = new data
      uint8_t byte = partial ? this->prev_[this->current_data_index_] : this->buffer_[this->current_data_index_];
      this->write_byte(byte);
      this->current_data_index_++;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        this->end_data_();
        return false;  // Yield and continue next loop
      }
    }
    this->end_data_();

    // Start Frame 2 transfer
    this->command(CMD_FRAME2);
    this->current_data_index_ = 0;
    this->transfer_phase_ = 2;
  }

  // Phase 2: Transfer Frame 2 data
  if (this->transfer_phase_ == 2) {
    this->start_data_();
    while (this->current_data_index_ < buffer_length) {
      // For partial updates, Frame 2 = new data; for full updates, Frame 2 = zeros
      uint8_t byte = partial ? this->buffer_[this->current_data_index_] : 0x00;
      this->write_byte(byte);
      this->current_data_index_++;

      if (millis() - start_time > MAX_TRANSFER_TIME) {
        this->end_data_();
        return false;  // Yield and continue next loop
      }
    }
    this->end_data_();

    // Finalize
    if (partial) {
      uint8_t vcom = 0x07;
      this->cmd_data(CMD_VCOM_CDI, &vcom, 1);
    }

    // Reset for next transfer
    this->transfer_phase_ = 0;
    this->current_data_index_ = 0;
    return true;  // Transfer complete
  }

  return true;
}

void EPaperE2271KS0C1::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->command(CMD_PWR_ON);
}

void EPaperE2271KS0C1::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh");
  this->command(CMD_REFRESH);
  // Store current frame for next partial update
  const size_t buffer_length = this->buffer_length_;
  if (this->prev_.size() != buffer_length) {
    this->prev_.resize(buffer_length);
  }
  for (size_t i = 0; i < buffer_length; i++) {
    this->prev_[i] = this->buffer_[i];
  }
}

void EPaperE2271KS0C1::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->command(CMD_PWR_OFF);
}

void EPaperE2271KS0C1::deep_sleep() { ESP_LOGV(TAG, "Deep sleep"); }

}  // namespace esphome::epaper_spi
