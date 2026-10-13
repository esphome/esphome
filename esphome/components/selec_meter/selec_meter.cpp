#include "selec_meter.h"
#include "selec_meter_registers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <optional>

namespace esphome::selec_meter {

namespace helpers = modbus::helpers;

ESPHOME_LOG_TAG(TAG, "selec_meter");

static const uint8_t EM2M_REGISTER_COUNT = 34;  // 34 x 16-bit registers

// Durable refusals an optional read tolerates before it is given up on
static const uint8_t MAX_OPTIONAL_READ_FAILURES = 5;

static const LogString *read_state_name(ReadState state) {
  switch (state) {
    case ReadState::MAIN_BLOCK:
      return LOG_STR("main block");
    case ReadState::SERIAL_NUMBER:
      return LOG_STR("serial number");
    case ReadState::DG_SENSING:
      return LOG_STR("DG sensing");
    case ReadState::IDLE:
    default:
      return LOG_STR("idle");
  }
}

// One measurement: sensor, register address and unit multiplier, so both decoders run off a table
struct FloatFieldSpec {
  sensor::Sensor *sensor;
  uint16_t reg;
  float unit;
};

// FP32 at `reg` in the configured word order (MSRF high word first, LSRF low word first); nullopt when
// `reg` is not wholly inside `registers`
static std::optional<float> read_float(std::span<const uint16_t> registers, uint16_t start_address, uint16_t reg,
                                       bool word_swapped) {
  return word_swapped ? helpers::value_at<helpers::SensorValueType::FP32_R>(registers, start_address, reg)
                      : helpers::value_at<helpers::SensorValueType::FP32>(registers, start_address, reg);
}

// Publishes the field at `reg` (scaled by `unit`) unless it decodes to NaN/Inf, e.g. an unloaded
// phase's power factor; a non-finite field is skipped on its own so the others still publish.
static void publish_float(sensor::Sensor *sens, std::span<const uint16_t> registers, uint16_t reg, float unit,
                          bool word_swapped) {
  auto value = read_float(registers, 0, reg, word_swapped);
  if (!value || !std::isfinite(*value)) {
    ESP_LOGD(TAG, "Skipping non-finite value for '%s'", sens->get_name().c_str());
    return;
  }
  sens->publish_state(*value * unit);
}

ReadState SelecMeter::next_read_state_after_main_block_() {
#ifdef USE_TEXT_SENSOR
  if (this->serial_number_sensor_ != nullptr && !this->serial_number_published_ && !this->serial_number_disabled_)
    return ReadState::SERIAL_NUMBER;
#endif
#ifdef USE_BINARY_SENSOR
  if (this->dg_sensing_sensor_ != nullptr && !this->dg_sensing_disabled_)
    return ReadState::DG_SENSING;
#endif
  return ReadState::IDLE;
}

ReadState SelecMeter::next_read_state_after_(ReadState current) {
  switch (current) {
    case ReadState::MAIN_BLOCK:
      return this->next_read_state_after_main_block_();
    case ReadState::SERIAL_NUMBER:
#ifdef USE_BINARY_SENSOR
      return (this->dg_sensing_sensor_ != nullptr && !this->dg_sensing_disabled_) ? ReadState::DG_SENSING
                                                                                  : ReadState::IDLE;
#else
      return ReadState::IDLE;
#endif
    case ReadState::DG_SENSING:
    case ReadState::IDLE:
    default:
      return ReadState::IDLE;
  }
}

// Start address the read for `state` was sent with, to match a response to the pending read
static uint16_t expected_start_address(ReadState state) {
  switch (state) {
    case ReadState::SERIAL_NUMBER:
      return EM4M_SERIAL_NUMBER;
    case ReadState::DG_SENSING:
      return EM4M_DG_SENSING;
    case ReadState::MAIN_BLOCK:
    case ReadState::IDLE:
    default:
      return 0;
  }
}

void SelecMeter::on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                         modbus::ResponseStatus status) {
  this->waiting_for_response_ = false;
  ReadState current = this->read_state_;
  if (current == ReadState::IDLE) {
    ESP_LOGW(TAG, "Unexpected response while idle, dropping");
    return;
  }
  // start_address comes from the exact request this answers, so a stale reply cannot match another read
  if (start_address != expected_start_address(current)) {
    this->fail_current_read_(LOG_STR("Response does not match pending read"), /*durable=*/false);
    return;
  }
  if (status.has_value()) {
    const auto code = *status;
    ESP_LOGW(TAG, "Modbus error 0x%02X while reading %s", static_cast<uint8_t>(code),
             LOG_STR_ARG(read_state_name(current)));
    // Illegal function/address are durable; busy, acknowledge and the rest are transient
    const bool durable =
        code == modbus::ExceptionCode::ILLEGAL_FUNCTION || code == modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS;
    this->fail_current_read_(nullptr, durable);
    return;
  }
  switch (current) {
    case ReadState::MAIN_BLOCK: {
      bool ok = this->model_ == Model::EM4M ? this->decode_em4m_(registers) : this->decode_em2m_(registers);
      if (ok) {
        this->status_clear_warning();
      } else {
        this->status_set_warning();
      }
      break;
    }
    case ReadState::SERIAL_NUMBER:
#ifdef USE_TEXT_SENSOR
      this->decode_serial_number_(registers);
#endif
      break;
    case ReadState::DG_SENSING:
#ifdef USE_BINARY_SENSOR
      this->decode_dg_sensing_(registers);
#endif
      break;
    case ReadState::IDLE:
      return;
  }
  // Safe per modbus.h's callback contract: sending from inside a callback is picked up normally.
  this->start_read_(this->next_read_state_after_(current));
}

