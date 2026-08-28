#include "pzemac.h"
#include "esphome/core/log.h"

namespace esphome::pzemac {

namespace helpers = modbus::helpers;

static const char *const TAG = "pzemac";

static const uint8_t PZEM_CMD_RESET_ENERGY = 0x42;
static const uint8_t PZEM_REGISTER_COUNT = 10;  // 10x 16-bit registers

// Register map, see https://github.com/esphome/feature-requests/issues/49#issuecomment-538636809
// 32-bit values are two registers, low word first.
static const uint16_t PZEM_REGISTER_VOLTAGE = 0;        // 1 register, 0.1 V
static const uint16_t PZEM_REGISTER_CURRENT = 1;        // 2 registers, 0.001 A
static const uint16_t PZEM_REGISTER_ACTIVE_POWER = 3;   // 2 registers, 0.1 W
static const uint16_t PZEM_REGISTER_ACTIVE_ENERGY = 5;  // 2 registers, 1 Wh
static const uint16_t PZEM_REGISTER_FREQUENCY = 7;      // 1 register, 0.1 Hz
static const uint16_t PZEM_REGISTER_POWER_FACTOR = 8;   // 1 register, 0.01

void PZEMAC::on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                     modbus::ResponseStatus status) {
  if (!modbus::succeeded(status))
    return;  // the hub already logs exception responses

  // Publish a sensor if its register(s) are in this response; skipping absent registers keeps this
  // correct for any read range, so the poll may be split into multiple requests.
  auto publish_1_register = [&](sensor::Sensor *sensor, uint16_t reg, float divisor) -> void {
    if (sensor == nullptr)
      return;
    if (auto value = helpers::value_at<helpers::SensorValueType::U_WORD>(registers, start_address, reg))
      sensor->publish_state(*value / divisor);
  };

  auto publish_2_registers = [&](sensor::Sensor *sensor, uint16_t reg, float divisor) -> void {
    if (sensor == nullptr)
      return;
    if (auto value = helpers::value_at<helpers::SensorValueType::U_DWORD_R>(registers, start_address, reg))
      sensor->publish_state(*value / divisor);
  };

  publish_1_register(this->voltage_sensor_, PZEM_REGISTER_VOLTAGE, 10.0f);
  publish_2_registers(this->current_sensor_, PZEM_REGISTER_CURRENT, 1000.0f);
  publish_2_registers(this->power_sensor_, PZEM_REGISTER_ACTIVE_POWER, 10.0f);
  publish_2_registers(this->energy_sensor_, PZEM_REGISTER_ACTIVE_ENERGY, 1.0f);
  publish_1_register(this->frequency_sensor_, PZEM_REGISTER_FREQUENCY, 10.0f);
  publish_1_register(this->power_factor_sensor_, PZEM_REGISTER_POWER_FACTOR, 100.0f);
}

void PZEMAC::on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
                                modbus::ResponseStatus status) {
  // The only custom request this component sends is the energy reset; acknowledge its echo here so
  // the default unhandled-response warning stays meaningful.
  if (!request_pdu.empty() && request_pdu[0] == PZEM_CMD_RESET_ENERGY) {
    if (modbus::succeeded(status)) {
      ESP_LOGD(TAG, "Energy reset acknowledged");
    } else {
      ESP_LOGW(TAG, "Energy reset rejected");
    }
    return;
  }
  modbus::ModbusClientDevice::on_custom_response(request_pdu, response_pdu, status);
}

void PZEMAC::update() { this->read_input_registers(0, PZEM_REGISTER_COUNT); }
void PZEMAC::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PZEMAC:\n"
                "  Address: 0x%02X",
                this->address_);
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
  LOG_SENSOR("", "Energy", this->energy_sensor_);
  LOG_SENSOR("", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("", "Power Factor", this->power_factor_sensor_);
}

void PZEMAC::reset_energy_() {
  const uint8_t pdu[] = {PZEM_CMD_RESET_ENERGY};
  this->queue_pdu(pdu);
}

}  // namespace esphome::pzemac
