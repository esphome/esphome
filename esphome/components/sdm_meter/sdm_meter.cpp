#include "sdm_meter.h"
#include "sdm_meter_registers.h"
#include "esphome/core/log.h"

namespace esphome::sdm_meter {

namespace helpers = modbus::helpers;

static const char *const TAG = "sdm_meter";

static const uint8_t MODBUS_REGISTER_COUNT = 80;  // 80 x 16-bit registers (40 float values)

void SDMMeter::on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                       modbus::ResponseStatus status) {
  if (!modbus::succeeded(status))
    return;  // the hub already logs exception responses

  // Publish a sensor if both of its registers are in this response; skipping absent registers keeps
  // this correct for any read range, so the poll may be split into multiple requests.
  auto publish = [&](uint16_t reg, sensor::Sensor *sensor) {
    if (sensor == nullptr)
      return;
    if (auto value = helpers::value_at<helpers::SensorValueType::FP32>(registers, start_address, reg))
      sensor->publish_state(*value);
  };

  for (uint8_t i = 0; i < 3; i++) {
    auto &phase = this->phases_[i];
    if (!phase.setup)
      continue;
    publish(SDM_PHASE_1_VOLTAGE + i * 2, phase.voltage_sensor_);
    publish(SDM_PHASE_1_CURRENT + i * 2, phase.current_sensor_);
    publish(SDM_PHASE_1_ACTIVE_POWER + i * 2, phase.active_power_sensor_);
    publish(SDM_PHASE_1_APPARENT_POWER + i * 2, phase.apparent_power_sensor_);
    publish(SDM_PHASE_1_REACTIVE_POWER + i * 2, phase.reactive_power_sensor_);
    publish(SDM_PHASE_1_POWER_FACTOR + i * 2, phase.power_factor_sensor_);
    publish(SDM_PHASE_1_ANGLE + i * 2, phase.phase_angle_sensor_);
  }

  publish(SDM_TOTAL_SYSTEM_POWER, this->total_power_sensor_);
  publish(SDM_FREQUENCY, this->frequency_sensor_);
  publish(SDM_IMPORT_ACTIVE_ENERGY, this->import_active_energy_sensor_);
  publish(SDM_EXPORT_ACTIVE_ENERGY, this->export_active_energy_sensor_);
  publish(SDM_IMPORT_REACTIVE_ENERGY, this->import_reactive_energy_sensor_);
  publish(SDM_EXPORT_REACTIVE_ENERGY, this->export_reactive_energy_sensor_);
}

void SDMMeter::update() { this->read_input_registers(0, MODBUS_REGISTER_COUNT); }
void SDMMeter::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SDM Meter:\n"
                "  Address: 0x%02X",
                this->address_);
  for (uint8_t i = 0; i < 3; i++) {
    auto phase = this->phases_[i];
    if (!phase.setup)
      continue;
    ESP_LOGCONFIG(TAG, "  Phase %c", i + 'A');
    LOG_SENSOR("    ", "Voltage", phase.voltage_sensor_);
    LOG_SENSOR("    ", "Current", phase.current_sensor_);
    LOG_SENSOR("    ", "Active Power", phase.active_power_sensor_);
    LOG_SENSOR("    ", "Apparent Power", phase.apparent_power_sensor_);
    LOG_SENSOR("    ", "Reactive Power", phase.reactive_power_sensor_);
    LOG_SENSOR("    ", "Power Factor", phase.power_factor_sensor_);
    LOG_SENSOR("    ", "Phase Angle", phase.phase_angle_sensor_);
  }
  LOG_SENSOR("  ", "Total Power", this->total_power_sensor_);
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Import Active Energy", this->import_active_energy_sensor_);
  LOG_SENSOR("  ", "Export Active Energy", this->export_active_energy_sensor_);
  LOG_SENSOR("  ", "Import Reactive Energy", this->import_reactive_energy_sensor_);
  LOG_SENSOR("  ", "Export Reactive Energy", this->export_reactive_energy_sensor_);
}

}  // namespace esphome::sdm_meter
