#include "epaper_spi.h"
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi";

static constexpr const char *const EPAPER_STATE_STRINGS[] = {
    "IDLE",          "UPDATE",   "RESET",          "RESET_END", "SHOULD_WAIT", "INITIALISE",
    "TRANSFER_DATA", "POWER_ON", "REFRESH_SCREEN", "POWER_OFF", "DEEP_SLEEP",
};

const char *EPaperBase::epaper_state_to_string_() {
  if (auto idx = static_cast<unsigned>(this->state_); idx < std::size(EPAPER_STATE_STRINGS))
    return EPAPER_STATE_STRINGS[idx];
  return "Unknown";
}

void EPaperBase::setup() {
  if (!this->init_buffer_(this->buffer_length_)) {
    this->mark_failed(LOG_STR("Failed to initialise buffer"));
    return;
  }
  this->setup_pins_();
  this->spi_setup();
}

bool EPaperBase::init_buffer_(size_t buffer_length) {
  if (!this->buffer_.init(buffer_length)) {
    return false;
  }
  this->clear();
  return true;
}

void EPaperBase::setup_pins_() const {
  this->dc_pin_->setup();  // OUTPUT
  this->dc_pin_->digital_write(false);

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();  // OUTPUT
    this->reset_pin_->digital_write(true);
  }

  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();  // INPUT
  }
}

float EPaperBase::get_setup_priority() const { return setup_priority::PROCESSOR; }

void EPaperBase::command(uint8_t value) {
  this->start_command_();
  this->write_byte(value);
  this->end_command_();
}

void EPaperBase::data(uint8_t value) {
  this->start_data_();
  this->write_byte(value);
  this->end_data_();
}

// write a command followed by zero or more bytes of data.
// The command is the first byte, length is the length of data only in the second byte, followed by the data.
// [COMMAND, LENGTH, DATA...]
void EPaperBase::cmd_data(uint8_t command, const uint8_t *ptr, size_t length) {
  ESP_LOGV(TAG, "Command: 0x%02X, Length: %d, Data: %s", command, length,
           format_hex_pretty(ptr, length, '.', false).c_str());

  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(command);
  if (length > 0) {
    this->dc_pin_->digital_write(true);
    this->write_array(ptr, length);
  }
  this->disable();
}

bool EPaperBase::is_idle_() const {
  if (this->busy_pin_ == nullptr) {
    return true;
  }
  return !this->busy_pin_->digital_read();
}

bool EPaperBase::reset() {
  if (this->reset_pin_ != nullptr) {
    if (this->state_ == EPaperState::RESET) {
      this->reset_pin_->digital_write(false);
      return false;
    }
    this->reset_pin_->digital_write(true);
  }
  return true;
}

void EPaperBase::update() {
  if (this->state_ != EPaperState::IDLE) {
    ESP_LOGE(TAG, "Display already in state %s", epaper_state_to_string_());
    return;
  }
  this->set_state_(EPaperState::UPDATE);
  this->enable_loop();
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  this->update_start_time_ = millis();
#endif
}

void EPaperBase::wait_for_idle_(bool should_wait) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  this->waiting_for_idle_start_ = millis();
#endif
  this->waiting_for_idle_ = should_wait;
}

/**
 * Called during the loop task.
 * First defer for any pending delays, then check if we are waiting for the display to become idle.
 * If not waiting for idle, process the state machine.
 */

void EPaperBase::loop() {
  auto now = millis();
  if (this->delay_until_ != 0) {
    // using modulus arithmetic to handle wrap-around
    int diff = now - this->delay_until_;
    if (diff < 0) {
      return;
    }
    this->delay_until_ = 0;
  }
  if (this->waiting_for_idle_) {
    if (this->is_idle_()) {
      this->waiting_for_idle_ = false;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      ESP_LOGV(TAG, "Screen was busy for %u ms", (unsigned) (millis() - this->waiting_for_idle_start_));
#endif
    } else {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      if (now - this->waiting_for_idle_last_print_ >= 1000) {
        ESP_LOGV(TAG, "Waiting for idle in state %s", this->epaper_state_to_string_());
        this->waiting_for_idle_last_print_ = millis();
      }
#endif
      return;
    }
  }
  this->process_state_();
}

/**
 * Process the state machine.
 * Typical state sequence:
 * IDLE -> RESET -> RESET_END -> UPDATE -> INITIALISE -> TRANSFER_DATA -> POWER_ON -> REFRESH_SCREEN -> POWER_OFF ->
 * DEEP_SLEEP -> IDLE
 *
 * Should a subclassed class need to override this, the method will need to be made virtual.
 */
