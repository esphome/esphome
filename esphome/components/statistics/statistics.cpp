#include "statistics.h"

#include "aggregate.h"
#include "daba_lite_queue.h"
#include "continuous_singular.h"
#include "continuous_queue.h"

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace statistics {

static const char *const TAG = "statistics";
static const uint32_t NEVER_BOUND = 4294967295UL;  // uint32_t maximum

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_CONFIG
static const LogString *time_conversion_factor_to_string(TimeConversionFactor factor) {
  switch (factor) {
    case FACTOR_MS:
      return LOG_STR("milliseconds");
    case FACTOR_S:
      return LOG_STR("seconds");
    case FACTOR_MIN:
      return LOG_STR("minutes");
    case FACTOR_HOUR:
      return LOG_STR("hours");
    case FACTOR_DAY:
      return LOG_STR("days");
    default:
      return LOG_STR("");
  }
}

static const LogString *average_type_to_string(AverageType type) {
  if (type == SIMPLE_AVERAGE)
    return LOG_STR("simple");

  return LOG_STR("time_weighted");
}

static const LogString *group_type_to_string(GroupType type) {
  if (type == SAMPLE_GROUP_TYPE)
    return LOG_STR("sample");

  return LOG_STR("population");
}

static const LogString *window_type_to_string(WindowType type) {
  switch (type) {
    case WINDOW_TYPE_SLIDING:
      return LOG_STR("sliding");
    case WINDOW_TYPE_CONTINUOUS:
      return LOG_STR("continuous");
    case WINDOW_TYPE_CONTINUOUS_LONG_TERM:
      return LOG_STR("continuous_long_term");
    default:
      return LOG_STR("");
  }
}
#endif

//////////////////////////
// Public Class Methods //
//////////////////////////

void StatisticsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Statistics Component:");

  LOG_SENSOR("  ", "Source Sensor:", this->source_sensor_);

  ESP_LOGCONFIG(TAG, "  Average Type: %s", LOG_STR_ARG(average_type_to_string(this->average_type_)));
  ESP_LOGCONFIG(TAG, "  Group Type: %s", LOG_STR_ARG(group_type_to_string(this->group_type_)));

  ESP_LOGCONFIG(TAG, "  Time Unit: %s", LOG_STR_ARG(time_conversion_factor_to_string(this->time_conversion_factor_)));

  if (this->restore_) {
    ESP_LOGCONFIG(TAG, "  Restore Hash: %u", this->hash_);
  }

  ESP_LOGCONFIG(TAG, "  Window Configuration:");
  ESP_LOGCONFIG(TAG, "    Type: %s", LOG_STR_ARG(window_type_to_string(this->window_type_)));

  if (this->window_size_ < NEVER_BOUND) {
    ESP_LOGCONFIG(TAG, "    Window Size: %u", this->window_size_);
  } else {
    ESP_LOGCONFIG(TAG, "    Window Size: infinite");
  }

  if (this->chunk_size_ < NEVER_BOUND) {
    ESP_LOGCONFIG(TAG, "    Chunk Size: %u", this->chunk_size_);
  }

  if (this->chunk_duration_ < NEVER_BOUND) {
    ESP_LOGCONFIG(TAG, "    Chunk Duration: %u ms", this->chunk_duration_);
  }

  if (this->send_every_ < NEVER_BOUND) {
    ESP_LOGCONFIG(TAG, "    Send Every: %u", this->send_every_);
  } else {
    ESP_LOGCONFIG(TAG, "    Send Every: never");
  }

  this->dump_enabled_sensors_();
}

void StatisticsComponent::setup() {
  EnabledAggregatesConfiguration config = this->determine_enabled_statistics_config_();

  if (this->window_type_ == WINDOW_TYPE_SLIDING) {
    this->queue_ = new DABALiteQueue();
  } else if (this->window_type_ == WINDOW_TYPE_CONTINUOUS_LONG_TERM) {
    this->queue_ = new ContinuousQueue();
  } else if (this->window_type_ == WINDOW_TYPE_CONTINUOUS) {
    this->queue_ = new ContinuousSingular();
  }

  if (!this->queue_->set_capacity(this->window_size_, config)) {
    ESP_LOGE(TAG, "Failed to allocate memory for statistical aggregates.");
    this->mark_failed();
  }

  if (this->is_time_weighted_()) {
    this->queue_->enable_time_weighted();
  }

  if (this->restore_) {
    this->pref_ = global_preferences->make_preference<Aggregate>(this->hash_);

    Aggregate restored_value;

    // If a value is successfully loaded, then insert it into the queue
    if (this->pref_.load(&restored_value))
      this->queue_->insert(restored_value);

    // Force publish restored values
    this->force_publish();
  }

  // If chunk_duration is configured, use an interval timer to handle running chunk insertion
  if (this->chunk_duration_ < NEVER_BOUND) {
    this->set_interval(this->chunk_duration_, [this] { this->insert_running_chunk_(); });
  }

  // Ensure the first sensor update is when configured
  this->set_first_at(this->send_every_ - this->send_at_chunks_counter_);

  // On every source sensor update, call handle_new_value_()
  this->source_sensor_->add_on_state_callback([this](float value) -> void { this->handle_new_value_(value); });
}

