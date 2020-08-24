#include "sdm220m.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sdm220m {

static const char *TAG = "sdm220m";

static const uint8_t MODBUS_CMD_READ_IN_REGISTERS = 0x04;
static const uint8_t MODBUS_REGISTER_COUNT = 74;  // 74 x 16-bit registers

void SDM220M::on_modbus_data(const std::vector<uint8_t> &data) {
  if (data.size() < MODBUS_REGISTER_COUNT * 2) {
    ESP_LOGW(TAG, "Invalid size for SDM220M!");
    return;
  }

  auto sdm220m_get_16bit = [&](size_t i) -> uint16_t {
    return (uint16_t(data[i + 0]) << 8) | (uint16_t(data[i + 1]) << 0);
  };
  auto sdm220m_get_32bit = [&](size_t i) -> uint32_t {
    return (uint32_t(sdm220m_get_16bit(i + 0)) << 16) | (uint32_t(sdm220m_get_16bit(i + 2)) << 0);
  };

  uint32_t temp;

  temp = sdm220m_get_32bit(0);
  float voltage = *((float *) &temp);

  temp = sdm220m_get_32bit(12);
  float current = *((float *) &temp);

  temp = sdm220m_get_32bit(24);
  float active_power = *((float *) &temp);

  temp = sdm220m_get_32bit(36);
  float apparent_power = *((float *) &temp);

  temp = sdm220m_get_32bit(48);
  float reactive_power = *((float *) &temp);

  temp = sdm220m_get_32bit(60);
  float power_factor = *((float *) &temp);

  temp = sdm220m_get_32bit(72);
  float phase_angle = *((float *) &temp);

  temp = sdm220m_get_32bit(140);
  float frequency = *((float *) &temp);

  temp = sdm220m_get_32bit(144);
  float import_active_energy = *((float *) &temp);

  temp = sdm220m_get_32bit(148);
  float export_active_energy = *((float *) &temp);

  temp = sdm220m_get_32bit(152);
  float import_reactive_energy = *((float *) &temp);

  temp = sdm220m_get_32bit(156);
  float export_reactive_energy = *((float *) &temp);

  ESP_LOGD(TAG,
           "SDM220M: V=%.3f V, I=%.3f A, Active P=%.3f W, Apparent P=%.3f VA, Reactive P=%.3f VAR, PF=%.3f, PA=%.3f °, "
           "F=%.3f Hz, Im.A.E=%.3f Wh, Ex.A.E=%.3f Wh, Im.R.E=%.3f VARh, Ex.R.E=%.3f VARh",
           voltage, current, active_power, apparent_power, reactive_power, power_factor, phase_angle, frequency,
           import_active_energy, export_active_energy, import_reactive_energy, export_reactive_energy);
  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(voltage);
  if (this->current_sensor_ != nullptr)
    this->current_sensor_->publish_state(current);
  if (this->active_power_sensor_ != nullptr)
    this->active_power_sensor_->publish_state(active_power);
  if (this->apparent_power_sensor_ != nullptr)
    this->apparent_power_sensor_->publish_state(apparent_power);
  if (this->reactive_power_sensor_ != nullptr)
    this->reactive_power_sensor_->publish_state(reactive_power);
  if (this->power_factor_sensor_ != nullptr)
    this->power_factor_sensor_->publish_state(power_factor);
  if (this->phase_angle_sensor_ != nullptr)
    this->phase_angle_sensor_->publish_state(phase_angle);
  if (this->frequency_sensor_ != nullptr)
    this->frequency_sensor_->publish_state(frequency);
  if (this->import_active_energy_sensor_ != nullptr)
    this->import_active_energy_sensor_->publish_state(import_active_energy);
  if (this->export_active_energy_sensor_ != nullptr)
    this->export_active_energy_sensor_->publish_state(export_active_energy);
  if (this->import_reactive_energy_sensor_ != nullptr)
    this->import_reactive_energy_sensor_->publish_state(import_reactive_energy);
  if (this->export_reactive_energy_sensor_ != nullptr)
    this->export_reactive_energy_sensor_->publish_state(export_reactive_energy);
}

void SDM220M::update() { this->send(MODBUS_CMD_READ_IN_REGISTERS, 0, MODBUS_REGISTER_COUNT); }
void SDM220M::dump_config() {
  ESP_LOGCONFIG(TAG, "SDM220M:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("", "Current", this->current_sensor_);
  LOG_SENSOR("", "Active Power", this->active_power_sensor_);
  LOG_SENSOR("", "Apparent Power", this->apparent_power_sensor_);
  LOG_SENSOR("", "Reactive Power", this->reactive_power_sensor_);
  LOG_SENSOR("", "Power Factor", this->power_factor_sensor_);
  LOG_SENSOR("", "Phase Angle", this->phase_angle_sensor_);
  LOG_SENSOR("", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("", "Import Active Energy", this->import_active_energy_sensor_);
  LOG_SENSOR("", "Export Active Energy", this->export_active_energy_sensor_);
  LOG_SENSOR("", "Import Reactive Energy", this->import_reactive_energy_sensor_);
  LOG_SENSOR("", "Export Reactive Energy", this->export_reactive_energy_sensor_);
}

}  // namespace sdm220m
}  // namespace esphome
