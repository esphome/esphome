#include "growatt_solar.h"
#include "growatt_solar_registers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace growatt_solar {

static const char *const TAG = "growatt_solar";

static const uint8_t MODBUS_CMD_READ_IN_REGISTERS = 0x04;  // 0x03;
static const uint8_t MODBUS_REGISTER_COUNT = 33;           // 48;           // 48;  // 48 x 16-bit registers

void GrowattSolar::update() {
  ESP_LOGW(TAG, "Request send");
  this->send(MODBUS_CMD_READ_IN_REGISTERS, 0, MODBUS_REGISTER_COUNT);
}

float GrowattSolar::glueFloat(uint16_t w1, uint16_t w0) {
  unsigned long t;
  t = w1 << 16;
  t += w0;

  float f;
  f = t;
  f = f / 10;
  return f;
}
void GrowattSolar::on_modbus_data(const std::vector<uint8_t> &data) {

  auto get_4_registers = [&](size_t i, float unit) -> float {
    uint32_t temp = encode_uint32(data[i], data[i + 1], data[i + 2], data[i + 3]);
    return temp * unit;
  };

  auto get_2_register = [&](size_t i) -> float {
    uint16_t temp = encode_uint16(data[i], data[i + 1]);
    return temp;
  };

  auto get_1_register = [&](size_t i, float unit) -> float {
    uint16_t temp = data[i];
    return temp * unit;
  };

  auto publish_1_reg_sensor_state = [&](sensor::Sensor *sensor, size_t i, float unit) -> void {
    float value = this->glueFloat(0, get_2_register(i * 2)) * unit;
    if (sensor != nullptr)
      sensor->publish_state(value);
  };

  auto publish_2_reg_sensor_state = [&](sensor::Sensor *sensor, size_t reg1, size_t reg2, float unit) -> void {
    float value = this->glueFloat(get_2_register(reg1 * 2), get_2_register(reg2 * 2)) * unit;
    if (sensor != nullptr)
      sensor->publish_state(value);
  };

  // this->publishState(this->grid_active_power_sensor_,
  //                    this->glueFloat(get_2_register(11 * 2), get_2_register(12 * 2)) * ONE_DEC_UNIT);

  publish_1_reg_sensor_state(this->inverter_status_, 00, ONE_DEC_UNIT);
  // publish_2_reg_sensor_state(this->pv_input_power_, 01, 02, ONE_DEC_UNIT);

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
  // 30,31 = work time total

  publish_1_reg_sensor_state(this->inverter_module_temp_, 32, ONE_DEC_UNIT);

  // this->publishState(this->grid_frequency_sensor_, this->glueFloat(0, get_2_register(13 * 2)) * TWO_DEC_UNIT);
}

void GrowattSolar::dump_config() {
  ESP_LOGCONFIG(TAG, "GROWATT Solar:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
}

}  // namespace growatt_solar
}  // namespace esphome