void SelecMeter::fail_current_read_(const LogString *reason, bool durable) {
  if (this->read_state_ == ReadState::IDLE) {
    ESP_LOGW(TAG, "Unexpected failure while idle, dropping");
    return;
  }
  ReadState failed_state = this->read_state_;
  if (reason != nullptr) {
    ESP_LOGW(TAG, "%s while reading %s", LOG_STR_ARG(reason), LOG_STR_ARG(read_state_name(failed_state)));
  }
  this->waiting_for_response_ = false;
  // Only the main block drives component health; an unsupported optional read is not a fault
  if (failed_state == ReadState::MAIN_BLOCK)
    this->status_set_warning();
#ifdef USE_TEXT_SENSOR
  if (failed_state == ReadState::SERIAL_NUMBER && durable)
    this->note_serial_number_failure_();
#endif
#ifdef USE_BINARY_SENSOR
  if (failed_state == ReadState::DG_SENSING && durable)
    this->note_dg_sensing_failure_();
#endif
  this->start_read_(this->next_read_state_after_(failed_state));
}

bool SelecMeter::on_no_response(std::span<const uint8_t> /*request_pdu*/) {
  this->fail_current_read_(LOG_STR("No Modbus response"));
  return false;
}

void SelecMeter::on_not_sent(std::span<const uint8_t> /*request_pdu*/) {
  this->fail_current_read_(LOG_STR("Modbus request not sent"));
}

#ifdef USE_TEXT_SENSOR
void SelecMeter::note_serial_number_failure_() {
  if (++this->serial_number_failures_ >= MAX_OPTIONAL_READ_FAILURES) {
    ESP_LOGW(TAG, "Serial number read failed %u times in a row, giving up", this->serial_number_failures_);
    this->serial_number_disabled_ = true;
  }
}

