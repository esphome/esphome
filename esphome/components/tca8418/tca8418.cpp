#include "tca8418.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::tca8418 {

ESPHOME_LOG_TAG(TAG, "tca8418");

//  Minimum gap between event queue reads; the ten entry queue holds anything arriving sooner.
static constexpr uint32_t POLL_INTERVAL_MS = 50;

void IRAM_ATTR TCA8418Component::interrupt_handler(TCA8418Component *arg) { arg->enable_loop_soon_any_context(); }

void TCA8418Component::setup() {
  //  The configuration register reads back, so use it to check the device is there.
  uint8_t config;
  if (!this->read_byte(TCA8418_REG_CFG, &config)) {
    this->mark_failed(LOG_STR("Failed to read from device - check wiring / address"));
    return;
  }

  if (!this->configure_pins_()) {
    this->mark_failed(LOG_STR("Failed to configure pins"));
    return;
  }

  //  Report key events, and keep the interrupt asserted while events remain queued.
  if (!this->write_byte(TCA8418_REG_CFG, TCA8418_CFG_KEY_INT_EN | TCA8418_CFG_INT_CFG)) {
    this->mark_failed(LOG_STR("Failed to write the configuration register"));
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
  if (!this->drain_events_(false)) {
    this->mark_failed(LOG_STR("Failed to clear the event queue"));
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
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_poll_ < POLL_INTERVAL_MS)
    return;
  this->last_poll_ = now;

  if (!this->drain_events_(true)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

  // Events that arrive during a read keep INT asserted without a new edge, so only stop once it is clear
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read())
    this->disable_loop();
}

bool TCA8418Component::drain_events_(bool dispatch) {
  uint8_t count;
  if (!this->read_byte(TCA8418_REG_KEY_LCK_EC, &count))
    return false;
  count &= TCA8418_EVENT_COUNT_MASK;
  // An idle poll has no flags to clear, unless INT is still asserted from a failed clear
  if (count == 0 && dispatch && (this->interrupt_pin_ == nullptr || this->interrupt_pin_->digital_read()))
    return true;
  // The overflow flag stays clear with a full queue, so check the count
  if (dispatch && count >= TCA8418_FIFO_DEPTH) {
    ESP_LOGW(TAG, "Event queue is full - some key presses may have been lost");
  }

  uint8_t event;
  for (uint8_t i = 0; i < count; i++) {
    if (!this->read_byte(TCA8418_REG_KEY_EVENT_A, &event))
      return false;
    if (dispatch)
      this->dispatch_(event & TCA8418_KEY_CODE_MASK, (event & TCA8418_KEY_PRESSED) != 0);
  }
  return this->write_byte(TCA8418_REG_INT_STAT, TCA8418_INT_STAT_ALL);
}

uint8_t TCA8418Component::key_char_(uint8_t key) const {
  if (key == 0 || key > this->keys_size_)
    return 0;
  return progmem_read_byte(&this->keys_[key - 1]);
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
  ESP_LOGCONFIG(TAG,
                "TCA8418:\n"
                "  Individual inputs: %s",
                YESNO(this->gpi_events_));
  if (this->rows_ > 0) {
    ESP_LOGCONFIG(TAG, "  Key matrix: %u rows x %u columns", this->rows_, this->columns_);
  }
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this);
}

}  // namespace esphome::tca8418
