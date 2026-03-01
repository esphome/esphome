#include "decoder.h"

#include "esphome/core/log.h"

namespace esphome {
namespace bthome {

static const char *const TAG = "bthome";

static uint16_t read_uint16_le(const uint8_t *data) { return (uint16_t) data[0] | ((uint16_t) data[1] << 8); }

static uint32_t read_uint24_le(const uint8_t *data) {
  return (uint32_t) data[0] | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16);
}

static uint32_t read_uint32_le(const uint8_t *data) {
  return (uint32_t) data[0] | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

static int16_t read_sint16_le(const uint8_t *data) { return (int16_t) read_uint16_le(data); }

static int32_t read_sint32_le(const uint8_t *data) { return (int32_t) read_uint32_le(data); }

float BTHomeObject::scaling_factor() const { return bthome_scaling_factor(this->type); }

bool BTHomeObject::is_signed() const { return bthome_is_signed(this->type); }

uint32_t BTHomeObject::as_uint() const {
  switch (length) {
    case 1:
      return data[0];
    case 2:
      return read_uint16_le(data);
    case 3:
      return read_uint24_le(data);
    case 4:
      return read_uint32_le(data);
    default:
      return 0.0f;
  }
}

int32_t BTHomeObject::as_int() const {
  switch (length) {
    case 1:
      return (int8_t) data[0];
    case 2:
      return read_sint16_le(data);
    case 3:
      return read_uint24_le(data);
    case 4:
      return read_uint32_le(data);
    default:
      return 0.0f;
  }
}

float BTHomeObject::as_float() const { return scaling_factor() * (is_signed() ? float(as_int()) : float(as_uint())); }

bool BTHomeObject::as_bool() const { return as_uint() != 0; }

std::string_view BTHomeObject::as_string() const { return std::string_view((const char *) data, length); }

BTHomePayloadDecoder::Iterator::Iterator(const uint8_t *ptr, size_t remaining) : ptr_(ptr), remaining_(remaining) {
  this->parse_next_();
}

BTHomeObject BTHomePayloadDecoder::Iterator::operator*() const { return current_obj_; }

BTHomePayloadDecoder::Iterator &BTHomePayloadDecoder::Iterator::operator++() {
  this->parse_next_();
  return *this;
}

bool BTHomePayloadDecoder::Iterator::operator!=(const Iterator &other) const { return ptr_ != other.ptr_; }

void BTHomePayloadDecoder::Iterator::parse_next_() {
  if (remaining_ == 0) {
    ptr_ = nullptr;
    return;
  }

  const uint8_t *start = ptr_;
  BTHomeObjectType obj_type = static_cast<BTHomeObjectType>(*ptr_++);
  remaining_--;

  size_t value_length = 0;
  if (obj_type == BTHomeObjectType::TEXT || obj_type == BTHomeObjectType::RAW) {  // variable-size objects
    if (remaining_ == 0) {
      ptr_ = nullptr;
      remaining_ = 0;
      return;
    }
    value_length = *ptr_++;
    remaining_--;
  } else {
    value_length = get_bthome_value_length(obj_type);
    if (value_length == 0) {
      ptr_ = nullptr;  // Invalid type, stop iteration
      remaining_ = 0;
      return;
    }
  }

  if (remaining_ < value_length || value_length == 0) {
    ptr_ = nullptr;
    remaining_ = 0;
    return;
  }

  if (obj_type < current_obj_.type) {
    ESP_LOGVV(TAG, "BTHome objects not in ascending order");
  }

  current_obj_ = {obj_type, ptr_, value_length};
  ptr_ += value_length;
  remaining_ -= value_length;
}

BTHomePayloadDecoder::BTHomePayloadDecoder(const uint8_t *payload, size_t size) : payload_(payload), size_(size) {}

BTHomePayloadDecoder::Iterator BTHomePayloadDecoder::begin() const { return Iterator(payload_, size_); }
BTHomePayloadDecoder::Iterator BTHomePayloadDecoder::end() const { return Iterator(nullptr, 0); }

}  // namespace bthome
}  // namespace esphome
