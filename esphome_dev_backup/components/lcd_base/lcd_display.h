#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"

#include <map>
#include <vector>

namespace esphome {
namespace lcd_base {

class LCDDisplay;

class LCDDisplay : public PollingComponent {
 public:
  void set_dimensions(uint8_t columns, uint8_t rows) {
    this->columns_ = columns;
    this->rows_ = rows;
  }

  void set_user_defined_char(uint8_t pos, const std::vector<uint8_t> &data) { this->user_defined_chars_[pos] = data; }

  void setup() override;
  float get_setup_priority() const override;
  void update() override;
  void display();
  //// Clear LCD display
  void clear();

  /// Print the given text at the specified column and row.
  void print(uint8_t column, uint8_t row, const char *str);
  /// Print the given string at the specified column and row.
  void print(uint8_t column, uint8_t row, const std::string &str);
  /// Print the given text at column=0 and row=0.
  void print(const char *str);
  /// Print the given string at column=0 and row=0.
  void print(const std::string &str);
  /// Evaluate the printf-format and print the text at the specified column and row.
  void printf(uint8_t column, uint8_t row, const char *format, ...) __attribute__((format(printf, 4, 5)));
  /// Evaluate the printf-format and print the text at column=0 and row=0.
  void printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

  /// Evaluate the strftime-format and print the text at the specified column and row.
  void strftime(uint8_t column, uint8_t row, const char *format, ESPTime time) __attribute__((format(strftime, 4, 0)));
  /// Evaluate the strftime-format and print the text at column=0 and row=0.
  void strftime(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));

  /// Load custom char to given location
  void loadchar(uint8_t location, uint8_t charmap[]);

 protected:
  virtual bool is_four_bit_mode() = 0;
  virtual void write_n_bits(uint8_t value, uint8_t n) = 0;
  virtual void send(uint8_t value, bool rs) = 0;

  void command_(uint8_t value);
  virtual void call_writer() = 0;

  uint8_t columns_;
  uint8_t rows_;
  uint8_t *buffer_{nullptr};
  std::map<uint8_t, std::vector<uint8_t> > user_defined_chars_;
};

}  // namespace lcd_base
}  // namespace esphome
