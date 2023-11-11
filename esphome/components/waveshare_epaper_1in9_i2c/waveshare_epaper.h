#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

static const unsigned const FRAMEBUFFER_SIZE = 15;
static const unsigned const CHAR_SLOTS = 2;

static const uint8_t const CHAR_EMPTY = 0x00;
static const uint8_t const CHAR_CELSIUS = 0x05;
static const uint8_t const CHAR_FAHRENHEIT = 0x06;
static const uint8_t const CHAR_MINUS_SIGN[CHAR_SLOTS] = {0b01000100, 0b00000};
static const uint8_t const CHAR_DIGITS[10][CHAR_SLOTS] = {
    {0xbf, 0xff},  // 0
    {0x00, 0xff},  // 1
    {0xfd, 0x17},  // 2
    {0xf5, 0xff},  // 3
    {0x47, 0xff},  // 4
    {0xf7, 0x1d},  // 5
    {0xff, 0x1d},  // 6
    {0x21, 0xff},  // 7
    {0xff, 0xff},  // 8
    {0xf7, 0xff},  // 9
};

static const unsigned const TEMPERATURE_DIGITS_LEN = 4;
static const unsigned const HUMIDITY_DIGITS_LEN = 3;

static bool is_naN(float a) { return a != a; }

static void parse_number(float number, int *digits, int digits_count) {
  for (int i = 0; i < digits_count; i++) {
    if (is_naN(number)) {
      digits[i] = -1;
    } else {
      digits[i] = (int) (number / pow(10, (digits_count - i) - 2)) % 10;

      if (digits[i] == 0 && (i == 0 || digits[i - 1] == -1)) {
        digits[i] = -1;
      }
    }
  }
}

static uint8_t get_pixel(int number, int order) { return number == -1 ? CHAR_EMPTY : CHAR_DIGITS[number][order]; }

class WaveShareEPaper1in9I2C : public PollingComponent {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override;

  void create_command_device(i2c::I2CBus *bus, uint8_t address) {
    this->command_device_ = new i2c::I2CDevice();
    this->command_device_->set_i2c_address(address);
    this->command_device_->set_i2c_bus(bus);
    this->command_device_address_ = address;
  }

  void create_data_device(i2c::I2CBus *bus, uint8_t address) {
    this->data_device_ = new i2c::I2CDevice();
    this->data_device_->set_i2c_address(address);
    this->data_device_->set_i2c_bus(bus);
    this->data_device_address_ = address;
  }

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_busy_pin(GPIOPin *busy_pin) { this->busy_pin_ = busy_pin; }
  void set_temperature_for_compensation(float temp) { this->compensation_temp_ = temp; }
  void set_temperature(float temp);
  void set_humidity(float humidity);
  void set_low_power_indicator(bool enabled);
  void set_bluetooth_indicator(bool enabled);
  void apply_temperature_compensation();
  void dump_config() override;

 protected:
  float compensation_temp_ = 20;
  bool potential_refresh_ = false;
  bool inverted_colors_ = false;

  uint8_t framebuffer_[FRAMEBUFFER_SIZE] = {CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY,
                                     CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY,
                                     CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY, CHAR_EMPTY};

  uint8_t degrees_type_ = CHAR_CELSIUS;
  int temperature_digits_[TEMPERATURE_DIGITS_LEN] = {-1, -1, -1, -1};
  bool temperature_positive_ = true;

  int humidity_digits_[HUMIDITY_DIGITS_LEN] = {-1, -1, -1};
  bool humidity_positive_ = true;

  uint8_t command_device_address_;
  i2c::I2CDevice *command_device_;

  uint8_t data_device_address_;
  i2c::I2CDevice *data_device_;
  GPIOPin *reset_pin_;
  GPIOPin *busy_pin_;

  void init_screen();
  void reset_screen();
  void read_busy();
  void write_lut(const uint8_t *lut);
  void write_screen();
  void deep_sleep();
  bool update_framebuffer(uint8_t new_image[FRAMEBUFFER_SIZE]);
};
}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome
