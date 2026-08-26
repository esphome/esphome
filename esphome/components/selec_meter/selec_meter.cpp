#include "selec_meter.h"
#include "selec_meter_registers.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace esphome::selec_meter {

static const char *const TAG = "selec_meter";

static const uint8_t EM2M_REGISTER_COUNT = 34;  // 34 x 16-bit registers

// Consecutive modbus-level failures (error/no-response/not-sent/refused) an optional extra read
// tolerates before it's given up on for good, instead of being retried silently forever.
static const uint8_t MAX_OPTIONAL_READ_FAILURES = 5;

static const char *read_state_name(ReadState state) {
  switch (state) {
    case ReadState::MAIN_BLOCK:
      return "main block";
    case ReadState::SERIAL_NUMBER:
      return "serial number";
    case ReadState::DG_SENSING:
      return "DG sensing";
    case ReadState::IDLE:
    default:
      return "idle";
  }
}

static float decode_float(std::span<const uint8_t> data, size_t i, float unit, bool word_swapped) {
  uint32_t temp = word_swapped ? encode_uint32(data[i + 2], data[i + 3], data[i], data[i + 1])
                               : encode_uint32(data[i], data[i + 1], data[i + 2], data[i + 3]);

  float f;
  memcpy(&f, &temp, sizeof(f));
  return (f * unit);
}

// One measurement's sensor, register offset, and unit multiplier -- lets decode_em2m_()/decode_em4m_()
// drive their (near-identical) decode-and-publish loops off a table instead of a repeated if-block
// per field.
struct FloatFieldSpec {
  sensor::Sensor *sensor;
  uint16_t reg;
  float unit;
};

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

// The start address a read for `state` was sent with -- used to confirm an incoming response actually
// belongs to the read the state machine is waiting for.
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

void SelecMeter::on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) {
  this->waiting_for_response_ = false;
  ReadState current = this->read_state_;
  if (current == ReadState::IDLE) {
    ESP_LOGW(TAG, "Unexpected response while idle, dropping");
    return;
  }
  // The hub pairs an incoming frame to the waiting request by device address and function code only
  // (see modbus.cpp), and serial number / DG sensing are both 2-register FC 0x04 reads at the same
  // address -- so a stale reply for one can be delivered as the terminal for the other once a timeout
  // has advanced the state machine. Confirm the request this response was sent for actually matches
  // what `current` expects before trusting the payload.
  if (request_pdu.size() < 3 || encode_uint16(request_pdu[1], request_pdu[2]) != expected_start_address(current)) {
    this->fail_current_read_("Response does not match pending read", /*durable=*/false);
    return;
  }
  auto data = modbus::helpers::server_pdu_payload(response_pdu);
  switch (current) {
    case ReadState::MAIN_BLOCK: {
      bool ok = this->model_ == Model::EM4M ? this->decode_em4m_(data) : this->decode_em2m_(data);
      if (ok) {
        this->status_clear_warning();
      } else {
        this->status_set_warning();
      }
      break;
    }
    case ReadState::SERIAL_NUMBER:
#ifdef USE_TEXT_SENSOR
      this->decode_serial_number_(data);
#endif
      break;
    case ReadState::DG_SENSING:
#ifdef USE_BINARY_SENSOR
      this->decode_dg_sensing_(data);
#endif
      break;
    case ReadState::IDLE:
      return;
  }
  // Safe per modbus.h's callback contract: sending from inside a callback is picked up normally.
  this->start_read_(this->next_read_state_after_(current));
}

