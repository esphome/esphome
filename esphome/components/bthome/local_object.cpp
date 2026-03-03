#include "local_object.h"

namespace esphome {
namespace bthome {
namespace server {

#ifdef USE_SENSOR
size_t BTHomeLocalSensor::get_encoded_size() const {
  if (std::isnan(this->source_->get_state())) {
    return 0;  // Don't include in frame if state is not a number
  }
  return sizeof(BTHomeHeader) + get_bthome_value_length(this->object_type_);
}

bool BTHomeLocalSensor::write(BTHomeEncoder &encoder) const {
  return encoder.write_float(this->object_type_, this->source_->state);
}

void BTHomeLocalSensor::register_immediate_callback(std::function<void()> &&callback) {
  this->source_->add_on_state_callback([callback](float) { callback(); });
}
#endif

#ifdef USE_BINARY_SENSOR
size_t BTHomeLocalBinarySensor::get_encoded_size() const {
  if (!this->source_->has_state()) {
    return 0;  // Don't include in frame if state is not valid
  }
  return sizeof(BTHomeHeader) + get_bthome_value_length(this->object_type_);
}

bool BTHomeLocalBinarySensor::write(BTHomeEncoder &encoder) const {
  return encoder.write_bool(this->object_type_, this->source_->state);
}

void BTHomeLocalBinarySensor::register_immediate_callback(std::function<void()> &&callback) {
  this->source_->add_on_state_callback([callback](bool) { callback(); });
}
#endif

#ifdef USE_TEXT_SENSOR
size_t BTHomeLocalTextSensor::get_encoded_size() const {
  if (!this->source_->has_state())
    return 0;
  size_t content_len = std::min(this->source_->state.size(), this->max_length_);
  return 1 + 1 + content_len;  // type + length_byte + content
}

bool BTHomeLocalTextSensor::write(BTHomeEncoder &encoder) const {
  return encoder.write_text(this->object_type_, this->source_->state.c_str(), this->source_->state.size(),
                            this->max_length_);
}

void BTHomeLocalTextSensor::register_immediate_callback(std::function<void()> &&callback) {
  this->source_->add_on_state_callback([callback](const std::string &) { callback(); });
}
#endif

}  // namespace server
}  // namespace bthome
}  // namespace esphome
