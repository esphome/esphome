#include "havells_solar.h"
#include "havells_solar_registers.h"
#include "esphome/core/log.h"

namespace esphome::havells_solar {

namespace helpers = modbus::helpers;

static const char *const TAG = "havells_solar";

static const uint8_t MODBUS_REGISTER_COUNT = 48;  // 48 x 16-bit registers

void HavellsSolar::on_read_holding_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                             modbus::ResponseStatus status) {
  if (!modbus::succeeded(status))
    return;  // the hub already logs exception responses

  // Publish a sensor if its register(s) are in this response; skipping absent registers keeps this
  // correct for any read range, so the poll may be split into multiple requests.
  auto publish_1_register = [&](sensor::Sensor *sensor, uint16_t reg, float unit) -> void {
    if (sensor == nullptr)
      return;
    if (auto value = helpers::value_at<helpers::SensorValueType::U_WORD>(registers, start_address, reg))
      sensor->publish_state(*value * unit);
  };

  auto publish_2_registers = [&](sensor::Sensor *sensor, uint16_t reg, float unit) -> void {
    if (sensor == nullptr)
      return;
    if (auto value = helpers::value_at<helpers::SensorValueType::U_DWORD>(registers, start_address, reg))
      sensor->publish_state(*value * unit);
  };

  for (uint8_t i = 0; i < 3; i++) {
    auto &phase = this->phases_[i];
    if (!phase.setup)
      continue;
    publish_1_register(phase.voltage_sensor_, HAVELLS_PHASE_1_VOLTAGE + i * 2, ONE_DEC_UNIT);
    publish_1_register(phase.current_sensor_, HAVELLS_PHASE_1_CURRENT + i * 2, TWO_DEC_UNIT);
  }

  for (uint8_t i = 0; i < 2; i++) {
    auto &pv = this->pvs_[i];
    if (!pv.setup)
      continue;
    publish_1_register(pv.voltage_sensor_, HAVELLS_PV_1_VOLTAGE + i * 2, ONE_DEC_UNIT);
    publish_1_register(pv.current_sensor_, HAVELLS_PV_1_CURRENT + i * 2, TWO_DEC_UNIT);
    publish_1_register(pv.active_power_sensor_, HAVELLS_PV_1_POWER + i, MULTIPLY_TEN_UNIT);
    publish_1_register(pv.voltage_sampled_by_secondary_cpu_sensor_, HAVELLS_PV1_VOLTAGE_SAMPLED_BY_SECONDARY_CPU + i,
                       ONE_DEC_UNIT);
    publish_1_register(pv.insulation_of_p_to_ground_sensor_, HAVELLS_PV1_INSULATION_OF_P_TO_GROUND + i, NO_DEC_UNIT);
  }

  publish_1_register(this->frequency_sensor_, HAVELLS_GRID_FREQUENCY, TWO_DEC_UNIT);
  publish_1_register(this->active_power_sensor_, HAVELLS_SYSTEM_ACTIVE_POWER, MULTIPLY_TEN_UNIT);
  publish_1_register(this->reactive_power_sensor_, HAVELLS_SYSTEM_REACTIVE_POWER, TWO_DEC_UNIT);
  publish_1_register(this->today_production_sensor_, HAVELLS_TODAY_PRODUCTION, TWO_DEC_UNIT);
  publish_2_registers(this->total_energy_production_sensor_, HAVELLS_TOTAL_ENERGY_PRODUCTION, NO_DEC_UNIT);
  publish_2_registers(this->total_generation_time_sensor_, HAVELLS_TOTAL_GENERATION_TIME, NO_DEC_UNIT);
  publish_1_register(this->today_generation_time_sensor_, HAVELLS_TODAY_GENERATION_TIME, NO_DEC_UNIT);
  publish_1_register(this->inverter_module_temp_sensor_, HAVELLS_INVERTER_MODULE_TEMP, NO_DEC_UNIT);
  publish_1_register(this->inverter_inner_temp_sensor_, HAVELLS_INVERTER_INNER_TEMP, NO_DEC_UNIT);
  publish_1_register(this->inverter_bus_voltage_sensor_, HAVELLS_INVERTER_BUS_VOLTAGE, NO_DEC_UNIT);
  publish_1_register(this->insulation_pv_n_to_ground_sensor_, HAVELLS_INSULATION_OF_PV_N_TO_GROUND, NO_DEC_UNIT);
  publish_1_register(this->gfci_value_sensor_, HAVELLS_GFCI_VALUE, NO_DEC_UNIT);
  publish_1_register(this->dci_of_r_sensor_, HAVELLS_DCI_OF_R, NO_DEC_UNIT);
  publish_1_register(this->dci_of_s_sensor_, HAVELLS_DCI_OF_S, NO_DEC_UNIT);
  publish_1_register(this->dci_of_t_sensor_, HAVELLS_DCI_OF_T, NO_DEC_UNIT);
}

void HavellsSolar::update() { this->read_holding_registers(0, MODBUS_REGISTER_COUNT); }
void HavellsSolar::dump_config() {
  ESP_LOGCONFIG(TAG,
                "HAVELLS Solar:\n"
                "  Address: 0x%02X",
                this->address_);
  for (uint8_t i = 0; i < 3; i++) {
    auto phase = this->phases_[i];
    if (!phase.setup)
      continue;
    ESP_LOGCONFIG(TAG, "  Phase %c", i + 'A');
    LOG_SENSOR("    ", "Voltage", phase.voltage_sensor_);
    LOG_SENSOR("    ", "Current", phase.current_sensor_);
  }
  for (uint8_t i = 0; i < 2; i++) {
    auto pv = this->pvs_[i];
    if (!pv.setup)
      continue;
    ESP_LOGCONFIG(TAG, "  PV %d", i + 1);
    LOG_SENSOR("    ", "Voltage", pv.voltage_sensor_);
    LOG_SENSOR("    ", "Current", pv.current_sensor_);
    LOG_SENSOR("    ", "Active Power", pv.active_power_sensor_);
    LOG_SENSOR("    ", "Voltage Sampled By Secondary CPU", pv.voltage_sampled_by_secondary_cpu_sensor_);
    LOG_SENSOR("    ", "Insulation Of PV+ To Ground", pv.insulation_of_p_to_ground_sensor_);
  }
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Active Power", this->active_power_sensor_);
  LOG_SENSOR("  ", "Reactive Power", this->reactive_power_sensor_);
  LOG_SENSOR("  ", "Today Generation", this->today_production_sensor_);
  LOG_SENSOR("  ", "Total Generation", this->total_energy_production_sensor_);
  LOG_SENSOR("  ", "Total Generation Time", this->total_generation_time_sensor_);
  LOG_SENSOR("  ", "Today Generation Time", this->today_generation_time_sensor_);
  LOG_SENSOR("  ", "Inverter Module Temp", this->inverter_module_temp_sensor_);
  LOG_SENSOR("  ", "Inverter Inner Temp", this->inverter_inner_temp_sensor_);
  LOG_SENSOR("  ", "Inverter Bus Voltage", this->inverter_bus_voltage_sensor_);
  LOG_SENSOR("  ", "Insulation Of PV- To Ground", this->insulation_pv_n_to_ground_sensor_);
  LOG_SENSOR("  ", "GFCI Value", this->gfci_value_sensor_);
  LOG_SENSOR("  ", "DCI Of R", this->dci_of_r_sensor_);
  LOG_SENSOR("  ", "DCI Of S", this->dci_of_s_sensor_);
  LOG_SENSOR("  ", "DCI Of T", this->dci_of_t_sensor_);
}

}  // namespace esphome::havells_solar
