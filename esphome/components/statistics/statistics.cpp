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

//////////////////////////
// Public Class Methods //
//////////////////////////

void StatisticsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Statistics Component:");

  LOG_SENSOR("  ", "Source Sensor:", this->source_sensor_);

  if (this->window_type_ == WINDOW_TYPE_SLIDING) {
    ESP_LOGCONFIG(TAG, "  Statistics Type: sliding");
    ESP_LOGCONFIG(TAG, "  Window Size: %u", this->window_size_);
  } else if (this->window_type_ == WINDOW_TYPE_CHUNKED_SLIDING) {
    ESP_LOGCONFIG(TAG, "  Statistics Type: chunked_sliding");
    ESP_LOGCONFIG(TAG, "  Chunks in Window: %u", this->window_size_);
    if (this->chunk_size_) {
      ESP_LOGCONFIG(TAG, "  Measurements per Chunk: %u", this->chunk_size_);
    } else {
      ESP_LOGCONFIG(TAG, "  Duration of Chunk: %u ms", this->chunk_duration_);
    }
  } else if (this->window_type_ == WINDOW_TYPE_CONTINUOUS) {
    ESP_LOGCONFIG(TAG, "  Statistics Type: continuous");
    ESP_LOGCONFIG(TAG, "  Chunks Before Reset: %u", this->window_size_);
    if (this->chunk_size_) {
      ESP_LOGCONFIG(TAG, "  Measurements per Chunk: %u", this->chunk_size_);
    } else {
      ESP_LOGCONFIG(TAG, "  Duration of Chunk: %u ms", this->chunk_duration_);
    }
  } else if (this->window_type_ == WINDOW_TYPE_CHUNKED_CONTINUOUS) {
    ESP_LOGCONFIG(TAG, "  Statistics Type: chunked_continuous");
    ESP_LOGCONFIG(TAG, "  Measurements Before Reset: %u", this->window_size_);
  }

  ESP_LOGCONFIG(TAG, "  Send Every: %u", this->send_every_);

  if (this->average_type_ == SIMPLE_AVERAGE) {
    ESP_LOGCONFIG(TAG, "  Average Type: simple");
  } else {
    ESP_LOGCONFIG(TAG, "  Average Type: time_weighted");
  }

  ESP_LOGCONFIG(TAG, "  Time Unit: %s", LOG_STR_ARG(time_conversion_factor_to_string(this->time_conversion_factor_)));

  if (this->restore_) {
    ESP_LOGCONFIG(TAG, "  Restore Hash: %u", this->hash_);
  }

  this->dump_enabled_sensors_();
}

