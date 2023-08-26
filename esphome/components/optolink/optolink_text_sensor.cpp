#ifdef USE_ARDUINO

#include "optolink_text_sensor.h"
#include "optolink.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

void OptolinkTextSensor::setup() {
  if (mode_ == RAW) {
    datapoint_ = new Datapoint<convRaw>(get_sensor_name().c_str(), "optolink", address_, writeable_);
    datapoint_->setLength(bytes_);
    datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
      uint8_t buffer[bytes_ + 1];
      dp_value.getRaw(buffer);
      buffer[bytes_] = 0x0;
      ESP_LOGD("OptolinkTextSensor", "Datapoint %s - %s: %s", dp.getGroup(), dp.getName(), buffer);
      publish_state((char *) buffer);
    });
  } else if (mode_ == DAY_SCHEDULE) {
    datapoint_ = new Datapoint<convRaw>(get_sensor_name().c_str(), "optolink", address_ + 8 * dow_, writeable_);
    datapoint_->setLength(8);
    datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
      uint8_t data[8];
      dp_value.getRaw(data);
      ESP_LOGD("OptolinkTextSensor", "Datapoint %s - %s", dp.getGroup(), dp.getName());
      char buffer[100];
      for (int i = 0; i < 8; i++) {
        if (data[i] != 0xFF) {
          int hour = data[i] >> 3;
          int minute = (data[i] & 0b111) * 10;
          sprintf(buffer + i * 6, "%02d:%02d ", hour, minute);
        } else {
          sprintf(buffer + i * 6, "      ");
        }
      }
      publish_state(buffer);
    });
  } else {
    setup_datapoint_();
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
