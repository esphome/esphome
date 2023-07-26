/*
 * ESPHome component to compute several summary statistics for a set of measurements from a sensor in an efficient
 * (computationally and memory-wise) manner while being numerically stable and accurate. The set of measurements can be
 * collected over a sliding window or as a resettable running total. Each measurement can have equal weight or
 * have weights given by their duration.
 *
 * Available statistics as sensors
 *  - count: number of valid measurements in the window (component ignores NaN values in the window)
 *  - duration: the duration in milliseconds between the first and last measurement's timestamps
 *  - min: minimum of the set of measurements
 *  - mean: average of the set of measurements
 *  - max: maximum of the set of measurements
 *  - since_argmax: the time in seconds since the most recent maximum value
 *  - since_argmin: the time in seconds since the most recent minimum value
 *  - std_dev: sample or population standard deviation of the set of measurements
 *  - trend: the slope of the line of best fit for the measurement values versus timestamps
 *      - can be used as an approximation for the rate of change (derivative) of the measurements
 *      - computed using the covariance of timestamps versus measurements and the variance of timestamps
 *
 * Terms and definitions used in this component:
 *  - measurement: a single reading from a sensor
 *  - observation: a single reading from a sensor
 *  - set of measurements: a collection of measurements from a sensor
 *    - it can be the null set; i.e., it does not contain a measurement
 *  - summary statistic: a numerical value that summarizes a set of measurements
 *  - Aggregate: a collection of summary statistics for a set of measurements
 *  - to aggregate: adding a measurement to the set of measurements and updating the Aggregate to include the new
 *    measurement
 *  - queue: a set of Aggregates that can compute all summary statistics for all Aggregates in the set combined
 *  - to insert: adding an Aggregate to a queue
 *  - to evict: remove the oldest Aggregate from a queue
 *  - chunk: an Aggregate that specifically aggregates incoming measurements and is inserted into a queue
 *  - chunk size: the number of measurements to Aggregate in a chunk before it inserted into a queue
 *  - chunk duration: the timespan between the first and last measurement in a chunk before being inserted into a queue
 *  - sliding window queue: a queue that can insert new chunks and evict the oldest chunks
 *  - sliding window aggregate: an Aggregate that stores the summary statistics for all Aggregates in a sliding
 *    window queue
 *  - continuous queue: a queue that can only insert new Aggregates and reset
 *  - continuous aggregate: an Aggregate that stores the summary statistics for all Aggregates in a continuous queue
 *  - simple average: every measurement has equal weight when computing summary statistics
 *  - time-weighted average: each measurement is weighted by the time until the next observed measurement
 *  - window: a sliding window queue or a continuous queue
 *
 * Component code structure: (see specific header files for more detailed descriptions):
 *  - statistics.h - Statistics is a class that sets up the component by allocating memory for a configured queue and
 *    handles new measurements
 *  - automation.h - Handles the ESPHome automation for resetting the window
 *  - aggregate.h - Aggregate is a class that stores a collection of summary statistics and can combine two aggregates
 *    into one
 *  - aggregate_queue.h - AggregateQueue is a class that allocates memory for a set of Aggregates for the enabled
 *    sensors, as well as store and retrieve Aggregates from the memory
 *  - daba_lite_queue.h - DABALiteQueue is a child of AggregateQueue. It implements the De-Amortized Banker's Aggregator
 *    (DABA) Lite algorithm for sliding window queues
 *  - continuous_queue.h - ContinuousQueue is a child of AggregateQueue. It stores Aggregates and combines them when
 *    they have the same number of measurements. Numerically stable for long-term aggregation of measurements in a
 *    continuous queue, but not as efficient computationally or memory-wise
 *  - continous_singular.h - ContinuousSingular is a child of AggregateQueue. It stores a single running Aggregate.
 *    Memory and computationally efficient for continuous aggregates, but is not numerically stable for long-term
 *    aggregation of measurements.
 *
 * Implemented by Kevin Ahrendt for the ESPHome project, June and July of 2023
 */

#pragma once

#include "aggregate.h"
#include "aggregate_queue.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"

#include <limits>  // necessary for std::numeric_limits infinity

namespace esphome {
namespace statistics {

enum AverageType {
  SIMPLE_AVERAGE,
  TIME_WEIGHTED_AVERAGE,
};

enum WindowType {
  WINDOW_TYPE_SLIDING,
  WINDOW_TYPE_CONTINUOUS,
  WINDOW_TYPE_CONTINUOUS_LONG_TERM,
};

enum TimeConversionFactor {
  FACTOR_MS = 1,          // timestamps already are in ms
  FACTOR_S = 1000,        // 1000 ms per second
  FACTOR_MIN = 60000,     // 60000 ms per minute
  FACTOR_HOUR = 3600000,  // 3600000 ms per hour
  FACTOR_DAY = 86400000,  // 86400000 ms per day
};

class StatisticsComponent : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void dump_config() override;

  /// @brief Determine which statistics need to be stored, set up the appropriate queue, and configure various
  /// options.
  void setup() override;

  /// @brief Reset the window by clearing it.
  void reset();

  /// @brief Forces sensor updates using the current aggregate from the queue combined with the running chunk aggregate.
  void force_publish();

  // source sensor of measurements
  void set_source_sensor(sensor::Sensor *source_sensor) { this->source_sensor_ = source_sensor; }

