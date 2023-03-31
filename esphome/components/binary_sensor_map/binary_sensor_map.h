#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

#include <vector>

namespace esphome {
namespace binary_sensor_map {

enum BinarySensorMapType {
  BINARY_SENSOR_MAP_TYPE_GROUP,
  BINARY_SENSOR_MAP_TYPE_SUM,
  BINARY_SENSOR_MAP_TYPE_BAYESIAN,
};

struct BinarySensorMapChannel {
  binary_sensor::BinarySensor *binary_sensor;
  union {
    float sensor_value;
    struct {
      float given_true;
      float given_false;
    } probabilities;
  } parameters;
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

  /**
   * Add binary_sensors to the group when only parameter is needed for the calculation type.
   * Each binary_sensor represents a float value when its state is true
   *
   * @param *sensor The binary sensor.
   * @param value  The value this binary_sensor represents
   */
  void add_channel(binary_sensor::BinarySensor *sensor, float value);

  /**
   * Add binary_sensors to the group when two parameters are needed for the Bayesian calculation type.
   * The state of each binary_sensor deci
   *
   * @param *sensor The binary sensor.
   * @param prob_given_true Probability that this binary sensor is true when when the Bayesian sensor overall should be
   * true
   * @param prob_given_false Probability that this binary sensor is true when the Bayesian sensor overall should be
   * false
   */
  void add_channel(binary_sensor::BinarySensor *sensor, float prob_given_true, float prob_given_false);

  void set_sensor_type(BinarySensorMapType sensor_type) { this->sensor_type_ = sensor_type; }

  void set_bayesian_prior(float prior) { this->bayesian_prior_ = prior; };

 protected:
  std::vector<BinarySensorMapChannel> channels_{};
  BinarySensorMapType sensor_type_{BINARY_SENSOR_MAP_TYPE_GROUP};

  // this gives max 64 channels per binary_sensor_map
  uint64_t last_mask_{0x00};

  // Bayesian prior probability before taking into account sensor states
  float bayesian_prior_{};

  /**
   * methods to process the types of binary_sensor_maps
   * GROUP: process_group_() averages all the values
   * ADD: process_add_() adds all the values
   * BAYESIAN: process_bayesian_() computes the predicate probability
   * */
  void process_group_();
  void process_sum_();
  void process_bayesian_();

  /**
   * @brief
   *
   * @param sensor_state
   * @param prior
   * @param prob_given_true
   * @param prob_given_false
   * @return float
   * */
  float bayesian_predicate_(bool sensor_state, float prior, float prob_given_true, float prob_given_false);
};

}  // namespace binary_sensor_map
}  // namespace esphome
