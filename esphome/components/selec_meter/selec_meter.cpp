#include "selec_meter.h"
#include "selec_meter_registers.h"
#include "esphome/core/log.h"

namespace esphome::selec_meter {

namespace helpers = modbus::helpers;

static const char *const TAG = "selec_meter";

static const uint8_t MODBUS_REGISTER_COUNT = 34;  // 34 x 16-bit registers

void SelecMeter::on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                         modbus::ResponseStatus status) {
  if (!modbus::succeeded(status))
    return;  // the hub already logs exception responses

  // Publish a sensor if both of its registers are in this response; skipping absent registers keeps
  // this correct for any read range, so the poll may be split into multiple requests.
  // Values are 32-bit floats, low word first.
  auto publish = [&](sensor::Sensor *sensor, uint16_t reg, float unit) -> void {
    if (sensor == nullptr)
      return;
    if (auto value = helpers::value_at<helpers::SensorValueType::FP32_R>(registers, start_address, reg))
      sensor->publish_state(*value * unit);
  };

  publish(this->total_active_energy_sensor_, SELEC_TOTAL_ACTIVE_ENERGY, NO_DEC_UNIT);
  publish(this->import_active_energy_sensor_, SELEC_IMPORT_ACTIVE_ENERGY, NO_DEC_UNIT);
  publish(this->export_active_energy_sensor_, SELEC_EXPORT_ACTIVE_ENERGY, NO_DEC_UNIT);
  publish(this->total_reactive_energy_sensor_, SELEC_TOTAL_REACTIVE_ENERGY, NO_DEC_UNIT);
  publish(this->import_reactive_energy_sensor_, SELEC_IMPORT_REACTIVE_ENERGY, NO_DEC_UNIT);
  publish(this->export_reactive_energy_sensor_, SELEC_EXPORT_REACTIVE_ENERGY, NO_DEC_UNIT);
  publish(this->apparent_energy_sensor_, SELEC_APPARENT_ENERGY, NO_DEC_UNIT);
  publish(this->active_power_sensor_, SELEC_ACTIVE_POWER, MULTIPLY_THOUSAND_UNIT);
  publish(this->reactive_power_sensor_, SELEC_REACTIVE_POWER, MULTIPLY_THOUSAND_UNIT);
  publish(this->apparent_power_sensor_, SELEC_APPARENT_POWER, MULTIPLY_THOUSAND_UNIT);
  publish(this->voltage_sensor_, SELEC_VOLTAGE, NO_DEC_UNIT);
  publish(this->current_sensor_, SELEC_CURRENT, NO_DEC_UNIT);
  publish(this->power_factor_sensor_, SELEC_POWER_FACTOR, NO_DEC_UNIT);
  publish(this->frequency_sensor_, SELEC_FREQUENCY, NO_DEC_UNIT);
  publish(this->maximum_demand_active_power_sensor_, SELEC_MAXIMUM_DEMAND_ACTIVE_POWER, MULTIPLY_THOUSAND_UNIT);
  publish(this->maximum_demand_reactive_power_sensor_, SELEC_MAXIMUM_DEMAND_REACTIVE_POWER, MULTIPLY_THOUSAND_UNIT);
  publish(this->maximum_demand_apparent_power_sensor_, SELEC_MAXIMUM_DEMAND_APPARENT_POWER, MULTIPLY_THOUSAND_UNIT);
}

void SelecMeter::update() { this->read_input_registers(0, MODBUS_REGISTER_COUNT); }
void SelecMeter::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SELEC Meter:\n"
                "  Address: 0x%02X",
                this->address_);
  LOG_SENSOR("  ", "Total Active Energy", this->total_active_energy_sensor_);
  LOG_SENSOR("  ", "Import Active Energy", this->import_active_energy_sensor_);
  LOG_SENSOR("  ", "Export Active Energy", this->export_active_energy_sensor_);
  LOG_SENSOR("  ", "Total Reactive Energy", this->total_reactive_energy_sensor_);
  LOG_SENSOR("  ", "Import Reactive Energy", this->import_reactive_energy_sensor_);
  LOG_SENSOR("  ", "Export Reactive Energy", this->export_reactive_energy_sensor_);
  LOG_SENSOR("  ", "Apparent Energy", this->apparent_energy_sensor_);
  LOG_SENSOR("  ", "Active Power", this->active_power_sensor_);
  LOG_SENSOR("  ", "Reactive Power", this->reactive_power_sensor_);
  LOG_SENSOR("  ", "Apparent Power", this->apparent_power_sensor_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power Factor", this->power_factor_sensor_);
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Maximum Demand Active Power", this->maximum_demand_active_power_sensor_);
  LOG_SENSOR("  ", "Maximum Demand Reactive Power", this->maximum_demand_reactive_power_sensor_);
  LOG_SENSOR("  ", "Maximum Demand Apparent Power", this->maximum_demand_apparent_power_sensor_);
}

}  // namespace esphome::selec_meter
