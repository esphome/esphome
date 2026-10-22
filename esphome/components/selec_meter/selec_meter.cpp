#include "selec_meter.h"
#include "selec_meter_registers.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>

namespace esphome::selec_meter {

namespace helpers = modbus::helpers;

ESPHOME_LOG_TAG(TAG, "selec_meter");

static constexpr uint8_t EM2M_REGISTER_COUNT = 34;  // 34 x 16-bit registers

// Durable refusals an optional read tolerates before it is given up on
static constexpr uint8_t MAX_OPTIONAL_READ_FAILURES = 5;

static const LogString *read_state_name(ReadState state) {
  switch (state) {
    case ReadState::READ_STATE_MAIN_BLOCK:
      return LOG_STR("main block");
    case ReadState::READ_STATE_SERIAL_NUMBER:
      return LOG_STR("serial number");
    case ReadState::READ_STATE_DG_SENSING:
      return LOG_STR("DG sensing");
    case ReadState::READ_STATE_IDLE:
    default:
      return LOG_STR("idle");
  }
}

// FP32 at `reg` in the configured word order (MSRF high word first, LSRF low word first); NaN when
// `reg` is not wholly inside `registers`
static float read_float(std::span<const uint16_t> registers, uint16_t start_address, uint16_t reg, bool word_swapped) {
  auto value = word_swapped ? helpers::value_at<helpers::SensorValueType::FP32_R>(registers, start_address, reg)
                            : helpers::value_at<helpers::SensorValueType::FP32>(registers, start_address, reg);
  return value.value_or(NAN);
}

// Publishes the field at `reg` (x1000 when `kilo`) unless it decodes to NaN/Inf, e.g. an unloaded
// phase's power factor; a non-finite field is skipped on its own so the others still publish.
static void publish_float(sensor::Sensor *sens, std::span<const uint16_t> registers, uint16_t reg, bool kilo,
                          bool word_swapped) {
  float value = read_float(registers, 0, reg, word_swapped);
  if (!std::isfinite(value)) {
    ESP_LOGD(TAG, "Skipping non-finite value for '%s'", LOG_STR_ARG(sens->get_log_name()));
    return;
  }
  sens->publish_state(kilo ? value * 1000.0f : value);
}

#if defined(USE_SELEC_METER_EM4M) && (defined(USE_TEXT_SENSOR) || defined(USE_BINARY_SENSOR))
// Counts one failure of an optional read; callers only run while `failures` is below the limit
static void note_optional_read_failure(uint8_t &failures, const LogString *name) {
  if (++failures == MAX_OPTIONAL_READ_FAILURES) {
    ESP_LOGW(TAG, "Reading %s failed %u times in a row, giving up", LOG_STR_ARG(name), failures);
  }
}
#endif

SelecMeter::ReadRange SelecMeter::read_range_(ReadState state) const {
  switch (state) {
    case ReadState::READ_STATE_MAIN_BLOCK:
#ifdef USE_SELEC_METER_EM4M
      if (this->model_ == Model::MODEL_EM4M)
        return {0, EM4M_REGISTER_COUNT};
#endif
      return {0, EM2M_REGISTER_COUNT};
    case ReadState::READ_STATE_SERIAL_NUMBER:
      return {EM4M_SERIAL_NUMBER, 2};
    case ReadState::READ_STATE_DG_SENSING:
      return {EM4M_DG_SENSING, 2};
    case ReadState::READ_STATE_IDLE:
    default:
      return {0, 0};
  }
}

bool SelecMeter::wanted_(ReadState state) const {
  switch (state) {
    case ReadState::READ_STATE_MAIN_BLOCK:
      return true;
#if defined(USE_SELEC_METER_EM4M) && defined(USE_TEXT_SENSOR)
    case ReadState::READ_STATE_SERIAL_NUMBER:
      return this->serial_number_sensor_ != nullptr && this->serial_number_failures_ < MAX_OPTIONAL_READ_FAILURES;
#endif
#if defined(USE_SELEC_METER_EM4M) && defined(USE_BINARY_SENSOR)
    case ReadState::READ_STATE_DG_SENSING:
      return this->dg_sensing_sensor_ != nullptr && this->dg_sensing_failures_ < MAX_OPTIONAL_READ_FAILURES;
#endif
    default:
      return false;
  }
}

ReadState SelecMeter::next_read_state_after_(ReadState current) const {
  for (auto next = static_cast<uint8_t>(current) + 1; next < static_cast<uint8_t>(ReadState::READ_STATE_IDLE); next++) {
    if (this->wanted_(static_cast<ReadState>(next)))
      return static_cast<ReadState>(next);
  }
  return ReadState::READ_STATE_IDLE;
}

void SelecMeter::on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                         modbus::ResponseStatus status) {
  ReadState current = this->read_state_;
  if (current == ReadState::READ_STATE_IDLE) {
    ESP_LOGW(TAG, "Unexpected response while idle, dropping");
    return;
  }
  // start_address comes from the exact request this answers, so a stale reply cannot match another read
  if (start_address != this->read_range_(current).start) {
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
    case ReadState::READ_STATE_MAIN_BLOCK: {
#ifdef USE_SELEC_METER_EM4M
      bool ok = this->model_ == Model::MODEL_EM4M ? this->decode_em4m_(registers) : this->decode_em2m_(registers);
#else
      bool ok = this->decode_em2m_(registers);
#endif
      this->status_comm_result(ok);
      break;
    }
#if defined(USE_SELEC_METER_EM4M) && defined(USE_TEXT_SENSOR)
    case ReadState::READ_STATE_SERIAL_NUMBER:
      this->decode_serial_number_(registers);
      break;
#endif
#if defined(USE_SELEC_METER_EM4M) && defined(USE_BINARY_SENSOR)
    case ReadState::READ_STATE_DG_SENSING:
      this->decode_dg_sensing_(registers);
      break;
#endif
    default:
      break;
  }
  // Safe per modbus.h's callback contract: sending from inside a callback is picked up normally.
  this->start_read_(this->next_read_state_after_(current));
}

