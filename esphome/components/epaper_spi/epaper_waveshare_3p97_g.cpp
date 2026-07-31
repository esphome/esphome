#include "epaper_waveshare_3p97_g.h"

#include "colorconv.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.waveshare_3p97_g";

enum class Waveshare3P97GColor : uint8_t {
  BLACK = 0b00,
  WHITE = 0b01,
  YELLOW = 0b10,
  RED = 0b11,
};

static Waveshare3P97GColor HOT color_to_native(Color color) {
  return color_to_bwyr(color, Waveshare3P97GColor::BLACK, Waveshare3P97GColor::WHITE, Waveshare3P97GColor::YELLOW,
                       Waveshare3P97GColor::RED);
}

void EPaperWaveshare3P97InG::fill(Color color) {
  if (this->get_clipping().is_set()) {
    EPaperBase::fill(color);
    return;
  }

  const auto pixel_color = static_cast<uint8_t>(color_to_native(color));
  this->buffer_.fill(pixel_color | (pixel_color << 2) | (pixel_color << 4) | (pixel_color << 6));
  this->x_low_ = 0;
  this->y_low_ = 0;
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
}

void HOT EPaperWaveshare3P97InG::draw_pixel_at(int x, int y, Color color) {
  if (!this->rotate_coordinates_(x, y))
    return;

  const auto pixel_bits = static_cast<uint8_t>(color_to_native(color));
  const uint32_t pixel_position = x + y * this->get_width_internal();
  const uint32_t byte_position = pixel_position / 4;
  const uint32_t bit_offset = 6 - ((pixel_position % 4) * 2);
  const auto original = this->buffer_[byte_position];
  this->buffer_[byte_position] = (original & ~(0b11 << bit_offset)) | (pixel_bits << bit_offset);
}

bool EPaperWaveshare3P97InG::reset() {
  if (this->reset_pin_ == nullptr) {
    this->step_ = Step::INIT_SEQUENCE;
    return true;
  }

  if (this->state_ == EPaperState::RESET)
    this->step_ = Step::RESET_HIGH;

  switch (this->step_) {
    case Step::RESET_HIGH:
      this->reset_pin_->digital_write(true);
      this->reset_duration_ = 200;
      this->step_ = Step::RESET_LOW;
      return false;
    case Step::RESET_LOW:
      this->reset_pin_->digital_write(false);
      delay(2);
      this->reset_pin_->digital_write(true);
      this->reset_duration_ = 200;
      this->step_ = Step::RESET_SETTLE;
      return false;
    case Step::RESET_SETTLE:
      this->step_ = Step::INIT_SEQUENCE;
      return true;
    default:
      this->mark_failed(LOG_STR("Invalid reset state"));
      return true;
  }
}

bool EPaperWaveshare3P97InG::initialise([[maybe_unused]] bool partial) {
  switch (this->step_) {
    case Step::INIT_SEQUENCE:
      this->send_init_sequence_(this->init_sequence_, this->init_sequence_length_);
      this->delay_until_ = millis() + 100;
      this->wait_for_idle_(true);
      this->step_ = Step::POWER_ON;
      return false;
    case Step::POWER_ON:
      this->command(0x04);
      this->delay_until_ = millis() + 100;
      this->wait_for_idle_(true);
      this->step_ = Step::INIT_DONE;
      return false;
    case Step::INIT_DONE:
      this->step_ = Step::NONE;
      return true;
    default:
      this->mark_failed(LOG_STR("Invalid initialise state"));
      return true;
  }
}

bool HOT EPaperWaveshare3P97InG::transfer_data() {
  if (this->current_data_index_ == 0)
    this->command(0x10);

  const uint32_t start_time = App.get_loop_component_start_time();
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];
  while (this->current_data_index_ < this->buffer_length_) {
    const size_t bytes_to_copy =
        std::min(static_cast<size_t>(MAX_TRANSFER_SIZE), this->buffer_length_ - this->current_data_index_);
    for (size_t i = 0; i < bytes_to_copy; i++)
      bytes_to_send[i] = this->buffer_[this->current_data_index_ + i];

    this->start_data_();
    this->write_array(bytes_to_send, bytes_to_copy);
    this->disable();
    this->current_data_index_ += bytes_to_copy;

    if (millis() - start_time > MAX_TRANSFER_TIME)
      return false;
  }

  this->current_data_index_ = 0;
  return true;
}

void EPaperWaveshare3P97InG::refresh_screen([[maybe_unused]] bool partial) {
  this->cmd_data(0x12, {0x00});
  this->next_delay_ = 100;
}

void EPaperWaveshare3P97InG::power_off() { this->cmd_data(0x02, {0x00}); }

void EPaperWaveshare3P97InG::deep_sleep() { this->cmd_data(0x07, {0xA5}); }

}  // namespace esphome::epaper_spi
