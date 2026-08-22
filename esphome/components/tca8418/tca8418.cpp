#include "tca8418.h"
#include "esphome/core/log.h"

namespace esphome::tca8418 {

static const char *const TAG = "tca8418";

//  How often to ask the device whether it has events, when no interrupt pin is
//  configured. Reading the interrupt pin is nearly free, so it is checked every
//  loop instead.
static constexpr uint32_t POLL_INTERVAL_MS = 10;

void TCA8418Component::setup() {
  //  The configuration register reads back, so use it to check the device is there.
  uint8_t config;
  if (!this->read_byte(TCA8418_REG_CFG, &config)) {
    ESP_LOGE(TAG, "Failed to read from device - check wiring / address");
    this->mark_failed();
    return;
  }

  if (!this->configure_pins_()) {
    ESP_LOGE(TAG, "Failed to configure pins");
    this->mark_failed();
    return;
  }

  //  Report key events, and keep the interrupt asserted while events remain queued.
  if (!this->write_byte(TCA8418_REG_CFG, TCA8418_CFG_KEY_INT_EN | TCA8418_CFG_INT_CFG)) {
    ESP_LOGE(TAG, "Failed to write the configuration register");
    this->mark_failed();
    return;
  }

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
  }

  //  Discard anything the device queued before it was configured, and clear
  //  every interrupt flag: leaving one set holds the interrupt output asserted.
  uint8_t key;
  while (this->read_byte(TCA8418_REG_KEY_EVENT_A, &key) && key != 0) {
  }
  this->write_byte(TCA8418_REG_INT_STAT, TCA8418_INT_STAT_ALL);
}

bool TCA8418Component::configure_pins_() {
  //  Each of the three registers covers a group of pins: ROW0-7, COL0-7, COL8-9.
  uint8_t matrix[3] = {0, 0, 0};
  if (this->rows_ > 0) {
    matrix[0] = static_cast<uint8_t>((1U << this->rows_) - 1U);
    const uint8_t low_columns = this->columns_ > 8 ? 8 : this->columns_;
    matrix[1] = static_cast<uint8_t>((1U << low_columns) - 1U);
    if (this->columns_ > 8)
      matrix[2] = static_cast<uint8_t>((1U << (this->columns_ - 8U)) - 1U);
  }

  //  Pins outside the matrix report individually when they are in event mode.
  //  The device ignores these bits for pins that belong to the matrix.
  const uint8_t gpi[3] = {
      static_cast<uint8_t>(this->gpi_events_ ? ~matrix[0] : 0),
      static_cast<uint8_t>(this->gpi_events_ ? ~matrix[1] : 0),
      static_cast<uint8_t>(this->gpi_events_ ? (0x03 & ~matrix[2]) : 0),
  };

  for (uint8_t i = 0; i < 3; i++) {
    //  Enable the internal pull-ups (a 0 bit enables the pull-up) so that
    //  buttons can simply switch their pin to ground.
    if (!this->write_byte(TCA8418_REG_GPIO_PULL1 + i, 0x00) || !this->write_byte(TCA8418_REG_KP_GPIO1 + i, matrix[i]) ||
        !this->write_byte(TCA8418_REG_GPI_EM1 + i, gpi[i]) || !this->write_byte(TCA8418_REG_GPIO_INT_EN1 + i, gpi[i])) {
      return false;
    }
  }
  return true;
}

void TCA8418Component::loop() {
  if (this->interrupt_pin_ != nullptr) {
    //  The interrupt output is active low, and stays asserted while events are
    //  queued. Reading the level, rather than waiting for an edge, means a
    //  missed edge cannot leave events stranded in the queue.
    if (this->interrupt_pin_->digital_read())
      return;
  } else {
    const uint32_t now = millis();
    if (now - this->last_poll_ < POLL_INTERVAL_MS)
      return;
    this->last_poll_ = now;

    uint8_t count;
    if (!this->read_byte(TCA8418_REG_KEY_LCK_EC, &count)) {
      this->status_set_warning();
      return;
    }
    if ((count & TCA8418_EVENT_COUNT_MASK) == 0)
      return;
  }

  this->process_events_();
}

void TCA8418Component::process_events_() {
  //  Reading the top of the FIFO pops an event and returns 0 once it is empty.
  uint8_t event;
  while (this->read_byte(TCA8418_REG_KEY_EVENT_A, &event) && event != 0) {
    this->dispatch_(event & TCA8418_KEY_CODE_MASK, (event & TCA8418_KEY_PRESSED) != 0);
  }

  if (!this->write_byte(TCA8418_REG_INT_STAT, TCA8418_INT_STAT_KEY)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();
}

uint8_t TCA8418Component::key_char_(uint8_t key) const {
  if (this->keys_.empty() || key < TCA8418_MATRIX_KEY_MIN || key > TCA8418_MATRIX_KEY_MAX)
    return 0;
  const uint8_t row = (key - 1) / TCA8418_MATRIX_COLUMNS;
  const uint8_t col = (key - 1) % TCA8418_MATRIX_COLUMNS;
  if (row >= this->rows_ || col >= this->columns_)
    return 0;
  const size_t index = static_cast<size_t>(row) * this->columns_ + col;
  if (index >= this->keys_.size())
    return 0;
  return static_cast<uint8_t>(this->keys_[index]);
}

void TCA8418Component::dispatch_(uint8_t key, bool pressed) {
  ESP_LOGV(TAG, "Key %u %s", key, pressed ? "pressed" : "released");

  for (auto *listener : this->listeners_) {
    if (pressed) {
      listener->key_pressed(key);
    } else {
      listener->key_released(key);
    }
  }

  //  Matrix keys also report their position.
  if (key >= TCA8418_MATRIX_KEY_MIN && key <= TCA8418_MATRIX_KEY_MAX) {
    const uint8_t row = (key - 1) / TCA8418_MATRIX_COLUMNS;
    const uint8_t col = (key - 1) % TCA8418_MATRIX_COLUMNS;
    for (auto *listener : this->listeners_) {
      if (pressed) {
        listener->button_pressed(row, col);
      } else {
        listener->button_released(row, col);
      }
    }
  }

  const uint8_t key_char = this->key_char_(key);
  if (key_char != 0) {
    for (auto *listener : this->listeners_) {
      if (pressed) {
        listener->key_char_pressed(key_char);
      } else {
        listener->key_char_released(key_char);
      }
    }
  }

  if (!pressed)
    return;

  //  Triggers and key collectors receive the mapped character when a key map is
  //  configured, and the key number otherwise.
  const uint8_t value = key_char != 0 ? key_char : key;
  this->send_key_(value);
  for (auto *trigger : this->key_triggers_)
    trigger->trigger(value);
}

void TCA8418Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TCA8418:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  if (this->rows_ > 0) {
    ESP_LOGCONFIG(TAG, "  Key matrix: %u rows x %u columns", this->rows_, this->columns_);
  } else {
    ESP_LOGCONFIG(TAG, "  Key matrix: none");
  }
  ESP_LOGCONFIG(TAG, "  Individual inputs: %s", YESNO(this->gpi_events_));
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
}

}  // namespace esphome::tca8418
