#include "havells_solar.h"
#include "havells_solar_registers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace havells_solar {

static const char *const TAG = "havells_solar";

static const uint8_t MODBUS_CMD_READ_IN_REGISTERS = 0x03;
static const uint8_t MODBUS_REGISTER_COUNT = 48;  // 48 x 16-bit registers

void HAVELLSSolar::on_modbus_data(const std::vector<uint8_t> &data) {
  if (data.size() < MODBUS_REGISTER_COUNT * 2) {
    ESP_LOGW(TAG, "Invalid size for HAVELLSSolar!");
    return;
  }

  auto havells_solar_get_2_registers = [&](size_t i,float unit) -> float {
    uint32_t temp = encode_uint32(data[i], data[i + 1], data[i + 2], data[i + 3]);
    return temp * unit;
  };

  auto havells_solar_get_1_register = [&](size_t i,float unit) -> float {
    uint16_t temp = encode_uint16(data[i], data[i + 1]);
	  return temp * unit ;
  };


  for (uint8_t i = 0; i < 3; i++) {
    auto phase = this->phases_[i];
    if (!phase.setup)
      continue;

    float voltage = havells_solar_get_1_register(HAVELLS_PHASE_1_VOLTAGE * 2 + (i * 4),ONE_DEC_UNIT);
    float current = havells_solar_get_1_register(HAVELLS_PHASE_1_CURRENT * 2 + (i * 4),TWO_DEC_UNIT);

//    ESP_LOGD(
//        TAG,
//        "HAVELLSSolar Phase %c: V=%.3f V, I=%.3f A ",
//        i + 'A', voltage, current);
    if (phase.voltage_sensor_ != nullptr)
      phase.voltage_sensor_->publish_state(voltage);
    if (phase.current_sensor_ != nullptr)
      phase.current_sensor_->publish_state(current);
  }

  for (uint8_t i = 0; i < 2; i++) {
    auto pv = this->pvs_[i];
    if (!pv.setup)
      continue;

    float voltage = havells_solar_get_1_register(HAVELLS_PV_1_VOLTAGE * 2 + (i * 4),ONE_DEC_UNIT);
    float current = havells_solar_get_1_register(HAVELLS_PV_1_CURRENT * 2 + (i * 4),TWO_DEC_UNIT);
    float active_power = havells_solar_get_1_register(HAVELLS_PV_1_POWER * 2 + (i * 2),MULTIPLY_TEN_UNIT);

//    ESP_LOGD(
//        TAG,
//        "HAVELLSSolar PV %c: V=%.3f V, I=%.3f A ,W=%.3f W",
//        i + 'A', voltage, current, active_power);
    if (pv.voltage_sensor_ != nullptr)
      pv.voltage_sensor_->publish_state(voltage);
    if (pv.current_sensor_ != nullptr)
      pv.current_sensor_->publish_state(current);
    if (pv.active_power_sensor_ != nullptr)
      pv.active_power_sensor_->publish_state(active_power);
  }

  float frequency = havells_solar_get_1_register(HAVELLS_GRID_FREQUENCY * 2,TWO_DEC_UNIT);
  float active_power = havells_solar_get_1_register(HAVELLS_SYSTEM_ACTIVE_POWER * 2,MULTIPLY_TEN_UNIT);
  float reactive_power = havells_solar_get_1_register(HAVELLS_SYSTEM_REACTIVE_POWER * 2,TWO_DEC_UNIT);
  float today_production = havells_solar_get_1_register(HAVELLS_TODAY_PRODUCTION * 2,TWO_DEC_UNIT);
  float total_energy_production = havells_solar_get_2_registers(HAVELLS_TOTAL_ENERGY_PRODUCTION * 2,NO_DEC_UNIT);
  float total_generation_time = havells_solar_get_2_registers(HAVELLS_TOTAL_GENERATION_TIME * 2,NO_DEC_UNIT);
  float today_generation_time = havells_solar_get_1_register(HAVELLS_TODAY_GENERATION_TIME * 2,NO_DEC_UNIT);
  float inverter_module_temp = havells_solar_get_1_register(HAVELLS_INVERTER_MODULE_TEMP * 2,NO_DEC_UNIT);
  float inverter_inner_temp = havells_solar_get_1_register(HAVELLS_INVERTER_INNER_TEMP * 2,NO_DEC_UNIT);
  float inverter_bus_voltage = havells_solar_get_1_register(HAVELLS_INVERTER_BUS_VOLTAGE * 2,NO_DEC_UNIT);
  float pv1_volt_sampled_by_slave_cpu = havells_solar_get_1_register(HAVELLS_PV1_VOLTAGE_SAMPLED_BY_SLAVE_CPU * 2,NO_DEC_UNIT);
  float pv2_volt_sampled_by_slave_cpu = havells_solar_get_1_register(HAVELLS_PV2_VOLTAGE_SAMPLED_BY_SLAVE_CPU * 2,NO_DEC_UNIT);
  float insulation_pv1_p_to_ground = havells_solar_get_1_register(HAVELLS_INSULATION_OF_PV1_P_TO_GROUND * 2,NO_DEC_UNIT);
  float insulation_pv2_p_to_ground = havells_solar_get_1_register(HAVELLS_INSULATION_OF_PV2_P_TO_GROUND * 2,NO_DEC_UNIT);
  float insulation_pv_n_to_ground = havells_solar_get_1_register(HAVELLS_INSULATION_OF_PV_N_TO_GROUND * 2,NO_DEC_UNIT);
  float gfci_value = havells_solar_get_1_register(HAVELLS_GFCI_VALUE * 2,NO_DEC_UNIT);
  float dci_of_r = havells_solar_get_1_register(HAVELLS_DCI_OF_R * 2,NO_DEC_UNIT);
  float dci_of_s = havells_solar_get_1_register(HAVELLS_DCI_OF_S * 2,NO_DEC_UNIT);
  float dci_of_t = havells_solar_get_1_register(HAVELLS_DCI_OF_T * 2,NO_DEC_UNIT);

//  ESP_LOGD(TAG, "HAVELLSSolar: F=%.3f Hz, Active P=%.3f W, Reactive P=%.3f VAR, TodayGeneration E=%.3f kWH",
//           frequency, active_power, reactive_power,today_production);
//  ESP_LOGD(TAG, "HAVELLSSolar: Total Generation E=%.3f kWh, Total Generation Time H=%.3f hrs, Today Generation Time H=%.3f hrs,",
//           total_energy_production,total_generation_time,today_generation_time);
//  ESP_LOGD(TAG, "HAVELLSSolar: Inverter Module Temp C=%.3f Degree, Inverter Inner Temp C=%.3f Degree,",
//           inverter_module_temp,inverter_inner_temp);
//  ESP_LOGD(TAG, "HAVELLSSolar: Inverter Bus Voltage V=%.3f V, PV1 Voltage Sampled By Slave CPU V=%.3f V, PV2 Voltage Sampled By Slave CPU V=%.3f V,",
//           inverter_bus_voltage,pv1_volt_sampled_by_slave_cpu,pv2_volt_sampled_by_slave_cpu);
//  ESP_LOGD(TAG, "HAVELLSSolar: Insulation Of PV1+ To Ground OHM=%.3f KΩ, Insulation Of PV2+ To Ground OHM=%.3f KΩ,Insulation Of PV- To Ground OHM=%.3f KΩ,",
//           insulation_pv1_p_to_ground,insulation_pv2_p_to_ground,insulation_pv_n_to_ground);
//  ESP_LOGD(TAG, "HAVELLSSolar: GFCI Value mA=%.3f mA, DCI Of R mA=%.3f mA, DCI Of S mA=%.3f mA, DCI Of T mA=%.3f mA,",
//           gfci_value,dci_of_r,dci_of_s,dci_of_t);
  
  if (this->frequency_sensor_ != nullptr)
    this->frequency_sensor_->publish_state(frequency);
  if (this->active_power_sensor_ != nullptr)
    this->active_power_sensor_->publish_state(active_power);
  if (this->reactive_power_sensor_ != nullptr)
    this->reactive_power_sensor_->publish_state(reactive_power);
  if (this->today_production_sensor_ != nullptr)
    this->today_production_sensor_->publish_state(today_production);
  if (this->total_energy_production_sensor_ != nullptr)
    this->total_energy_production_sensor_->publish_state(total_energy_production);
  if (this->total_generation_time_sensor_ != nullptr)
    this->total_generation_time_sensor_->publish_state(total_generation_time);
  if (this->today_generation_time_sensor_ != nullptr)
    this->today_generation_time_sensor_->publish_state(today_generation_time);
  if (this->inverter_module_temp_sensor_ != nullptr)
    this->inverter_module_temp_sensor_->publish_state(inverter_module_temp);
  if (this->inverter_inner_temp_sensor_ != nullptr)
    this->inverter_inner_temp_sensor_->publish_state(inverter_inner_temp);
  if (this->inverter_bus_voltage_sensor_ != nullptr)
    this->inverter_bus_voltage_sensor_->publish_state(inverter_bus_voltage);
  if (this->pv1_volt_sampled_by_slave_cpu_sensor_ != nullptr)
    this->pv1_volt_sampled_by_slave_cpu_sensor_->publish_state(pv1_volt_sampled_by_slave_cpu);
  if (this->pv2_volt_sampled_by_slave_cpu_sensor_ != nullptr)
    this->pv2_volt_sampled_by_slave_cpu_sensor_->publish_state(pv2_volt_sampled_by_slave_cpu);
  if (this->insulation_pv1_p_to_ground_sensor_ != nullptr)
    this->insulation_pv1_p_to_ground_sensor_->publish_state(insulation_pv1_p_to_ground);
  if (this->insulation_pv2_p_to_ground_sensor_ != nullptr)
    this->insulation_pv2_p_to_ground_sensor_->publish_state(insulation_pv2_p_to_ground);
  if (this->insulation_pv_n_to_ground_sensor_ != nullptr)
    this->insulation_pv_n_to_ground_sensor_->publish_state(insulation_pv_n_to_ground);
  if (this->gfci_value_sensor_ != nullptr)
    this->gfci_value_sensor_->publish_state(gfci_value);
  if (this->dci_of_r_sensor_ != nullptr)
    this->dci_of_r_sensor_->publish_state(dci_of_r);
  if (this->dci_of_s_sensor_ != nullptr)
    this->dci_of_s_sensor_->publish_state(dci_of_s);
  if (this->dci_of_t_sensor_ != nullptr)
    this->dci_of_t_sensor_->publish_state(dci_of_t);
}

