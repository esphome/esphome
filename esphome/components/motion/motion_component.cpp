#include "motion_component.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::motion {

static const char *const TAG = "motion";

static void log_matrix(const float m[9]) {
  ESP_LOGCONFIG(TAG, "  Calibration matrix:");
  ESP_LOGCONFIG(TAG, "    - [%9.6f, %9.6f, %9.6f]", m[0], m[1], m[2]);
  ESP_LOGCONFIG(TAG, "    - [%9.6f, %9.6f, %9.6f]", m[3], m[4], m[5]);
  ESP_LOGCONFIG(TAG, "    - [%9.6f, %9.6f, %9.6f]", m[6], m[7], m[8]);
}

void MotionComponent::setup() {
  if (this->pref_key_ != 0) {
    this->pref_ = global_preferences->make_preference<float[9]>(this->pref_key_);
    float saved[9];
    if (this->pref_.load(&saved)) {
      memcpy(this->matrix_, saved, sizeof(this->matrix_));
      ESP_LOGI(TAG, "Restored calibration from NVS");
      log_matrix(this->matrix_);
    }
  }
}
void MotionComponent::dump_config() { log_matrix(this->matrix_); }
void MotionComponent::save_calibration() {
  if (this->pref_key_ == 0) {
    ESP_LOGW(TAG, "Cannot save calibration: no preference key set");
    return;
  }
  this->pref_.save(&this->matrix_);
  ESP_LOGI(TAG, "Saved calibration to NVS");
}
void MotionComponent::update() {
  if (this->is_failed())
    return;
  MotionData motion_data{};
  MotionData raw_data{};
  if (!this->update_data(raw_data))
    return;
  this->map_axes_(motion_data.acceleration, raw_data.acceleration);
  this->map_axes_(motion_data.angular_rate, raw_data.angular_rate);
  this->motion_data_callback_.call(motion_data);

  ESP_LOGV(TAG, "Accel: [%.3f, %.3f, %.3f] g; Gyro: [%.3f, %.3f, %.3f] °/s", motion_data.acceleration[X_AXIS],
           motion_data.acceleration[Y_AXIS], motion_data.acceleration[Z_AXIS], motion_data.angular_rate[X_AXIS],
           motion_data.angular_rate[Y_AXIS], motion_data.angular_rate[Z_AXIS]);
}

bool MotionComponent::calibrate_level() {
  MotionData raw{};
  if (!this->update_data(raw)) {
    ESP_LOGW(TAG, "calibrate_level: failed to read sensor data");
    return false;
  }

  // Apply the current matrix first so any existing axis mapping is preserved.
  float mapped[3];
  this->map_axes_(mapped, raw.acceleration);

  float nx = mapped[X_AXIS];
  float ny = mapped[Y_AXIS];
  float nz = mapped[Z_AXIS];
  float mag = std::sqrt(nx * nx + ny * ny + nz * nz);
  if (mag < 0.1f) {
    ESP_LOGW(TAG, "calibrate_level: acceleration magnitude too small (%.3f)", mag);
    return false;
  }

  // Normalize
  nx /= mag;
  ny /= mag;
  nz /= mag;

  // Compute rotation matrix R such that R * [nx, ny, nz] = [0, 0, 1]
  // using Rodrigues' rotation formula, then compose with the existing matrix.
  if (nz > 0.9999f) {
    // Already aligned with +Z — nothing to compose
    ESP_LOGI(TAG, "Level calibration: already aligned");
    log_matrix(this->matrix_);
    return true;
  }

  float r[9];
  if (nz < -0.9999f) {
    // Aligned with -Z — 180° rotation about X
    float m[9] = {1, 0, 0, 0, -1, 0, 0, 0, -1};
    memcpy(r, m, sizeof(r));
  } else {
    float f = 1.0f / (1.0f + nz);
    r[0] = 1.0f - nx * nx * f;
    r[1] = -nx * ny * f;
    r[2] = -nx;
    r[3] = -nx * ny * f;
    r[4] = 1.0f - ny * ny * f;
    r[5] = -ny;
    r[6] = nx;
    r[7] = ny;
    r[8] = nz;
  }

  // Compose: new_matrix = R * old_matrix
  float old[9];
  memcpy(old, this->matrix_, sizeof(old));
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      this->matrix_[i * 3 + j] = r[i * 3 + 0] * old[j] + r[i * 3 + 1] * old[3 + j] + r[i * 3 + 2] * old[6 + j];
    }
  }

  ESP_LOGI(TAG, "Level calibration applied (mapped accel: [%.3f, %.3f, %.3f])", mapped[X_AXIS], mapped[Y_AXIS],
           mapped[Z_AXIS]);
  log_matrix(this->matrix_);
  return true;
}

bool MotionComponent::calibrate_heading() {
  MotionData raw{};
  if (!this->update_data(raw)) {
    ESP_LOGW(TAG, "calibrate_heading: failed to read sensor data");
    return false;
  }

  // Apply current matrix to get the mapped acceleration
  float mapped[3];
  this->map_axes_(mapped, raw.acceleration);

  float mx = mapped[X_AXIS];
  float my = mapped[Y_AXIS];
  float h = std::sqrt(mx * mx + my * my);
  if (h < 0.05f) {
    ESP_LOGW(TAG, "calibrate_heading: device must be tilted (XY magnitude %.3f too small)", h);
    return false;
  }

  // Rotation angle in the XY plane: eliminate Y component while preserving X sign.
  // Without the sign correction, atan2(my,mx) would rotate everything to +X,
  // flipping the sign when the tilt projects onto -X.
  float sign_mx = mx >= 0 ? 1.0f : -1.0f;
  float cos_phi = sign_mx * mx / h;  // = |mx| / h
  float sin_phi = sign_mx * my / h;

  // Compose Rz(-phi) with the current matrix
  // Rz(-phi) = [[cos_phi, sin_phi, 0], [-sin_phi, cos_phi, 0], [0, 0, 1]]
  float old[9];
  memcpy(old, this->matrix_, sizeof(old));

  this->matrix_[0] = cos_phi * old[0] + sin_phi * old[3];
  this->matrix_[1] = cos_phi * old[1] + sin_phi * old[4];
  this->matrix_[2] = cos_phi * old[2] + sin_phi * old[5];
  this->matrix_[3] = -sin_phi * old[0] + cos_phi * old[3];
  this->matrix_[4] = -sin_phi * old[1] + cos_phi * old[4];
  this->matrix_[5] = -sin_phi * old[2] + cos_phi * old[5];
  // Row 2 unchanged

  ESP_LOGI(TAG, "Heading calibration applied (mapped accel: [%.3f, %.3f, %.3f])", mapped[X_AXIS], mapped[Y_AXIS],
           mapped[Z_AXIS]);
  log_matrix(this->matrix_);
  return true;
}

}  // namespace esphome::motion
