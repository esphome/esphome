#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace tm1637 {

class TM1637Display;

using tm1637_writer_t = std::function<void(TM1637Display &)>;

class TM1637Display : public PollingComponent {
 public:
  void set_writer(tm1637_writer_t &&writer) { this->writer_ = writer; }

  void setup() override;

  void dump_config() override;

  void set_clk_pin(GPIOPin *pin) { clk_pin_ = pin; }
  void set_dio_pin(GPIOPin *pin) { dio_pin_ = pin; }

  float get_setup_priority() const override;

  void update() override;

  /// Evaluate the printf-format and print the result at the given position.
  uint8_t printf(uint8_t pos, const char *format, ...) __attribute__((format(printf, 3, 4)));
  /// Evaluate the printf-format and print the result at position 0.
  uint8_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

  /// Print `str` at the given position.
  uint8_t print(uint8_t pos, const char *str);
  /// Print `str` at position 0.
  uint8_t print(const char *str);

  void set_intensity(uint8_t intensity) { this->intensity_ = intensity; }

  void display();

#ifdef USE_TIME
  /// Evaluate the strftime-format and print the result at the given position.
  uint8_t strftime(uint8_t pos, const char *format, time::ESPTime time) __attribute__((format(strftime, 3, 0)));

  /// Evaluate the strftime-format and print the result at position 0.
  uint8_t strftime(const char *format, time::ESPTime time) __attribute__((format(strftime, 2, 0)));
#endif

 protected:
  void bit_delay_();
  void setup_pins_();
  bool send_byte_(uint8_t b);
  void start_();
  void stop_();

  GPIOPin *dio_pin_;
  GPIOPin *clk_pin_;
  uint8_t intensity_;
  optional<tm1637_writer_t> writer_{};
  uint8_t buffer_[6] = {0};
};

}  // namespace tm1637
}  // namespace esphome
