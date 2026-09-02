#include "tca8418.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::tca8418 {

static const char *const TAG = "tca8418";

//  The shortest gap between asking the device whether it has events. It applies
//  whether or not an interrupt pin is configured. A typical main loop is slower
//  than this, so in normal use it does not hold anything up; it is a floor that
//  keeps a fast loop, or a pin stuck asserted, from turning into I2C traffic on
//  every pass.
static constexpr uint32_t POLL_INTERVAL_MS = 10;

void IRAM_ATTR TCA8418Component::interrupt_handler(TCA8418Component *arg) { arg->enable_loop_soon_any_context(); }

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
    //  The interrupt output is active low, so a falling edge means an event was
    //  queued. The handler only wakes the loop; the queue is read from there.
    this->interrupt_pin_->attach_interrupt(&TCA8418Component::interrupt_handler, this, gpio::INTERRUPT_FALLING_EDGE);
  }

  //  Discard anything the device queued before it was configured, and clear
  //  every interrupt flag: leaving one set holds the interrupt output asserted.
  uint8_t key;
  for (uint8_t i = 0; i < TCA8418_FIFO_DEPTH; i++) {
    if (!this->read_byte(TCA8418_REG_KEY_EVENT_A, &key) || key == 0)
      break;
  }
  if (!this->write_byte(TCA8418_REG_INT_STAT, TCA8418_INT_STAT_ALL)) {
    ESP_LOGE(TAG, "Failed to clear the interrupt flags");
    this->mark_failed();
    return;
  }

  //  With an interrupt pin there is nothing to do until it fires. Stay running
  //  if the device still has something queued, since that produces no new edge.
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read())
    this->disable_loop();
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
  //  One path for both ways of noticing events. Asking the device how many it
  //  has queued is a single read, and rate limiting it means a pin left
  //  floating, or a device that has stopped answering, cannot turn into a
  //  stream of I2C traffic on every pass of the main loop.
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_poll_ < POLL_INTERVAL_MS)
    return;
  this->last_poll_ = now;

  uint8_t count;
  if (!this->read_byte(TCA8418_REG_KEY_LCK_EC, &count)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

  const uint8_t queued = count & TCA8418_EVENT_COUNT_MASK;
  //  A full queue means any further key event was dropped before it could be
  //  read. The device's own overflow flag is not used for this: it stays clear
  //  even with a queue that has been sitting full, so it would never report.
  if (queued >= TCA8418_FIFO_DEPTH) {
    ESP_LOGW(TAG, "Event queue is full - some key presses may have been lost");
  }

  if (queued != 0)
    this->process_events_();

  //  With an interrupt pin, wait for the next interrupt once the device has
  //  nothing left. Events that arrive while the queue is being read keep the
  //  interrupt output asserted without producing another edge, so the level has
  //  to be clear before the loop stops.
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read())
    this->disable_loop();
}

void TCA8418Component::process_events_() {
  //  Reading the top of the queue removes an event and returns 0 once it is
  //  empty. The queue holds ten entries, so that bounds the number of reads.
  uint8_t event;
  for (uint8_t i = 0; i < TCA8418_FIFO_DEPTH; i++) {
    if (!this->read_byte(TCA8418_REG_KEY_EVENT_A, &event)) {
      this->status_set_warning();
      return;
    }
    if (event == 0)
      break;
    this->dispatch_(event & TCA8418_KEY_CODE_MASK, (event & TCA8418_KEY_PRESSED) != 0);
  }

  if (!this->write_byte(TCA8418_REG_INT_STAT, TCA8418_INT_STAT_ALL)) {
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
  ESP_LOGV(TAG, "Key %u %s", key, pressed ? LOG_STR_LITERAL("pressed") : LOG_STR_LITERAL("released"));

#ifdef TCA8418_LISTENER_COUNT
  for (auto *listener : this->listeners_) {
    if (pressed) {
      listener->key_pressed(key);
    } else {
      listener->key_released(key);
    }
  }
#endif

  if (!pressed)
    return;

  //  Key collectors and automations receive the mapped character when a key map
  //  is configured, and the key number otherwise.
  const uint8_t key_char = this->key_char_(key);
  this->send_key_(key_char != 0 ? key_char : key);
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
