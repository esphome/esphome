#include "error.h"

#include <cstdio>
#include <string>

#include "error_lookup.h"

namespace esphome::duco {

void DucoError::update() { this->parent_->send(this, 0x30, {this->address_, 0x00, 0x00, 0x04}); }

void DucoError::receive_response(const DucoMessage &message) {
  if (message.size == 0) {
    this->publish_state("No errors");
    return;
  }

  const uint8_t error_count = message.data[0];
  const size_t expected_size = 1 + static_cast<size_t>(error_count) * 5;
  if (error_count == 0 || message.size < expected_size) {
    this->publish_state("No errors");
    return;
  }

  std::string errors;
  for (size_t index = 0; index < error_count; index++) {
    const size_t offset = 1 + index * 5;
    char code[13];
    snprintf(code, sizeof(code), "E%02u.%02u.%02u", static_cast<unsigned>(message.data[offset + 1]),
             static_cast<unsigned>(message.data[offset + 2]), static_cast<unsigned>(message.data[offset + 3]));
    const auto *definition = error_lookup::find(code);
    errors += code;
    errors += "  ";
    errors += definition != nullptr ? definition->description : "Unknown DUCO error";
    errors += '\n';
  }
  this->publish_state(errors);
}

}  // namespace esphome::duco
