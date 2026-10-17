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

enum class Model : uint8_t {
  MODEL_EM2M,
  MODEL_EM4M,
};

// Read cycle steps in the order they run. The extra reads (serial number, DG sensing) sit far outside
// the main block and get their own transaction.
enum class ReadState : uint8_t {
  READ_STATE_MAIN_BLOCK,
  READ_STATE_SERIAL_NUMBER,
  READ_STATE_DG_SENSING,
  READ_STATE_IDLE,
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

#ifdef USE_SELEC_METER_EM4M
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
#endif

  void set_model(Model model) { this->model_ = model; }
  // true: registers are word-swapped (LSRF / Mid Little Endian, CDAB byte order)
  // false: registers are in the meter's straightforward order (MSRF / Big Endian, ABCD byte order)
  void set_word_swap(bool word_swap) { this->word_swap_ = word_swap; }
#if defined(USE_SELEC_METER_EM4M) && defined(USE_TEXT_SENSOR)
  void set_serial_number_sensor(text_sensor::TextSensor *serial_number) { this->serial_number_sensor_ = serial_number; }
#endif
#if defined(USE_SELEC_METER_EM4M) && defined(USE_BINARY_SENSOR)
  void set_dg_sensing_sensor(binary_sensor::BinarySensor *dg_sensing) { this->dg_sensing_sensor_ = dg_sensing; }
#endif

  void update() override;

  void on_read_input_registers(uint16_t start_address, std::span<const uint16_t> registers,
                               modbus::ResponseStatus status) override;
  bool on_no_response(std::span<const uint8_t> request_pdu) override;
  void on_not_sent(std::span<const uint8_t> request_pdu) override;

  void dump_config() override;

 protected:
  // One float field of the main block: target sensor, register and whether it is published x1000
  struct FloatField {
    sensor::Sensor *SelecMeter::*sensor;
    uint16_t reg;
    bool kilo;
  };

  // Publishes each configured field in `fields` (a PROGMEM table); false on a payload shorter than
  // `min_count`, and the caller sets or clears the warning from the result
  bool publish_fields_(std::span<const uint16_t> registers, std::span<const FloatField> fields, uint8_t min_count);
  bool decode_em2m_(std::span<const uint16_t> registers);
#ifdef USE_SELEC_METER_EM4M
  bool decode_em4m_(std::span<const uint16_t> registers);
#ifdef USE_TEXT_SENSOR
  void decode_serial_number_(std::span<const uint16_t> registers);
#endif
#ifdef USE_BINARY_SENSOR
  void decode_dg_sensing_(std::span<const uint16_t> registers);
#endif
#endif
  // First register and register count of the read for `state`
  struct ReadRange {
    uint16_t start;
    uint8_t count;
  };
  ReadRange read_range_(ReadState state) const;
  // Whether the read cycle should run `state` on this pass
  bool wanted_(ReadState state) const;
  // Next wanted step after `current`, so one failed range does not cancel the other reads
  ReadState next_read_state_after_(ReadState current) const;
  // Sends the read for `state`; safe from a callback per the modbus callback contract
  void start_read_(ReadState state);
  // Clears the pending read and advances the chain. Only a MAIN_BLOCK failure sets the warning and
  // only a `durable` refusal counts toward MAX_OPTIONAL_READ_FAILURES; `reason` nullptr means already logged
  void fail_current_read_(const LogString *reason, bool durable = false);

#ifdef USE_SELEC_METER_EM4M
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
#endif

  Model model_{Model::MODEL_EM2M};
  bool word_swap_{false};
  ReadState read_state_{ReadState::READ_STATE_IDLE};

  // An optional read stops once its counter reaches MAX_OPTIONAL_READ_FAILURES
#if defined(USE_SELEC_METER_EM4M) && defined(USE_TEXT_SENSOR)
  text_sensor::TextSensor *serial_number_sensor_{nullptr};
  // Also set to the limit once the serial number is published, since it never changes
  uint8_t serial_number_failures_{0};
#endif
#if defined(USE_SELEC_METER_EM4M) && defined(USE_BINARY_SENSOR)
  binary_sensor::BinarySensor *dg_sensing_sensor_{nullptr};
  uint8_t dg_sensing_failures_{0};
#endif
};

}  // namespace esphome::selec_meter
