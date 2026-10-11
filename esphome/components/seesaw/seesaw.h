#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::seesaw {

enum SeesawModule : uint8_t {
  SEESAW_STATUS = 0x00,
  SEESAW_GPIO = 0x01,
  SEESAW_SERCOM0 = 0x02,
  SEESAW_TIMER = 0x08,
  SEESAW_ADC = 0x09,
  SEESAW_DAC = 0x0A,
  SEESAW_INTERRUPT = 0x0B,
  SEESAW_DAP = 0x0C,
  SEESAW_EEPROM = 0x0D,
  SEESAW_NEOPIXEL = 0x0E,
  SEESAW_TOUCH = 0x0F,
  SEESAW_KEYPAD = 0x10,
  SEESAW_ENCODER = 0x11,
  SEESAW_SPECTRUM = 0x12,
  SEESAW_SOIL = 0x13,
};

enum : uint8_t {
  SEESAW_STATUS_HW_ID = 0x01,
  SEESAW_STATUS_VERSION = 0x02,
  SEESAW_STATUS_OPTIONS = 0x03,
  SEESAW_STATUS_TEMP = 0x04,
  SEESAW_STATUS_SWRST = 0x7F,
};

enum : uint8_t {
  SEESAW_ENCODER_STATUS = 0x00,
  SEESAW_ENCODER_INTENSET = 0x10,
  SEESAW_ENCODER_INTENCLR = 0x20,
  SEESAW_ENCODER_POSITION = 0x30,
  SEESAW_ENCODER_DELTA = 0x40,
};

enum : uint8_t {
  SEESAW_GPIO_DIRSET_BULK = 0x02,
  SEESAW_GPIO_DIRCLR_BULK = 0x03,
  SEESAW_GPIO_BULK = 0x04,
  SEESAW_GPIO_BULK_SET = 0x05,
  SEESAW_GPIO_BULK_CLR = 0x06,
  SEESAW_GPIO_BULK_TOGGLE = 0x07,
  SEESAW_GPIO_INTENSET = 0x08,
  SEESAW_GPIO_INTENCLR = 0x09,
  SEESAW_GPIO_INTFLAG = 0x0A,
  SEESAW_GPIO_PULLENSET = 0x0B,
  SEESAW_GPIO_PULLENCLR = 0x0C,
};

enum : uint8_t {
  SEESAW_NEOPIXEL_STATUS = 0x00,
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
  SEESAW_ADC_STATUS = 0x00,
  SEESAW_ADC_INTEN = 0x02,
  SEESAW_ADC_INTENCLR = 0x03,
  SEESAW_ADC_WINMODE = 0x04,
  SEESAW_ADC_WINTHRESH = 0x05,
  SEESAW_ADC_CHANNEL_OFFSET = 0x07,
};

enum : uint8_t {
  SEESAW_SOIL_STATUS = 0x00,
  SEESAW_SOIL_RATE = 0x01,
  SEESAW_SOIL_VALUE = 0x10,
  SEESAW_SOIL_SAMPLES = 0x20,
  SEESAW_SOIL_XDELAY = 0x30,
  SEESAW_SOIL_TIMEOUT = 0x40,
};

class Seesaw : public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

  void enable_encoder(uint8_t number);
  bool get_encoder_position(uint8_t number, int32_t *position);
  int16_t get_touch_value(uint8_t channel);
  float get_temperature();
  void set_pinmode(uint8_t pin, uint8_t mode);
  uint16_t analog_read(uint8_t pin);
  bool digital_read(uint8_t pin);
  void digital_write(uint8_t pin, bool state);
  void set_gpio_interrupt(uint32_t pin, bool enabled);
  void setup_neopixel(int pin, uint16_t num_leds);
  void color_neopixel(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
  void update_neopixel();

 protected:
  i2c::ErrorCode write8_(SeesawModule mod, uint8_t reg, uint8_t value);
  i2c::ErrorCode write16_(SeesawModule mod, uint8_t reg, uint16_t value);
  i2c::ErrorCode write32_(SeesawModule mod, uint8_t reg, uint32_t value);
  i2c::ErrorCode readbuf_(SeesawModule mod, uint8_t reg, uint8_t *buf, uint8_t len, int wait_us = 250);

  uint8_t cpuid_;
  uint32_t version_;
  uint32_t options_;
};

class SeesawGPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  size_t dump_summary(char *buffer, size_t len) const override;

  void set_parent(Seesaw *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }
  gpio::Flags get_flags() const override { return this->flags_; }

 protected:
  Seesaw *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

}  // namespace esphome::seesaw
