#pragma once

#include "esphome/core/component.h"

namespace esphome::text_display {

class TextDisplay : public PollingComponent {
 public:
  void set_dimensions(uint8_t columns, uint8_t rows) {
    this->columns_ = columns;
    this->rows_ = rows;
  }

  void setup() override;
  void update() override;

  virtual void display() = 0;
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

 protected:
  virtual void call_writer() = 0;
  uint8_t columns_;
  uint8_t rows_;
  uint8_t *buffer_{nullptr};
};

}  // namespace esphome::text_display
