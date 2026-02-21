#include "epaper_spi_e2271ks0c1.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {
static constexpr const char *const TAG = "epaper_spi.e2271ks0c1";

static inline uint8_t encode_temp(float temp, bool fast) {
  uint8_t ts = static_cast<uint8_t>(static_cast<int>(lroundf(temp)) & 0xFF);
  if (fast)
    ts |= 0x40;
  return ts;
}

bool EPaperE2271KS0C1::initialise(bool partial) {
  // Partial updates require non-negative temperature (>= 0C per datasheet)
  const bool do_partial = partial && this->temperature_ >= 0;

  // Full updates require a soft reset first; use state machine to avoid blocking
  if (!do_partial) {
    if (this->transfer_phase_ == 0) {
      // Issue soft reset command
      ESP_LOGV(TAG, "Soft reset for full update");
      this->cmd_data(CMD_PSR, {0x0E});
      this->transfer_phase_ = 1;
      this->next_delay_ = 50;  // Wait 50ms for soft reset to complete
      return false;            // Come back next loop
    }
    // Soft reset delay completed
    this->transfer_phase_ = 0;
  }

  // Initialize previous buffer on first use
  const size_t buffer_length = this->buffer_length_;
  if (this->prev_.size() != buffer_length) {
    this->prev_.resize(buffer_length, 0x00);
  }

  // Temperature configuration
  uint8_t ts = encode_temp(this->temperature_, do_partial);
  this->cmd_data(CMD_INPUT_TEMP, {ts});
  this->cmd_data(CMD_ACTIVE_TEMP, {0x02});

  if (do_partial) {
    // Fast/partial update: enable partial mode in PSR
    this->cmd_data(CMD_PSR, {static_cast<uint8_t>(PSR_DEFAULT[0] | 0x10), static_cast<uint8_t>(PSR_DEFAULT[1] | 0x02)});
    this->cmd_data(CMD_VCOM_CDI, {0x27});
  } else {
    // Full update: use default PSR
    this->cmd_data(CMD_PSR, PSR_DEFAULT, 2);
  }

  return true;
}

bool HOT EPaperE2271KS0C1::transfer_data() {
  const uint32_t start_time = millis();
  // Partial updates require non-negative temperature (>= 0C per datasheet)
  const bool partial = this->update_count_ != 0 && this->temperature_ >= 0;
  const size_t buffer_length = this->buffer_length_;

  // Phase 0: Start Frame 1 transfer
  if (this->transfer_phase_ == 0) {
    ESP_LOGV(TAG, "Transfer data, partial=%s", YESNO(partial));
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
        this->disable();
        return false;  // Yield and continue next loop
      }
    }
    this->disable();

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
        this->disable();
        return false;  // Yield and continue next loop
      }
    }
    this->disable();

    // Finalize: restore VCOM for partial updates
    if (partial) {
      this->cmd_data(CMD_VCOM_CDI, {0x07});
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
