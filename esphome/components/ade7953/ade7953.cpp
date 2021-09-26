#include "ade7953.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ade7953 {

static const char *const TAG = "ade7953";

void ADE7953::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7953:");
  LOG_PIN("  IRQ Pin: ", irq_pin_);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Voltage Sensor", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current A Sensor", this->current_a_sensor_);
  LOG_SENSOR("  ", "Current B Sensor", this->current_b_sensor_);
  LOG_SENSOR("  ", "Active Power A Sensor", this->active_power_a_sensor_);
  LOG_SENSOR("  ", "Active Power B Sensor", this->active_power_b_sensor_);
}

#define ADE_PUBLISH_(name, val, factor) \
  if (err == i2c::ERROR_OK && this->name##_sensor_) { \
    float value = (val) / (factor); \
    this->name##_sensor_->publish_state(value); \
  }
#define ADE_PUBLISH(name, val, factor) ADE_PUBLISH_(name, val, factor)

void ADE7953::update() {
  if (!this->is_setup_)
    return;

  uint32_t val;
  i2c::ErrorCode err = ade_read_32_(0x0312, &val);
  ADE_PUBLISH(active_power_a, (int32_t) val, 154.0f);
  err = ade_read_32_(0x0313, &val);
  ADE_PUBLISH(active_power_b, (int32_t) val, 154.0f);
  err = ade_read_32_(0x031A, &val);
  ADE_PUBLISH(current_a, (uint32_t) val, 100000.0f);
  err = ade_read_32_(0x031B, &val);
  ADE_PUBLISH(current_b, (uint32_t) val, 100000.0f);
  err = ade_read_32_(0x031C, &val);
  ADE_PUBLISH(voltage, (uint32_t) val, 26000.0f);

  //    auto apparent_power_a = this->ade_read_<int32_t>(0x0310);
  //    auto apparent_power_b = this->ade_read_<int32_t>(0x0311);
  //    auto reactive_power_a = this->ade_read_<int32_t>(0x0314);
  //    auto reactive_power_b = this->ade_read_<int32_t>(0x0315);
  //    auto power_factor_a = this->ade_read_<int16_t>(0x010A);
  //    auto power_factor_b = this->ade_read_<int16_t>(0x010B);
}

}  // namespace ade7953
}  // namespace esphome