  // sensors for aggregate statistics
  void set_count_sensor(sensor::Sensor *count_sensor) { this->count_sensor_ = count_sensor; }
  void set_duration_sensor(sensor::Sensor *duration_sensor) { this->duration_sensor_ = duration_sensor; }
  void set_max_sensor(sensor::Sensor *max_sensor) { this->max_sensor_ = max_sensor; }
  void set_mean_sensor(sensor::Sensor *mean_sensor) { this->mean_sensor_ = mean_sensor; }
  void set_min_sensor(sensor::Sensor *min_sensor) { this->min_sensor_ = min_sensor; }
  void set_since_argmax_sensor(sensor::Sensor *argmax_sensor) { this->since_argmax_sensor_ = argmax_sensor; }
  void set_since_argmin_sensor(sensor::Sensor *argmin_sensor) { this->since_argmin_sensor_ = argmin_sensor; }
  void set_std_dev_sensor(sensor::Sensor *std_dev_sensor) { this->std_dev_sensor_ = std_dev_sensor; }
  void set_trend_sensor(sensor::Sensor *trend_sensor) { this->trend_sensor_ = trend_sensor; }

  void set_window_size(size_t window_size) { this->window_size_ = window_size; }
  void set_window_duration(size_t duration) { this->window_duration_ = duration; }

  void set_send_every(size_t send_every) { this->send_every_ = send_every; }
  void set_first_at(size_t send_first_at) { this->send_at_chunks_counter_ = send_first_at; }

  void set_chunk_size(size_t size) { this->chunk_size_ = size; }
  void set_chunk_duration(uint32_t time_delta) { this->chunk_duration_ = time_delta; }

  void set_average_type(AverageType type) { this->average_type_ = type; }
  void set_group_type(GroupType type) { this->group_type_ = type; }
  void set_window_type(WindowType type) { this->window_type_ = type; }
  void set_time_conversion_factor(TimeConversionFactor conversion_factor) {
    this->time_conversion_factor_ = conversion_factor;
  }

  void set_hash(const std::string &config_id) { this->hash_ = fnv1_hash("statistics_component_" + config_id); }
  void set_restore(bool restore) { this->restore_ = restore; }

 protected:
  // source sensor of measurement data
  sensor::Sensor *source_sensor_{nullptr};

  // sensors for aggregate statistics from measurements in window
  sensor::Sensor *count_sensor_{nullptr};
  sensor::Sensor *duration_sensor_{nullptr};
  sensor::Sensor *max_sensor_{nullptr};
  sensor::Sensor *mean_sensor_{nullptr};
  sensor::Sensor *min_sensor_{nullptr};
  sensor::Sensor *since_argmax_sensor_{nullptr};
  sensor::Sensor *since_argmin_sensor_{nullptr};
  sensor::Sensor *std_dev_sensor_{nullptr};
  sensor::Sensor *trend_sensor_{nullptr};

  time::RealTimeClock *time_;

  AggregateQueue *queue_{nullptr};

  Aggregate running_chunk_aggregate_{};

  uint32_t hash_{};

  size_t window_size_{std::numeric_limits<size_t>::max()};
  uint64_t window_duration_{std::numeric_limits<uint64_t>::max()};  // max duration of measurements in window

  size_t chunk_size_{std::numeric_limits<size_t>::max()};  // number of measurements aggregated in a chunk before
                                                           // being inserted into the queue
  uint64_t chunk_duration_{std::numeric_limits<uint64_t>::max()};  // duration of measurements agggregated in a
                                                                   // chunk before being inserted into the queue

  size_t send_every_{};
  size_t send_at_chunks_counter_{};

  uint64_t running_window_duration_{0};  // duration of measurements currently stored in the running window

  size_t running_chunk_count_{0};       // number of measurements currently stored in the running aggregate chunk
  uint32_t running_chunk_duration_{0};  // duration of measurements currently stored in the running aggregate chunk

  AverageType average_type_{};                     // either simple or time-weighted
  GroupType group_type_{};                         // measurements come from either a population or a sample
  WindowType window_type_{};                       // type of queue to store measurements/chunks in
  TimeConversionFactor time_conversion_factor_{};  // time unit conversion trend

  // If the Aggregates are time-weighted, store info about the previous observation
  float previous_value_{NAN};
  uint32_t previous_timestamp_{0};

  bool restore_{false};
  ESPPreferenceObject pref_;

  /// @brief  Use log_sensor to dump information about all enabled statistics sensors.
  void dump_enabled_sensors_();

  /** Enable the statistics needed for the configured sensors.
   *
   * @return EnabledAggregatesConfiguration for AggregateQueue children classes
   */
  EnabledAggregatesConfiguration determine_enabled_statistics_config_();

  /// @brief Insert new sensor measurements and update sensors.
  void handle_new_value_(float value);

  /** Publish sensor values and save to flash memory (if configured).
   *
   * Saves value to flash memory only if <restore_> is true.
   * @param value aggregate value to published and saved
   * @param timestamp current UTC Unix time (in seconds)
   */
  void publish_and_save_(Aggregate value, time_t timestamp);

  /// @brief Return if averages are weighted by measurement duration.
  inline bool is_time_weighted_();
};

}  // namespace statistics
}  // namespace esphome
