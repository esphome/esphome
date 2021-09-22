#include "selec_meter.h"
#include "selec_meter_registers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace selec_meter {

static const char *const TAG = "selec_meter";

static const uint8_t MODBUS_CMD_READ_IN_REGISTERS = 0x04;
static const uint8_t MODBUS_REGISTER_COUNT = 34;  // 34 x 16-bit registers

void SelecMeter::on_modbus_data(const std::vector<uint8_t> &data) {
  if (data.size() < MODBUS_REGISTER_COUNT * 2) {
    ESP_LOGW(TAG, "Invalid size for SelecMeter!");
    return;
  }

  auto selec_meter_get_float = [&](size_t i, float unit) -> float {
    uint32_t temp = encode_uint32(data[i + 2], data[i + 3], data[i], data[i + 1]);

    float f;
    memcpy(&f, &temp, sizeof(f));
    return (f * unit);
  };

  float total_active_energy = selec_meter_get_float(SELEC_TOTAL_ACTIVE_ENERGY * 2, NO_DEC_UNIT);
  float import_active_energy = selec_meter_get_float(SELEC_IMPORT_ACTIVE_ENERGY * 2, NO_DEC_UNIT);
  float export_active_energy = selec_meter_get_float(SELEC_EXPORT_ACTIVE_ENERGY * 2, NO_DEC_UNIT);
  float total_reactive_energy = selec_meter_get_float(SELEC_TOTAL_REACTIVE_ENERGY * 2, NO_DEC_UNIT);
  float import_reactive_energy = selec_meter_get_float(SELEC_IMPORT_REACTIVE_ENERGY * 2, NO_DEC_UNIT);
  float export_reactive_energy = selec_meter_get_float(SELEC_EXPORT_REACTIVE_ENERGY * 2, NO_DEC_UNIT);
  float apparent_energy = selec_meter_get_float(SELEC_APPARENT_ENERGY * 2, NO_DEC_UNIT);
  float active_power = selec_meter_get_float(SELEC_ACTIVE_POWER * 2, MULTIPLY_THOUSAND_UNIT);
  float reactive_power = selec_meter_get_float(SELEC_REACTIVE_POWER * 2, MULTIPLY_THOUSAND_UNIT);
  float apparent_power = selec_meter_get_float(SELEC_APPARENT_POWER * 2, MULTIPLY_THOUSAND_UNIT);
  float voltage = selec_meter_get_float(SELEC_VOLTAGE * 2, NO_DEC_UNIT);
  float current = selec_meter_get_float(SELEC_CURRENT * 2, NO_DEC_UNIT);
  float power_factor = selec_meter_get_float(SELEC_POWER_FACTOR * 2, NO_DEC_UNIT);
  float frequency = selec_meter_get_float(SELEC_FREQUENCY * 2, NO_DEC_UNIT);
  float maximum_demand_active_power =
      selec_meter_get_float(SELEC_MAXIMUM_DEMAND_ACTIVE_POWER * 2, MULTIPLY_THOUSAND_UNIT);
  float maximum_demand_reactive_power =
      selec_meter_get_float(SELEC_MAXIMUM_DEMAND_REACTIVE_POWER * 2, MULTIPLY_THOUSAND_UNIT);
  float maximum_demand_apparent_power =
      selec_meter_get_float(SELEC_MAXIMUM_DEMAND_APPARENT_POWER * 2, MULTIPLY_THOUSAND_UNIT);

  if (this->total_active_energy_sensor_ != nullptr)
    this->total_active_energy_sensor_->publish_state(total_active_energy);
  if (this->import_active_energy_sensor_ != nullptr)
    this->import_active_energy_sensor_->publish_state(import_active_energy);
  if (this->export_active_energy_sensor_ != nullptr)
    this->export_active_energy_sensor_->publish_state(export_active_energy);
  if (this->total_reactive_energy_sensor_ != nullptr)
    this->total_reactive_energy_sensor_->publish_state(total_reactive_energy);
  if (this->import_reactive_energy_sensor_ != nullptr)
    this->import_reactive_energy_sensor_->publish_state(import_reactive_energy);
  if (this->export_reactive_energy_sensor_ != nullptr)
    this->export_reactive_energy_sensor_->publish_state(export_reactive_energy);
  if (this->apparent_energy_sensor_ != nullptr)
    this->apparent_energy_sensor_->publish_state(apparent_energy);
  if (this->active_power_sensor_ != nullptr)
    this->active_power_sensor_->publish_state(active_power);
  if (this->reactive_power_sensor_ != nullptr)
    this->reactive_power_sensor_->publish_state(reactive_power);
  if (this->apparent_power_sensor_ != nullptr)
    this->apparent_power_sensor_->publish_state(apparent_power);
  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(voltage);
  if (this->current_sensor_ != nullptr)
    this->current_sensor_->publish_state(current);
  if (this->power_factor_sensor_ != nullptr)
    this->power_factor_sensor_->publish_state(power_factor);
  if (this->frequency_sensor_ != nullptr)
    this->frequency_sensor_->publish_state(frequency);
  if (this->maximum_demand_active_power_sensor_ != nullptr)
    this->maximum_demand_active_power_sensor_->publish_state(maximum_demand_active_power);
  if (this->maximum_demand_reactive_power_sensor_ != nullptr)
    this->maximum_demand_reactive_power_sensor_->publish_state(maximum_demand_reactive_power);
  if (this->maximum_demand_apparent_power_sensor_ != nullptr)
    this->maximum_demand_apparent_power_sensor_->publish_state(maximum_demand_apparent_power);
}

void SelecMeter::update() { this->send(MODBUS_CMD_READ_IN_REGISTERS, 0, MODBUS_REGISTER_COUNT); }
void SelecMeter::dump_config() {
  ESP_LOGCONFIG(TAG, "SELEC Meter:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
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

}  // namespace selec_meter
}  // namespace esphome
