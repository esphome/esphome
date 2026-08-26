#include "pzemdc.h"
#include "esphome/core/log.h"

namespace esphome::pzemdc {

static const char *const TAG = "pzemdc";

static const uint8_t PZEM_CMD_RESET_ENERGY = 0x42;
static const uint8_t PZEM_REGISTER_COUNT = 10;  // 10x 16-bit registers

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
    if (sensor == nullptr || reg < start_address)
      return;
    size_t offset = reg - start_address;
    if (offset + 2 > registers.size())
      return;
    auto value =
        modbus::helpers::registers_to_number(registers.data() + offset, 2, modbus::helpers::SensorValueType::U_DWORD_R);
    if (value.has_value())
      sensor->publish_state(*value / divisor);
  };

  publish_1_register(this->voltage_sensor_, PZEM_REGISTER_VOLTAGE, 100.0f);
  publish_1_register(this->current_sensor_, PZEM_REGISTER_CURRENT, 100.0f);
  publish_2_registers(this->power_sensor_, PZEM_REGISTER_POWER, 10.0f);
  publish_2_registers(this->energy_sensor_, PZEM_REGISTER_ENERGY, 1000.0f);
}

void PZEMDC::update() { this->read_input_registers(0, 8); }
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
