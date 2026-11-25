#include "tmcm1240_sensor.h"
#include "esphome/core/log.h"

namespace esphome::tmcm1240 {

ESPHOME_LOG_TAG(TAG, "tmcm1240.sensor");

void TMCM1240Sensor::update() {
  if (this->pending_)
    return;
  this->pending_ = true;
  this->parent_->read_sdo(this->index_, 0, on_result, this);
}

void TMCM1240Sensor::on_result(void *context, const canopen::SDOReply &reply) {
  auto *sensor = static_cast<TMCM1240Sensor *>(context);
  sensor->pending_ = false;
  if (!reply.ok() || reply.size != sensor->size_) {
    if (!sensor->status_has_warning()) {
      ESP_LOGW(TAG, "Read 0x%04X failed (result %u, width %u, expected %u)", sensor->index_,
               static_cast<unsigned>(reply.result), reply.size, sensor->size_);
    }
    sensor->status_set_warning();
    sensor->publish_state(NAN);
    return;
  }
  sensor->status_clear_warning();
  sensor->publish_state(sensor->index_ == 0x209E ? static_cast<float>(sign_extend<8>(reply.value))
                                                 : static_cast<float>(reply.value));
}

}  // namespace esphome::tmcm1240
