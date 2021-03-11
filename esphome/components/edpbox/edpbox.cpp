#include "edpbox.h"
#include "esphome/core/log.h"

namespace esphome {
namespace edpbox {

static const char *TAG = "edpbox";

static const uint8_t EDPBOX_CMD_READ_IN_REGISTERS = 0x04;
static const uint8_t EDPBOX_REGISTER_COUNT = 7;  // 7x 16-bit registers
static const uint8_t EDPBOX_REGISTER_START = 108;  // 006C

void EDPBOX::on_modbus_data(const std::vector<uint8_t> &data) {
  if (data.size() < 10) {
    ESP_LOGW(TAG, "Invalid size for EDPBOX!");
    return;
  }

  // 01 04 0e 09 21 00 37 09 32 00 01 09 2f 00 0c 00 45 65 9a 
  // Id Cc Sz Volt  Curr  Volt  Curr  Volt  Curr  Curtt crc
  //           0     2    4     6     8     10    12

  auto edpbox_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto edpbox_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(edpbox_get_16bit(i + 2)) << 16) | (uint32_t(edpbox_get_16bit(i + 0)) << 0);
  };

  uint16_t raw_voltage = edpbox_get_16bit(0);
  float voltage = raw_voltage / 10.0f;  // max 6553.5 V

  uint32_t raw_current = edpbox_get_32bit(2);
  float current = raw_current / 1000.0f;  // max 4294967.295 A

  uint32_t raw_active_power = edpbox_get_32bit(6);
  float active_power = raw_active_power / 10.0f;  // max 429496729.5 W

  float active_energy = static_cast<float>(edpbox_get_32bit(10));

  uint16_t raw_frequency = edpbox_get_16bit(14);
  float frequency = raw_frequency / 10.0f;

  uint16_t raw_power_factor = edpbox_get_16bit(16);
  float power_factor = raw_power_factor / 100.0f;

  ESP_LOGD(TAG, "EDPBOX: V=%.1f V, I=%.3f A, P=%.1f W, E=%.1f Wh, F=%.1f Hz, PF=%.2f", voltage, current, active_power,
           active_energy, frequency, power_factor);
  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(voltage);
  if (this->current_sensor_ != nullptr)
    this->current_sensor_->publish_state(current);
  if (this->power_sensor_ != nullptr)
    this->power_sensor_->publish_state(active_power);
  if (this->energy_sensor_ != nullptr)
    this->energy_sensor_->publish_state(active_energy);
  if (this->frequency_sensor_ != nullptr)
    this->frequency_sensor_->publish_state(frequency);
  if (this->power_factor_sensor_ != nullptr)
    this->power_factor_sensor_->publish_state(power_factor);
}

void EDPBOX::update() { this->send(EDPBOX_CMD_READ_IN_REGISTERS, EDPBOX_REGISTER_START, EDPBOX_REGISTER_COUNT); }
void EDPBOX::dump_config() {
  ESP_LOGCONFIG(TAG, "EDPBOX:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Power", this->power_sensor_);
  LOG_SENSOR("", "Energy", this->energy_sensor_);
  LOG_SENSOR("", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("", "Power Factor", this->power_factor_sensor_);
}

}  // namespace edpbox
}  // namespace esphome
