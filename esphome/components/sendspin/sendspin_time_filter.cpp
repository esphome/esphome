// Copyright 2025 Sendspin Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sendspin_time_filter.h"

#ifdef USE_ESP32

#include <cmath>
#include <cstdint>
#include <limits>

namespace esphome::sendspin {

SendspinTimeFilter::SendspinTimeFilter(double process_std_dev, double drift_process_std_dev, double forget_factor,
                                       double adaptive_cutoff, uint8_t min_samples, double drift_significance_threshold)
    : process_variance_(process_std_dev * process_std_dev),
      drift_process_variance_(drift_process_std_dev * drift_process_std_dev),
      forget_variance_factor_(forget_factor * forget_factor),
      adaptive_forgetting_cutoff_(adaptive_cutoff),
      drift_significance_threshold_squared_(drift_significance_threshold * drift_significance_threshold),
      min_samples_for_forgetting_(min_samples) {
  this->reset();
}

void SendspinTimeFilter::update(int64_t measurement, int64_t max_error, int64_t time_added) {
  esphome::LockGuard lock(this->state_mutex_);

  if (time_added <= this->last_update_) {
    // Skip non-monotonic timestamps (duplicates or out-of-order packets)
    // This protects against division by zero and backwards time progression
    return;
  }

  const double dt = time_added - this->last_update_;
  const double dt_squared = dt * dt;
  this->last_update_ = time_added;

  const double update_std_dev = max_error;
  const double measurement_variance = update_std_dev * update_std_dev;

  // Filter initialization: First measurement establishes offset baseline
  if (this->count_ <= 0) {
    ++this->count_;

    this->offset_ = measurement;
    this->offset_covariance_ = measurement_variance;
    this->drift_ = 0;  // No drift information available yet

    return;
  }

  // Second measurement: Initial drift estimation from finite differences
  if (this->count_ == 1) {
    ++this->count_;

    this->drift_ = (measurement - this->offset_) / dt;
    this->offset_ = measurement;

    // Drift variance estimated from propagation of offset uncertainties
    this->drift_covariance_ = (this->offset_covariance_ + measurement_variance) / dt_squared;
    this->offset_covariance_ = measurement_variance;

    return;
  }

  /*** Kalman Prediction Step ***/
  // State prediction: x_k|k-1 = F * x_k-1|k-1
  double offset = this->offset_ + this->drift_ * dt;

  // Covariance prediction: P_k|k-1 = F * P_k-1|k-1 * F^T + Q
  // State transition matrix F = [1, dt; 0, 1]

  // Process noise for both offset and drift (full random walk model)
  // We assume clock jitter (offset noise) and wander (drift noise) are independent processes
  const double drift_process_variance = dt * this->drift_process_variance_;
  double new_drift_covariance = this->drift_covariance_ + drift_process_variance;

  double new_offset_drift_covariance = this->offset_drift_covariance_ + this->drift_covariance_ * dt;

  const double offset_process_variance = dt * this->process_variance_;
  double new_offset_covariance = this->offset_covariance_ + 2 * this->offset_drift_covariance_ * dt +
                                 this->drift_covariance_ * dt_squared + offset_process_variance;

  /*** Innovation and Adaptive Forgetting ***/
  const double residual = measurement - offset;  // Innovation: y_k = z_k - H * x_k|k-1
  const double max_residual_cutoff = max_error * this->adaptive_forgetting_cutoff_;

  if (this->count_ < this->min_samples_for_forgetting_) {
    // Build sufficient history before enabling adaptive forgetting
    ++this->count_;
  } else if (std::abs(residual) > max_residual_cutoff) {
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

  // Update drift significance flag for time conversion methods
  // Only apply drift compensation if statistically significant (SNR check)
  const double drift_squared = this->drift_ * this->drift_;
  this->use_drift_ = drift_squared > this->drift_significance_threshold_squared_ * this->drift_covariance_;
}

int64_t SendspinTimeFilter::compute_server_time(int64_t client_time) const {
  // Transform: T_server = T_client + offset + drift * (T_client - T_last_update)
  // Compute instantaneous offset accounting for linear drift:
  // offset(t) = offset_base + drift * (t - t_last_update)

  esphome::LockGuard lock(this->state_mutex_);
  const double dt = client_time - this->last_update_;
  const double effective_drift = this->use_drift_ ? this->drift_ : 0.0;

  const int64_t offset = std::round(this->offset_ + effective_drift * dt);
  return client_time + offset;
}

int64_t SendspinTimeFilter::compute_client_time(int64_t server_time) const {
  // Inverse transform solving for T_client:
  // T_server = T_client + offset + drift * (T_client - T_last_update)
  // T_server = (1 + drift) * T_client + offset - drift * T_last_update
  // T_client = (T_server - offset + drift * T_last_update) / (1 + drift)

  esphome::LockGuard lock(this->state_mutex_);
  const double effective_drift = this->use_drift_ ? this->drift_ : 0.0;

  return std::round((static_cast<double>(server_time) - this->offset_ + effective_drift * this->last_update_) /
                    (1.0 + effective_drift));
}

void SendspinTimeFilter::reset() {
  esphome::LockGuard lock(this->state_mutex_);
  this->count_ = 0;
  this->offset_ = 0.0;
  this->drift_ = 0.0;
  this->offset_covariance_ = std::numeric_limits<double>::infinity();
  this->offset_drift_covariance_ = 0.0;
  this->drift_covariance_ = 0.0;
  this->last_update_ = 0;
  this->use_drift_ = false;
}

bool SendspinTimeFilter::has_update() const {
  esphome::LockGuard lock(this->state_mutex_);
  return this->count_ >= 1;
}

int64_t SendspinTimeFilter::get_error() const {
  esphome::LockGuard lock(this->state_mutex_);
  return std::round(sqrt(this->offset_covariance_));
}

int64_t SendspinTimeFilter::get_covariance() const {
  esphome::LockGuard lock(this->state_mutex_);
  return std::round(this->offset_covariance_);
}
}  // namespace esphome::sendspin

#endif  // USE_ESP32
