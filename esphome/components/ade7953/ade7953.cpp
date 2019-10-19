#include "ade7953.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ade7953 {

static const char *TAG = "ade7953";

void ADE7953::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7953:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Voltage Sensor", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current A Sensor", this->current_a_sensor_);
  LOG_SENSOR("  ", "Current B Sensor", this->current_b_sensor_);
  LOG_SENSOR("  ", "Active Power A Sensor", this->active_power_a_sensor_);
  LOG_SENSOR("  ", "Active Power B Sensor", this->active_power_b_sensor_);
}

#define ADE_PUBLISH_(name, factor) \
  if (name) { \
    float value = *name / factor; \
    this->name##_sensor_->publish_state(value); \
  }
#define ADE_PUBLISH(name, factor) ADE_PUBLISH_(name, factor)

void ADE7953::update() {
  if (!this->is_setup_)
    return;

  auto active_power_a = this->ade_read_<int32_t>(0x0312);
  ADE_PUBLISH(active_power_a, 154.0f);
  auto active_power_b = this->ade_read_<int32_t>(0x0313);
  ADE_PUBLISH(active_power_b, 154.0f);
  auto current_a = this->ade_read_<uint32_t>(0x031A);
  ADE_PUBLISH(current_a, 100000.0f);
  auto current_b = this->ade_read_<uint32_t>(0x031B);
  ADE_PUBLISH(current_b, 100000.0f);
  auto voltage = this->ade_read_<uint32_t>(0x031C);
  ADE_PUBLISH(voltage, 26000.0f);

  //    auto apparent_power_a = this->ade_read_<int32_t>(0x0310);
  //    auto apparent_power_b = this->ade_read_<int32_t>(0x0311);
  //    auto reactive_power_a = this->ade_read_<int32_t>(0x0314);
  //    auto reactive_power_b = this->ade_read_<int32_t>(0x0315);
  //    auto power_factor_a = this->ade_read_<int16_t>(0x010A);
  //    auto power_factor_b = this->ade_read_<int16_t>(0x010B);
}

}  // namespace ade7953
}  // namespace esphome
