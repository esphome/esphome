#include "tmcm1240_sensor.h"
#include "esphome/core/log.h"

namespace esphome::tmcm1240 {

ESPHOME_LOG_TAG(TAG, "tmcm1240.sensor");

void TMCM1240Sensor::update() {
  if (this->pending_)
    return;
  this->pending_ = this->parent_->read_sdo(this->index_, 0, on_result, this);
  if (!this->pending_)
    on_result(this, canopen::SDOResult::SDO_RESULT_REJECTED, 0, 0);
}

void TMCM1240Sensor::on_result(void *context, canopen::SDOResult result, uint32_t value, uint8_t size) {
  auto *sensor = static_cast<TMCM1240Sensor *>(context);
  sensor->pending_ = false;
  if (result != canopen::SDOResult::SDO_RESULT_SUCCESS || size != sensor->size_) {
    if (!sensor->status_has_warning()) {
      ESP_LOGW(TAG, "Read 0x%04X failed (result %u, width %u, expected %u)", sensor->index_,
               static_cast<unsigned>(result), size, sensor->size_);
    }
    sensor->status_set_warning();
    sensor->publish_state(NAN);
    return;
  }
  sensor->status_clear_warning();
  sensor->publish_state(sensor->index_ == 0x209E ? static_cast<float>(sign_extend<8>(value)) : static_cast<float>(value));
}

}  // namespace esphome::tmcm1240
