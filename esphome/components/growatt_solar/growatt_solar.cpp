#include "growatt_solar.h"
#include "esphome/core/log.h"

namespace esphome::growatt_solar {

static const char *const TAG = "growatt_solar";

static const uint8_t MODBUS_REGISTER_COUNT[] = {33, 95};  // indexed with enum GrowattProtocolVersion

void GrowattSolar::update() { this->read_input_registers(0, MODBUS_REGISTER_COUNT[this->protocol_version_]); }

void GrowattSolar::on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                           modbus::ResponseStatus status) {
  if (!modbus::succeeded(status))
    return;

  // Publish a sensor if its register(s) are in this response; skipping absent registers keeps this
  // correct for any read range, so the poll may be split into multiple requests.
  auto publish_1_reg_sensor_state = [&](sensor::Sensor *sensor, size_t reg, float unit) -> void {
    if (sensor == nullptr || reg < start_address)
      return;
    size_t offset = reg - start_address;
    if (offset >= registers.size())
      return;
    sensor->publish_state(registers[offset] * unit);
  };

  auto publish_2_reg_sensor_state = [&](sensor::Sensor *sensor, size_t reg, float unit) -> void {
    constexpr auto value_type = modbus::helpers::SensorValueType::U_DWORD;
    if (sensor == nullptr || reg < start_address)
      return;
    size_t offset = reg - start_address;
    if (offset + modbus::helpers::register_width_for(value_type) > registers.size())
      return;
    sensor->publish_state(modbus::helpers::registers_to_value<value_type>(registers.data() + offset) * unit);
  };

  switch (this->protocol_version_) {
    case RTU: {
      publish_1_reg_sensor_state(this->inverter_status_, RTU_INVERTER_STATUS, 1);

      publish_2_reg_sensor_state(this->pv_active_power_sensor_, RTU_PV_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_1_reg_sensor_state(this->pvs_[0].voltage_sensor_, RTU_PV1_VOLTAGE, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->pvs_[0].current_sensor_, RTU_PV1_CURRENT, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->pvs_[0].active_power_sensor_, RTU_PV1_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_1_reg_sensor_state(this->pvs_[1].voltage_sensor_, RTU_PV2_VOLTAGE, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->pvs_[1].current_sensor_, RTU_PV2_CURRENT, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->pvs_[1].active_power_sensor_, RTU_PV2_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_2_reg_sensor_state(this->grid_active_power_sensor_, RTU_GRID_ACTIVE_POWER, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->grid_frequency_sensor_, RTU_GRID_FREQUENCY, TWO_DEC_UNIT);

      publish_1_reg_sensor_state(this->phases_[0].voltage_sensor_, RTU_PHASE1_VOLTAGE, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->phases_[0].current_sensor_, RTU_PHASE1_CURRENT, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->phases_[0].active_power_sensor_, RTU_PHASE1_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_1_reg_sensor_state(this->phases_[1].voltage_sensor_, RTU_PHASE2_VOLTAGE, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->phases_[1].current_sensor_, RTU_PHASE2_CURRENT, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->phases_[1].active_power_sensor_, RTU_PHASE2_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_1_reg_sensor_state(this->phases_[2].voltage_sensor_, RTU_PHASE3_VOLTAGE, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->phases_[2].current_sensor_, RTU_PHASE3_CURRENT, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->phases_[2].active_power_sensor_, RTU_PHASE3_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_2_reg_sensor_state(this->today_production_, RTU_TODAY_PRODUCTION, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->total_energy_production_, RTU_TOTAL_ENERGY_PRODUCTION, ONE_DEC_UNIT);

      publish_1_reg_sensor_state(this->inverter_module_temp_, RTU_INVERTER_MODULE_TEMP, ONE_DEC_UNIT);
      break;
    }
    case RTU2: {
      publish_1_reg_sensor_state(this->inverter_status_, RTU2_INVERTER_STATUS, 1);

      publish_2_reg_sensor_state(this->pv_active_power_sensor_, RTU2_PV_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_1_reg_sensor_state(this->pvs_[0].voltage_sensor_, RTU2_PV1_VOLTAGE, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->pvs_[0].current_sensor_, RTU2_PV1_CURRENT, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->pvs_[0].active_power_sensor_, RTU2_PV1_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_1_reg_sensor_state(this->pvs_[1].voltage_sensor_, RTU2_PV2_VOLTAGE, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->pvs_[1].current_sensor_, RTU2_PV2_CURRENT, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->pvs_[1].active_power_sensor_, RTU2_PV2_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_2_reg_sensor_state(this->grid_active_power_sensor_, RTU2_GRID_ACTIVE_POWER, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->grid_frequency_sensor_, RTU2_GRID_FREQUENCY, TWO_DEC_UNIT);

      publish_1_reg_sensor_state(this->phases_[0].voltage_sensor_, RTU2_PHASE1_VOLTAGE, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->phases_[0].current_sensor_, RTU2_PHASE1_CURRENT, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->phases_[0].active_power_sensor_, RTU2_PHASE1_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_1_reg_sensor_state(this->phases_[1].voltage_sensor_, RTU2_PHASE2_VOLTAGE, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->phases_[1].current_sensor_, RTU2_PHASE2_CURRENT, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->phases_[1].active_power_sensor_, RTU2_PHASE2_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_1_reg_sensor_state(this->phases_[2].voltage_sensor_, RTU2_PHASE3_VOLTAGE, ONE_DEC_UNIT);
      publish_1_reg_sensor_state(this->phases_[2].current_sensor_, RTU2_PHASE3_CURRENT, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->phases_[2].active_power_sensor_, RTU2_PHASE3_ACTIVE_POWER, ONE_DEC_UNIT);

      publish_2_reg_sensor_state(this->today_production_, RTU2_TODAY_PRODUCTION, ONE_DEC_UNIT);
      publish_2_reg_sensor_state(this->total_energy_production_, RTU2_TOTAL_ENERGY_PRODUCTION, ONE_DEC_UNIT);

      publish_1_reg_sensor_state(this->inverter_module_temp_, RTU2_INVERTER_MODULE_TEMP, ONE_DEC_UNIT);
      break;
    }
  }
}

void GrowattSolar::dump_config() {
  ESP_LOGCONFIG(TAG,
                "GROWATT Solar:\n"
                "  Address: 0x%02X",
                this->address_);
}

}  // namespace esphome::growatt_solar
