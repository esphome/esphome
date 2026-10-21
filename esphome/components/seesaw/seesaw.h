#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
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
  bool get_encoder_position(uint8_t number, int32_t *position);
  void set_encoder_position(uint8_t number, int32_t position);
  int16_t get_touch_value(uint8_t channel);
  float get_temperature();
  void set_pinmode(uint8_t pin, gpio::Flags mode);
  uint16_t analog_read(uint8_t pin);
  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool state);
  void setup_neopixel(int pin, uint16_t num_leds);
  void color_neopixel(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
  void update_neopixel();

 protected:
  i2c::ErrorCode write8_(SeesawModule mod, uint8_t reg, uint8_t value);
  i2c::ErrorCode write16_(SeesawModule mod, uint8_t reg, uint16_t value);
  i2c::ErrorCode write32_(SeesawModule mod, uint8_t reg, uint32_t value);
  i2c::ErrorCode readbuf_(SeesawModule mod, uint8_t reg, uint8_t *buf, uint8_t len, int wait_us = 250);

  uint32_t version_{0};
  uint32_t options_{0};
  // Last GPIO levels read, returned when a read fails
  uint32_t gpio_levels_{0};
  // Loop pass of the last GPIO read; every pin read in the same pass shares it
  uint32_t gpio_read_time_{0};
  uint8_t cpuid_{0};
  bool gpio_read_valid_{false};
};

class SeesawGPIOPin : public GPIOPin, public Parented<Seesaw> {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }
  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{gpio::FLAG_NONE};
};

}  // namespace esphome::seesaw
