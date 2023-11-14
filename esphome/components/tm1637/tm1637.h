#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/time.h"

#include <vector>

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome {
namespace tm1637 {

class TM1637Display;
#ifdef USE_BINARY_SENSOR
class TM1637Key;
#endif

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
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_length(uint8_t length) { this->length_ = length; }

  void display();

#ifdef USE_BINARY_SENSOR
  void loop() override;
  uint8_t get_keys();
  void add_tm1637_key(TM1637Key *tm1637_key) { this->tm1637_keys_.push_back(tm1637_key); }
#endif

  /// Evaluate the strftime-format and print the result at the given position.
  uint8_t strftime(uint8_t pos, const char *format, ESPTime time) __attribute__((format(strftime, 3, 0)));
  /// Evaluate the strftime-format and print the result at position 0.
  uint8_t strftime(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));

 protected:
  void bit_delay_();
  void setup_pins_();
  bool send_byte_(uint8_t b);
  uint8_t read_byte_();
  void start_();
  void stop_();

  GPIOPin *dio_pin_;
  GPIOPin *clk_pin_;
  uint8_t intensity_;
  uint8_t length_;
  bool inverted_;
  optional<tm1637_writer_t> writer_{};
  uint8_t buffer_[6] = {0};
#ifdef USE_BINARY_SENSOR
  std::vector<TM1637Key *> tm1637_keys_{};
#endif
};

#ifdef USE_BINARY_SENSOR
class TM1637Key : public binary_sensor::BinarySensor {
  friend class TM1637Display;

 public:
  void set_keycode(uint8_t key_code) { key_code_ = key_code; }
  void process(uint8_t data) { this->publish_state(static_cast<bool>(data == this->key_code_)); }

 protected:
  uint8_t key_code_{0};
};
#endif

}  // namespace tm1637
}  // namespace esphome
