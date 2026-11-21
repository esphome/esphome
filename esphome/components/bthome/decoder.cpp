#include "decoder.h"

#include "esphome/core/log.h"

namespace esphome::bthome {

ESPHOME_LOG_TAG(TAG, "bthome");

static uint16_t read_uint16_le(const uint8_t *data) { return (uint16_t) data[0] | ((uint16_t) data[1] << 8); }

static uint32_t read_uint24_le(const uint8_t *data) {
  return (uint32_t) data[0] | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16);
}

static uint32_t read_uint32_le(const uint8_t *data) {
  return (uint32_t) data[0] | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

static int16_t read_sint16_le(const uint8_t *data) { return (int16_t) read_uint16_le(data); }

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

StringRef BTHomeObject::as_string() const { return StringRef((const char *) this->data, this->length); }

BTHomePayloadDecoder::Iterator::Iterator(const uint8_t *ptr, size_t remaining) : ptr_(ptr), remaining_(remaining) {
  this->parse_next_();
}

BTHomeObject BTHomePayloadDecoder::Iterator::operator*() const { return current_obj_; }

BTHomePayloadDecoder::Iterator &BTHomePayloadDecoder::Iterator::operator++() {
  this->parse_next_();
  return *this;
}

bool BTHomePayloadDecoder::Iterator::operator!=(const Iterator &other) const { return ptr_ != other.ptr_; }

void BTHomePayloadDecoder::Iterator::stop_() {
  this->ptr_ = nullptr;
  this->remaining_ = 0;
}

void BTHomePayloadDecoder::Iterator::parse_next_() {
  if (this->remaining_ == 0) {
    this->stop_();
    return;
  }

  const auto obj_type = static_cast<BTHomeObjectType>(*this->ptr_++);
  this->remaining_--;

  size_t value_length;
  switch (obj_type) {
    case BTHomeObjectType::TEXT:
    case BTHomeObjectType::RAW:
      // A length byte, then the value. A length of 0 is a valid empty value.
      if (this->remaining_ == 0) {
        this->stop_();
        return;
      }
      value_length = *this->ptr_++;
      this->remaining_--;
      break;
    case BTHomeObjectType::COMMAND:
      // The value keeps its length byte and opcode, followed by the arguments.
      if (this->remaining_ == 0) {
        this->stop_();
        return;
      }
      value_length = BTHOME_COMMAND_HEADER_SIZE + (*this->ptr_ & BTHOME_COMMAND_ARG_LENGTH_MASK);
      break;
    default:
      value_length = get_bthome_value_length(obj_type);
      if (value_length == 0) {
        // Unknown type: its length is unknown, so nothing after it can be read.
        this->stop_();
        return;
      }
      break;
  }

  if (this->remaining_ < value_length) {
    this->stop_();
    return;
  }

  if (obj_type < this->current_obj_.type) {
    ESP_LOGVV(TAG, "BTHome objects not in ascending order");
  }

  this->current_obj_ = {obj_type, this->ptr_, value_length};
  this->ptr_ += value_length;
  this->remaining_ -= value_length;
}

BTHomePayloadDecoder::BTHomePayloadDecoder(const uint8_t *payload, size_t size) : payload_(payload), size_(size) {}

BTHomePayloadDecoder::Iterator BTHomePayloadDecoder::begin() const { return Iterator(payload_, size_); }
BTHomePayloadDecoder::Iterator BTHomePayloadDecoder::end() const { return Iterator(nullptr, 0); }

}  // namespace esphome::bthome
