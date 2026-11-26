#pragma once

#include "esphome/components/key_provider/key_provider.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

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
static constexpr uint8_t TCA8418_INT_STAT_ALL = 0x1F;  // every interrupt flag
//  KEY_LCK_EC: the low nibble is the number of queued events
static constexpr uint8_t TCA8418_EVENT_COUNT_MASK = 0x0F;
//  Key events: bit 7 set = pressed, clear = released; bits 6-0 are the key number
static constexpr uint8_t TCA8418_KEY_PRESSED = 0x80;
static constexpr uint8_t TCA8418_KEY_CODE_MASK = 0x7F;
//  The event queue holds ten entries, which bounds how many can be read at once.
static constexpr uint8_t TCA8418_FIFO_DEPTH = 10;

// A template on the sensor type keeps binary_sensor.h out of this hub header
template<typename T> inline void publish_key(T *sensor, uint8_t key, uint8_t sensor_key, bool pressed) {
  if (key == sensor_key)
    sensor->publish_state(pressed);
}

class TCA8418Component : public key_provider::KeyProvider, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_rows(uint8_t rows) { this->rows_ = rows; }
  void set_columns(uint8_t columns) { this->columns_ = columns; }
  void set_gpi_events(bool gpi_events) { this->gpi_events_ = gpi_events; }
  void set_keys(const uint8_t *keys, uint8_t size) {
    this->keys_ = keys;
    this->keys_size_ = size;
  }
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }

#ifdef USE_TCA8418_KEY_PUBLISHER
  // key is 1..80 for the matrix, 97..114 for individual inputs
  HUB_PUBLISHER(key, uint8_t, bool)
#endif

  /// Wakes the loop so queued events are read. Runs in interrupt context.
  static void interrupt_handler(TCA8418Component *arg);

 protected:
  /// Write the matrix / individual-input pin configuration. Returns false on I2C failure.
  bool configure_pins_();
  /// Read every queued key event, dispatching them if asked, then clear the interrupt flags.
  /// Returns false on I2C failure.
  bool drain_events_(bool dispatch);
  void dispatch_(uint8_t key, bool pressed);
  /// The character mapped to a matrix key, or 0 when it has none.
  uint8_t key_char_(uint8_t key) const;

  const uint8_t *keys_{nullptr};  // PROGMEM, indexed by key number - 1
  InternalGPIOPin *interrupt_pin_{nullptr};
  uint32_t last_poll_{0};
  uint8_t rows_{0};
  uint8_t columns_{0};
  uint8_t keys_size_{0};
  bool gpi_events_{true};
};

}  // namespace esphome::tca8418