void SelecMeter::fail_current_read_(const char *reason, bool durable) {
  if (this->read_state_ == ReadState::IDLE) {
    ESP_LOGW(TAG, "Unexpected failure while idle, dropping");
    return;
  }
  ReadState failed_state = this->read_state_;
  ESP_LOGW(TAG, "%s while reading %s", reason, read_state_name(failed_state));
  this->waiting_for_response_ = false;
  // Only the main block (the actual measurements) drives component health -- a meter that simply
  // doesn't support an optional side read shouldn't flap an otherwise-healthy meter into a warning.
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

void SelecMeter::on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) {
  char reason[32];
  snprintf(reason, sizeof(reason), "Modbus error 0x%02X", static_cast<uint8_t>(exception_code));
  // ILLEGAL_FUNCTION/ILLEGAL_DATA_ADDRESS are a durable "this register isn't supported" signal.
  // ACKNOWLEDGE/SERVER_DEVICE_BUSY (and any other exception) mean the meter is busy right now --
  // treat those like a transient timeout so a temporarily busy meter can't permanently latch an
  // optional read disabled.
  const bool durable = exception_code == modbus::ExceptionCode::ILLEGAL_FUNCTION ||
                       exception_code == modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS;
  this->fail_current_read_(reason, durable);
}

bool SelecMeter::on_no_response(std::span<const uint8_t> request_pdu) {
  this->fail_current_read_("No Modbus response");
  return false;
}

void SelecMeter::on_not_sent(std::span<const uint8_t> request_pdu) {
  this->fail_current_read_("Modbus request not sent");
}

#ifdef USE_TEXT_SENSOR
void SelecMeter::note_serial_number_failure_() {
  if (++this->serial_number_failures_ >= MAX_OPTIONAL_READ_FAILURES) {
    ESP_LOGW(TAG, "Serial number read failed %u times in a row, giving up", this->serial_number_failures_);
    this->serial_number_disabled_ = true;
  }
}

