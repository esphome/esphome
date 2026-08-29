#pragma once

#include "esphome/core/component.h"
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

enum Model : uint8_t {
  EM2M,
  EM4M,
};

// Cycles the extra Modbus reads (serial number, DG sensing) that live far outside the
// main 0-113 register block and therefore need their own transaction each update.
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

  // EM4M per-phase sensors
  SELEC_METER_SENSOR(voltage_l1)
  SELEC_METER_SENSOR(voltage_l2)
  SELEC_METER_SENSOR(voltage_l3)
  SELEC_METER_SENSOR(voltage_l12)
  SELEC_METER_SENSOR(voltage_l23)
  SELEC_METER_SENSOR(voltage_l31)
  SELEC_METER_SENSOR(current_l1)
  SELEC_METER_SENSOR(current_l2)
  SELEC_METER_SENSOR(current_l3)
  SELEC_METER_SENSOR(active_power_l1)
  SELEC_METER_SENSOR(active_power_l2)
  SELEC_METER_SENSOR(active_power_l3)
  SELEC_METER_SENSOR(reactive_power_l1)
  SELEC_METER_SENSOR(reactive_power_l2)
  SELEC_METER_SENSOR(reactive_power_l3)
  SELEC_METER_SENSOR(apparent_power_l1)
  SELEC_METER_SENSOR(apparent_power_l2)
  SELEC_METER_SENSOR(apparent_power_l3)
  SELEC_METER_SENSOR(power_factor_l1)
  SELEC_METER_SENSOR(power_factor_l2)
  SELEC_METER_SENSOR(power_factor_l3)
  SELEC_METER_SENSOR(import_active_energy_l1)
  SELEC_METER_SENSOR(import_active_energy_l2)
  SELEC_METER_SENSOR(import_active_energy_l3)
  SELEC_METER_SENSOR(export_active_energy_l1)
  SELEC_METER_SENSOR(export_active_energy_l2)
  SELEC_METER_SENSOR(export_active_energy_l3)
  SELEC_METER_SENSOR(import_reactive_energy_l1)
  SELEC_METER_SENSOR(import_reactive_energy_l2)
  SELEC_METER_SENSOR(import_reactive_energy_l3)
  SELEC_METER_SENSOR(export_reactive_energy_l1)
  SELEC_METER_SENSOR(export_reactive_energy_l2)
  SELEC_METER_SENSOR(export_reactive_energy_l3)
  SELEC_METER_SENSOR(apparent_energy_l1)
  SELEC_METER_SENSOR(apparent_energy_l2)
  SELEC_METER_SENSOR(apparent_energy_l3)
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
  // Returns false (and publishes nothing) on a malformed payload; drives status_set_warning()/
  // status_clear_warning() in on_read_input_registers() so component health tracks the main block
  // specifically.
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
  // Advances the state machine from `current`, whether it just succeeded or failed -- a bad
  // transaction on one register range shouldn't cancel the other, independent reads in the chain.
  ReadState next_read_state_after_(ReadState current);
  // Issues the read for `state` (or does nothing for IDLE), setting read_state_/waiting_for_response_
  // as it goes. Sending from within a callback is safe per modbus.h's callback contract, so this is
  // called directly from update() and, after decoding, from on_read_input_registers()/fail_current_read_()
  // -- no deferral through loop() is needed.
  void start_read_(ReadState state);
  // Drops the failure (like on_read_input_registers() drops an unexpected terminal) if no read is in
  // flight, then clears waiting_for_response_ and advances to the next read in the chain. Only marks
  // the component unhealthy for a MAIN_BLOCK failure -- an optional side read (serial number/DG
  // sensing) that the meter simply doesn't support shouldn't flip an otherwise-healthy meter into a
  // warning state. Shared by on_read_input_registers()'s exception path, on_no_response()/on_not_sent(),
  // and a refused send in start_read_().
  // `durable` distinguishes a response the meter actually sent back refusing the read (a real "this
  // register doesn't exist" signal) from a transient failure (no response/not sent/refused send, which
  // could just as easily mean the bus is briefly busy). Only durable failures count toward
  // MAX_OPTIONAL_READ_FAILURES -- a transient failure retries forever instead of eventually latching
  // an optional read disabled for a reason that was never actually "unsupported register".
  void fail_current_read_(const char *reason, bool durable = false);

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
