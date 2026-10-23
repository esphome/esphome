#include "sensor.h"

namespace esphome::duco {

void DucoSensor::update() {
  switch (this->function_) {
    case FUNCTION_NODE_DATA:
      this->parent_->send(this, FUNCTION_NODE_DATA, {0x01, this->argument_, 0x00, 0x49, 0x04});
      break;
    case FUNCTION_PARAMETER:
      this->parent_->send(this, FUNCTION_PARAMETER, {0x00, this->argument_, 0x09});
      break;
    default:
      this->parent_->send(this, FUNCTION_NODE, {0x02, this->argument_});
      break;
  }
}

bool DucoSensor::receive_response(const DucoMessage &message) {
  if (message.function != this->function_ + 2)
    return false;
  const bool wide = this->format_ != DucoValueFormat::DUCO_VALUE_FORMAT_UINT8;
  if (message.size < this->offset_ + (wide ? 2 : 1))
    return true;

  const uint8_t *field = message.data + this->offset_;
  int32_t value = field[0];
  if (this->format_ == DucoValueFormat::DUCO_VALUE_FORMAT_UINT16) {
    value = encode_uint16(field[1], field[0]);
  } else if (this->format_ == DucoValueFormat::DUCO_VALUE_FORMAT_INT16) {
    value = static_cast<int16_t>(encode_uint16(field[1], field[0]));
  }
  if (value >= this->minimum_ && value <= this->maximum_)
    this->publish_state(static_cast<float>(value) / this->divisor_);
  return true;
}

}  // namespace esphome::duco
