#include "pzemac.h"
#include "esphome/core/log.h"

namespace esphome::pzemac {

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
static const uint16_t PZEM_REGISTER_ALARM = 9;          // 1 register

// Installation sanity limits, expressed in raw register units.
// Voltage headroom allows recording an open-neutral fault in a 230/400 V system.
static const uint16_t PZEM_MAX_VOLTAGE = 4500;         // 450.0 V
static const uint32_t PZEM_MAX_CURRENT = 100000;       // 100.000 A
static const uint32_t PZEM_MAX_ACTIVE_POWER = 450000;  // 45.0 kW at 450 V / 100 A
static const uint16_t PZEM_MIN_FREQUENCY = 450;        // 45.0 Hz
static const uint16_t PZEM_MAX_FREQUENCY = 650;        // 65.0 Hz
static const uint16_t PZEM_MAX_POWER_FACTOR = 100;     // 1.00

void PZEMAC::on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                     modbus::ResponseStatus status) {
  if (!modbus::succeeded(status))
    return;  // the hub already logs exception responses

  // A complete frame is required because plausibility is checked across all readings before any are published.
  if (start_address != 0 || registers.size() < PZEM_REGISTER_COUNT) {
    ESP_LOGW(TAG, "PZEM AC Addr 0x%02X: Ignoring incomplete response", int(this->address_));
    return;
  }

  auto pzem_get_32bit = [&](uint16_t reg) -> uint32_t {
    return (static_cast<uint32_t>(registers[reg + 1]) << 16) | registers[reg];
  };

  uint16_t raw_voltage = registers[PZEM_REGISTER_VOLTAGE];
  uint32_t raw_current = pzem_get_32bit(PZEM_REGISTER_CURRENT);
  uint32_t raw_active_power = pzem_get_32bit(PZEM_REGISTER_ACTIVE_POWER);
  uint32_t raw_active_energy = pzem_get_32bit(PZEM_REGISTER_ACTIVE_ENERGY);
  uint16_t raw_frequency = registers[PZEM_REGISTER_FREQUENCY];
  uint16_t raw_power_factor = registers[PZEM_REGISTER_POWER_FACTOR];
  uint16_t raw_alarm = registers[PZEM_REGISTER_ALARM];

  // The meter can return a CRC-valid but nonsensical frame while powering up.
  // Reject the whole frame so no sensor receives a mixture of fresh and stale data.
  if (raw_voltage > PZEM_MAX_VOLTAGE || raw_current > PZEM_MAX_CURRENT || raw_active_power > PZEM_MAX_ACTIVE_POWER ||
      raw_frequency < PZEM_MIN_FREQUENCY || raw_frequency > PZEM_MAX_FREQUENCY ||
      raw_power_factor > PZEM_MAX_POWER_FACTOR || (raw_alarm != 0 && raw_alarm != 0xFFFF)) {
    ESP_LOGW(TAG, "PZEM AC Addr 0x%02X: Ignoring implausible response", int(this->address_));
    return;
  }

  this->last_update_time_ = millis();

  float voltage = raw_voltage / 10.0f;            // max 6553.5 V
  float current = raw_current / 1000.0f;          // max 4294967.295 A
  float active_power = raw_active_power / 10.0f;  // max 429496729.5 W
  float active_energy = static_cast<float>(raw_active_energy);
  float frequency = raw_frequency / 10.0f;
  float power_factor = raw_power_factor / 100.0f;

  ESP_LOGD(TAG, "PZEM AC: Addr 0x%02X, V=%.1f V, I=%.3f A, P=%.1f W, E=%.1f Wh, F=%.1f Hz, PF=%.2f", this->address_,
           voltage, current, active_power, active_energy, frequency, power_factor);
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

void PZEMAC::update() {
  this->read_input_registers(0, PZEM_REGISTER_COUNT);

  if (this->get_update_interval() != SCHEDULER_DONT_RUN &&
      (millis() - this->last_update_time_) > this->get_update_interval() * 2) {
    ESP_LOGE(TAG, "PZEM AC Addr 0x%02X: Timeout!", int(this->address_));
    if (this->voltage_sensor_ != nullptr)
      this->voltage_sensor_->publish_state(0.0f);
    if (this->current_sensor_ != nullptr)
      this->current_sensor_->publish_state(0.0f);
    if (this->power_sensor_ != nullptr)
      this->power_sensor_->publish_state(0.0f);
    if (this->frequency_sensor_ != nullptr)
      this->frequency_sensor_->publish_state(0.0f);
    if (this->power_factor_sensor_ != nullptr)
      this->power_factor_sensor_->publish_state(0.0f);
  }
}

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
