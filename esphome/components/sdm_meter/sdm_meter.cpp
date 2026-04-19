#include "sdm_meter.h"
#include "sdm_meter_registers.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sdm_meter {

static const char *const TAG = "sdm_meter";

static const uint8_t MODBUS_CMD_READ_IN_REGISTERS = 0x04;
static const uint16_t MODBUS_REGISTER_COUNT = 80;  // 40 x FP32 registers (0x00..0x4F)

void SDMMeter::loop() {
  // If update() was unable to send we retry until we can send.
  if (!this->waiting_to_update_)
    return;
  this->update();
}

void SDMMeter::update() {
  // The bus might be slow, or there might be other devices, or other components might be talking to our device.
  if (!this->ready_for_immediate_send()) {
    this->waiting_to_update_ = true;
    return;
  }
  this->waiting_to_update_ = false;

  if (this->polling_) {
    ESP_LOGW(TAG, "Previous poll incomplete (stuck at chunk 0x%04X) - restarting", this->next_chunk_start_);
  }
  this->polling_ = true;
  this->next_chunk_start_ = 0;
  this->chunk_buffer_.assign(MODBUS_REGISTER_COUNT * 2, 0);
  const uint16_t first_count = (this->chunk_size_ < MODBUS_REGISTER_COUNT) ? this->chunk_size_ : MODBUS_REGISTER_COUNT;
  this->send(MODBUS_CMD_READ_IN_REGISTERS, 0, first_count);
}

void SDMMeter::on_modbus_data(const std::vector<uint8_t> &data) {
  if (!this->polling_) {
    ESP_LOGW(TAG, "Received modbus data while not polling - ignoring");
    return;
  }

  const uint16_t pending_count = ((MODBUS_REGISTER_COUNT - this->next_chunk_start_) < this->chunk_size_)
                                     ? (MODBUS_REGISTER_COUNT - this->next_chunk_start_)
                                     : this->chunk_size_;
  const size_t expected = static_cast<size_t>(pending_count) * 2;
  if (data.size() != expected) {
    ESP_LOGW(TAG, "Chunk size mismatch at register 0x%04X: got %u expected %u - aborting this poll",
             this->next_chunk_start_, static_cast<unsigned>(data.size()), static_cast<unsigned>(expected));
    this->polling_ = false;
    return;
  }

  const size_t offset = static_cast<size_t>(this->next_chunk_start_) * 2;
  std::copy(data.begin(), data.end(), this->chunk_buffer_.begin() + offset);

  this->next_chunk_start_ += this->chunk_size_;

  // Compute size of next chunk, clamped so we don't over-read past MODBUS_REGISTER_COUNT.
  const uint16_t remaining =
      (this->next_chunk_start_ < MODBUS_REGISTER_COUNT) ? (MODBUS_REGISTER_COUNT - this->next_chunk_start_) : 0;
  const uint16_t this_count = (remaining < this->chunk_size_) ? remaining : this->chunk_size_;

  if (this_count > 0) {
    this->send(MODBUS_CMD_READ_IN_REGISTERS, this->next_chunk_start_, this_count);
    return;
  }

  this->polling_ = false;
  this->publish_all_(this->chunk_buffer_);
}

