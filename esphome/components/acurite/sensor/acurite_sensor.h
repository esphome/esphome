
#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/acurite/acurite.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace acurite {

class AcuRiteSensor : public Component, public AcuRiteDevice {
 public:
  void set_id(uint16_t id) { id_ = id; }
  void update_speed(float value) override;
  void update_direction(float value) override;
  void update_temperature(float value) override;
  void update_humidity(float value) override;
  void update_distance(float value) override;
  void update_rainfall(uint32_t count) override;
  void update_lightning(uint32_t count) override;
  void update_uv(float value) override;
  void update_lux(float value) override;
  void dump_config() override;
  void setup() override;

 protected:
  struct RainfallState {
    uint32_t total{0};
    uint32_t device{0};
  };
  ESPPreferenceObject preferences_;
  RainfallState rainfall_state_;
  uint32_t rainfall_last_{0xFFFFFFFF};
  uint32_t lightning_last_{0xFFFFFFFF};
  float distance_last_{1000};
  float humidity_last_{1000};
  float temperature_last_{1000};
  uint16_t id_{0};

  SUB_SENSOR(direction)
  SUB_SENSOR(speed)
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(distance)
  SUB_SENSOR(rainfall)
  SUB_SENSOR(lightning)
  SUB_SENSOR(uv)
  SUB_SENSOR(lux)
};

}  // namespace acurite
}  // namespace esphome
