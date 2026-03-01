#pragma once
#include <cstdint>
#include <cstddef>

#include "esphome/core/defines.h"
#include "decoder.h"

#ifndef BTHOME_SERVER_MAX_PAYLOAD
#define BTHOME_SERVER_MAX_PAYLOAD 23
#endif

namespace esphome {
namespace bthome {
namespace server {

class BTHomeEncoder {
 public:
  void reset();
  bool write_float(BTHomeObjectType type, float value);
  bool write_bool(BTHomeObjectType type, bool value);
  const uint8_t *get_buffer() const { return this->buffer_; }
  size_t get_size() const { return this->offset_; }
  size_t get_remaining() const { return BTHOME_SERVER_MAX_PAYLOAD - this->offset_; }

 protected:
  bool write_raw_(BTHomeObjectType type, const uint8_t *data, size_t length);

  uint8_t buffer_[BTHOME_SERVER_MAX_PAYLOAD]{};
  size_t offset_{0};
};

}  // namespace server
}  // namespace bthome
}  // namespace esphome
