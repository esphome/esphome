#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/text_sensor/text_sensor.h"
#include "optolink.h"
#include "optolink_sensor_base.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class OptolinkDeviceInfoSensor : public esphome::text_sensor::TextSensor, public esphome::PollingComponent {
 public:
  OptolinkDeviceInfoSensor(const std::string &name, Optolink *optolink) {
    optolink_ = optolink;
    set_name(name.c_str());
    set_update_interval(1800000);
    set_entity_category(esphome::ENTITY_CATEGORY_DIAGNOSTIC);
  }

 protected:
  void setup() override;
  void update() override { optolink_->read_value(datapoint_); }

 private:
  Optolink *optolink_;
  IDatapoint *datapoint_;
};

}  // namespace optolink
}  // namespace esphome

#endif