void SelecMeter::decode_serial_number_(std::span<const uint16_t> registers) {
  if (this->serial_number_sensor_ == nullptr)
    return;
  auto serial =
      this->word_swap_
          ? helpers::value_at<helpers::SensorValueType::U_DWORD_R>(registers, EM4M_SERIAL_NUMBER, EM4M_SERIAL_NUMBER)
          : helpers::value_at<helpers::SensorValueType::U_DWORD>(registers, EM4M_SERIAL_NUMBER, EM4M_SERIAL_NUMBER);
  if (!serial) {
    ESP_LOGW(TAG, "Unexpected response size for serial number");
    this->note_serial_number_failure_();
    return;
  }
  char buf[9];
  snprintf(buf, sizeof(buf), "%08" PRIX32, *serial);
  this->serial_number_sensor_->publish_state(buf);
  this->serial_number_published_ = true;
  this->serial_number_failures_ = 0;
}
#endif

#ifdef USE_BINARY_SENSOR
void SelecMeter::note_dg_sensing_failure_() {
  if (++this->dg_sensing_failures_ >= MAX_OPTIONAL_READ_FAILURES) {
    ESP_LOGW(TAG, "DG sensing read failed %u times in a row, giving up", this->dg_sensing_failures_);
    this->dg_sensing_disabled_ = true;
  }
}

void SelecMeter::decode_dg_sensing_(std::span<const uint16_t> registers) {
  if (this->dg_sensing_sensor_ == nullptr)
    return;
  auto value = read_float(registers, EM4M_DG_SENSING, EM4M_DG_SENSING, this->word_swap_);
  if (!value) {
    ESP_LOGW(TAG, "Unexpected response size for DG sensing");
    this->note_dg_sensing_failure_();
    return;
  }
  if (!std::isfinite(*value)) {
    ESP_LOGW(TAG, "Non-finite DG sensing value, ignoring");
    this->note_dg_sensing_failure_();
    return;
  }
  this->dg_sensing_sensor_->publish_state(*value != 0);
  this->dg_sensing_failures_ = 0;
}
#endif

bool SelecMeter::decode_em2m_(std::span<const uint16_t> registers) {
  const FloatFieldSpec fields[] = {
      {this->total_active_energy_sensor_, SELEC_TOTAL_ACTIVE_ENERGY, NO_DEC_UNIT},
      {this->import_active_energy_sensor_, SELEC_IMPORT_ACTIVE_ENERGY, NO_DEC_UNIT},
      {this->export_active_energy_sensor_, SELEC_EXPORT_ACTIVE_ENERGY, NO_DEC_UNIT},
      {this->total_reactive_energy_sensor_, SELEC_TOTAL_REACTIVE_ENERGY, NO_DEC_UNIT},
      {this->import_reactive_energy_sensor_, SELEC_IMPORT_REACTIVE_ENERGY, NO_DEC_UNIT},
      {this->export_reactive_energy_sensor_, SELEC_EXPORT_REACTIVE_ENERGY, NO_DEC_UNIT},
      {this->apparent_energy_sensor_, SELEC_APPARENT_ENERGY, NO_DEC_UNIT},
      {this->active_power_sensor_, SELEC_ACTIVE_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->reactive_power_sensor_, SELEC_REACTIVE_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->apparent_power_sensor_, SELEC_APPARENT_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->voltage_sensor_, SELEC_VOLTAGE, NO_DEC_UNIT},
      {this->current_sensor_, SELEC_CURRENT, NO_DEC_UNIT},
      {this->power_factor_sensor_, SELEC_POWER_FACTOR, NO_DEC_UNIT},
      {this->frequency_sensor_, SELEC_FREQUENCY, NO_DEC_UNIT},
      {this->maximum_demand_active_power_sensor_, SELEC_MAXIMUM_DEMAND_ACTIVE_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->maximum_demand_reactive_power_sensor_, SELEC_MAXIMUM_DEMAND_REACTIVE_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->maximum_demand_apparent_power_sensor_, SELEC_MAXIMUM_DEMAND_APPARENT_POWER, MULTIPLY_THOUSAND_UNIT},
  };

  if (registers.size() < EM2M_REGISTER_COUNT) {
    ESP_LOGW(TAG, "Short response: %u registers", static_cast<unsigned>(registers.size()));
    return false;
  }
  for (const auto &f : fields) {
    if (f.sensor != nullptr)
      publish_float(f.sensor, registers, f.reg, f.unit, this->word_swap_);
  }
  return true;
}

