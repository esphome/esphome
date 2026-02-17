#include "terminal.h"
#include <iostream>

namespace esphome::uart_terminal {

void HOT Terminal::display() {
  // Clear the whole screen
  this->write_str("\x1B[2J");
  // Move cursor to row 1, column 1
  this->write_str("\x1B[H");
  for (uint16_t i = 0; i < this->rows_; ++i) {
    this->write_array(&this->buffer_[i * this->columns_], this->columns_);
    this->write_array(reinterpret_cast<const uint8_t *>("\r\n"), 2);
  }
}

void Terminal::setup() { TextDisplay::setup(); }

}  // namespace esphome::uart_terminal
