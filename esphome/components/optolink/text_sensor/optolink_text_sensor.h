#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/text_sensor/text_sensor.h"
#include "../optolink.h"
#include "../datapoint_component.h"

namespace esphome {
namespace optolink {

enum TextSensorType {
  TEXT_SENSOR_TYPE_MAP,
  TEXT_SENSOR_TYPE_RAW,
  TEXT_SENSOR_TYPE_DAY_SCHEDULE,
  TEXT_SENSOR_TYPE_DEVICE_INFO,
  TEXT_SENSOR_TYPE_STATE_INFO
};

class OptolinkTextSensor : public DatapointComponent, public esphome::text_sensor::TextSensor {
 public:
  OptolinkTextSensor(Optolink *optolink) : DatapointComponent(optolink) {}

  void set_type(TextSensorType type) { type_ = type; }
  void set_day_of_week(int dow) { dow_ = dow; }
  void set_entity_id(const std::string &entity_id) { entity_id_ = entity_id; }

 protected:
  void setup() override;
  void update() override;

  const StringRef &get_component_name() override { return get_name(); }
  void datapoint_value_changed(float value) override { publish_state(std::to_string(value)); };
  void datapoint_value_changed(uint8_t value) override { publish_state(std::to_string(value)); };
  void datapoint_value_changed(uint16_t value) override { publish_state(std::to_string(value)); };
  void datapoint_value_changed(uint32_t value) override;
  void datapoint_value_changed(uint8_t *value, size_t length) override;

 private:
  TextSensorType type_ = TEXT_SENSOR_TYPE_MAP;
  int dow_ = 0;
  std::string entity_id_;
};

}  // namespace optolink
}  // namespace esphome

#endif
