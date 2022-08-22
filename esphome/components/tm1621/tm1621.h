#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace tm1621 {

class TM1621Display;

using tm1621_writer_t = std::function<void(TM1621Display &)>;

class TM1621Display : public PollingComponent {
 public:
  void set_writer(tm1621_writer_t &&writer) { this->writer_ = writer; }

  void setup() override;

  void dump_config() override;

  void set_cs_pin(GPIOPin *pin) { cs_pin_ = pin; }
  void set_data_pin(GPIOPin *pin) { data_pin_ = pin; }
  void set_read_pin(GPIOPin *pin) { read_pin_ = pin; }
  void set_write_pin(GPIOPin *pin) { write_pin_ = pin; }

  void display_celsius(bool d) { celsius_ = d; }
  void display_fahrenheit(bool d) { fahrenheit_ = d; }
  void display_humidity(bool d) { humidity_ = d; }
  void display_voltage(bool d) { voltage_ = d; }
  void display_kwh(bool d) { kwh_ = d; }

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

  void display();

 protected:
  void bit_delay_();
  void setup_pins_();
  bool send_command_(uint16_t command);
  bool send_common_(uint8_t common);
  bool send_address_(uint16_t address);
  void stop_();
  int get_command_code_(char *destination, size_t destination_size, const char *needle, const char *haystack);

  GPIOPin *data_pin_;
  GPIOPin *cs_pin_;
  GPIOPin *read_pin_;
  GPIOPin *write_pin_;
  optional<tm1621_writer_t> writer_{};
  char row_[2][12];
  uint8_t state_;
  uint8_t device_;
  bool celsius_;
  bool fahrenheit_;
  bool humidity_;
  bool voltage_;
  bool kwh_;
};

}  // namespace tm1621
}  // namespace esphome
