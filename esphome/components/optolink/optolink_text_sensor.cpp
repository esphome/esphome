#include "optolink_text_sensor.h"
#include "optolink.h"
#include <VitoWiFi.h>

namespace esphome {
namespace optolink {

void OptolinkTextSensor::setup() {
  if (!raw_) {
    setup_datapoint();
  } else {
    datapoint_ = new Datapoint<convRaw>(get_sensor_name().c_str(), "optolink", address_, writeable_);
    datapoint_->setLength(bytes_);
    datapoint_->setCallback([this](const IDatapoint &dp, DPValue dpValue) {
      ESP_LOGD("OptolinkSensorBase", "Datapoint %s - %s: <raw>", dp.getGroup(), dp.getName());
      uint8_t buffer[bytes_ + 1];
      dpValue.getRaw(buffer);
      buffer[bytes_] = 0x0;
      publish_state((char *) buffer);
    });
  }
};

}  // namespace optolink
}  // namespace esphome
