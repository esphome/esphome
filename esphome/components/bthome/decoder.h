#pragma once
#include <cstdint>
#include <cstddef>
#include <string_view>
#include "bthome.h"

namespace esphome {
namespace bthome {

struct BTHomeObject {
  BTHomeObjectType type = BTHomeObjectType::PACKET_ID;
  const uint8_t *data = nullptr;
  size_t length = 0;
  float scaling_factor() const;
  bool is_signed() const;
  uint32_t as_uint() const;
  int32_t as_int() const;
  float as_float() const;
  bool as_bool() const;
  std::string_view as_string() const;
};

class BTHomePayloadDecoder {
 public:
  class Iterator {
   public:
    Iterator(const uint8_t *ptr, size_t remaining);
    BTHomeObject operator*() const;
    Iterator &operator++();
    bool operator!=(const Iterator &other) const;

   private:
    void parse_next_();
    const uint8_t *ptr_;
    size_t remaining_;
    BTHomeObject current_obj_{};
  };
  BTHomePayloadDecoder(const uint8_t *payload, size_t size);

  Iterator begin() const;
  Iterator end() const;

 private:
  const uint8_t *payload_;
  size_t size_;
};
}  // namespace bthome
}  // namespace esphome
