#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace binary_sensor_map {

enum BinarySensorMapType {
  BINARY_SENSOR_MAP_TYPE_GROUP,
};

struct BinarySensorMapChannel {
  binary_sensor::BinarySensor *binary_sensor;
  float sensor_value;
};

/** Class to group binary_sensors to one Sensor.
 *
 * Each binary sensor represents a float value in the group.
 */
class BinarySensorMap : public sensor::Sensor, public Component {
 public:
  void dump_config() override;
  /**
   * The loop checks all binary_sensor states
   * When the binary_sensor reports a true value for its state, then the float value it represents is added to the
   * total_current_value
   *
   * Only when the total_current_value changed and at least one sensor reports an active state we publish the sensors
   * average value. When the value changed and no sensors ar active we publish NAN.
   * */
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  /** Add binary_sensors to the group.
   * Each binary_sensor represents a float value when its state is true
   *
   * @param *sensor The binary sensor.
   * @param value  The value this binary_sensor represents
   */
  void add_channel(binary_sensor::BinarySensor *sensor, float value);
  void set_sensor_type(BinarySensorMapType sensor_type);

 protected:
  std::vector<BinarySensorMapChannel> channels_{};
  BinarySensorMapType sensor_type_{BINARY_SENSOR_MAP_TYPE_GROUP};
  // this gives max 64 channels per binary_sensor_map
  uint64_t last_mask_{0x00};
  /**
   * methods to process the types of binary_sensor_maps
   * GROUP: process_group_() just map to a value
   * */
  void process_group_();
};

}  // namespace binary_sensor_map
}  // namespace esphome
