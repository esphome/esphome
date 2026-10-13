#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <span>

namespace esphome::selec_meter {

#define SELEC_METER_SENSOR(name) \
 protected: \
  sensor::Sensor *name##_sensor_{nullptr}; \
\
 public: \
  void set_##name##_sensor(sensor::Sensor *(name)) { this->name##_sensor_ = name; }

// Per-phase sensor in the Phase struct, setter indexed by phase (0 = L1/A, 1 = L2/B, 2 = L3/C)
#define SELEC_METER_PHASE_SENSOR(name) \
  void set_##name##_sensor(uint8_t phase, sensor::Sensor *(name)) { this->phases_[phase].name##_sensor_ = name; }

enum Model : uint8_t {
  EM2M,
  EM4M,
};

// Extra reads (serial number, DG sensing) sit far outside the main block and get their own transaction
enum class ReadState : uint8_t {
  MAIN_BLOCK,
  SERIAL_NUMBER,
  DG_SENSING,
  IDLE,
};

class SelecMeter final : public PollingComponent, public modbus::ModbusClientDevice {
 public:
  SELEC_METER_SENSOR(total_active_energy)
  SELEC_METER_SENSOR(import_active_energy)
  SELEC_METER_SENSOR(export_active_energy)
  SELEC_METER_SENSOR(total_reactive_energy)
  SELEC_METER_SENSOR(import_reactive_energy)
  SELEC_METER_SENSOR(export_reactive_energy)
  SELEC_METER_SENSOR(apparent_energy)
  SELEC_METER_SENSOR(active_power)
  SELEC_METER_SENSOR(reactive_power)
  SELEC_METER_SENSOR(apparent_power)
  SELEC_METER_SENSOR(voltage)
  SELEC_METER_SENSOR(current)
  SELEC_METER_SENSOR(power_factor)
  SELEC_METER_SENSOR(frequency)
  SELEC_METER_SENSOR(maximum_demand_active_power)
  SELEC_METER_SENSOR(maximum_demand_reactive_power)
  SELEC_METER_SENSOR(maximum_demand_apparent_power)

  // EM4M per-phase sensors, indexed by phase (see Phase below)
  SELEC_METER_PHASE_SENSOR(voltage)
  SELEC_METER_PHASE_SENSOR(current)
  SELEC_METER_PHASE_SENSOR(active_power)
  SELEC_METER_PHASE_SENSOR(reactive_power)
  SELEC_METER_PHASE_SENSOR(apparent_power)
  SELEC_METER_PHASE_SENSOR(power_factor)
  SELEC_METER_PHASE_SENSOR(import_active_energy)
  SELEC_METER_PHASE_SENSOR(export_active_energy)
  SELEC_METER_PHASE_SENSOR(import_reactive_energy)
  SELEC_METER_PHASE_SENSOR(export_reactive_energy)
  SELEC_METER_PHASE_SENSOR(apparent_energy)

  // Line-to-line and aggregate sensors, not tied to one phase
  SELEC_METER_SENSOR(voltage_l12)
  SELEC_METER_SENSOR(voltage_l23)
  SELEC_METER_SENSOR(voltage_l31)
  SELEC_METER_SENSOR(average_voltage_ll)
  SELEC_METER_SENSOR(net_active_energy_mains)
  SELEC_METER_SENSOR(net_reactive_energy_mains)
  SELEC_METER_SENSOR(net_apparent_energy_mains)
  SELEC_METER_SENSOR(net_active_energy_dg)
  SELEC_METER_SENSOR(net_reactive_energy_dg)
  SELEC_METER_SENSOR(net_apparent_energy_dg)

  void set_model(Model model) { this->model_ = model; }
  // true: registers are word-swapped (LSRF / Mid Little Endian, CDAB byte order)
  // false: registers are in the meter's straightforward order (MSRF / Big Endian, ABCD byte order)
  void set_word_swap(bool word_swap) { this->word_swap_ = word_swap; }
#ifdef USE_TEXT_SENSOR
  void set_serial_number_sensor(text_sensor::TextSensor *serial_number) { this->serial_number_sensor_ = serial_number; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_dg_sensing_sensor(binary_sensor::BinarySensor *dg_sensing) { this->dg_sensing_sensor_ = dg_sensing; }
#endif

  void update() override;

  void on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                               modbus::ResponseStatus status) override;
  bool on_no_response(std::span<const uint8_t> request_pdu) override;
  void on_not_sent(std::span<const uint8_t> request_pdu) override;

  void dump_config() override;

 protected:
  // False on a short payload; the caller sets or clears the warning from the result
  bool decode_em2m_(std::span<const uint16_t> registers);
  bool decode_em4m_(std::span<const uint16_t> registers);
#ifdef USE_TEXT_SENSOR
  void decode_serial_number_(std::span<const uint16_t> registers);
  void note_serial_number_failure_();
#endif
#ifdef USE_BINARY_SENSOR
  void decode_dg_sensing_(std::span<const uint16_t> registers);
  void note_dg_sensing_failure_();
#endif
  ReadState next_read_state_after_main_block_();
  // Next step after `current`, so one failed range does not cancel the other reads
  ReadState next_read_state_after_(ReadState current);
  // Sends the read for `state`; safe from a callback per the modbus callback contract
  void start_read_(ReadState state);
  // Clears the pending read and advances the chain. Only a MAIN_BLOCK failure sets the warning and
  // only a `durable` refusal counts toward MAX_OPTIONAL_READ_FAILURES; `reason` nullptr means already logged
  void fail_current_read_(const LogString *reason, bool durable = false);

  // phases_[0..2] are L1/L2/L3 (A/B/C), EM4M_PHASE_STRIDE registers apart
  struct Phase {
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *active_power_sensor_{nullptr};
    sensor::Sensor *reactive_power_sensor_{nullptr};
    sensor::Sensor *apparent_power_sensor_{nullptr};
    sensor::Sensor *power_factor_sensor_{nullptr};
    sensor::Sensor *import_active_energy_sensor_{nullptr};
    sensor::Sensor *export_active_energy_sensor_{nullptr};
    sensor::Sensor *import_reactive_energy_sensor_{nullptr};
    sensor::Sensor *export_reactive_energy_sensor_{nullptr};
    sensor::Sensor *apparent_energy_sensor_{nullptr};
  };
  Phase phases_[3];

  Model model_{Model::EM2M};
  bool word_swap_{false};
  ReadState read_state_{ReadState::IDLE};
  bool waiting_for_response_{false};

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *serial_number_sensor_{nullptr};
  bool serial_number_published_{false};
  bool serial_number_disabled_{false};
  uint8_t serial_number_failures_{0};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *dg_sensing_sensor_{nullptr};
  uint8_t dg_sensing_failures_{0};
  bool dg_sensing_disabled_{false};
#endif
};

}  // namespace esphome::selec_meter
