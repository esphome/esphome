#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

#include <vector>

namespace esphome {
namespace combination {

class CombinationComponent : public Component, public sensor::Sensor {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  /// @brief Logs all source sensor's names
  virtual void log_source_sensors() = 0;

 protected:
  /// @brief Logs the sensor for use in dump_config
  /// @param combo_type Name of the combination operation
  void log_config_(const LogString *combo_type);
};

/// @brief Base class for operations that do not require an extra parameter to compute the combination
class CombinationNoParameterComponent : public CombinationComponent {
 public:
  /// @brief Adds a callback to each source sensor
  void setup() override;

  void add_source(Sensor *sensor);

  /// @brief Computes the combination
  /// @param value Newest sensor measurement
  virtual void handle_new_value(float value) = 0;

  /// @brief Logs all source sensor's names in sensors_
  void log_source_sensors() override;

 protected:
  std::vector<Sensor *> sensors_;
};

// Base class for opertions that require one parameter to compute the combination
class CombinationOneParameterComponent : public CombinationComponent {
 public:
  void add_source(Sensor *sensor, std::function<float(float)> const &stddev);
  void add_source(Sensor *sensor, float stddev);

  /// @brief Logs all source sensor's names in sensor_pairs_
  void log_source_sensors() override;

 protected:
  std::vector<std::pair<Sensor *, std::function<float(float)>>> sensor_pairs_;
};

class KalmanCombinationComponent : public CombinationOneParameterComponent {
 public:
  void dump_config() override;
  void setup() override;

  void set_process_std_dev(float process_std_dev) {
    this->update_variance_value_ = process_std_dev * process_std_dev * 0.001f;
  }
  void set_std_dev_sensor(Sensor *sensor) { this->std_dev_sensor_ = sensor; }

 protected:
  void update_variance_();
  void correct_(float value, float stddev);

  // Optional sensor for publishing the current error
  sensor::Sensor *std_dev_sensor_{nullptr};

  // Tick of the last update
  uint32_t last_update_{0};
  // Change of the variance, per ms
  float update_variance_value_{0.f};

  // Best guess for the state and its variance
  float state_{NAN};
  float variance_{INFINITY};
};

class LinearCombinationComponent : public CombinationOneParameterComponent {
 public:
  void dump_config() override { this->log_config_(LOG_STR("linear")); }
  void setup() override;

  void handle_new_value(float value);
};

class MaximumCombinationComponent : public CombinationNoParameterComponent {
 public:
  void dump_config() override { this->log_config_(LOG_STR("max")); }

  void handle_new_value(float value) override;
};

class MeanCombinationComponent : public CombinationNoParameterComponent {
 public:
  void dump_config() override { this->log_config_(LOG_STR("mean")); }

  void handle_new_value(float value) override;
};

class MedianCombinationComponent : public CombinationNoParameterComponent {
 public:
  void dump_config() override { this->log_config_(LOG_STR("median")); }

  void handle_new_value(float value) override;
};

class MinimumCombinationComponent : public CombinationNoParameterComponent {
 public:
  void dump_config() override { this->log_config_(LOG_STR("min")); }

  void handle_new_value(float value) override;
};

class MostRecentCombinationComponent : public CombinationNoParameterComponent {
 public:
  void dump_config() override { this->log_config_(LOG_STR("most_recently_updated")); }

  void handle_new_value(float value) override;
};

class RangeCombinationComponent : public CombinationNoParameterComponent {
 public:
  void dump_config() override { this->log_config_(LOG_STR("range")); }

  void handle_new_value(float value) override;
};

class SumCombinationComponent : public CombinationNoParameterComponent {
 public:
  void dump_config() override { this->log_config_(LOG_STR("sum")); }

  void handle_new_value(float value) override;
};

}  // namespace combination
}  // namespace esphome