bool SelecMeter::decode_em4m_(std::span<const uint16_t> registers) {
  const FloatFieldSpec fields[] = {
      // Common quantities, shared sensor keys with EM2M
      {this->voltage_sensor_, EM4M_VOLTAGE, NO_DEC_UNIT},
      {this->current_sensor_, EM4M_CURRENT, NO_DEC_UNIT},
      {this->active_power_sensor_, EM4M_ACTIVE_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->reactive_power_sensor_, EM4M_REACTIVE_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->apparent_power_sensor_, EM4M_APPARENT_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->power_factor_sensor_, EM4M_POWER_FACTOR, NO_DEC_UNIT},
      {this->frequency_sensor_, EM4M_FREQUENCY, NO_DEC_UNIT},
      {this->maximum_demand_active_power_sensor_, EM4M_MAXIMUM_DEMAND_ACTIVE_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->maximum_demand_reactive_power_sensor_, EM4M_MAXIMUM_DEMAND_REACTIVE_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->maximum_demand_apparent_power_sensor_, EM4M_MAXIMUM_DEMAND_APPARENT_POWER, MULTIPLY_THOUSAND_UNIT},
      {this->import_active_energy_sensor_, EM4M_IMPORT_ACTIVE_ENERGY, NO_DEC_UNIT},
      {this->export_active_energy_sensor_, EM4M_EXPORT_ACTIVE_ENERGY, NO_DEC_UNIT},
      {this->import_reactive_energy_sensor_, EM4M_IMPORT_REACTIVE_ENERGY, NO_DEC_UNIT},
      {this->export_reactive_energy_sensor_, EM4M_EXPORT_REACTIVE_ENERGY, NO_DEC_UNIT},
      // Line-to-line and net (mains/DG) quantities have no per-phase equivalent
      {this->voltage_l12_sensor_, EM4M_VOLTAGE_L12, NO_DEC_UNIT},
      {this->voltage_l23_sensor_, EM4M_VOLTAGE_L23, NO_DEC_UNIT},
      {this->voltage_l31_sensor_, EM4M_VOLTAGE_L31, NO_DEC_UNIT},
      {this->average_voltage_ll_sensor_, EM4M_AVERAGE_VOLTAGE_LL, NO_DEC_UNIT},
      {this->net_active_energy_mains_sensor_, EM4M_NET_ACTIVE_ENERGY_MAINS, NO_DEC_UNIT},
      {this->net_reactive_energy_mains_sensor_, EM4M_NET_REACTIVE_ENERGY_MAINS, NO_DEC_UNIT},
      {this->net_apparent_energy_mains_sensor_, EM4M_NET_APPARENT_ENERGY_MAINS, NO_DEC_UNIT},
      {this->net_active_energy_dg_sensor_, EM4M_NET_ACTIVE_ENERGY_DG, NO_DEC_UNIT},
      {this->net_reactive_energy_dg_sensor_, EM4M_NET_REACTIVE_ENERGY_DG, NO_DEC_UNIT},
      {this->net_apparent_energy_dg_sensor_, EM4M_NET_APPARENT_ENERGY_DG, NO_DEC_UNIT},
  };

  // reg_l1 is the L1 register; L2/L3 follow EM4M_PHASE_STRIDE apart
  struct PhaseField {
    sensor::Sensor *Phase::*sensor_member;
    uint16_t reg_l1;
    float unit;
  };
  const PhaseField phase_fields[] = {
      {&Phase::voltage_sensor_, EM4M_VOLTAGE_L1, NO_DEC_UNIT},
      {&Phase::current_sensor_, EM4M_CURRENT_L1, NO_DEC_UNIT},
      {&Phase::active_power_sensor_, EM4M_ACTIVE_POWER_L1, MULTIPLY_THOUSAND_UNIT},
      {&Phase::reactive_power_sensor_, EM4M_REACTIVE_POWER_L1, MULTIPLY_THOUSAND_UNIT},
      {&Phase::apparent_power_sensor_, EM4M_APPARENT_POWER_L1, MULTIPLY_THOUSAND_UNIT},
      {&Phase::power_factor_sensor_, EM4M_POWER_FACTOR_L1, NO_DEC_UNIT},
      {&Phase::import_active_energy_sensor_, EM4M_IMPORT_ACTIVE_ENERGY_L1, NO_DEC_UNIT},
      {&Phase::export_active_energy_sensor_, EM4M_EXPORT_ACTIVE_ENERGY_L1, NO_DEC_UNIT},
      {&Phase::import_reactive_energy_sensor_, EM4M_IMPORT_REACTIVE_ENERGY_L1, NO_DEC_UNIT},
      {&Phase::export_reactive_energy_sensor_, EM4M_EXPORT_REACTIVE_ENERGY_L1, NO_DEC_UNIT},
      {&Phase::apparent_energy_sensor_, EM4M_APPARENT_ENERGY_L1, NO_DEC_UNIT},
  };

  if (registers.size() < EM4M_REGISTER_COUNT) {
    ESP_LOGW(TAG, "Short response: %u registers", static_cast<unsigned>(registers.size()));
    return false;
  }
  for (const auto &f : fields) {
    if (f.sensor != nullptr)
      publish_float(f.sensor, registers, f.reg, f.unit, this->word_swap_);
  }
  for (uint8_t phase = 0; phase < 3; phase++) {
    for (const auto &f : phase_fields) {
      sensor::Sensor *sens = this->phases_[phase].*f.sensor_member;
      if (sens != nullptr)
        publish_float(sens, registers, f.reg_l1 + phase * EM4M_PHASE_STRIDE, f.unit, this->word_swap_);
    }
  }
  return true;
}