void SelecMeter::decode_serial_number_(std::span<const uint8_t> data) {
  if (this->serial_number_sensor_ == nullptr)
    return;
  if (data.size() != 4) {
    ESP_LOGW(TAG, "Unexpected response size for serial number: %zu bytes", data.size());
    this->note_serial_number_failure_();
    return;
  }
  uint32_t serial = this->word_swap_ ? encode_uint32(data[2], data[3], data[0], data[1])
                                     : encode_uint32(data[0], data[1], data[2], data[3]);
  char buf[9];
  snprintf(buf, sizeof(buf), "%08" PRIX32, serial);
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

void SelecMeter::decode_dg_sensing_(std::span<const uint8_t> data) {
  if (this->dg_sensing_sensor_ == nullptr)
    return;
  if (data.size() != 4) {
    ESP_LOGW(TAG, "Unexpected response size for DG sensing: %zu bytes", data.size());
    this->note_dg_sensing_failure_();
    return;
  }
  float value = decode_float(data, 0, NO_DEC_UNIT, this->word_swap_);
  if (!std::isfinite(value)) {
    ESP_LOGW(TAG, "Non-finite DG sensing value, ignoring");
    this->note_dg_sensing_failure_();
    return;
  }
  this->dg_sensing_sensor_->publish_state(value != 0);
  this->dg_sensing_failures_ = 0;
}
#endif

bool SelecMeter::decode_em2m_(std::span<const uint8_t> data) {
  if (data.size() < EM2M_REGISTER_COUNT * 2) {
    ESP_LOGW(TAG, "Invalid size for SelecMeter: expected %u bytes, got %zu", EM2M_REGISTER_COUNT * 2, data.size());
    return false;
  }

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

  // A wrong byte_order corrupts every value the same way, so check all of them (independent of each
  // field's unit multiplier) before publishing any -- an all-or-nothing block, same as decode_em4m_().
  for (const auto &f : fields) {
    if (!std::isfinite(decode_float(data, f.reg * 2, NO_DEC_UNIT, this->word_swap_))) {
      ESP_LOGW(TAG, "Non-finite value(s) decoded, check byte_order setting");
      return false;
    }
  }
  for (const auto &f : fields) {
    if (f.sensor != nullptr)
      f.sensor->publish_state(decode_float(data, f.reg * 2, f.unit, this->word_swap_));
  }
  return true;
}

bool SelecMeter::decode_em4m_(std::span<const uint8_t> data) {
  if (data.size() < EM4M_REGISTER_COUNT * 2) {
    ESP_LOGW(TAG, "Invalid size for SelecMeter: expected %u bytes, got %zu", EM4M_REGISTER_COUNT * 2, data.size());
    return false;
  }

  // A wrong byte_order corrupts every value in the block the same way, so one pass over the raw
  // floats (independent of each field's unit multiplier) catches it before anything is published.
  for (size_t i = 0; i + 4 <= EM4M_REGISTER_COUNT * 2; i += 4) {
    if (!std::isfinite(decode_float(data, i, NO_DEC_UNIT, this->word_swap_))) {
      ESP_LOGW(TAG, "Non-finite value(s) decoded, check byte_order setting");
      return false;
    }
  }

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
      // Per-phase quantities
      {this->voltage_l1_sensor_, EM4M_VOLTAGE_L1, NO_DEC_UNIT},
      {this->voltage_l2_sensor_, EM4M_VOLTAGE_L2, NO_DEC_UNIT},
      {this->voltage_l3_sensor_, EM4M_VOLTAGE_L3, NO_DEC_UNIT},
      {this->voltage_l12_sensor_, EM4M_VOLTAGE_L12, NO_DEC_UNIT},
      {this->voltage_l23_sensor_, EM4M_VOLTAGE_L23, NO_DEC_UNIT},
      {this->voltage_l31_sensor_, EM4M_VOLTAGE_L31, NO_DEC_UNIT},
      {this->current_l1_sensor_, EM4M_CURRENT_L1, NO_DEC_UNIT},
      {this->current_l2_sensor_, EM4M_CURRENT_L2, NO_DEC_UNIT},
      {this->current_l3_sensor_, EM4M_CURRENT_L3, NO_DEC_UNIT},
      {this->active_power_l1_sensor_, EM4M_ACTIVE_POWER_L1, MULTIPLY_THOUSAND_UNIT},
      {this->active_power_l2_sensor_, EM4M_ACTIVE_POWER_L2, MULTIPLY_THOUSAND_UNIT},
      {this->active_power_l3_sensor_, EM4M_ACTIVE_POWER_L3, MULTIPLY_THOUSAND_UNIT},
      {this->reactive_power_l1_sensor_, EM4M_REACTIVE_POWER_L1, MULTIPLY_THOUSAND_UNIT},
      {this->reactive_power_l2_sensor_, EM4M_REACTIVE_POWER_L2, MULTIPLY_THOUSAND_UNIT},
      {this->reactive_power_l3_sensor_, EM4M_REACTIVE_POWER_L3, MULTIPLY_THOUSAND_UNIT},
      {this->apparent_power_l1_sensor_, EM4M_APPARENT_POWER_L1, MULTIPLY_THOUSAND_UNIT},
      {this->apparent_power_l2_sensor_, EM4M_APPARENT_POWER_L2, MULTIPLY_THOUSAND_UNIT},
      {this->apparent_power_l3_sensor_, EM4M_APPARENT_POWER_L3, MULTIPLY_THOUSAND_UNIT},
      {this->power_factor_l1_sensor_, EM4M_POWER_FACTOR_L1, NO_DEC_UNIT},
      {this->power_factor_l2_sensor_, EM4M_POWER_FACTOR_L2, NO_DEC_UNIT},
      {this->power_factor_l3_sensor_, EM4M_POWER_FACTOR_L3, NO_DEC_UNIT},
      {this->import_active_energy_l1_sensor_, EM4M_IMPORT_ACTIVE_ENERGY_L1, NO_DEC_UNIT},
      {this->import_active_energy_l2_sensor_, EM4M_IMPORT_ACTIVE_ENERGY_L2, NO_DEC_UNIT},
      {this->import_active_energy_l3_sensor_, EM4M_IMPORT_ACTIVE_ENERGY_L3, NO_DEC_UNIT},
      {this->export_active_energy_l1_sensor_, EM4M_EXPORT_ACTIVE_ENERGY_L1, NO_DEC_UNIT},
      {this->export_active_energy_l2_sensor_, EM4M_EXPORT_ACTIVE_ENERGY_L2, NO_DEC_UNIT},
      {this->export_active_energy_l3_sensor_, EM4M_EXPORT_ACTIVE_ENERGY_L3, NO_DEC_UNIT},
      {this->import_reactive_energy_l1_sensor_, EM4M_IMPORT_REACTIVE_ENERGY_L1, NO_DEC_UNIT},
      {this->import_reactive_energy_l2_sensor_, EM4M_IMPORT_REACTIVE_ENERGY_L2, NO_DEC_UNIT},
      {this->import_reactive_energy_l3_sensor_, EM4M_IMPORT_REACTIVE_ENERGY_L3, NO_DEC_UNIT},
      {this->export_reactive_energy_l1_sensor_, EM4M_EXPORT_REACTIVE_ENERGY_L1, NO_DEC_UNIT},
      {this->export_reactive_energy_l2_sensor_, EM4M_EXPORT_REACTIVE_ENERGY_L2, NO_DEC_UNIT},
      {this->export_reactive_energy_l3_sensor_, EM4M_EXPORT_REACTIVE_ENERGY_L3, NO_DEC_UNIT},
      {this->apparent_energy_l1_sensor_, EM4M_APPARENT_ENERGY_L1, NO_DEC_UNIT},
      {this->apparent_energy_l2_sensor_, EM4M_APPARENT_ENERGY_L2, NO_DEC_UNIT},
      {this->apparent_energy_l3_sensor_, EM4M_APPARENT_ENERGY_L3, NO_DEC_UNIT},
      {this->average_voltage_ll_sensor_, EM4M_AVERAGE_VOLTAGE_LL, NO_DEC_UNIT},
      {this->net_active_energy_mains_sensor_, EM4M_NET_ACTIVE_ENERGY_MAINS, NO_DEC_UNIT},
      {this->net_reactive_energy_mains_sensor_, EM4M_NET_REACTIVE_ENERGY_MAINS, NO_DEC_UNIT},
      {this->net_apparent_energy_mains_sensor_, EM4M_NET_APPARENT_ENERGY_MAINS, NO_DEC_UNIT},
      {this->net_active_energy_dg_sensor_, EM4M_NET_ACTIVE_ENERGY_DG, NO_DEC_UNIT},
      {this->net_reactive_energy_dg_sensor_, EM4M_NET_REACTIVE_ENERGY_DG, NO_DEC_UNIT},
      {this->net_apparent_energy_dg_sensor_, EM4M_NET_APPARENT_ENERGY_DG, NO_DEC_UNIT},
  };
  for (const auto &f : fields) {
    if (f.sensor != nullptr)
      f.sensor->publish_state(decode_float(data, f.reg * 2, f.unit, this->word_swap_));
  }
  return true;
}

