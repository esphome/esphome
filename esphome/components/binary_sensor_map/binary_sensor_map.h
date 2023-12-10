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

/** Class to map one or more binary_sensors to one Sensor.
 *
 * Each binary sensor has configured parameters that each mapping type uses to compute the single numerical result
 */
class BinarySensorMap : public sensor::Sensor, public Component {
 public:
  void dump_config() override;

  /**
   * The loop calls the configured type processing method
   *
   * The processing method loops through all sensors and calculates the numerical result
   * The result is only published if a binary sensor state has changed or, for some types, on initial boot
   */
  void loop() override;

  /**
   * Add binary_sensors to the group when only one parameter is needed for the configured mapping type.
   *
   * @param *sensor The binary sensor.
   * @param value  The value this binary_sensor represents
   */
  void add_channel(binary_sensor::BinarySensor *sensor, float value);

  /**
   * Add binary_sensors to the group when two parameters are needed for the Bayesian mapping type.
   *
   * @param *sensor The binary sensor.
   * @param prob_given_true Probability this observation is on when the Bayesian event is true
   * @param prob_given_false Probability this observation is on when the Bayesian event is false
   */
  void add_channel(binary_sensor::BinarySensor *sensor, float prob_given_true, float prob_given_false);

  void set_sensor_type(BinarySensorMapType sensor_type) { this->sensor_type_ = sensor_type; }

  void set_bayesian_prior(float prior) { this->bayesian_prior_ = prior; };

 protected:
  std::vector<BinarySensorMapChannel> channels_{};
  BinarySensorMapType sensor_type_{BINARY_SENSOR_MAP_TYPE_GROUP};

  // this allows a max of 64 channels/observations in order to keep track of binary_sensor states
  uint64_t last_mask_{0x00};

  // Bayesian event prior probability before taking into account any observations
  float bayesian_prior_{};

  /**
   * Methods to process the binary_sensor_maps types
   *
   * GROUP: process_group_() averages all the values
   * ADD: process_add_() adds all the values
   * BAYESIAN: process_bayesian_() computes the predicate probability
   * */
  void process_group_();
  void process_sum_();
  void process_bayesian_();

  /**
   * Computes the Bayesian predicate for a specific observation
   * If the sensor state is false, then we use the parameters' probabilities for the observatiosn complement
   *
   * @param sensor_state  State of observation
   * @param prior Prior probability before accounting for this observation
   * @param prob_given_true Probability this observation is on when the Bayesian event is true
   * @param prob_given_false Probability this observation is on when the Bayesian event is false
   * */
  float bayesian_predicate_(bool sensor_state, float prior, float prob_given_true, float prob_given_false);
};

}  // namespace binary_sensor_map
}  // namespace esphome
