#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/text_sensor/text_sensor.h"
#include "../optolink.h"
#include "../datapoint_component.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

enum TextSensorMode { MAP, RAW, DAY_SCHEDULE, DAY_SCHEDULE_SYNCHRONIZED, DEVICE_INFO, STATE_INFO };

class OptolinkTextSensor : public DatapointComponent,
                           public esphome::text_sensor::TextSensor,
                           public esphome::PollingComponent {
 public:
  OptolinkTextSensor(Optolink *optolink) : DatapointComponent(optolink) {}

  void set_mode(TextSensorMode mode) { mode_ = mode; }
  void set_day_of_week(int dow) { dow_ = dow; }
  void set_entity_id(const std::string &entity_id) { entity_id_ = entity_id; }

 protected:
  void setup() override;
  void update() override;

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(float state) override { publish_state(std::to_string(state)); };
  void datapoint_value_changed(uint8_t state) override { publish_state(std::to_string(state)); };
  void datapoint_value_changed(uint16_t state) override { publish_state(std::to_string(state)); };
  void datapoint_value_changed(uint32_t state) override;
  void datapoint_value_changed(uint8_t *state, size_t length) override;

 private:
  TextSensorMode mode_ = MAP;
  int dow_ = 0;
  std::string entity_id_;
};

}  // namespace optolink
}  // namespace esphome

#endif