void SelecMeter::fail_current_read_(const LogString *reason, bool durable) {
  if (this->read_state_ == ReadState::READ_STATE_IDLE) {
    ESP_LOGW(TAG, "Unexpected failure while idle, dropping");
    return;
  }
  ReadState failed_state = this->read_state_;
  if (reason != nullptr) {
    ESP_LOGW(TAG, "%s while reading %s", LOG_STR_ARG(reason), LOG_STR_ARG(read_state_name(failed_state)));
  }
  // Only the main block drives component health; an unsupported optional read is not a fault
  if (failed_state == ReadState::READ_STATE_MAIN_BLOCK)
    this->status_set_warning();
#if defined(USE_SELEC_METER_EM4M) && defined(USE_TEXT_SENSOR)
  if (failed_state == ReadState::READ_STATE_SERIAL_NUMBER && durable)
    note_optional_read_failure(this->serial_number_failures_, read_state_name(failed_state));
#endif
#if defined(USE_SELEC_METER_EM4M) && defined(USE_BINARY_SENSOR)
  if (failed_state == ReadState::READ_STATE_DG_SENSING && durable)
    note_optional_read_failure(this->dg_sensing_failures_, read_state_name(failed_state));
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

#if defined(USE_SELEC_METER_EM4M) && defined(USE_TEXT_SENSOR)
void SelecMeter::decode_serial_number_(std::span<const uint16_t> registers) {
  auto serial =
      this->word_swap_
          ? helpers::value_at<helpers::SensorValueType::U_DWORD_R>(registers, EM4M_SERIAL_NUMBER, EM4M_SERIAL_NUMBER)
          : helpers::value_at<helpers::SensorValueType::U_DWORD>(registers, EM4M_SERIAL_NUMBER, EM4M_SERIAL_NUMBER);
  if (!serial) {
    ESP_LOGW(TAG, "Unexpected response size for serial number");
    note_optional_read_failure(this->serial_number_failures_, read_state_name(ReadState::READ_STATE_SERIAL_NUMBER));
    return;
  }
  char buf[9];
  snprintf(buf, sizeof(buf), "%08" PRIX32, *serial);
  this->serial_number_sensor_->publish_state(buf);
  // The serial number never changes, so stop reading it
  this->serial_number_failures_ = MAX_OPTIONAL_READ_FAILURES;
}
#endif

#if defined(USE_SELEC_METER_EM4M) && defined(USE_BINARY_SENSOR)
void SelecMeter::decode_dg_sensing_(std::span<const uint16_t> registers) {
  float value = read_float(registers, EM4M_DG_SENSING, EM4M_DG_SENSING, this->word_swap_);
  if (!std::isfinite(value)) {
    ESP_LOGW(TAG, "Short or non-finite DG sensing response, ignoring");
    note_optional_read_failure(this->dg_sensing_failures_, read_state_name(ReadState::READ_STATE_DG_SENSING));
    return;
  }
  this->dg_sensing_sensor_->publish_state(value != 0);
  this->dg_sensing_failures_ = 0;
}
#endif

bool SelecMeter::publish_fields_(std::span<const uint16_t> registers, std::span<const FloatField> fields,
                                 uint8_t min_count) {
  if (registers.size() < min_count) {
    ESP_LOGW(TAG, "Short response: %u registers", static_cast<unsigned>(registers.size()));
    return false;
  }
  for (const auto &entry : fields) {
    FloatField field;
    progmem_memcpy(&field, &entry, sizeof(field));
    sensor::Sensor *sens = this->*field.sensor;
    if (sens != nullptr)
      publish_float(sens, registers, field.reg, field.kilo, this->word_swap_);
  }
  return true;
}

bool SelecMeter::decode_em2m_(std::span<const uint16_t> registers) {
  static constexpr FloatField EM2M_FIELDS[] PROGMEM = {
      {&SelecMeter::total_active_energy_sensor_, SELEC_TOTAL_ACTIVE_ENERGY, false},
      {&SelecMeter::import_active_energy_sensor_, SELEC_IMPORT_ACTIVE_ENERGY, false},
      {&SelecMeter::export_active_energy_sensor_, SELEC_EXPORT_ACTIVE_ENERGY, false},
      {&SelecMeter::total_reactive_energy_sensor_, SELEC_TOTAL_REACTIVE_ENERGY, false},
      {&SelecMeter::import_reactive_energy_sensor_, SELEC_IMPORT_REACTIVE_ENERGY, false},
      {&SelecMeter::export_reactive_energy_sensor_, SELEC_EXPORT_REACTIVE_ENERGY, false},
      {&SelecMeter::apparent_energy_sensor_, SELEC_APPARENT_ENERGY, false},
      {&SelecMeter::active_power_sensor_, SELEC_ACTIVE_POWER, true},
      {&SelecMeter::reactive_power_sensor_, SELEC_REACTIVE_POWER, true},
      {&SelecMeter::apparent_power_sensor_, SELEC_APPARENT_POWER, true},
      {&SelecMeter::voltage_sensor_, SELEC_VOLTAGE, false},
      {&SelecMeter::current_sensor_, SELEC_CURRENT, false},
      {&SelecMeter::power_factor_sensor_, SELEC_POWER_FACTOR, false},
      {&SelecMeter::frequency_sensor_, SELEC_FREQUENCY, false},
      {&SelecMeter::maximum_demand_active_power_sensor_, SELEC_MAXIMUM_DEMAND_ACTIVE_POWER, true},
      {&SelecMeter::maximum_demand_reactive_power_sensor_, SELEC_MAXIMUM_DEMAND_REACTIVE_POWER, true},
      {&SelecMeter::maximum_demand_apparent_power_sensor_, SELEC_MAXIMUM_DEMAND_APPARENT_POWER, true},
  };
  return this->publish_fields_(registers, EM2M_FIELDS, EM2M_REGISTER_COUNT);
}

#ifdef USE_SELEC_METER_EM4M
bool SelecMeter::decode_em4m_(std::span<const uint16_t> registers) {
  static constexpr FloatField EM4M_FIELDS[] PROGMEM = {
      // Common quantities, shared sensor keys with EM2M
      {&SelecMeter::voltage_sensor_, EM4M_VOLTAGE, false},
      {&SelecMeter::current_sensor_, EM4M_CURRENT, false},
      {&SelecMeter::active_power_sensor_, EM4M_ACTIVE_POWER, true},
      {&SelecMeter::reactive_power_sensor_, EM4M_REACTIVE_POWER, true},
      {&SelecMeter::apparent_power_sensor_, EM4M_APPARENT_POWER, true},
      {&SelecMeter::power_factor_sensor_, EM4M_POWER_FACTOR, false},
      {&SelecMeter::frequency_sensor_, EM4M_FREQUENCY, false},
      {&SelecMeter::maximum_demand_active_power_sensor_, EM4M_MAXIMUM_DEMAND_ACTIVE_POWER, true},
      {&SelecMeter::maximum_demand_reactive_power_sensor_, EM4M_MAXIMUM_DEMAND_REACTIVE_POWER, true},
      {&SelecMeter::maximum_demand_apparent_power_sensor_, EM4M_MAXIMUM_DEMAND_APPARENT_POWER, true},
      {&SelecMeter::import_active_energy_sensor_, EM4M_IMPORT_ACTIVE_ENERGY, false},
      {&SelecMeter::export_active_energy_sensor_, EM4M_EXPORT_ACTIVE_ENERGY, false},
      {&SelecMeter::import_reactive_energy_sensor_, EM4M_IMPORT_REACTIVE_ENERGY, false},
      {&SelecMeter::export_reactive_energy_sensor_, EM4M_EXPORT_REACTIVE_ENERGY, false},
      // Line-to-line and net (mains/DG) quantities have no per-phase equivalent
      {&SelecMeter::voltage_l12_sensor_, EM4M_VOLTAGE_L12, false},
      {&SelecMeter::voltage_l23_sensor_, EM4M_VOLTAGE_L23, false},
      {&SelecMeter::voltage_l31_sensor_, EM4M_VOLTAGE_L31, false},
      {&SelecMeter::average_voltage_ll_sensor_, EM4M_AVERAGE_VOLTAGE_LL, false},
      {&SelecMeter::net_active_energy_mains_sensor_, EM4M_NET_ACTIVE_ENERGY_MAINS, false},
      {&SelecMeter::net_reactive_energy_mains_sensor_, EM4M_NET_REACTIVE_ENERGY_MAINS, false},
      {&SelecMeter::net_apparent_energy_mains_sensor_, EM4M_NET_APPARENT_ENERGY_MAINS, false},
      {&SelecMeter::net_active_energy_dg_sensor_, EM4M_NET_ACTIVE_ENERGY_DG, false},
      {&SelecMeter::net_reactive_energy_dg_sensor_, EM4M_NET_REACTIVE_ENERGY_DG, false},
      {&SelecMeter::net_apparent_energy_dg_sensor_, EM4M_NET_APPARENT_ENERGY_DG, false},
  };

  // reg_l1 is the L1 register; L2/L3 follow EM4M_PHASE_STRIDE apart
  struct PhaseField {
    sensor::Sensor *Phase::*sensor;
    uint16_t reg_l1;
    bool kilo;
  };
  static constexpr PhaseField PHASE_FIELDS[] PROGMEM = {
      {&Phase::voltage_sensor_, EM4M_VOLTAGE_L1, false},
      {&Phase::current_sensor_, EM4M_CURRENT_L1, false},
      {&Phase::active_power_sensor_, EM4M_ACTIVE_POWER_L1, true},
      {&Phase::reactive_power_sensor_, EM4M_REACTIVE_POWER_L1, true},
      {&Phase::apparent_power_sensor_, EM4M_APPARENT_POWER_L1, true},
      {&Phase::power_factor_sensor_, EM4M_POWER_FACTOR_L1, false},
      {&Phase::import_active_energy_sensor_, EM4M_IMPORT_ACTIVE_ENERGY_L1, false},
      {&Phase::export_active_energy_sensor_, EM4M_EXPORT_ACTIVE_ENERGY_L1, false},
      {&Phase::import_reactive_energy_sensor_, EM4M_IMPORT_REACTIVE_ENERGY_L1, false},
      {&Phase::export_reactive_energy_sensor_, EM4M_EXPORT_REACTIVE_ENERGY_L1, false},
      {&Phase::apparent_energy_sensor_, EM4M_APPARENT_ENERGY_L1, false},
  };

  if (!this->publish_fields_(registers, EM4M_FIELDS, EM4M_REGISTER_COUNT))
    return false;
  for (const auto &entry : PHASE_FIELDS) {
    PhaseField field;
    progmem_memcpy(&field, &entry, sizeof(field));
    for (uint8_t phase = 0; phase < 3; phase++) {
      sensor::Sensor *sens = this->phases_[phase].*field.sensor;
      if (sens != nullptr)
        publish_float(sens, registers, field.reg_l1 + phase * EM4M_PHASE_STRIDE, field.kilo, this->word_swap_);
    }
  }
  return true;
}
#endif

void SelecMeter::update() {
  if (this->read_state_ != ReadState::READ_STATE_IDLE) {
    ESP_LOGD(TAG, "Skipping update: previous read cycle (%s) still in progress",
             LOG_STR_ARG(read_state_name(this->read_state_)));
    return;
  }
  this->start_read_(ReadState::READ_STATE_MAIN_BLOCK);
}

void SelecMeter::start_read_(ReadState state) {
  this->read_state_ = state;
  if (state == ReadState::READ_STATE_IDLE)
    return;
  const ReadRange range = this->read_range_(state);
  // A refused send gets no callback, so unwind here like the async failure paths
  if (!this->read_input_registers(range.start, range.count))
    this->fail_current_read_(LOG_STR("Modbus request refused"));
}

void SelecMeter::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SELEC Meter:\n"
                "  Model: %s\n"
                "  Byte Order: %s\n"
                "  Address: 0x%02X",
                this->model_ == Model::MODEL_EM4M ? LOG_STR_LITERAL("EM4M") : LOG_STR_LITERAL("EM2M"),
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
#ifdef USE_SELEC_METER_EM4M
  if (this->model_ == Model::MODEL_EM4M) {
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
  }
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Serial Number", this->serial_number_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "DG Sensing", this->dg_sensing_sensor_);
#endif
#endif
}

}  // namespace esphome::selec_meter
