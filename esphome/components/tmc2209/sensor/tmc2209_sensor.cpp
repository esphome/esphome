#include "tmc2209_sensor.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::tmc2209 {

ESPHOME_LOG_TAG(TAG, "tmc2209.sensor");

using tmc22xx::TMC22XXStepper;

// Highest StallGuard result, reached with no load
static constexpr float STALLGUARD_RESULT_MAX = 510.0f;

static void publish_field(sensor::Sensor *sensor, const optional<uint32_t> &data, const RegisterField &field) {
  if (sensor == nullptr)
    return;
  if (!data.has_value()) {
    sensor->publish_state(NAN);
    return;
  }
  uint32_t value = TMC22XXStepper::extract_field(*data, field);
  sensor->publish_state(field.is_signed ? static_cast<float>(static_cast<int32_t>(value)) : static_cast<float>(value));
}

void TMC2209Sensor::update() {
  if (this->parent_->is_failed())
    return;

  if (this->actual_current_sensor_ != nullptr) {
    auto scale = this->parent_->read_field(tmc22xx::CS_ACTUAL);
    this->actual_current_sensor_->publish_state(scale.has_value() ? this->parent_->scale_to_current(*scale) : NAN);
  }

  if (this->stallguard_result_sensor_ != nullptr || this->motor_load_sensor_ != nullptr) {
    auto result = this->parent_->read_field(SG_RESULT);
    if (this->stallguard_result_sensor_ != nullptr)
      this->stallguard_result_sensor_->publish_state(result.has_value() ? *result : NAN);
    if (this->motor_load_sensor_ != nullptr) {
      // 100% is the load at which a stall is reported, where the result drops below twice the threshold
      float stall_result = 2.0f * this->parent_->get_stallguard_threshold();
      float load = NAN;
      if (result.has_value() && stall_result < STALLGUARD_RESULT_MAX)
        load = (STALLGUARD_RESULT_MAX - *result) / (STALLGUARD_RESULT_MAX - stall_result) * 100.0f;
      this->motor_load_sensor_->publish_state(load);
    }
  }

  if (this->pwm_scale_sum_sensor_ != nullptr || this->pwm_scale_auto_sensor_ != nullptr) {
    auto pwm_scale = this->parent_->read_register(tmc22xx::REG_PWM_SCALE);
    publish_field(this->pwm_scale_sum_sensor_, pwm_scale, tmc22xx::PWM_SCALE_SUM);
    publish_field(this->pwm_scale_auto_sensor_, pwm_scale, tmc22xx::PWM_SCALE_AUTO);
  }

  if (this->pwm_ofs_auto_sensor_ != nullptr || this->pwm_grad_auto_sensor_ != nullptr) {
    auto pwm_auto = this->parent_->read_register(tmc22xx::REG_PWM_AUTO);
    publish_field(this->pwm_ofs_auto_sensor_, pwm_auto, tmc22xx::PWM_OFS_AUTO);
    publish_field(this->pwm_grad_auto_sensor_, pwm_auto, tmc22xx::PWM_GRAD_AUTO);
  }
}

void TMC2209Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "TMC2209 Sensor:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Actual Current", this->actual_current_sensor_);
  LOG_SENSOR("  ", "StallGuard Result", this->stallguard_result_sensor_);
  LOG_SENSOR("  ", "Motor Load", this->motor_load_sensor_);
  LOG_SENSOR("  ", "PWM Scale Sum", this->pwm_scale_sum_sensor_);
  LOG_SENSOR("  ", "PWM Scale Auto", this->pwm_scale_auto_sensor_);
  LOG_SENSOR("  ", "PWM Offset Auto", this->pwm_ofs_auto_sensor_);
  LOG_SENSOR("  ", "PWM Gradient Auto", this->pwm_grad_auto_sensor_);
}

}  // namespace esphome::tmc2209
