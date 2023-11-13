#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/text_sensor/text_sensor.h"
#include "../optolink.h"
#include "../optolink_sensor_base.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

enum TextSensorMode { MAP, RAW, DAY_SCHEDULE, DAY_SCHEDULE_SYNCHRONIZED, DEVICE_INFO, STATE_INFO };

class OptolinkTextSensor : public OptolinkSensorBase,
                           public esphome::text_sensor::TextSensor,
                           public esphome::PollingComponent {
 public:
  OptolinkTextSensor(Optolink *optolink) : OptolinkSensorBase(optolink) {}

  void set_mode(TextSensorMode mode) { mode_ = mode; }
  void set_day_of_week(int dow) { dow_ = dow; }
  void set_entity_id(const std::string &entity_id) { entity_id_ = entity_id; }

 protected:
  void setup() override;
  void update() override {
    if (mode_ == STATE_INFO) {
      publish_state(optolink_->get_error());
    } else {
      optolink_->read_value(datapoint_);
    }
  }

  const StringRef &get_component_name() override { return get_name(); }
  void value_changed(float state) override { publish_state(std::to_string(state)); };
  void value_changed(uint8_t state) override { publish_state(std::to_string(state)); };
  void value_changed(uint16_t state) override { publish_state(std::to_string(state)); };
  void value_changed(uint32_t state) override;
  void value_changed(uint8_t *state, size_t length) override;

 private:
  TextSensorMode mode_ = MAP;
  int dow_ = 0;
  std::string entity_id_;
};

}  // namespace optolink
}  // namespace esphome

#endif