void EPaperBase::process_state_() {
  ESP_LOGV(TAG, "Process state entered in state %s", epaper_state_to_string_());
  switch (this->state_) {
    default:
      ESP_LOGE(TAG, "Display is in unhandled state %s", epaper_state_to_string_());
      this->set_state_(EPaperState::IDLE);
      break;
    case EPaperState::IDLE:
      this->disable_loop();
      break;
    case EPaperState::RESET:
    case EPaperState::RESET_END:
      if (this->reset()) {
        this->set_state_(EPaperState::INITIALISE);
      } else {
        this->set_state_(EPaperState::RESET_END, this->reset_duration_);
      }
      break;
    case EPaperState::UPDATE:
      this->do_update_();  // Calls ESPHome (current page) lambda
      if (this->x_high_ < this->x_low_ || this->y_high_ < this->y_low_) {
        this->set_state_(EPaperState::IDLE);
        return;
      }
      this->set_state_(EPaperState::RESET);
      break;
    case EPaperState::INITIALISE:
      this->initialise_();
      this->set_state_(EPaperState::TRANSFER_DATA);
      break;
    case EPaperState::TRANSFER_DATA:
      if (!this->transfer_data()) {
        return;  // Not done yet, come back next loop
      }
      this->x_low_ = this->width_;
      this->x_high_ = 0;
      this->y_low_ = this->height_;
      this->y_high_ = 0;
      this->set_state_(EPaperState::POWER_ON);
      break;
    case EPaperState::POWER_ON:
      this->power_on();
      this->set_state_(EPaperState::REFRESH_SCREEN);
      break;
    case EPaperState::REFRESH_SCREEN:
      this->refresh_screen(this->update_count_ != 0);
      this->update_count_ = (this->update_count_ + 1) % this->full_update_every_;
      this->set_state_(EPaperState::POWER_OFF);
      break;
    case EPaperState::POWER_OFF:
      this->power_off();
      this->set_state_(EPaperState::DEEP_SLEEP);
      break;
    case EPaperState::DEEP_SLEEP:
      this->deep_sleep();
      this->set_state_(EPaperState::IDLE);
      ESP_LOGD(TAG, "Display update took %" PRIu32 " ms", millis() - this->update_start_time_);
      break;
  }
}

void EPaperBase::set_state_(EPaperState state, uint16_t delay) {
  ESP_LOGV(TAG, "Exit state %s", this->epaper_state_to_string_());
  this->state_ = state;
  this->wait_for_idle_(state > EPaperState::SHOULD_WAIT);
  if (delay != 0) {
    this->delay_until_ = millis() + delay;
  } else {
    this->delay_until_ = 0;
  }
  ESP_LOGV(TAG, "Enter state %s, delay %u, wait_for_idle=%s", this->epaper_state_to_string_(), delay,
           TRUEFALSE(this->waiting_for_idle_));
  if (state == EPaperState::IDLE) {
    this->disable_loop();
  }
}

void EPaperBase::start_command_() {
  this->dc_pin_->digital_write(false);
  this->enable();
}

void EPaperBase::end_command_() { this->disable(); }

void EPaperBase::start_data_() {
  this->dc_pin_->digital_write(true);
  this->enable();
}
void EPaperBase::end_data_() { this->disable(); }

void EPaperBase::on_safe_shutdown() { this->deep_sleep(); }

void EPaperBase::initialise_() {
  size_t index = 0;

  auto *sequence = this->init_sequence_;
  auto length = this->init_sequence_length_;
  while (index != length) {
    if (length - index < 2) {
      this->mark_failed(LOG_STR("Malformed init sequence"));
      return;
    }
    const uint8_t cmd = sequence[index++];
    if (const uint8_t x = sequence[index++]; x == DELAY_FLAG) {
      ESP_LOGV(TAG, "Delay %dms", cmd);
      delay(cmd);
    } else {
      const uint8_t num_args = x & 0x7F;
      if (length - index < num_args) {
        ESP_LOGE(TAG, "Malformed init sequence, cmd = %X, num_args = %u", cmd, num_args);
        this->mark_failed();
        return;
      }
      this->cmd_data(cmd, sequence + index, num_args);
      index += num_args;
    }
  }
}

/**
 * Check and rotate coordinates based on the transform flags.
 * @param x
 * @param y
 * @return false if the coordinates are out of bounds
 */
bool EPaperBase::rotate_coordinates_(int &x, int &y) {
  if (!this->get_clipping().inside(x, y))
    return false;
  if (this->transform_ & SWAP_XY)
    std::swap(x, y);
  if (this->transform_ & MIRROR_X)
    x = this->width_ - x - 1;
  if (this->transform_ & MIRROR_Y)
    y = this->height_ - y - 1;
  if (x >= this->width_ || y >= this->height_ || x < 0 || y < 0)
    return false;
  this->x_low_ = clamp_at_most(this->x_low_, x);
  this->x_high_ = clamp_at_least(this->x_high_, x + 1);
  this->y_low_ = clamp_at_most(this->y_low_, y);
  this->y_high_ = clamp_at_least(this->y_high_, y + 1);
  return true;
}

/**
 *  Default implementation for monochrome displays where 8 pixels are packed to a byte.
 * @param x
 * @param y
 * @param color
 */
void HOT EPaperBase::draw_pixel_at(int x, int y, Color color) {
  if (!rotate_coordinates_(x, y))
    return;
  const size_t pixel_position = y * this->width_ + x;
  const size_t byte_position = pixel_position / 8;
  const uint8_t bit_position = pixel_position % 8;
  const uint8_t pixel_bit = 0x80 >> bit_position;
  const auto original = this->buffer_[byte_position];
  if ((color_to_bit(color) == 0)) {
    this->buffer_[byte_position] = original & ~pixel_bit;
  } else {
    this->buffer_[byte_position] = original | pixel_bit;
  }
}

void EPaperBase::dump_config() {
  LOG_DISPLAY("", "E-Paper SPI", this);
  ESP_LOGCONFIG(TAG, "  Model: %s", this->name_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG,
                "  SPI Data Rate: %uMHz\n"
                "  Full update every: %d\n"
                "  Swap X/Y: %s\n"
                "  Mirror X: %s\n"
                "  Mirror Y: %s",
                (unsigned) (this->data_rate_ / 1000000), this->full_update_every_, YESNO(this->transform_ & SWAP_XY),
                YESNO(this->transform_ & MIRROR_X), YESNO(this->transform_ & MIRROR_Y));
}

}  // namespace esphome::epaper_spi