void SelecMeter::update() {
  if (this->waiting_for_response_ || this->read_state_ != ReadState::IDLE) {
    ESP_LOGD(TAG, "Skipping update: previous read cycle (%s) still in progress", read_state_name(this->read_state_));
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

  // A false return means the request was refused at send_pdu() (e.g. tx buffer full) and no terminal
  // callback will ever follow -- unwind ourselves via the same path the async failure callbacks use.
  if (!sent)
    this->fail_current_read_("Modbus request refused");
}

void SelecMeter::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SELEC Meter:\n"
                "  Model: %s\n"
                "  Byte Order: %s\n"
                "  Address: 0x%02X",
                this->model_ == Model::EM4M ? "EM4M" : "EM2M", this->word_swap_ ? "LSRF (word-swapped)" : "MSRF",
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
  LOG_SENSOR("  ", "Voltage L1", this->voltage_l1_sensor_);
  LOG_SENSOR("  ", "Voltage L2", this->voltage_l2_sensor_);
  LOG_SENSOR("  ", "Voltage L3", this->voltage_l3_sensor_);
  LOG_SENSOR("  ", "Voltage L1-L2", this->voltage_l12_sensor_);
  LOG_SENSOR("  ", "Voltage L2-L3", this->voltage_l23_sensor_);
  LOG_SENSOR("  ", "Voltage L3-L1", this->voltage_l31_sensor_);
  LOG_SENSOR("  ", "Current L1", this->current_l1_sensor_);
  LOG_SENSOR("  ", "Current L2", this->current_l2_sensor_);
  LOG_SENSOR("  ", "Current L3", this->current_l3_sensor_);
  LOG_SENSOR("  ", "Active Power L1", this->active_power_l1_sensor_);
  LOG_SENSOR("  ", "Active Power L2", this->active_power_l2_sensor_);
  LOG_SENSOR("  ", "Active Power L3", this->active_power_l3_sensor_);
  LOG_SENSOR("  ", "Reactive Power L1", this->reactive_power_l1_sensor_);
  LOG_SENSOR("  ", "Reactive Power L2", this->reactive_power_l2_sensor_);
  LOG_SENSOR("  ", "Reactive Power L3", this->reactive_power_l3_sensor_);
  LOG_SENSOR("  ", "Apparent Power L1", this->apparent_power_l1_sensor_);
  LOG_SENSOR("  ", "Apparent Power L2", this->apparent_power_l2_sensor_);
  LOG_SENSOR("  ", "Apparent Power L3", this->apparent_power_l3_sensor_);
  LOG_SENSOR("  ", "Power Factor L1", this->power_factor_l1_sensor_);
  LOG_SENSOR("  ", "Power Factor L2", this->power_factor_l2_sensor_);
  LOG_SENSOR("  ", "Power Factor L3", this->power_factor_l3_sensor_);
  LOG_SENSOR("  ", "Import Active Energy L1", this->import_active_energy_l1_sensor_);
  LOG_SENSOR("  ", "Import Active Energy L2", this->import_active_energy_l2_sensor_);
  LOG_SENSOR("  ", "Import Active Energy L3", this->import_active_energy_l3_sensor_);
  LOG_SENSOR("  ", "Export Active Energy L1", this->export_active_energy_l1_sensor_);
  LOG_SENSOR("  ", "Export Active Energy L2", this->export_active_energy_l2_sensor_);
  LOG_SENSOR("  ", "Export Active Energy L3", this->export_active_energy_l3_sensor_);
  LOG_SENSOR("  ", "Import Reactive Energy L1", this->import_reactive_energy_l1_sensor_);
  LOG_SENSOR("  ", "Import Reactive Energy L2", this->import_reactive_energy_l2_sensor_);
  LOG_SENSOR("  ", "Import Reactive Energy L3", this->import_reactive_energy_l3_sensor_);
  LOG_SENSOR("  ", "Export Reactive Energy L1", this->export_reactive_energy_l1_sensor_);
  LOG_SENSOR("  ", "Export Reactive Energy L2", this->export_reactive_energy_l2_sensor_);
  LOG_SENSOR("  ", "Export Reactive Energy L3", this->export_reactive_energy_l3_sensor_);
  LOG_SENSOR("  ", "Apparent Energy L1", this->apparent_energy_l1_sensor_);
  LOG_SENSOR("  ", "Apparent Energy L2", this->apparent_energy_l2_sensor_);
  LOG_SENSOR("  ", "Apparent Energy L3", this->apparent_energy_l3_sensor_);
  LOG_SENSOR("  ", "Average Voltage LL", this->average_voltage_ll_sensor_);
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
