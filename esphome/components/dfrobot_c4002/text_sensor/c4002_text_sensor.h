#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace dfrobot_c4002 {

class C4002TextSensorHub : public Component {
 public:
  void set_text_sensor(text_sensor::TextSensor *ts) { this->text_sensor_ = ts; }

  void publish(const std::string &msg) {
    if (this->text_sensor_ != nullptr) {
      this->text_sensor_->publish_state(msg);
    }
  }

 private:
  text_sensor::TextSensor *text_sensor_{nullptr};
};

}  // namespace dfrobot_c4002
}  // namespace esphome
