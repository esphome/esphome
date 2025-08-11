#if defined(USE_ESP_IDF)

#include "resonate_time_filter.h"

#include <algorithm>
#include <cinttypes>

#include <esp_timer.h>

namespace esphome {
namespace resonate {

ResonateTimeFilter::ResonateTimeFilter(double process_std_dev, double forget_factor) {
  this->process_variance_ = process_std_dev * process_std_dev;
  this->forget_variance_factor_ = forget_factor * forget_factor;

  // Use a queue since accessing the current time element isn't atomic
  this->time_element_queue_ = xQueueCreate(1, sizeof(TimeElement));
};

void ResonateTimeFilter::update(int64_t measurement, int64_t max_error, int64_t time_added) {
  double dt = time_added - this->last_update_;
  this->last_update_ = time_added;

  const double update_std_dev = max_error / 2.0;  // A Kalman filter assumes a normal distribution... so the standard
                                                  // deviation shouldn't reperesent a maximum error!
  const double measurement_variance = update_std_dev * update_std_dev;

  if (this->count_ <= 0) {
    ++this->count_;

    this->offset_ = measurement;
    this->offset_covariance_ = measurement_variance;
    this->drift_ = 0;

    TimeElement time_element = {.last_update = this->last_update_, .offset = this->offset_, .drift = this->drift_};
    xQueueOverwrite(this->time_element_queue_, &time_element);

    return;
  } else if (this->count_ == 1) {
    ++this->count_;

    this->drift_ = (measurement - this->offset_) / dt;
    this->offset_ = measurement;

    this->drift_covariance_ = (this->offset_covariance_ + measurement_variance) / dt;
    this->offset_covariance_ = measurement_variance;

    TimeElement time_element = {.last_update = this->last_update_, .offset = this->offset_, .drift = this->drift_};
    xQueueOverwrite(this->time_element_queue_, &time_element);

    return;
  }

  // Prediction
  double offset = this->offset_ + this->drift_ * dt;

  // Covariance
  //   - Assumes the process variance doesn't affect the drift covariance/offset drift covariance
  const double dt_squared = dt * dt;

  const double drift_process_variance = 0.0;
  double new_drift_covariance = this->drift_covariance_ + drift_process_variance;

  const double offset_drift_process_variance = 0.0;
  double new_offset_drift_covariance =
      this->offset_drift_covariance_ + this->drift_covariance_ * dt + offset_drift_process_variance;

  const double offset_process_variance = dt * this->process_variance_;
  double new_offset_covariance = this->offset_covariance_ + 2 * this->offset_drift_covariance_ * dt +
                                 this->drift_covariance_ * dt_squared + offset_process_variance;

  // Residual
  const double residual = measurement - offset;

  if (this->count_ < 100) {
    // Never apply the forget factor when the filter has received few updates
    ++this->count_;
  } else if (residual > (3 * max_error) / 4.0) {
    // Significant error in our prediction, apply a forgetting factor to more heavily weigh new measurements
    new_drift_covariance *= this->forget_variance_factor_;
    new_offset_drift_covariance *= this->forget_variance_factor_;
    new_offset_covariance *= this->forget_variance_factor_;
  }

  // System Uncertainty
  const double uncertainty = 1.0 / (new_offset_covariance + measurement_variance);

  // Kalman Gain
  const double offset_gain = new_offset_covariance * uncertainty;
  const double drift_gain = new_offset_drift_covariance * uncertainty;

  // State Update
  this->offset_ = offset + offset_gain * residual;
  this->drift_ += drift_gain * residual;

  // Covariance update
  this->drift_covariance_ = new_drift_covariance - drift_gain * new_offset_drift_covariance;
  this->offset_drift_covariance_ = new_offset_drift_covariance - drift_gain * new_offset_covariance;
  this->offset_covariance_ = new_offset_covariance - offset_gain * new_offset_covariance;

  TimeElement time_element = {.last_update = this->last_update_, .offset = this->offset_, .drift = this->drift_};
  xQueueOverwrite(this->time_element_queue_, &time_element);
}

int64_t ResonateTimeFilter::compute_server_time(int64_t client_time) {
  // server_timestamp = client_timestamp + offset

  return client_time + this->compute_offset_(client_time);
}

int64_t ResonateTimeFilter::compute_client_time(int64_t server_time) {
  // server_timestamp = client_timestamp + current_offset + drift*(client_timestamp - last_update_timestamp)
  //  = (1+drift)*(client_timestamp) + current_offset - drift*last_update_timestamp
  // client_timestamp = (server_timestamp - current_offset + drift*last_update_timestamp)/(1+drift)
  xQueueReceive(this->time_element_queue_, &this->current_time_element_, 0);

  return round((static_cast<double>(server_time) - this->current_time_element_.offset +
                this->current_time_element_.drift * this->current_time_element_.last_update) /
               (1.0 + this->current_time_element_.drift));
}

int64_t ResonateTimeFilter::compute_offset_(int64_t timestamp) {
  // server_timestamp = client_timestamp + current_offset + drift*(client_timestamp - last_update_timestamp)
  //  = (1+drift)*(client_timestamp) + current_offset - drift*last_update_timestamp
  // client_timestamp = (server_timestamp - current_offset + drift*last_update_timestamp)/(1+drift)
  xQueueReceive(this->time_element_queue_, &this->current_time_element_, 0);

  const double dt = timestamp - this->current_time_element_.last_update;
  return round(this->current_time_element_.offset + this->current_time_element_.drift * dt);
}

void ResonateTimeFilter::reset() {
  this->count_ = 0;
  this->offset_ = 0.0;
  this->drift_ = 0.0;
  this->offset_covariance_ = std::numeric_limits<double>::infinity();
  this->offset_drift_covariance_ = 0.0;
  this->drift_covariance_ = 0.0;

  xQueueReset(this->time_element_queue_);
  this->current_time_element_ = TimeElement();
}

}  // namespace resonate
}  // namespace esphome

#endif
