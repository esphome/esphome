#pragma once

#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace sevenseg {

class SEVENSEGComponent;

using sevenseg_writer_t = std::function<void(SEVENSEGComponent &)>;

class SEVENSEGComponent : public PollingComponent {
 public:
  void set_writer(sevenseg_writer_t &&writer);

  void setup() override;

  void dump_config() override;

  void update() override;

  float get_setup_priority() const override;

  void display();

  void set_a_pin(GPIOPin *a_pin);
  void set_b_pin(GPIOPin *b_pin);
  void set_c_pin(GPIOPin *c_pin);
  void set_d_pin(GPIOPin *d_pin);
  void set_e_pin(GPIOPin *e_pin);
  void set_f_pin(GPIOPin *f_pin);
  void set_g_pin(GPIOPin *g_pin);
  void set_dp_pin(GPIOPin *dp_pin);
  void set_hold_time(uint16_t hold_time);
  void set_blank_time(uint16_t blank_time);
  void set_digits(const std::vector<GPIOPin *> &digits);

  /// Evaluate the printf-format and print the result at the given position.
  uint8_t printf(uint8_t pos, const char *format, ...) __attribute__((format(printf, 3, 4)));
  /// Evaluate the printf-format and print the result at position 0.
  uint8_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

  /// Print `str` at the given position.
  uint8_t print(uint8_t pos, const char *str);
  /// Print `str` at position 0.
  uint8_t print(const char *str);
  /// Print `str` at position 0.
  uint8_t print(std::string str);

  /// Evaluate the strftime-format and print the result at the given position.
  uint8_t strftime(uint8_t pos, const char *format, ESPTime time) __attribute__((format(strftime, 3, 0)));

  /// Evaluate the strftime-format and print the result at position 0.
  uint8_t strftime(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));

 protected:
  void clear_display_();
  void set_digit_(uint8_t digit, uint8_t value, bool dp);

  GPIOPin *a_pin_{nullptr};
  GPIOPin *b_pin_{nullptr};
  GPIOPin *c_pin_{nullptr};
  GPIOPin *d_pin_{nullptr};
  GPIOPin *e_pin_{nullptr};
  GPIOPin *f_pin_{nullptr};
  GPIOPin *g_pin_{nullptr};
  GPIOPin *dp_pin_{nullptr};
  std::vector<GPIOPin *> digits_;

  uint16_t hold_time_{5};
  uint16_t blank_time_{0};
  uint8_t *buffer_;
  uint16_t buffer_size_{0};
  bool setup_complete_{false};
  uint8_t num_digits_{0};

  optional<sevenseg_writer_t> writer_{};
};

}  // namespace sevenseg
}  // namespace esphome
