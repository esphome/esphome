#include "growatt_solar.h"
#include "esphome/core/log.h"

namespace esphome {
namespace growatt_solar {

static const char *const TAG = "growatt_solar";

static const uint8_t MODBUS_CMD_READ_IN_REGISTERS = 0x04;
static const uint8_t MODBUS_REGISTER_COUNT = 33;

void GrowattSolar::update() { this->send(MODBUS_CMD_READ_IN_REGISTERS, 0, MODBUS_REGISTER_COUNT); }

void GrowattSolar::on_modbus_data(const std::vector<uint8_t> &data) {
  auto publish_1_reg_sensor_state = [&](sensor::Sensor *sensor, size_t i, float unit) -> void {
    if (sensor == nullptr)
      return;
    float value = encode_uint16(data[i * 2], data[i * 2 + 1]) * unit;
    sensor->publish_state(value);
  };

  auto publish_2_reg_sensor_state = [&](sensor::Sensor *sensor, size_t reg1, size_t reg2, float unit) -> void {
    float value = ((encode_uint16(data[reg1 * 2], data[reg1 * 2 + 1]) << 16) +
                   encode_uint16(data[reg2 * 2], data[reg2 * 2 + 1])) *
                  unit;
    if (sensor != nullptr)
      sensor->publish_state(value);
  };

  publish_1_reg_sensor_state(this->inverter_status_, 0, 1);

  publish_2_reg_sensor_state(this->pv_active_power_sensor_, 1, 2, ONE_DEC_UNIT);

  publish_1_reg_sensor_state(this->pvs_[0].voltage_sensor_, 3, ONE_DEC_UNIT);
  publish_1_reg_sensor_state(this->pvs_[0].current_sensor_, 4, ONE_DEC_UNIT);
  publish_2_reg_sensor_state(this->pvs_[0].active_power_sensor_, 5, 6, ONE_DEC_UNIT);

  publish_1_reg_sensor_state(this->pvs_[1].voltage_sensor_, 7, ONE_DEC_UNIT);
  publish_1_reg_sensor_state(this->pvs_[1].current_sensor_, 8, ONE_DEC_UNIT);
  publish_2_reg_sensor_state(this->pvs_[1].active_power_sensor_, 9, 10, ONE_DEC_UNIT);

  publish_2_reg_sensor_state(this->grid_active_power_sensor_, 11, 12, ONE_DEC_UNIT);
  publish_1_reg_sensor_state(this->grid_frequency_sensor_, 13, TWO_DEC_UNIT);

  publish_1_reg_sensor_state(this->phases_[0].voltage_sensor_, 14, ONE_DEC_UNIT);
  publish_1_reg_sensor_state(this->phases_[0].current_sensor_, 15, ONE_DEC_UNIT);
  publish_2_reg_sensor_state(this->phases_[0].active_power_sensor_, 16, 17, ONE_DEC_UNIT);

  publish_1_reg_sensor_state(this->phases_[1].voltage_sensor_, 18, ONE_DEC_UNIT);
  publish_1_reg_sensor_state(this->phases_[1].current_sensor_, 19, ONE_DEC_UNIT);
  publish_2_reg_sensor_state(this->phases_[1].active_power_sensor_, 20, 21, ONE_DEC_UNIT);

  publish_1_reg_sensor_state(this->phases_[2].voltage_sensor_, 22, ONE_DEC_UNIT);
  publish_1_reg_sensor_state(this->phases_[2].current_sensor_, 23, ONE_DEC_UNIT);
  publish_2_reg_sensor_state(this->phases_[2].active_power_sensor_, 24, 25, ONE_DEC_UNIT);

  publish_2_reg_sensor_state(this->today_production_, 26, 27, ONE_DEC_UNIT);
  publish_2_reg_sensor_state(this->total_energy_production_, 28, 29, ONE_DEC_UNIT);

  publish_1_reg_sensor_state(this->inverter_module_temp_, 32, ONE_DEC_UNIT);
}

void GrowattSolar::dump_config() {
  ESP_LOGCONFIG(TAG, "GROWATT Solar:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
}

}  // namespace growatt_solar
}  // namespace esphome
