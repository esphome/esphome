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
 *  - quadrature: average value of the set of measurements times duration
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
 *  - simple weight type: every measurement has equal weight when computing summary statistics
 *  - duration weight type: each measurement is weighted by the time until the next observed measurement
 *  - window: a sliding window queue or a continuous queue
 *
 * Component code structure: (see specific header files for more detailed descriptions):
 *  - statistics.h - Statistics is a class that sets up the component by allocating memory for a configured queue and
 *    handles new measurements
 *  - automation.h - Handles ESPHome automations
 *  - aggregate.h - Aggregate is a class that stores a collection of summary statistics and can combine two aggregates
 *    into one
 *  - aggregate_queue.h - AggregateQueue is a class that allocates memory for specified track raw statistics and handles
 *    storing and retrieving Aggregates from the memory
 *  - daba_lite_queue.h - DABALiteQueue is a child of AggregateQueue. It implements the De-Amortized Banker's Aggregator
 *    (DABA) Lite algorithm for sliding window queues
 *  - continuous_queue.h - ContinuousQueue is a child of AggregateQueue. It stores Aggregates and combines them when
 *    they have the same number of measurements. Numerically stable for long-term aggregation of measurements in a
 *    continuous queue, but not as efficient computationally or memory-wise
 *  - continous_singular.h - ContinuousSingular is a child of AggregateQueue. It stores a single running Aggregate.
 *    Memory and computationally efficient for continuous aggregation, but is not numerically stable for long-term
 *    aggregation of measurements.
 *
 * Implemented by Kevin Ahrendt for the ESPHome project, 2023
 */

#pragma once

#include "aggregate.h"
#include "aggregate_queue.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace statistics {

enum WindowType {
  WINDOW_TYPE_SLIDING,
  WINDOW_TYPE_CONTINUOUS,
  WINDOW_TYPE_CONTINUOUS_LONG_TERM,
};

enum StatisticType {
  STATISTIC_MEAN,
  STATISTIC_MAX,
  STATISTIC_MIN,
  STATISTIC_STD_DEV,
  STATISTIC_COUNT,
  STATISTIC_DURATION,
  STATISTIC_SINCE_ARGMAX,
  STATISTIC_SINCE_ARGMIN,
  STATISTIC_TREND,
  STATISTIC_QUADRATURE,
};

struct StatisticSensorTuple {
  sensor::Sensor *sens;
  std::function<float(Aggregate)> lambda_fnctn;
  StatisticType type;
};

class StatisticsComponent : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void dump_config() override;

  /// @brief Determine which statistics need to be stored, set up the appropriate queue, and configure various
  /// options.
  void setup() override;

  /// @brief Add a callback that will be called after all sensors have finished updating
  void add_on_update_callback(std::function<void(Aggregate)> &&callback);

  /// @brief Forces sensor updates using the current aggregate from the queue combined with the running chunk aggregate.
  void force_publish();

  /// @brief Reset the window by clearing it.
  void reset();

  /// @brief Source sensor of measurements
  void set_source_sensor(sensor::Sensor *source_sensor) { this->source_sensor_ = source_sensor; }

  /// @brief Configure which raw statistics are tracked in the queue.
  void set_tracked_statistics(TrackedStatisticsConfiguration &&tracked_statistics) {
    this->tracked_statistics_ = tracked_statistics;
  }

  /// @brief Set the time component used for argmax and argmin
  void set_time(time::RealTimeClock *time) { this->time_ = time; }

  // Window configuration
  void set_window_type(WindowType type) { this->window_type_ = type; }
  void set_window_size(size_t window_size) { this->window_size_ = window_size; }
  void set_chunk_size(size_t size) { this->chunk_size_ = size; }
  void set_chunk_duration(uint32_t time_delta) { this->chunk_duration_ = time_delta; }
  void set_send_every(size_t send_every) { this->send_every_ = send_every; }
  void set_first_at(size_t send_first_at) { this->send_at_chunks_counter_ = send_first_at; }

  // Configure statistic calculations
  void set_statistics_calculation_config(StatisticsCalculationConfig &&config) {
    this->statistics_calculation_config_ = config;
  }

  // Configure saving state to flash
  void set_hash(const std::string &config_id) { this->hash_ = fnv1_hash("statistics_component_" + config_id); }
  void set_restore(bool restore) { this->restore_ = restore; }

  /// @brief Add a statistic sensor with the appropriate lambda call to compute it
  void add_sensor(sensor::Sensor *sensor, std::function<float(Aggregate)> const &func, StatisticType type) {
    struct StatisticSensorTuple sens = {
        .sens = sensor,
        .lambda_fnctn = func,
        .type = type,
    };
    this->sensors_.emplace_back(sens);
  }

 protected:
  // Source sensor of measurement data
  sensor::Sensor *source_sensor_{nullptr};

  // Stores added statistic sensors
  std::vector<StatisticSensorTuple> sensors_;

  // Statistics configuration
  TrackedStatisticsConfiguration tracked_statistics_;
  StatisticsCalculationConfig statistics_calculation_config_;

  // Time component for since_argmax and since_argmin
  time::RealTimeClock *time_{nullptr};

  // Storage for callbacks on sensor updates
  CallbackManager<void(Aggregate)> callback_;

  // Stores the Aggregates
  AggregateQueue *queue_{nullptr};

  // Restore continuous aggregate statistics from flash
  ESPPreferenceObject pref_{};
  uint32_t hash_{};
  bool restore_{false};

  // Configuration options for how statistics are calculated
  WindowType window_type_{};  // type of queue to store measurements/chunks in

  // Window configuration options
  size_t window_size_{};

  size_t chunk_size_{};        // number of measurements aggregated in a chunk before
                               // being inserted into the queue
  uint32_t chunk_duration_{};  // duration of measurements agggregated in a
                               // chunk before being inserted into the queue

  size_t send_every_{};
  size_t send_at_chunks_counter_{};

  // Running Aggregate chunk
  std::unique_ptr<Aggregate> running_chunk_aggregate_;
  size_t measurements_in_running_chunk_count_{0};  // number of measurements currently
                                                   // stored in the running Aggregate chunk

  // Store information about the previous observation for duration weighted statistics
  float previous_value_{NAN};
  uint32_t previous_timestamp_{0};

  //////////////////////
  // Internal Methods //
  //////////////////////

  /// @brief Insert new sensor measurements into running chunk and potentially update sensors.
  void handle_new_value_(float value);

  /// @brief Insert running chunk into the AggregateQueue.
  void insert_running_chunk_();

  /** Publish sensor values and save to flash memory (if configured).
   *
   * Saves value to flash memory only if <restore_> is true.
   * After all sensors publish, on_update triggers callbacks.
   * @param value aggregate value to published and saved
   */
  void publish_and_save_(Aggregate value);
};  // namespace statistics

}  // namespace statistics
}  // namespace esphome
