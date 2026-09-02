#include "epaper_spi_uc8179.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.uc8179";

bool EPaperUC8179::initialise(bool partial) {
  EPaperBase::initialise(partial);  // send the model init sequence
  this->partial_ = partial;
  ESP_LOGV(TAG, "Power on");
  // POWER ON must precede the waveform/mode registers and the data transfer
  // (the original driver powers on and busy-waits before writing them).
  // The state machine busy-waits before entering TRANSFER_DATA.
  this->command(0x04);
  // Give the busy line time to assert before the state machine polls it
  this->next_delay_ = 100;
  return true;
}

// Set up the refresh mode. Must be called after power-on has completed.
void EPaperUC8179::set_refresh_mode_() {
  if (!this->is_using_partial_update_()) {
    return;  // plain full refresh uses the mode set by the init sequence
  }
  // Fast and partial refresh use flipped data polarity and a floating border
  this->cmd_data(0x50, {0xA9, 0x07});
  // Force the waveform via the temperature registers: 0x5A selects the fast
  // full-refresh waveform, 0x6E the partial-refresh waveform
  this->cmd_data(0xE0, {0x02});
  if (this->partial_) {
    this->cmd_data(0xE5, {0x6E});
    this->command(0x91);  // enter partial mode
    // Set the partial window to the full screen
    const uint16_t x_end = this->width_ - 1;
    const uint16_t y_end = this->height_ - 1;
    this->cmd_data(0x90, {0x00, 0x00, static_cast<uint8_t>(x_end >> 8), static_cast<uint8_t>(x_end & 0xFF), 0x00, 0x00,
                          static_cast<uint8_t>(y_end >> 8), static_cast<uint8_t>(y_end & 0xFF), 0x01});
  } else {
    this->cmd_data(0xE5, {0x5A});
    this->command(0x92);  // exit partial mode
  }
}

bool HOT EPaperUC8179::transfer_data() {
  const uint32_t start_time = millis();
  const size_t buffer_length = this->buffer_length_;
  if (this->current_data_index_ == 0) {
    this->set_refresh_mode_();
  }
  // Fast full refresh sends the previous-image plane as well, so that every pixel transitions
  const bool two_pass = this->is_using_partial_update_() && !this->partial_;
  // Plain full refresh sends inverted data (buffer is 1=white, the wire wants 0=white);
  // in fast/partial mode the data polarity is flipped via the VCOM/data-interval
  // register instead, so the new-image plane is sent unmodified
  const bool invert_new_data = !this->is_using_partial_update_();

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];

  // Phase 1 (fast full refresh only): previous image via 0x10 (DTM1), inverse of the new image
  if (two_pass && this->current_data_index_ < buffer_length) {
    if (this->current_data_index_ == 0) {
      this->command(0x10);  // DATA START TRANSMISSION 1 (previous image)
    }
    this->start_data_();
    while (this->current_data_index_ < buffer_length) {
      const size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, buffer_length - this->current_data_index_);
      for (size_t i = 0; i < bytes_to_copy; i++) {
        bytes_to_send[i] = ~this->buffer_[this->current_data_index_ + i];
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

  // Phase 2: new image via 0x13 (DTM2)
  const size_t offset = two_pass ? buffer_length : 0;
  const size_t total = offset + buffer_length;
  if (this->current_data_index_ < total) {
    if (this->current_data_index_ == offset) {
      this->command(0x13);  // DATA START TRANSMISSION 2 (new image)
    }
    this->start_data_();
    while (this->current_data_index_ < total) {
      const size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, total - this->current_data_index_);
      const size_t data_idx = this->current_data_index_ - offset;
      for (size_t i = 0; i < bytes_to_copy; i++) {
        const uint8_t byte = this->buffer_[data_idx + i];
        bytes_to_send[i] = invert_new_data ? ~byte : byte;
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

void EPaperUC8179::power_on() {
  // Power-on is sent at the end of initialise() instead, because the
  // waveform/mode registers and the data transfer must follow it
}

void EPaperUC8179::refresh_screen(bool /*partial*/) {
  ESP_LOGV(TAG, "Refresh");
  this->command(0x12);  // DISPLAY REFRESH
  // Delay the next busy poll: the busy line takes a short time to assert after
  // the refresh command, and polling too early would read it as already idle
  this->next_delay_ = 100;
}

void EPaperUC8179::power_off() {
  ESP_LOGV(TAG, "Power off");
  this->command(0x02);  // POWER OFF
}

void EPaperUC8179::deep_sleep() {
  // Deep sleep loses the previous-image RAM that partial refresh compares against
  if (!this->is_using_partial_update_()) {
    ESP_LOGV(TAG, "Deep sleep");
    this->cmd_data(0x07, {0xA5});  // DEEP SLEEP with check code
  }
}

}  // namespace esphome::epaper_spi
