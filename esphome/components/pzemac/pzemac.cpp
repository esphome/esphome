#include "pzemac.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pzemac {

static const char *TAG = "pzemac";

static const uint8_t PZEM_CMD_READ_IN_REGISTERS = 0x04;
static const uint8_t PZEM_REGISTER_COUNT = 10;  // 10x 16-bit registers

void PZEMAC::on_modbus_data(const std::vector<uint8_t> &data) {
  if (data.size() < 20) {
    ESP_LOGW(TAG, "Invalid size for PZEM AC!");
    return;
  }

  // See https://github.com/esphome/feature-requests/issues/49#issuecomment-538636809
  //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
  // 01 04 14 08 D1 00 6C 00 00 00 F4 00 00 00 26 00 00 01 F4 00 64 00 00 51 34
  // Id Cc Sz Volt- Current---- Power------ Energy----- Frequ PFact Alarm Crc--

  auto pzem_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto pzem_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(pzem_get_16bit(i + 2)) << 16) | (uint32_t(pzem_get_16bit(i + 0)) << 0);
  };

  uint16_t raw_voltage = pzem_get_16bit(0);
  float voltage = raw_voltage / 10.0f;  // max 6553.5 V

  uint32_t raw_current = pzem_get_32bit(2);
  float current = raw_current / 1000.0f;  // max 4294967.295 A

  uint32_t raw_active_power = pzem_get_32bit(6);
  float active_power = raw_active_power / 10.0f;  // max 429496729.5 W

  uint16_t raw_frequency = pzem_get_16bit(14);
  float frequency = raw_frequency / 10.0f;

  uint16_t raw_power_factor = pzem_get_16bit(16);
  float power_factor = raw_power_factor / 100.0f;

  ESP_LOGD(TAG, "PZEM AC: V=%.1f V, I=%.3f A, P=%.1f W, F=%.1f Hz, PF=%.2f", voltage, current, active_power, frequency,
           power_factor);
  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(voltage);
  if (this->current_sensor_ != nullptr)
    this->current_sensor_->publish_state(current);
  if (this->power_sensor_ != nullptr)
    this->power_sensor_->publish_state(active_power);
  if (this->frequency_sensor_ != nullptr)
    this->frequency_sensor_->publish_state(frequency);
  if (this->power_factor_sensor_ != nullptr)
    this->power_factor_sensor_->publish_state(power_factor);
}

void PZEMAC::update() { this->send(PZEM_CMD_READ_IN_REGISTERS, 0, PZEM_REGISTER_COUNT); }
void PZEMAC::dump_config() {
  ESP_LOGCONFIG(TAG, "PZEMAC:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
  LOG_SENSOR("", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("", "Power Factor", this->power_factor_sensor_);
}

}  // namespace pzemac
}  // namespace esphome
