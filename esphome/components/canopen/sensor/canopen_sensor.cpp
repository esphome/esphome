#include "canopen_sensor.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen.sensor");

void CANopenSensor::dump_config() {
  LOG_SENSOR("", "CANopen sensor", this);
  ESP_LOGCONFIG(TAG, "  Node: %u, object: 0x%04X:%u", this->node_, this->index_, this->subindex_);
  LOG_UPDATE_INTERVAL(this);
}

void CANopenSensor::update() {
  if (this->pending_)
    return;
  this->pending_ = this->parent_->read_sdo(
      this->node_, this->index_, this->subindex_,
      [](void *context, SDOResult result, uint32_t value, uint8_t size) {
        static_cast<CANopenSensor *>(context)->on_result_(result, value, size);
      },
      this);
  if (!this->pending_) {
    this->status_set_warning();
    this->publish_state(NAN);
  }
}

void CANopenSensor::on_result_(SDOResult result, uint32_t value, uint8_t size) {
  this->pending_ = false;
  const uint8_t expected = this->type_ <= ValueType::VALUE_TYPE_INT8    ? 1
                           : this->type_ <= ValueType::VALUE_TYPE_INT16 ? 2
                                                                        : 4;
  if (result != SDOResult::SDO_RESULT_SUCCESS || size != expected) {
    this->status_set_warning();
    this->publish_state(NAN);
    return;
  }
  float state;
  switch (this->type_) {
    case ValueType::VALUE_TYPE_INT8:
      state = sign_extend<8>(value);
      break;
    case ValueType::VALUE_TYPE_INT16:
      state = sign_extend<16>(value);
      break;
    case ValueType::VALUE_TYPE_INT32:
      state = static_cast<int32_t>(value);
      break;
    case ValueType::VALUE_TYPE_FLOAT32:
      std::memcpy(&state, &value, sizeof(state));
      break;
    default:
      state = value;
  }
  this->status_clear_warning();
  this->publish_state(state);
}

}  // namespace esphome::canopen
