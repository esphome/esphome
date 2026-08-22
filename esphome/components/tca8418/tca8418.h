#pragma once

#include "esphome/components/key_provider/key_provider.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <string>
#include <vector>

namespace esphome::tca8418 {

//  Register map (see TI datasheet SCPS231)
static constexpr uint8_t TCA8418_REG_CFG = 0x01;
static constexpr uint8_t TCA8418_REG_INT_STAT = 0x02;
static constexpr uint8_t TCA8418_REG_KEY_LCK_EC = 0x03;
static constexpr uint8_t TCA8418_REG_KEY_EVENT_A = 0x04;  // top of the event FIFO
static constexpr uint8_t TCA8418_REG_GPIO_INT_EN1 = 0x1A;
static constexpr uint8_t TCA8418_REG_KP_GPIO1 = 0x1D;
static constexpr uint8_t TCA8418_REG_GPI_EM1 = 0x20;
static constexpr uint8_t TCA8418_REG_GPIO_PULL1 = 0x2C;

//  CFG
static constexpr uint8_t TCA8418_CFG_KEY_INT_EN = 0x01;  // interrupt on key events
static constexpr uint8_t TCA8418_CFG_INT_CFG = 0x10;     // re-assert INT while events remain
//  INT_STAT
static constexpr uint8_t TCA8418_INT_STAT_KEY = 0x01;
static constexpr uint8_t TCA8418_INT_STAT_ALL = 0x1F;  // every interrupt flag
//  KEY_LCK_EC: the low nibble is the number of queued events
static constexpr uint8_t TCA8418_EVENT_COUNT_MASK = 0x0F;
//  Key events: bit 7 set = pressed, clear = released; bits 6-0 are the key number
static constexpr uint8_t TCA8418_KEY_PRESSED = 0x80;
static constexpr uint8_t TCA8418_KEY_CODE_MASK = 0x7F;

//  Key numbering. Matrix keys are numbered row-major with 10 columns per row,
//  starting at 1, so ROW0/COL0 is 1 and ROW7/COL9 is 80. Pins that are not part
//  of the matrix report as individual inputs starting at 97 (ROW0..ROW7 are
//  97..104, COL0..COL9 are 105..114).
static constexpr uint8_t TCA8418_MATRIX_KEY_MIN = 1;
static constexpr uint8_t TCA8418_MATRIX_KEY_MAX = 80;
static constexpr uint8_t TCA8418_MATRIX_COLUMNS = 10;
static constexpr uint8_t TCA8418_GPI_KEY_MIN = 97;

class TCA8418Listener {
 public:
  /// Called with the key number reported by the device.
  virtual void key_pressed(uint8_t key) {}
  virtual void key_released(uint8_t key) {}
  /// Called for matrix keys with their position in the matrix.
  virtual void button_pressed(uint8_t row, uint8_t col) {}
  virtual void button_released(uint8_t row, uint8_t col) {}
  /// Called for matrix keys with their character, when a key map is configured.
  virtual void key_char_pressed(uint8_t key_char) {}
  virtual void key_char_released(uint8_t key_char) {}
};

class TCA8418KeyTrigger final : public Trigger<uint8_t> {};

class TCA8418Component : public key_provider::KeyProvider, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_rows(uint8_t rows) { this->rows_ = rows; }
  void set_columns(uint8_t columns) { this->columns_ = columns; }
  void set_gpi_events(bool gpi_events) { this->gpi_events_ = gpi_events; }
  void set_keys(std::string keys) { this->keys_ = std::move(keys); }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

  void register_listener(TCA8418Listener *listener) { this->listeners_.push_back(listener); }
  void register_key_trigger(TCA8418KeyTrigger *trigger) { this->key_triggers_.push_back(trigger); }

 protected:
  /// Write the matrix / individual-input pin configuration. Returns false on I2C failure.
  bool configure_pins_();
  /// Read and dispatch every queued key event.
  void process_events_();
  void dispatch_(uint8_t key, bool pressed);
  /// The character mapped to a matrix key, or 0 when it has none.
  uint8_t key_char_(uint8_t key) const;

  uint8_t rows_{0};
  uint8_t columns_{0};
  bool gpi_events_{true};
  std::string keys_;
  InternalGPIOPin *interrupt_pin_{nullptr};
  uint32_t last_poll_{0};

  std::vector<TCA8418Listener *> listeners_;
  std::vector<TCA8418KeyTrigger *> key_triggers_;
};

}  // namespace esphome::tca8418
