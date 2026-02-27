#pragma once
#include "../bthome_handler.h"
#include "../bthome_local_sensor.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bthome {

class BTHomeSensor : public BTHomeObjectHandler, public esphome::sensor::Sensor, public Component {
 public:
  bool process_object(const BTHomeObject &object) override;
};

namespace server {
class BTHomeLocalSensor : public BTHomeLocalBase {
 public:
  void set_source(sensor::Sensor *source) { this->source_ = source; }
  bool write(BTHomeEncoder &encoder) const override {
    return encoder.write_float(this->object_type_, this->source_->state);
  }
  void register_immediate_callback(std::function<void()> &&callback) override {
    this->source_->add_on_state_callback([callback](float) { callback(); });
  }

 protected:
  sensor::Sensor *source_{nullptr};
};
}  // namespace server
}  // namespace bthome

}  // namespace esphome
