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

  // Each failed read blocks the loop for the UART timeout. Leave the bus alone while the driver is known to be
  // unreachable or after the first failed read; the remaining sensors then publish NaN.
  bool reachable = !this->parent_->is_unreachable();
  auto read = [this, &reachable](uint8_t reg) -> optional<uint32_t> {
    if (!reachable)
      return {};
    auto data = this->parent_->read_register(reg);
    reachable = data.has_value();
    return data;
  };

  if (this->actual_current_sensor_ != nullptr) {
    auto drv_status = read(tmc22xx::REG_DRV_STATUS);
    float current = NAN;
    if (drv_status.has_value()) {
      auto scale = static_cast<uint8_t>(TMC22XXStepper::extract_field(*drv_status, tmc22xx::CS_ACTUAL));
      current = this->parent_->scale_to_current(scale);
    }
    this->actual_current_sensor_->publish_state(current);
  }

  // StallGuard only measures the load while the motor turns
  bool moving = !this->parent_->has_reached_target();
  if (this->stallguard_result_sensor_ != nullptr || (this->motor_load_sensor_ != nullptr && moving)) {
    auto sg_result = read(REG_SG_RESULT);
    publish_field(this->stallguard_result_sensor_, sg_result, SG_RESULT);
    if (this->motor_load_sensor_ != nullptr) {
      // 100% is the load at which a stall is reported, where the result drops below twice the threshold
      float stall_result = 2.0f * this->parent_->get_stallguard_threshold();
      float load = NAN;
      if (moving && sg_result.has_value() && stall_result < STALLGUARD_RESULT_MAX) {
        float result = TMC22XXStepper::extract_field(*sg_result, SG_RESULT);
        load = (STALLGUARD_RESULT_MAX - result) / (STALLGUARD_RESULT_MAX - stall_result) * 100.0f;
      }
      this->motor_load_sensor_->publish_state(load);
    }
  } else if (this->motor_load_sensor_ != nullptr) {
    this->motor_load_sensor_->publish_state(NAN);
  }

  if (this->pwm_scale_sum_sensor_ != nullptr || this->pwm_scale_auto_sensor_ != nullptr) {
    auto pwm_scale = read(tmc22xx::REG_PWM_SCALE);
    publish_field(this->pwm_scale_sum_sensor_, pwm_scale, tmc22xx::PWM_SCALE_SUM);
    publish_field(this->pwm_scale_auto_sensor_, pwm_scale, tmc22xx::PWM_SCALE_AUTO);
  }

  if (this->pwm_ofs_auto_sensor_ != nullptr || this->pwm_grad_auto_sensor_ != nullptr) {
    auto pwm_auto = read(tmc22xx::REG_PWM_AUTO);
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
