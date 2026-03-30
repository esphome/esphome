#include "epaper_uc8179_bwr.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.uc8179_bwr";

enum class BwrColor : uint8_t {
  BLACK,
  WHITE,
  RED,
};

static BwrColor color_to_bwr(Color color) {
  if (color.r > color.g + color.b && color.r > 127) {
    return BwrColor::RED;
  }
  if (color.r + color.g + color.b >= 382) {
    return BwrColor::WHITE;
  }
  return BwrColor::BLACK;
}

void HOT EPaperUC8179BWR::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  const uint32_t pos = (x / 8) + (y * this->row_width_);
  const uint8_t bit = 0x80 >> (x & 0x07);
  const uint32_t red_offset = this->buffer_length_ / 2u;

  auto bwr = color_to_bwr(color);

  // Update black/white plane (first half of buffer)
  // 0 = black, 1 = white
  if (bwr == BwrColor::WHITE) {
    this->buffer_[pos] |= bit;
  } else {
    this->buffer_[pos] &= ~bit;
  }

  // Update red plane (second half of buffer)
  // 1 = red, 0 = not red
  if (bwr == BwrColor::RED) {
    this->buffer_[red_offset + pos] |= bit;
  } else {
    this->buffer_[red_offset + pos] &= ~bit;
  }
}

void EPaperUC8179BWR::fill(Color color) {
  if (this->get_clipping().is_set()) {
    esphome::epaper_spi::EPaperBase::fill(color);
    return;
  }

  const size_t half_buffer = this->buffer_length_ / 2u;
  auto bwr = color_to_bwr(color);

  if (bwr == BwrColor::BLACK) {
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[i] = 0x00;
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[half_buffer + i] = 0x00;
  } else if (bwr == BwrColor::RED) {
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[i] = 0x00;
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[half_buffer + i] = 0xFF;
  } else {
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[i] = 0xFF;
    for (size_t i = 0; i < half_buffer; i++)
      this->buffer_[half_buffer + i] = 0x00;
  }

  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
  this->x_low_ = 0;
  this->y_low_ = 0;
}

void EPaperUC8179BWR::clear() { this->fill(COLOR_ON); }

void EPaperUC8179BWR::loop() {
  if (this->waiting_for_idle_) {
    if (this->state_ == EPaperState::POWER_OFF || this->state_ == EPaperState::DEEP_SLEEP) {
      // BWR refresh takes ~30s but the display refreshes autonomously after
      // the refresh command (0x12). Don't wait - just proceed with shutdown.
      // The old waveshare driver always timed out here (~1s) and moved on
      // otherwise we're stuck waiting for the busy pin to go idle, which never happens after refresh.
      this->waiting_for_idle_ = false;
    }
  }
  EPaperBase::loop();
}

bool EPaperUC8179BWR::reset() {
  // UC8179 reset sequence: HIGH -> LOW -> HIGH with specific timings
  if (this->reset_pin_ == nullptr)
    return true;

  if (this->state_ == EPaperState::RESET) {
    this->reset_pin_->digital_write(true);
    return false;  // Come back for RESET_END
  }
  // RESET_END state
  this->reset_pin_->digital_write(false);
  delay(5);
  this->reset_pin_->digital_write(true);
  this->next_delay_ = 200;
  return true;
}

bool HOT EPaperUC8179BWR::transfer_data() {
  const uint32_t start_time = millis();
  const size_t buffer_length = this->buffer_length_;
  const size_t half_buffer = buffer_length / 2u;

  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];

  // First: send B/W data (command 0x10)
  if (this->current_data_index_ < half_buffer) {
    if (this->current_data_index_ == 0) {
      ESP_LOGV(TAG, "Sending B/W data (0x10)");
      this->command(0x10);
    }

    this->start_data_();
    while (this->current_data_index_ < half_buffer) {
      size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, half_buffer - this->current_data_index_);

      for (size_t i = 0; i < bytes_to_copy; i++) {
        bytes_to_send[i] = this->buffer_[this->current_data_index_ + i];
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

  // Second: send Red data (command 0x13)
  if (this->current_data_index_ < buffer_length) {
    if (this->current_data_index_ == half_buffer) {
      ESP_LOGV(TAG, "Sending Red data (0x13)");
      this->command(0x13);
    }

    this->start_data_();
    while (this->current_data_index_ < buffer_length) {
      size_t remaining = buffer_length - this->current_data_index_;
      size_t bytes_to_copy = std::min(MAX_TRANSFER_SIZE, remaining);

      size_t buffer_offset = this->current_data_index_;
      for (size_t i = 0; i < bytes_to_copy; i++) {
        bytes_to_send[i] = this->buffer_[buffer_offset + i];
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

void EPaperUC8179BWR::power_on() {
  ESP_LOGV(TAG, "Power on");
  this->command(0x04);
}

void EPaperUC8179BWR::refresh_screen(bool partial) {
  ESP_LOGV(TAG, "Refresh");
  this->command(0x12);
  this->next_delay_ = 100;
}

void EPaperUC8179BWR::power_off() {
  // UC8179: skip power off command (0x02) - deep_sleep handles shutdown.
  // Sending 0x02 causes the busy pin to remain asserted, blocking the state machine.
}

void EPaperUC8179BWR::deep_sleep() {
  ESP_LOGV(TAG, "Deep sleep");
  this->cmd_data(0x07, {0xA5});
}

}  // namespace esphome::epaper_spi