void SDMMeter::publish_all_(const std::vector<uint8_t> &data) {
  auto sdm_meter_get_float = [&](size_t i) -> float {
    uint32_t temp = encode_uint32(data[i], data[i + 1], data[i + 2], data[i + 3]);
    float f;
    memcpy(&f, &temp, sizeof(f));
    return f;
  };

  for (uint8_t i = 0; i < 3; i++) {
    auto phase = this->phases_[i];
    if (!phase.setup)
      continue;

    float voltage = sdm_meter_get_float(SDM_PHASE_1_VOLTAGE * 2 + (i * 4));
    float current = sdm_meter_get_float(SDM_PHASE_1_CURRENT * 2 + (i * 4));
    float active_power = sdm_meter_get_float(SDM_PHASE_1_ACTIVE_POWER * 2 + (i * 4));
    float apparent_power = sdm_meter_get_float(SDM_PHASE_1_APPARENT_POWER * 2 + (i * 4));
    float reactive_power = sdm_meter_get_float(SDM_PHASE_1_REACTIVE_POWER * 2 + (i * 4));
    float power_factor = sdm_meter_get_float(SDM_PHASE_1_POWER_FACTOR * 2 + (i * 4));
    float phase_angle = sdm_meter_get_float(SDM_PHASE_1_ANGLE * 2 + (i * 4));

    ESP_LOGD(
        TAG,
        "SDMMeter Phase %c: V=%.3f V, I=%.3f A, Active P=%.3f W, Apparent P=%.3f VA, Reactive P=%.3f var, PF=%.3f, "
        "PA=%.3f °",
        i + 'A', voltage, current, active_power, apparent_power, reactive_power, power_factor, phase_angle);
    if (phase.voltage_sensor_ != nullptr)
      phase.voltage_sensor_->publish_state(voltage);
    if (phase.current_sensor_ != nullptr)
      phase.current_sensor_->publish_state(current);
    if (phase.active_power_sensor_ != nullptr)
      phase.active_power_sensor_->publish_state(active_power);
    if (phase.apparent_power_sensor_ != nullptr)
      phase.apparent_power_sensor_->publish_state(apparent_power);
    if (phase.reactive_power_sensor_ != nullptr)
      phase.reactive_power_sensor_->publish_state(reactive_power);
    if (phase.power_factor_sensor_ != nullptr)
      phase.power_factor_sensor_->publish_state(power_factor);
    if (phase.phase_angle_sensor_ != nullptr)
      phase.phase_angle_sensor_->publish_state(phase_angle);
  }

  float total_power = sdm_meter_get_float(SDM_TOTAL_SYSTEM_POWER * 2);
  float frequency = sdm_meter_get_float(SDM_FREQUENCY * 2);
  float import_active_energy = sdm_meter_get_float(SDM_IMPORT_ACTIVE_ENERGY * 2);
  float export_active_energy = sdm_meter_get_float(SDM_EXPORT_ACTIVE_ENERGY * 2);
  float import_reactive_energy = sdm_meter_get_float(SDM_IMPORT_REACTIVE_ENERGY * 2);
  float export_reactive_energy = sdm_meter_get_float(SDM_EXPORT_REACTIVE_ENERGY * 2);

  ESP_LOGD(TAG, "SDMMeter: F=%.3f Hz, Im.A.E=%.3f Wh, Ex.A.E=%.3f Wh, Im.R.E=%.3f VARh, Ex.R.E=%.3f VARh, T.P=%.3f W",
           frequency, import_active_energy, export_active_energy, import_reactive_energy, export_reactive_energy,
           total_power);

  if (this->total_power_sensor_ != nullptr)
    this->total_power_sensor_->publish_state(total_power);
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

void SDMMeter::dump_config() {
  const uint16_t chunks = (MODBUS_REGISTER_COUNT + this->chunk_size_ - 1) / this->chunk_size_;
  ESP_LOGCONFIG(TAG,
                "SDM Meter:\n"
                "  Address: 0x%02X\n"
                "  Chunk size: %u registers (%u request%s per poll)",
                this->address_, this->chunk_size_, chunks, chunks == 1 ? "" : "s");
  for (uint8_t i = 0; i < 3; i++) {
    auto phase = this->phases_[i];
    if (!phase.setup)
      continue;
    ESP_LOGCONFIG(TAG, "  Phase %c", i + 'A');
    LOG_SENSOR("    ", "Voltage", phase.voltage_sensor_);
    LOG_SENSOR("    ", "Current", phase.current_sensor_);
    LOG_SENSOR("    ", "Active Power", phase.active_power_sensor_);
    LOG_SENSOR("    ", "Apparent Power", phase.apparent_power_sensor_);
    LOG_SENSOR("    ", "Reactive Power", phase.reactive_power_sensor_);
    LOG_SENSOR("    ", "Power Factor", phase.power_factor_sensor_);
    LOG_SENSOR("    ", "Phase Angle", phase.phase_angle_sensor_);
  }
  LOG_SENSOR("  ", "Total Power", this->total_power_sensor_);
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Import Active Energy", this->import_active_energy_sensor_);
  LOG_SENSOR("  ", "Export Active Energy", this->export_active_energy_sensor_);
  LOG_SENSOR("  ", "Import Reactive Energy", this->import_reactive_energy_sensor_);
  LOG_SENSOR("  ", "Export Reactive Energy", this->export_reactive_energy_sensor_);
}

}  // namespace sdm_meter
}  // namespace esphome
