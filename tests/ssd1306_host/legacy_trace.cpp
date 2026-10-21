// Requires the isolated headers and flags from script/test_ssd1306_partial.py.
#ifdef SSD1306_STANDALONE_TEST

#include <iomanip>
#include <iostream>
#include <vector>
#include "esphome/components/ssd1306_i2c/ssd1306_i2c.h"
using esphome::ssd1306_i2c::I2CSSD1306;
using namespace esphome::ssd1306_base;

class TraceBus : public esphome::i2c::I2CBus {
 public:
  std::vector<uint8_t> bytes;
  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t length, uint8_t *, size_t) override {
    this->bytes.push_back(address);
    this->bytes.push_back(static_cast<uint8_t>(length));
    this->bytes.push_back(static_cast<uint8_t>(length >> 8));
    if (length)
      this->bytes.insert(this->bytes.end(), data, data + length);
    return esphome::i2c::ERROR_OK;
  }
};
class TracePin : public esphome::GPIOPin {
 public:
  unsigned setups{0};
  std::vector<bool> levels;
  void setup() override { ++this->setups; }
  void digital_write(bool level) override { this->levels.push_back(level); }
};
int main() {
  for (unsigned model = 0; model < SSD1306_MODEL_COUNT; model++) {
    for (unsigned variant = 0; variant < 8; variant++) {
      TraceBus bus;
      TracePin pin;
      I2CSSD1306 display;
      display.set_i2c_bus(&bus);
      display.set_i2c_address(0x3C);
      display.set_model(static_cast<SSD1306Model>(model));
      display.set_reset_pin(&pin);
      display.set_external_vcc(variant & 1);
      display.init_flip_x(variant & 2);
      display.init_flip_y(variant & 4);
      display.init_invert(variant & 1);
      display.init_contrast(variant / 8.0f);
      display.init_brightness(0.75f);
      display.init_offset_x(static_cast<uint8_t>(variant * 7));
      display.init_offset_y(static_cast<uint8_t>(variant * 3));
      display.setup();
      display.fill(esphome::Color::WHITE);
      display.update();
      display.set_contrast(0.5f);
      display.set_invert(false);
      display.turn_off();
      display.turn_on();
      std::cout << std::dec << model << ':' << variant << ' ';
      for (uint8_t value : bus.bytes)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(value);
      std::cout << ':' << std::dec << pin.setups << ':';
      for (bool level : pin.levels)
        std::cout << level;
      std::cout << '\n';
    }
  }
}

#endif  // SSD1306_STANDALONE_TEST
