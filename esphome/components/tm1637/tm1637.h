#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/esphal.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace tm1637 {

class TM1637Display;

using tm1637_writer_t = std::function<void(TM1637Display &)>;

class TM1637Display : public PollingComponent {
 public:
  void set_writer(tm1637_writer_t &&writer);

  void set_clk_pin(GPIOPin *pin) { clk_pin_ = pin; }
  void set_dio_pin(GPIOPin *pin) { dio_pin_ = pin; }

  float get_setup_priority() const override;

  void setSegments(const uint8_t segments[], uint8_t length = 4, uint8_t pos = 0);
  void setBrightness(uint8_t brightness);
  void clear();
  void showNumberDec(int num, bool leading_zero = false, uint8_t length = 4, uint8_t pos = 0);
  void showNumberDecEx(int num, uint8_t dots = 0, bool leading_zero = false, uint8_t length = 4, uint8_t pos = 0);
  void showNumberHexEx(uint16_t num, uint8_t dots = 0, bool leading_zero = false, uint8_t length = 4, uint8_t pos = 0);
  uint8_t encodeDigit(uint8_t digit);

 protected:
  void setup_pins_();

  void bitDelay();

  void start();
  void stop();

  bool writeByte(uint8_t b);

  void showDots(uint8_t dots, uint8_t *digits);
  void showNumberBaseEx(int8_t base, uint16_t num, uint8_t dots = 0, bool leading_zero = false, uint8_t length = 4,
                        uint8_t pos = 0);

  GPIOPin *dio_pin_;
  GPIOPin *clk_pin_;

 protected:
  uint8_t m_brightness;
  unsigned int m_bitDelay;
  optional<tm1637_writer_t> writer_{};
};

}  // namespace tm1637
}  // namespace esphome