// Automations

void StatisticsComponent::add_on_update_callback(std::function<void(Aggregate)> &&callback) {
  this->callback_.add(std::move(callback));
}

void StatisticsComponent::force_publish() {
  Aggregate aggregate_to_publish = this->queue_->compute_current_aggregate();

  if ((this->window_type_ == WINDOW_TYPE_CONTINUOUS) || (this->window_type_ == WINDOW_TYPE_CONTINUOUS_LONG_TERM))
    aggregate_to_publish = aggregate_to_publish.combine_with(this->running_chunk_aggregate_);

  this->publish_and_save_(aggregate_to_publish);
}

void StatisticsComponent::reset() {
  this->queue_->clear();

  this->running_chunk_aggregate_ = Aggregate();    // reset the running aggregate to the identity/null measurement
  this->measurements_in_running_chunk_count_ = 0;  // reset the running chunk count

  this->send_at_chunks_counter_ = 0;  // reset the inserted chunks counter
}

//////////////////////
// Internal Methods //
//////////////////////

EnabledAggregatesConfiguration StatisticsComponent::determine_enabled_statistics_config_() {
  EnabledAggregatesConfiguration config;

  if (this->duration_sensor_)
    config.duration = true;

  if (this->max_sensor_) {
    config.max = true;
  }

  if (this->mean_sensor_) {
    config.mean = true;
  }

  if (this->min_sensor_) {
    config.min = true;
  }

  if (this->since_argmax_sensor_) {
    config.max = true;
    config.argmax = true;
  }

  if (this->since_argmin_sensor_) {
    config.min = true;
    config.argmin = true;
  }

  if (this->std_dev_sensor_) {
    config.m2 = true;
    config.mean = true;
  }

  if (this->trend_sensor_) {
    config.c2 = true;
    config.m2 = true;
    config.mean = true;
    config.timestamp_m2 = true;
    config.timestamp_mean = true;
    config.timestamp_reference = true;
  }

  // if averages are time-weighted, then store duration information
  if (this->is_time_weighted_()) {
    config.duration = true;
    config.duration_squared = true;
  }

  return config;
}

void StatisticsComponent::dump_enabled_sensors_() {
  if (this->count_sensor_) {
    LOG_SENSOR("  ", "Count Sensor:", this->count_sensor_);
  }

  if (this->duration_sensor_) {
    LOG_SENSOR("  ", "Duration Sensor:", this->duration_sensor_);
  }

  if (this->max_sensor_) {
    LOG_SENSOR("  ", "Max Sensor:", this->max_sensor_);
  }

  if (this->mean_sensor_) {
    LOG_SENSOR("  ", "Mean Sensor:", this->mean_sensor_);
  }

  if (this->min_sensor_) {
    LOG_SENSOR("  ", "Min Sensor:", this->min_sensor_);
  }

  if (this->since_argmax_sensor_) {
    LOG_SENSOR("  ", "Since Argmax Sensor:", this->since_argmax_sensor_);
  }

  if (this->since_argmin_sensor_) {
    LOG_SENSOR("  ", "Since Argmin Sensor:", this->since_argmin_sensor_);
  }

  if (this->std_dev_sensor_) {
    LOG_SENSOR("  ", "Standard Deviation Sensor:", this->std_dev_sensor_);
  }

  if (this->trend_sensor_) {
    LOG_SENSOR("  ", "Trend Sensor:", this->trend_sensor_);
  }
}

void StatisticsComponent::handle_new_value_(float value) {
  //////////////////////////////////////////////
  // Prepare incoming values to be aggregated //
  //////////////////////////////////////////////

  uint32_t now_timestamp = millis();               // milliseconds since boot
  time_t now_time = this->time_->timestamp_now();  // UTC Unix time

  uint32_t duration_since_last_measurement = now_timestamp - this->previous_timestamp_;

  float insert_value = value;

  // If averages are time-weighted, then insert the previous value, as it has a duration of
  // duration_since_last_measurement
  if (this->is_time_weighted_()) {
    insert_value = this->previous_value_;
  }

  this->previous_timestamp_ = now_timestamp;
  this->previous_value_ = value;

  ////////////////////////////////////////////
  // Aggregate new value into running chunk //
  ////////////////////////////////////////////

  if (std::isnan(insert_value))
    return;

  this->running_chunk_aggregate_ = this->running_chunk_aggregate_.combine_with(
      Aggregate(insert_value, duration_since_last_measurement, now_timestamp, now_time), this->is_time_weighted_());

  ++this->measurements_in_running_chunk_count_;

  ////////////////////////////////////////
  // Add running chunk to queue if full //
  ////////////////////////////////////////

  if ((this->measurements_in_running_chunk_count_ + 1) > this->chunk_size_) {
    // Uses count_ + 1 to rollover and not insert if chunk_size_ is size_t max
    this->insert_running_chunk_();
  }
}

