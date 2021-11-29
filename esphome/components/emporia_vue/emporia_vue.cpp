#include "emporia_vue.h"
#include "esphome/core/log.h"

namespace esphome {
namespace emporia_vue {

static const char *const TAG = "emporia_vue";

void EmporiaVueComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Emporia Vue");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  // TODO: Log phases

  for (auto *power_sensor : this->_power_sensors) {
    LOG_SENSOR("  ", "Sensor", power_sensor);
    // TODO: Log other details
  }
}

void EmporiaVueComponent::set_phases(std::vector<PhaseConfig *> phases) { this->_phases = phases; }

void EmporiaVueComponent::set_power_sensors(std::vector<PowerSensor *> power_sensors) {
  this->_power_sensors = power_sensors;
}

void EmporiaVueComponent::update() {
  EmporiaSensorData data;
  i2c::ErrorCode error = this->read(reinterpret_cast<uint8_t *>(&data), sizeof(data));
  ESP_LOGVV(TAG, "Raw Sensor Data: %s", hexencode(reinterpret_cast<uint8_t *>(&data), sizeof(data)).c_str());

  for (PowerSensor *power_sensor : this->_power_sensors) {
    power_sensor->update_from_data(data);
  }
}

void PhaseConfig::set_input_color(PhaseInputColor input_color) { this->_input_color = input_color; }

int32_t PhaseConfig::extract_power_for_phase(const PowerDataEntry &entry) {
  switch (this->_input_color) {
    case PhaseInputColor::BLACK:
      return entry.phase_one;
    case PhaseInputColor::RED:
      return entry.phase_two;
    case PhaseInputColor::BLUE:
      return entry.phase_three;
    default:
      return -1;
  }
}

void PowerSensor::set_phase(PhaseConfig *phase) { this->_phase = phase; }

void PowerSensor::set_ct_input(CTInputPort ct_input) { this->_ct_input = ct_input; }

void PowerSensor::update_from_data(const EmporiaSensorData &data) {
  PowerDataEntry entry = data.power[this->_ct_input];
  int32_t raw_power = this->_phase->extract_power_for_phase(entry);
  float calibrated_power = (raw_power * 0.0229308) / 22;
  this->publish_state(calibrated_power);
}

}  // namespace emporia_vue
}  // namespace esphome
