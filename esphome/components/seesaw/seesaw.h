#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/gpio_expander/expander_gpio_pin.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::seesaw {

enum SeesawModule : uint8_t {
  SEESAW_STATUS = 0x00,
  SEESAW_GPIO = 0x01,
  SEESAW_ADC = 0x09,
  SEESAW_NEOPIXEL = 0x0E,
  SEESAW_TOUCH = 0x0F,
  SEESAW_ENCODER = 0x11,
};

enum : uint8_t {
  SEESAW_STATUS_HW_ID = 0x01,
  SEESAW_STATUS_VERSION = 0x02,
  SEESAW_STATUS_OPTIONS = 0x03,
  SEESAW_STATUS_TEMP = 0x04,
};

enum : uint8_t {
  SEESAW_ENCODER_INTENSET = 0x10,
  SEESAW_ENCODER_POSITION = 0x30,
  SEESAW_ENCODER_DELTA = 0x40,
};

enum : uint8_t {
  SEESAW_GPIO_DIRSET_BULK = 0x02,
  SEESAW_GPIO_DIRCLR_BULK = 0x03,
  SEESAW_GPIO_BULK = 0x04,
  SEESAW_GPIO_BULK_SET = 0x05,
  SEESAW_GPIO_BULK_CLR = 0x06,
  SEESAW_GPIO_PULLENSET = 0x0B,
  SEESAW_GPIO_PULLENCLR = 0x0C,
};

enum : uint8_t {
  SEESAW_NEOPIXEL_PIN = 0x01,
  SEESAW_NEOPIXEL_SPEED = 0x02,
  SEESAW_NEOPIXEL_BUF_LENGTH = 0x03,
  SEESAW_NEOPIXEL_BUF = 0x04,
  SEESAW_NEOPIXEL_SHOW = 0x05,
};

enum : uint8_t {
  SEESAW_TOUCH_CHANNEL_OFFSET = 0x10,
};

enum : uint8_t {
  SEESAW_ADC_CHANNEL_OFFSET = 0x07,
};

class Seesaw : public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

  void enable_encoder(uint8_t number);
  bool get_encoder_position(uint8_t number, int32_t &position);
  bool get_encoder_delta(uint8_t number, int32_t &delta);
  bool get_touch_value(uint8_t channel, uint16_t &value);
  bool get_temperature(float &value);
  bool analog_read(uint8_t pin, uint16_t &value);
  // GPIO expander interface, used by SeesawGPIOPin
  void pin_mode(uint8_t pin, gpio::Flags flags);
  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool state);
  void setup_neopixel(uint8_t pin, uint16_t num_leds);
  /// Send the pixel bytes, already in the strip's GRB order, and show them.
  void show_neopixel(const uint8_t *data, uint16_t len);

 protected:
  i2c::ErrorCode write_(SeesawModule mod, uint8_t reg, const uint8_t *data = nullptr, size_t len = 0) {
    return this->write_register16(encode_uint16(mod, reg), data, len);
  }
  template<typename T> i2c::ErrorCode write_value_(SeesawModule mod, uint8_t reg, T value) {
    const auto bytes = decode_value(value);
    return this->write_(mod, reg, bytes.data(), bytes.size());
  }
  // The chip needs time between the register write and the read
  bool readbuf_(SeesawModule mod, uint8_t reg, uint8_t *buf, uint8_t len, uint32_t wait_us = 250);

  uint32_t version_{0};
  uint32_t options_{0};
  // Last GPIO levels read, returned when a read fails
  uint32_t gpio_levels_{0};
  // Loop pass of the last GPIO read; every pin read in the same pass shares it
  uint32_t gpio_read_time_{0};
  uint8_t cpuid_{0};
  bool gpio_read_valid_{false};
};

using SeesawGPIOPin = gpio_expander::ExpanderGPIOPin<Seesaw>;

/// For a child's setup(): marks the child failed when the hub failed, so it stops using the bus.
inline bool seesaw_hub_failed(Component *child, const Seesaw *hub) {
  if (!hub->is_failed())
    return false;
  child->mark_failed(LOG_STR("Seesaw hub failed"));
  return true;
}

}  // namespace esphome::seesaw

ESPHOME_EXPANDER_PIN_SUMMARY_DECL(esphome::seesaw::Seesaw);