void StatisticsComponent::insert_running_chunk_() {
  // This function is called either by a preset interval of chunk_duration_ length or by handle_new_value_()

  //////////////////////////////////////////////////
  // Evict elements or reset queue if at capacity //
  //////////////////////////////////////////////////

  while ((this->queue_->size() + 1) > this->window_size_) {
    // Uses size() + 1 to rollover and never evict if window_size_ is size_t max
    this->queue_->evict();  // evict is equivalent to clearing the queue for ContinuousQueue and ContinuousSingular
  }

  ///////////////////////
  // Insert into queue //
  ///////////////////////

  this->queue_->insert(this->running_chunk_aggregate_);
  ++this->send_at_chunks_counter_;

  ///////////////////////////////////////////////////////////////////////////////
  // Reset running_chunk_aggregate to a null measurement and reset the coutner //
  ///////////////////////////////////////////////////////////////////////////////

  this->running_chunk_aggregate_ = Aggregate();
  this->measurements_in_running_chunk_count_ = 0;

  /////////////
  // Publish //
  /////////////

  if ((this->send_at_chunks_counter_ + 1) > this->send_every_) {
    // Uses size() + 1 to rollover and never send if send_every_ is size_t max
    // Ensure sensors update at the configured rate
    //  - send_at_chunks_counter_ counts the number of chunks inserted into the queue
    //  - after send_every_ chunks, each sensor updates

    this->send_at_chunks_counter_ = 0;  // reset send_at_chunks_counter_

    this->publish_and_save_(this->queue_->compute_current_aggregate());
  }
}

void StatisticsComponent::publish_and_save_(Aggregate value) {
  ////////////////////////////////////////////////
  // Publish new states for all enabled sensors //
  ////////////////////////////////////////////////

  // Only publish if we have a measurement stored in value
  if (value.get_count() > 0) {
    if (this->count_sensor_)
      this->count_sensor_->publish_state(value.get_count());

    if (this->duration_sensor_)
      this->duration_sensor_->publish_state(value.get_duration());

    if (this->max_sensor_) {
      float max = value.get_max();
      if (std::isinf(max)) {  // default aggregated max for 0 measuremnts is -infinity, switch to NaN for HA
        this->max_sensor_->publish_state(NAN);
      } else {
        this->max_sensor_->publish_state(max);
      }
    }

    if (this->mean_sensor_)
      this->mean_sensor_->publish_state(value.get_mean());

    if (this->min_sensor_) {
      float min = value.get_min();
      if (std::isinf(min)) {  // default aggregated min for 0 measurements is infinity, switch to NaN for HA
        this->min_sensor_->publish_state(NAN);
      } else {
        this->min_sensor_->publish_state(min);
      }
    }

    if (this->since_argmax_sensor_) {
      time_t argmax = value.get_argmax();
      if (argmax == 0) {  // default argmax is a Unix time of 0, so switch to NaN for HA
        this->since_argmax_sensor_->publish_state(NAN);
      } else {
        this->since_argmax_sensor_->publish_state(this->time_->timestamp_now() - argmax);
      }
    }

    if (this->since_argmin_sensor_) {
      time_t argmin = value.get_argmin();
      if (argmin == 0) {  // default argmin value is a Unix time of 0, so switch to NaN for HA
        this->since_argmin_sensor_->publish_state(NAN);
      } else {
        this->since_argmin_sensor_->publish_state(this->time_->timestamp_now() - argmin);
      }
    }

    if (this->std_dev_sensor_)
      this->std_dev_sensor_->publish_state(value.compute_std_dev(this->is_time_weighted_(), this->group_type_));

    if (this->trend_sensor_) {
      double trend_ms = value.compute_trend();
      double converted_trend = trend_ms * this->time_conversion_factor_;

      this->trend_sensor_->publish_state(converted_trend);
    }

    /////////////////////////////////////////
    // Save to flash and execute callbacks //
    /////////////////////////////////////////
    if (this->restore_)
      this->pref_.save(&value);

    this->callback_.call(value);
  }
}

/////////////////////////////
// Internal Inline Methods //
/////////////////////////////

inline bool StatisticsComponent::is_time_weighted_() { return (this->average_type_ == TIME_WEIGHTED_AVERAGE); }

}  // namespace statistics
}  // namespace esphome