void SelecMeter::update() {
  if (this->waiting_for_response_ || this->read_state_ != ReadState::IDLE) {
    ESP_LOGD(TAG, "Skipping update: previous read cycle (%s) still in progress",
             LOG_STR_ARG(read_state_name(this->read_state_)));
    return;
  }
  this->start_read_(ReadState::MAIN_BLOCK);
}

void SelecMeter::start_read_(ReadState state) {
  this->read_state_ = state;
  if (state == ReadState::IDLE)
    return;

  this->waiting_for_response_ = true;

  bool sent = false;
  switch (state) {
    case ReadState::MAIN_BLOCK: {
      uint8_t register_count = this->model_ == Model::EM4M ? EM4M_REGISTER_COUNT : EM2M_REGISTER_COUNT;
      sent = this->read_input_registers(0, register_count);
      break;
    }
    case ReadState::SERIAL_NUMBER:
      sent = this->read_input_registers(EM4M_SERIAL_NUMBER, 2);
      break;
    case ReadState::DG_SENSING:
      sent = this->read_input_registers(EM4M_DG_SENSING, 2);
      break;
    case ReadState::IDLE:
      return;
  }

  // A refused send gets no callback, so unwind here like the async failure paths
  if (!sent)
    this->fail_current_read_(LOG_STR("Modbus request refused"));
}

