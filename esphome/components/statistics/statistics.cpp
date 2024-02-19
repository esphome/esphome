#include "statistics.h"

#include "aggregate.h"
#include "daba_lite_queue.h"
#include "continuous_singular.h"
#include "continuous_queue.h"

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

#include <cinttypes>

namespace esphome {
namespace statistics {

static const char *const TAG = "statistics";
static const uint32_t NEVER_BOUND = 4294967295UL;  // uint32_t maximum

static const LogString *weight_type_to_string(WeightType type) {
  if (type == WEIGHT_TYPE_SIMPLE)
    return LOG_STR("simple");

  return LOG_STR("duration");
}

static const LogString *group_type_to_string(GroupType type) {
  if (type == GROUP_TYPE_SAMPLE)
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

static const LogString *statistic_type_to_string(StatisticType type) {
  switch (type) {
    case STATISTIC_COUNT:
      return LOG_STR("Count Sensor:");
    case STATISTIC_DURATION:
      return LOG_STR("Duration Sensor:");
    case STATISTIC_MAX:
      return LOG_STR("Max Sensor:");
    case STATISTIC_MEAN:
      return LOG_STR("Mean Sensor:");
    case STATISTIC_MIN:
      return LOG_STR("Min Sensor:");
    case STATISTIC_LAMBDA:
      return LOG_STR("Lambda Sensor:");
    case STATISTIC_QUADRATURE:
      return LOG_STR("Quadrature Sensor:");
    case STATISTIC_SINCE_ARGMAX:
      return LOG_STR("Since Argmax Sensor:");
    case STATISTIC_SINCE_ARGMIN:
      return LOG_STR("Since Argmin Sensor:");
    case STATISTIC_STD_DEV:
      return LOG_STR("Standard Deviation Sensor:");
    case STATISTIC_TREND:
      return LOG_STR("Trend Sensor:");
    default:
      return LOG_STR("");
  }
}

//////////////////////////
// Public Class Methods //
//////////////////////////

void StatisticsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Statistics Component:");

  ESP_LOGCONFIG(TAG, "  Source Sensor: %s", this->source_sensor_->get_name().c_str());

  ESP_LOGCONFIG(TAG, "  Average Type: %s",
                LOG_STR_ARG(weight_type_to_string(this->statistics_calculation_config_.weight_type)));
  ESP_LOGCONFIG(TAG, "  Group Type: %s",
                LOG_STR_ARG(group_type_to_string(this->statistics_calculation_config_.group_type)));

  if (this->restore_) {
    ESP_LOGCONFIG(TAG, "  Restore Hash: %" PRIu32, this->hash_);
  }

  ESP_LOGCONFIG(TAG, "  Window Configuration:");
  ESP_LOGCONFIG(TAG, "    Type: %s", LOG_STR_ARG(window_type_to_string(this->window_type_)));

  if (this->window_size_ < NEVER_BOUND) {
    ESP_LOGCONFIG(TAG, "    Window Size: %zu", this->window_size_);
  } else {
    ESP_LOGCONFIG(TAG, "    Window Size: infinite");
  }

  if (this->chunk_size_ < NEVER_BOUND) {
    ESP_LOGCONFIG(TAG, "    Chunk Size: %zu", this->chunk_size_);
  }

  if (this->chunk_duration_ < NEVER_BOUND) {
    ESP_LOGCONFIG(TAG, "    Chunk Duration: %" PRIu32 " ms", this->chunk_duration_);
  }

  if (this->send_every_ < NEVER_BOUND) {
    ESP_LOGCONFIG(TAG, "    Send Every: %zu", this->send_every_);
  } else {
    ESP_LOGCONFIG(TAG, "    Send Every: never");
  }

  // Log all statistic sensors
  for (const auto &sens : this->sensors_) {
    LOG_SENSOR("  ", LOG_STR_ARG(statistic_type_to_string(sens.type)), sens.sens);
  }
}

void StatisticsComponent::setup() {
  this->running_chunk_aggregate_ = make_unique<Aggregate>(this->statistics_calculation_config_);

  if (this->window_type_ == WINDOW_TYPE_SLIDING) {
    this->queue_ = new DABALiteQueue(this->statistics_calculation_config_);
  } else if (this->window_type_ == WINDOW_TYPE_CONTINUOUS_LONG_TERM) {
    this->queue_ = new ContinuousQueue(this->statistics_calculation_config_);
  } else if (this->window_type_ == WINDOW_TYPE_CONTINUOUS) {
    this->queue_ = new ContinuousSingular(this->statistics_calculation_config_);
  }

  if (!this->queue_->configure_capacity(this->window_size_, this->tracked_statistics_)) {
    ESP_LOGE(TAG, "Failed to allocate memory for statistical aggregates.");
    this->mark_failed();
    return;
  }

  if (this->restore_) {
    this->pref_ = global_preferences->make_preference<Aggregate>(this->hash_);

    Aggregate restored_value(this->statistics_calculation_config_);

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

void StatisticsComponent::add_sensor(sensor::Sensor *sensor, std::function<float(Aggregate, float)> const &func,
                                     StatisticType type) {
  struct StatisticSensorTuple sens = {
      .sens = sensor,
      .lambda_fnctn = func,
      .type = type,
  };
  this->sensors_.emplace_back(sens);
}

// Automations

void StatisticsComponent::add_on_update_callback(std::function<void(Aggregate, float)> &&callback) {
  this->callback_.add(std::move(callback));
}

void StatisticsComponent::force_publish() {
  Aggregate aggregate_to_publish = this->queue_->compute_current_aggregate();

  if ((this->window_type_ == WINDOW_TYPE_CONTINUOUS) || (this->window_type_ == WINDOW_TYPE_CONTINUOUS_LONG_TERM))
    aggregate_to_publish += *this->running_chunk_aggregate_;

  this->publish_and_save_(aggregate_to_publish);
}

void StatisticsComponent::reset() {
  this->queue_->clear();

  this->running_chunk_aggregate_->clear();
  this->measurements_in_running_chunk_count_ = 0;  // reset the running chunk count

  this->send_at_chunks_counter_ = 0;  // reset the inserted chunks counter
}

//////////////////////
// Internal Methods //
//////////////////////

void StatisticsComponent::handle_new_value_(float value) {
  //////////////////////////////////////////////
  // Prepare incoming values to be aggregated //
  //////////////////////////////////////////////

  uint32_t now_timestamp = millis();               // milliseconds since boot
  time_t now_time = this->time_->timestamp_now();  // UTC Unix time

  uint32_t duration_since_last_measurement = now_timestamp - this->previous_timestamp_;

  float insert_value = value;

  // If statistics are weighted by duration, then insert the previous value, as it has a duration of
  // duration_since_last_measurement
  if (this->statistics_calculation_config_.weight_type == WEIGHT_TYPE_DURATION) {
    insert_value = this->previous_value_;
  }

  this->previous_timestamp_ = now_timestamp;
  this->previous_value_ = value;

  ////////////////////////////////////////////
  // Aggregate new value into running chunk //
  ////////////////////////////////////////////

  if (std::isnan(insert_value))
    return;

  Aggregate new_aggregate = this->queue_->null_aggregate();
  new_aggregate.add_measurement(insert_value, duration_since_last_measurement, now_timestamp, now_time);
  *this->running_chunk_aggregate_ += new_aggregate;

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
  // This function is called either by a configured interval of chunk_duration_ length or by handle_new_value_()

  ///////////////////////
  // Insert into queue //
  ///////////////////////

  this->queue_->insert(*this->running_chunk_aggregate_);
  ++this->send_at_chunks_counter_;

  ///////////////////////////////////////////////////////////////////////////////
  // Reset running_chunk_aggregate to a null measurement and reset the counter //
  ///////////////////////////////////////////////////////////////////////////////

  this->running_chunk_aggregate_->clear();
  this->measurements_in_running_chunk_count_ = 0;

  /////////////
  // Publish //
  /////////////

  if ((this->send_at_chunks_counter_ + 1) > this->send_every_) {
    // Uses counter_ + 1 to rollover and never send if send_every_ is size_t max
    // Ensure sensors update at the configured rate
    //  - send_at_chunks_counter_ counts the number of chunks inserted into the queue
    //  - after send_every_ chunks, each sensor updates

    this->send_at_chunks_counter_ = 0;  // reset send_at_chunks_counter_

    this->publish_and_save_(this->queue_->compute_current_aggregate());
  }

  //////////////////////////////////////////////////
  // Evict elements or reset queue if at capacity //
  //////////////////////////////////////////////////

  while ((this->queue_->size() + 1) > this->window_size_) {
    // Uses size() + 1 to rollover and never evict if window_size_ is size_t max
    this->queue_->evict();  // evict is equivalent to clearing the queue for ContinuousQueue and ContinuousSingular
  }
}

void StatisticsComponent::publish_and_save_(Aggregate value) {
  ////////////////////////////////////////////////
  // Publish new states for all enabled sensors //
  ////////////////////////////////////////////////

  // Only publish if we have a measurement stored in value
  if (value.get_count() > 0) {
    // Publish updates for all statistic sensors
    for (const auto &sens : this->sensors_) {
      const auto func = sens.lambda_fnctn;
      sens.sens->publish_state(func(value, this->source_sensor_->state));
    }

    // Save results to flash if configured
    if (this->restore_) {
      this->pref_.save(&value);
    }

    // Execute trigger callbacks
    this->callback_.call(value, this->source_sensor_->state);
  }
}

}  // namespace statistics
}  // namespace esphome