void StatisticsComponent::setup() {
  EnabledAggregatesConfiguration config = this->determine_enabled_statistics_config_();

  if ((this->window_type_ == WINDOW_TYPE_SLIDING) || (this->window_type_ == WINDOW_TYPE_CHUNKED_SLIDING)) {
    this->queue_ = new DABALiteQueue();
  } else if (this->window_type_ == WINDOW_TYPE_CHUNKED_CONTINUOUS) {
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

  // On every source sensor update, call handle_new_value_()
  this->source_sensor_->add_on_state_callback([this](float value) -> void { this->handle_new_value_(value); });

  // Ensure the first sensor update is when configured
  this->set_first_at(this->send_every_ - this->send_at_chunks_counter_);
}

void StatisticsComponent::force_publish() {
  Aggregate aggregate_to_publish = this->queue_->compute_current_aggregate();
  aggregate_to_publish = aggregate_to_publish.combine_with(this->running_chunk_aggregate_);

  this->publish_and_save_(aggregate_to_publish, this->time_->timestamp_now());
}

void StatisticsComponent::reset() {
  this->queue_->clear();
  this->running_window_duration_ = 0;  // reset the duration of measurements in queue

  this->running_chunk_aggregate_ = Aggregate();  // reset the running aggregate to the identity/null measurement
  this->running_chunk_count_ = 0;                // reset the running chunk count

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

  ////////////////////////////////////////////////
  // Evict elements or reset queue if too large //
  ////////////////////////////////////////////////

  //  If window_size_ == 0, then there is a continuous queue with no automatic reset, so never evict/clear
  if (this->window_size_ > 0) {
    while (this->queue_->size() >= this->window_size_) {
      this->queue_->evict();  // evict is equivalent to clearing the queue for ContinuousQueue and ContinuousSingular
    }
  }

  // If the window_reset_duration_ == 0, then this is a sliding window queue or a continuous queue that does not reset
  // by duration
  if (this->window_reset_duration_ > 0) {
    if (this->running_window_duration_ >= this->window_reset_duration_) {
      this->reset();
    }
  }

  ////////////////////////////////////////////
  // Aggregate new value into running chunk //
  ////////////////////////////////////////////

  this->running_chunk_aggregate_ = this->running_chunk_aggregate_.combine_with(
      Aggregate(insert_value, duration_since_last_measurement, now_timestamp, now_time), this->is_time_weighted_());

  ++this->running_chunk_count_;
  this->running_chunk_duration_ += duration_since_last_measurement;

  this->running_window_duration_ += duration_since_last_measurement;

  ////////////////////////////
  // Add new chunk to queue //
  ////////////////////////////

  if (this->is_running_chunk_ready_()) {
    this->queue_->insert(this->running_chunk_aggregate_);

    // Reset counters and chunk to a null measurement
    this->running_chunk_aggregate_ = Aggregate();
    this->running_chunk_count_ = 0;
    this->running_chunk_duration_ = 0;

    ++this->send_at_chunks_counter_;
  }

  ////////////////////////////////////
  // Publish and save sensor values //
  ////////////////////////////////////

  // If send_every_ == 0, then automatic publication is disabled for a continuous queue
  if (this->send_every_ > 0) {
    if (this->send_at_chunks_counter_ >= this->send_every_) {
      // Ensure sensors update at the configured rate
      //  - send_at_chunks_counter_ counts the number of chunks inserted into the appropriate queue
      //  - after send_every_ chunks, each sensor updates

      this->send_at_chunks_counter_ = 0;  // reset send_at_chunks_counter_

      this->publish_and_save_(this->queue_->compute_current_aggregate(), now_time);
    }
  }
}

void StatisticsComponent::publish_and_save_(Aggregate value, time_t time) {
  ////////////////////////////////////////////////
  // Publish new states for all enabled sensors //
  ////////////////////////////////////////////////

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

  if (this->since_argmax_sensor_)
    this->since_argmax_sensor_->publish_state(time - value.get_argmax());

  if (this->since_argmin_sensor_)
    this->since_argmin_sensor_->publish_state(time - value.get_argmin());

  if (this->std_dev_sensor_)
    this->std_dev_sensor_->publish_state(value.compute_std_dev(this->is_time_weighted_(), this->group_type_));

  if (this->trend_sensor_) {
    double trend_ms = value.compute_trend();
    double converted_trend = trend_ms * this->time_conversion_factor_;

    this->trend_sensor_->publish_state(converted_trend);
  }

  //////////////////////////////
  // Save to flash if enabled //
  //////////////////////////////

  if (this->restore_)
    this->pref_.save(&value);
}

/////////////////////////////
// Internal Inline Methods //
/////////////////////////////

inline bool StatisticsComponent::is_time_weighted_() { return (this->average_type_ == TIME_WEIGHTED_AVERAGE); }

inline bool StatisticsComponent::is_running_chunk_ready_() {
  // If the chunk_size_ == 0, then the running chunk resets based on a duration
  if (this->chunk_size_ > 0) {
    if (this->running_chunk_count_ >= this->chunk_size_) {
      return true;
    }
  } else {
    if (this->running_chunk_duration_ >= this->chunk_duration_) {
      if (this->running_chunk_count_ > 0)  // ensure at least one measurement is aggregated in this timespan
        return true;
    }
  }

  return false;
}

}  // namespace statistics
}  // namespace esphome