void HAVELLSSolar::update() { this->send(MODBUS_CMD_READ_IN_REGISTERS, 0, MODBUS_REGISTER_COUNT); }
void HAVELLSSolar::dump_config() {
  ESP_LOGCONFIG(TAG, "HAVELLS Solar:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
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
    ESP_LOGCONFIG(TAG, "  PV %c", i + 'A');
    LOG_SENSOR("    ", "Voltage", pv.voltage_sensor_);
    LOG_SENSOR("    ", "Current", pv.current_sensor_);
    LOG_SENSOR("    ", "Active Power", pv.active_power_sensor_);
  }
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("    ", "Active Power", this->active_power_sensor_);
  LOG_SENSOR("    ", "Reactive Power", this->reactive_power_sensor_);
  LOG_SENSOR("    ", "Today Generation", this->today_production_sensor_);
  LOG_SENSOR("    ", "Total Generation", this->total_energy_production_sensor_);
  LOG_SENSOR("    ", "Total Generation Time", this->total_generation_time_sensor_);
  LOG_SENSOR("    ", "Today Generation Time", this->today_generation_time_sensor_);
  LOG_SENSOR("    ", "Inverter Module Temp", this->inverter_module_temp_sensor_);
  LOG_SENSOR("    ", "Inverter Inner Temp", this->inverter_inner_temp_sensor_);
  LOG_SENSOR("    ", "Inverter Bus Voltage", this->inverter_bus_voltage_sensor_);
  LOG_SENSOR("    ", "PV1 Voltage Sampled By Slave CPU", this->pv1_volt_sampled_by_slave_cpu_sensor_);
  LOG_SENSOR("    ", "PV2 Voltage Sampled By Slave CPU", this->pv2_volt_sampled_by_slave_cpu_sensor_);
  LOG_SENSOR("    ", "Insulation Of PV1+ To Ground", this->insulation_pv1_p_to_ground_sensor_);
  LOG_SENSOR("    ", "Insulation Of PV2+ To Ground", this->insulation_pv2_p_to_ground_sensor_);
  LOG_SENSOR("    ", "Insulation Of PV- To Ground", this->insulation_pv_n_to_ground_sensor_);
  LOG_SENSOR("    ", "GFCI Value", this->gfci_value_sensor_);
  LOG_SENSOR("    ", "DCI Of R", this->dci_of_r_sensor_);
  LOG_SENSOR("    ", "DCI Of S", this->dci_of_s_sensor_);
  LOG_SENSOR("    ", "DCI Of T", this->dci_of_t_sensor_);
}

}  // namespace havells_solar
}  // namespace esphome
