#include "pzemdc.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pzemdc {

static const char *TAG = "pzemdc";

static const uint8_t PZEM_CMD_READ_IN_REGISTERS = 0x04;
static const uint8_t PZEM_REGISTER_COUNT = 10;  // 10x 16-bit registers

void PZEMDC::on_modbus_data(const std::vector<uint8_t> &data) {
  if (data.size() < 16) {
    ESP_LOGW(TAG, "Invalid size for PZEM DC!");
    return;
  }

  // See https://github.com/esphome/feature-requests/issues/49#issuecomment-538636809
  //           0     1     2     3     4     5     6     7           = ModBus register
  //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20  = Buffer index
  // 01 04 10 05 40 00 0A 00 0D 00 00 00 02 00 00 00 00 00 00 D6 29
  // Id Cc Sz Volt- Curre Power------ Energy----- HiAlm LoAlm Crc--

  auto pzem_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto pzem_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(pzem_get_16bit(i + 2)) << 16) | (uint32_t(pzem_get_16bit(i + 0)) << 0);
  };

  uint16_t raw_voltage = pzem_get_16bit(0);
  float voltage = raw_voltage / 100.0f;  // max 655.35 V

  uint16_t raw_current = pzem_get_16bit(2);
  float current = raw_current / 100.0f;  // max 655.35 A

  uint32_t raw_power = pzem_get_32bit(4);
  float power = raw_power / 10.0f;  // max 429496729.5 W

  ESP_LOGD(TAG, "PZEM DC: V=%.1f V, I=%.3f A, P=%.1f W", voltage, current, power);
  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(voltage);
  if (this->current_sensor_ != nullptr)
    this->current_sensor_->publish_state(current);
  if (this->power_sensor_ != nullptr)
    this->power_sensor_->publish_state(power);
}

void PZEMDC::update() { this->send(PZEM_CMD_READ_IN_REGISTERS, 0, 8); }
void PZEMDC::dump_config() {
  ESP_LOGCONFIG(TAG, "PZEMDC:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
}

}  // namespace pzemdc
}  // namespace esphome
