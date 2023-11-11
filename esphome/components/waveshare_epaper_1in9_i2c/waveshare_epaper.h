#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

#define CELSIUS 0x05
#define FAHRENHEIT 0x06
#define DOT 0b0000000000100000
#define PERCENT 0b0000000000100000
#define LOW_POWER_ON 0b0000000000010000
#define LOW_POWER_OFF 0b1111111111101111

#define BT_ON 0b0000000000001000
#define BT_OFF 0b1111111111110111

namespace esphome {
namespace waveshare_epaper_1in9_i2c {

class WaveShareEPaper1in9I2C : public PollingComponent {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override;

  void create_command_device(i2c::I2CBus *bus, uint8_t address) {
    this->command_device_ = new i2c::I2CDevice();
    this->command_device_->set_i2c_address(address);
    this->command_device_->set_i2c_bus(bus);
  }

  void create_data_device(i2c::I2CBus *bus, uint8_t address) {
    this->data_device_ = new i2c::I2CDevice();
    this->data_device_->set_i2c_address(address);
    this->data_device_->set_i2c_bus(bus);
  }

  void set_rst_pin(GPIOPin *rst_pin) { this->rst_pin_ = rst_pin; }
  void set_busy_pin(GPIOPin *busy_pin) { this->busy_pin_ = busy_pin; }
  void set_temperature(float temp);
  void set_humidity(float humidity);
  void set_low_power_indicator(bool isLowPower);
  void set_bluetooth_indicator(bool isBluetooth);

 protected:
  bool potential_refresh = false;
  bool inverted_colors = false;

  unsigned char image[15] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  unsigned char degrees_type = CELSIUS;
  int temperature_digits[4] = {-1, -1, -1, -1};
  bool temperature_positive = true;

  int humidity_digits[3] = {-1, -1, -1};
  bool humidity_positive = true;

  i2c::I2CDevice *command_device_;
  i2c::I2CDevice *data_device_;
  GPIOPin *rst_pin_;
  GPIOPin *busy_pin_;

  void init();
  void reset();
  void read_busy();
  void lut_DU_WB();
  void lut_5S();
  void temperature_compensation();
  void write_screen(unsigned char *image);
  void deep_sleep();
  bool set_data(unsigned char new_image[15]);

  bool isNaN(float a) { return a != a; }

  void parseNumber(float number, int *digits, int digitsCount) {
    for (int i = 0; i < digitsCount; i++) {
      if (isNaN(number)) {
        digits[i] = -1;
      } else {
        digits[i] = (int) (number / pow(10, (digitsCount - i) - 2)) % 10;

        if (digits[i] == 0 && (i == 0 || digits[i - 1] == -1)) {
          digits[i] = -1;
        }
      }
    }
  }

  const unsigned char EMPTY = 0x00;
  const unsigned char MINUS_SIGN[2] = {0b01000100, 0b00000};

  const unsigned char DIGITS[10][2] = {
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

  unsigned char getPixel(int number, int order) { return number == -1 ? EMPTY : DIGITS[number][order]; }
};
}  // namespace waveshare_epaper_1in9_i2c
}  // namespace esphome
