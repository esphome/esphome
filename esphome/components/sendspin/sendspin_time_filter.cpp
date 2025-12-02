#if defined(USE_ESP_IDF)

#include "sendspin_time_filter.h"

namespace esphome {
namespace sendspin {

// Residual threshold as fraction of max_error for triggering adaptive forgetting.
// When residual > CUTOFF * max_error, the filter applies forgetting to recover from outliers.
const double ADAPTIVE_FORGETTING_CUTOFF = 0.75;

SendspinTimeFilter::SendspinTimeFilter(double process_std_dev, double forget_factor) {
  this->process_variance_ = process_std_dev * process_std_dev;
  this->forget_variance_factor_ = forget_factor * forget_factor;

  // Thread-safe queue for atomic transfer of time transformation parameters
  this->time_element_queue_ = xQueueCreate(1, sizeof(TimeElement));
};

SendspinTimeFilter::~SendspinTimeFilter() { vQueueDelete(this->time_element_queue_); };

void SendspinTimeFilter::update(int64_t measurement, int64_t max_error, int64_t time_added) {
  if (time_added == this->last_update_) {
    // Skip duplicate timestamps to avoid division by zero in drift calculation
    return;
  }

  double dt = time_added - this->last_update_;
  this->last_update_ = time_added;

  const double update_std_dev = max_error;
  const double measurement_variance = update_std_dev * update_std_dev;

  // Filter initialization: First measurement establishes offset baseline
  if (this->count_ <= 0) {
    ++this->count_;

    this->offset_ = measurement;
    this->offset_covariance_ = measurement_variance;
    this->drift_ = 0;  // No drift information available yet

    TimeElement time_element = {.last_update = this->last_update_, .offset = this->offset_, .drift = this->drift_};
    xQueueOverwrite(this->time_element_queue_, &time_element);

    return;
  }

  // Second measurement: Initial drift estimation from finite differences
  if (this->count_ == 1) {
    ++this->count_;

    this->drift_ = (measurement - this->offset_) / dt;
    this->offset_ = measurement;

    // Drift variance estimated from propagation of offset uncertainties
    this->drift_covariance_ = (this->offset_covariance_ + measurement_variance) / dt;
    this->offset_covariance_ = measurement_variance;

    TimeElement time_element = {.last_update = this->last_update_, .offset = this->offset_, .drift = this->drift_};
    xQueueOverwrite(this->time_element_queue_, &time_element);

    return;
  }

  /*** Kalman Prediction Step ***/
  // State prediction: x_k|k-1 = F * x_k-1|k-1
  double offset = this->offset_ + this->drift_ * dt;

  // Covariance prediction: P_k|k-1 = F * P_k-1|k-1 * F^T + Q
  // State transition matrix F = [1, dt; 0, 1]
  const double dt_squared = dt * dt;

  // Process noise only applied to offset (modeling clock jitter/wander)
  const double drift_process_variance = 0.0;  // Drift assumed stable
  double new_drift_covariance = this->drift_covariance_ + drift_process_variance;

  const double offset_drift_process_variance = 0.0;
  double new_offset_drift_covariance =
      this->offset_drift_covariance_ + this->drift_covariance_ * dt + offset_drift_process_variance;

  const double offset_process_variance = dt * this->process_variance_;
  double new_offset_covariance = this->offset_covariance_ + 2 * this->offset_drift_covariance_ * dt +
                                 this->drift_covariance_ * dt_squared + offset_process_variance;

  /*** Innovation and Adaptive Forgetting ***/
  const double residual = measurement - offset;  // Innovation: y_k = z_k - H * x_k|k-1
  const double max_residual_cutoff = max_error * ADAPTIVE_FORGETTING_CUTOFF;

  if (this->count_ < 100) {
    // Build sufficient history before enabling adaptive forgetting
    ++this->count_;
  } else if (residual > max_residual_cutoff) {
    // Large prediction error detected - likely network disruption or clock adjustment
    // Apply forgetting factor to increase Kalman gain and accelerate convergence
    new_drift_covariance *= this->forget_variance_factor_;
    new_offset_drift_covariance *= this->forget_variance_factor_;
    new_offset_covariance *= this->forget_variance_factor_;
  }

  /*** Kalman Update Step ***/
  // Innovation covariance: S = H * P * H^T + R, where H = [1, 0]
  const double uncertainty = 1.0 / (new_offset_covariance + measurement_variance);

  // Kalman gain: K = P * H^T * S^(-1)
  const double offset_gain = new_offset_covariance * uncertainty;
  const double drift_gain = new_offset_drift_covariance * uncertainty;

  // State update: x_k|k = x_k|k-1 + K * y_k
  this->offset_ = offset + offset_gain * residual;
  this->drift_ += drift_gain * residual;

  // Covariance update: P_k|k = (I - K*H) * P_k|k-1
  // Using simplified form to ensure numerical stability
  this->drift_covariance_ = new_drift_covariance - drift_gain * new_offset_drift_covariance;
  this->offset_drift_covariance_ = new_offset_drift_covariance - drift_gain * new_offset_covariance;
  this->offset_covariance_ = new_offset_covariance - offset_gain * new_offset_covariance;

  TimeElement time_element = {.last_update = this->last_update_, .offset = this->offset_, .drift = this->drift_};
  xQueueOverwrite(this->time_element_queue_, &time_element);
}

int64_t SendspinTimeFilter::compute_server_time(int64_t client_time) {
  // Transform: T_server = T_client + offset + drift * (T_client - T_last_update)
  // Compute instantaneous offset accounting for linear drift:
  // offset(t) = offset_base + drift * (t - t_last_update)

  // Atomically retrieve latest time transformation parameters
  xQueueReceive(this->time_element_queue_, &this->current_time_element_, 0);

  const double dt = client_time - this->current_time_element_.last_update;
  const int64_t offset = std::round(this->current_time_element_.offset + this->current_time_element_.drift * dt);

  return client_time + offset;
}

int64_t SendspinTimeFilter::compute_client_time(int64_t server_time) {
  // Inverse transform solving for T_client:
  // T_server = T_client + offset + drift * (T_client - T_last_update)
  // T_server = (1 + drift) * T_client + offset - drift * T_last_update
  // T_client = (T_server - offset + drift * T_last_update) / (1 + drift)

  // Atomically retrieve latest time transformation parameters
  xQueueReceive(this->time_element_queue_, &this->current_time_element_, 0);

  return std::round((static_cast<double>(server_time) - this->current_time_element_.offset +
                     this->current_time_element_.drift * this->current_time_element_.last_update) /
                    (1.0 + this->current_time_element_.drift));
}

void SendspinTimeFilter::reset() {
  this->count_ = 0;
  this->offset_ = 0.0;
  this->drift_ = 0.0;
  this->offset_covariance_ = std::numeric_limits<double>::infinity();
  this->offset_drift_covariance_ = 0.0;
  this->drift_covariance_ = 0.0;

  xQueueReset(this->time_element_queue_);
  this->current_time_element_ = TimeElement();
}

}  // namespace sendspin
}  // namespace esphome

#endif
