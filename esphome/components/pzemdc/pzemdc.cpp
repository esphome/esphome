#include "pzemdc.h"
#include "esphome/core/log.h"

namespace esphome::pzemdc {

static const char *const TAG = "pzemdc";

static const uint8_t PZEM_CMD_RESET_ENERGY = 0x42;
static const uint8_t PZEM_REGISTER_COUNT = 8;  // 8x 16-bit registers

// Register map, see https://github.com/esphome/feature-requests/issues/49#issuecomment-538636809
// 32-bit values are two registers, low word first.
static const uint16_t PZEM_REGISTER_VOLTAGE = 0;  // 1 register, 0.01 V
static const uint16_t PZEM_REGISTER_CURRENT = 1;  // 1 register, 0.01 A
static const uint16_t PZEM_REGISTER_POWER = 2;    // 2 registers, 0.1 W
static const uint16_t PZEM_REGISTER_ENERGY = 4;   // 2 registers, 1 Wh

void PZEMDC::on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                     modbus::ResponseStatus status) {
  if (!modbus::succeeded(status))
    return;  // the hub already logs exception responses

  // Publish a sensor if its register(s) are in this response; skipping absent registers keeps this
  // correct for any read range, so the poll may be split into multiple requests.
  auto publish_1_register = [&](sensor::Sensor *sensor, uint16_t reg, float divisor) -> void {
    if (sensor == nullptr || reg < start_address)
      return;
    size_t offset = reg - start_address;
    if (offset >= registers.size())
      return;
    sensor->publish_state(registers[offset] / divisor);
  };

  auto publish_2_registers = [&](sensor::Sensor *sensor, uint16_t reg, float divisor) -> void {
    constexpr auto value_type = modbus::helpers::SensorValueType::U_DWORD_R;
    if (sensor == nullptr || reg < start_address)
      return;
    size_t offset = reg - start_address;
    if (offset + modbus::helpers::register_width_for(value_type) > registers.size())
      return;
    sensor->publish_state(modbus::helpers::registers_to_value<value_type>(registers.data() + offset) / divisor);
  };

  publish_1_register(this->voltage_sensor_, PZEM_REGISTER_VOLTAGE, 100.0f);
  publish_1_register(this->current_sensor_, PZEM_REGISTER_CURRENT, 100.0f);
  publish_2_registers(this->power_sensor_, PZEM_REGISTER_POWER, 10.0f);
  publish_2_registers(this->energy_sensor_, PZEM_REGISTER_ENERGY, 1000.0f);
}

void PZEMDC::on_custom_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu,
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

void PZEMDC::update() { this->read_input_registers(0, PZEM_REGISTER_COUNT); }
void PZEMDC::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PZEMDC:\n"
                "  Address: 0x%02X",
                this->address_);
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
  LOG_SENSOR("", "Energy", this->energy_sensor_);
}

void PZEMDC::reset_energy() {
  const uint8_t pdu[] = {PZEM_CMD_RESET_ENERGY};
  this->queue_pdu(pdu);
}

}  // namespace esphome::pzemdc