void SelecMeter::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SELEC Meter:\n"
                "  Model: %s\n"
                "  Byte Order: %s\n"
                "  Address: 0x%02X",
                this->model_ == Model::EM4M ? LOG_STR_LITERAL("EM4M") : LOG_STR_LITERAL("EM2M"),
                this->word_swap_ ? LOG_STR_LITERAL("little_endian (LSRF)") : LOG_STR_LITERAL("big_endian (MSRF)"),
                this->address_);
  LOG_SENSOR("  ", "Total Active Energy", this->total_active_energy_sensor_);
  LOG_SENSOR("  ", "Import Active Energy", this->import_active_energy_sensor_);
  LOG_SENSOR("  ", "Export Active Energy", this->export_active_energy_sensor_);
  LOG_SENSOR("  ", "Total Reactive Energy", this->total_reactive_energy_sensor_);
  LOG_SENSOR("  ", "Import Reactive Energy", this->import_reactive_energy_sensor_);
  LOG_SENSOR("  ", "Export Reactive Energy", this->export_reactive_energy_sensor_);
  LOG_SENSOR("  ", "Apparent Energy", this->apparent_energy_sensor_);
  LOG_SENSOR("  ", "Active Power", this->active_power_sensor_);
  LOG_SENSOR("  ", "Reactive Power", this->reactive_power_sensor_);
  LOG_SENSOR("  ", "Apparent Power", this->apparent_power_sensor_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power Factor", this->power_factor_sensor_);
  LOG_SENSOR("  ", "Frequency", this->frequency_sensor_);
  LOG_SENSOR("  ", "Maximum Demand Active Power", this->maximum_demand_active_power_sensor_);
  LOG_SENSOR("  ", "Maximum Demand Reactive Power", this->maximum_demand_reactive_power_sensor_);
  LOG_SENSOR("  ", "Maximum Demand Apparent Power", this->maximum_demand_apparent_power_sensor_);
  LOG_SENSOR("  ", "Voltage L1-L2", this->voltage_l12_sensor_);
  LOG_SENSOR("  ", "Voltage L2-L3", this->voltage_l23_sensor_);
  LOG_SENSOR("  ", "Voltage L3-L1", this->voltage_l31_sensor_);
  LOG_SENSOR("  ", "Average Voltage LL", this->average_voltage_ll_sensor_);
  for (uint8_t phase = 0; phase < 3; phase++) {
    const auto &p = this->phases_[phase];
    ESP_LOGCONFIG(TAG, "  Phase %c", 'A' + phase);
    LOG_SENSOR("    ", "Voltage", p.voltage_sensor_);
    LOG_SENSOR("    ", "Current", p.current_sensor_);
    LOG_SENSOR("    ", "Active Power", p.active_power_sensor_);
    LOG_SENSOR("    ", "Reactive Power", p.reactive_power_sensor_);
    LOG_SENSOR("    ", "Apparent Power", p.apparent_power_sensor_);
    LOG_SENSOR("    ", "Power Factor", p.power_factor_sensor_);
    LOG_SENSOR("    ", "Import Active Energy", p.import_active_energy_sensor_);
    LOG_SENSOR("    ", "Export Active Energy", p.export_active_energy_sensor_);
    LOG_SENSOR("    ", "Import Reactive Energy", p.import_reactive_energy_sensor_);
    LOG_SENSOR("    ", "Export Reactive Energy", p.export_reactive_energy_sensor_);
    LOG_SENSOR("    ", "Apparent Energy", p.apparent_energy_sensor_);
  }
  LOG_SENSOR("  ", "Net Active Energy (Mains)", this->net_active_energy_mains_sensor_);
  LOG_SENSOR("  ", "Net Reactive Energy (Mains)", this->net_reactive_energy_mains_sensor_);
  LOG_SENSOR("  ", "Net Apparent Energy (Mains)", this->net_apparent_energy_mains_sensor_);
  LOG_SENSOR("  ", "Net Active Energy (DG)", this->net_active_energy_dg_sensor_);
  LOG_SENSOR("  ", "Net Reactive Energy (DG)", this->net_reactive_energy_dg_sensor_);
  LOG_SENSOR("  ", "Net Apparent Energy (DG)", this->net_apparent_energy_dg_sensor_);
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Serial Number", this->serial_number_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "DG Sensing", this->dg_sensing_sensor_);
#endif
}

}  // namespace esphome::selec_meter
