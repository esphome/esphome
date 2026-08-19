#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <array>
#include <span>

namespace esphome::pzem6l24 {

template<typename... Ts> class ResetEnergyAction;

// Options for the reset_energy action
enum ResetPhase : uint8_t {
  RESET_PHASE_A = 0x00,         // Reset phase A energy only
  RESET_PHASE_B = 0x01,         // Reset phase B energy only
  RESET_PHASE_C = 0x02,         // Reset phase C energy only
  RESET_PHASE_COMBINED = 0x03,  // Reset combined (sum) energy only
  RESET_PHASE_ALL = 0x0F,       // Reset all energy counters
};

// Reset energy function code (PZEM-6L24 specific, non-standard Modbus)
static constexpr uint8_t PZEM_CMD_RESET_ENERGY = 0x42;

// The energy reset command as it goes on the wire: function code 0x42, a reserved byte and the phase
// selector. The hub prepends the device address and appends the CRC. Split out from reset_energy_() so
// the byte order can be pinned by a test - a wrong phase byte irreversibly zeroes the wrong counters.
constexpr std::array<uint8_t, 3> build_reset_pdu(ResetPhase phase) {
  return {PZEM_CMD_RESET_ENERGY, 0x00, static_cast<uint8_t>(phase)};
}

class PZEM6L24 final : public PollingComponent, public modbus::ModbusClientDevice {
 public:
  // Per-phase voltage setters
  void set_voltage_sensor_a(sensor::Sensor *voltage_a) { this->voltage_a_ = voltage_a; }
  void set_voltage_sensor_b(sensor::Sensor *voltage_b) { this->voltage_b_ = voltage_b; }
  void set_voltage_sensor_c(sensor::Sensor *voltage_c) { this->voltage_c_ = voltage_c; }

  // Per-phase current setters
  void set_current_sensor_a(sensor::Sensor *current_a) { this->current_a_ = current_a; }
  void set_current_sensor_b(sensor::Sensor *current_b) { this->current_b_ = current_b; }
  void set_current_sensor_c(sensor::Sensor *current_c) { this->current_c_ = current_c; }

  // Per-phase active power setters
  void set_active_power_sensor_a(sensor::Sensor *active_power_a) { this->active_power_a_ = active_power_a; }
  void set_active_power_sensor_b(sensor::Sensor *active_power_b) { this->active_power_b_ = active_power_b; }
  void set_active_power_sensor_c(sensor::Sensor *active_power_c) { this->active_power_c_ = active_power_c; }

  // Per-phase reactive power setters
  void set_reactive_power_sensor_a(sensor::Sensor *reactive_power_a) { this->reactive_power_a_ = reactive_power_a; }
  void set_reactive_power_sensor_b(sensor::Sensor *reactive_power_b) { this->reactive_power_b_ = reactive_power_b; }
  void set_reactive_power_sensor_c(sensor::Sensor *reactive_power_c) { this->reactive_power_c_ = reactive_power_c; }

  // Per-phase apparent power setters
  void set_apparent_power_sensor_a(sensor::Sensor *apparent_power_a) { this->apparent_power_a_ = apparent_power_a; }
  void set_apparent_power_sensor_b(sensor::Sensor *apparent_power_b) { this->apparent_power_b_ = apparent_power_b; }
  void set_apparent_power_sensor_c(sensor::Sensor *apparent_power_c) { this->apparent_power_c_ = apparent_power_c; }

  // Per-phase power factor setters
  void set_power_factor_sensor_a(sensor::Sensor *power_factor_a) { this->power_factor_a_ = power_factor_a; }
  void set_power_factor_sensor_b(sensor::Sensor *power_factor_b) { this->power_factor_b_ = power_factor_b; }
  void set_power_factor_sensor_c(sensor::Sensor *power_factor_c) { this->power_factor_c_ = power_factor_c; }

  // Per-phase active energy setters
  void set_active_energy_sensor_a(sensor::Sensor *active_energy_a) { this->active_energy_a_ = active_energy_a; }
  void set_active_energy_sensor_b(sensor::Sensor *active_energy_b) { this->active_energy_b_ = active_energy_b; }
  void set_active_energy_sensor_c(sensor::Sensor *active_energy_c) { this->active_energy_c_ = active_energy_c; }

  // Per-phase reactive energy setters
  void set_reactive_energy_sensor_a(sensor::Sensor *reactive_energy_a) { this->reactive_energy_a_ = reactive_energy_a; }
  void set_reactive_energy_sensor_b(sensor::Sensor *reactive_energy_b) { this->reactive_energy_b_ = reactive_energy_b; }
  void set_reactive_energy_sensor_c(sensor::Sensor *reactive_energy_c) { this->reactive_energy_c_ = reactive_energy_c; }

  // Per-phase apparent energy setters
  void set_apparent_energy_sensor_a(sensor::Sensor *apparent_energy_a) { this->apparent_energy_a_ = apparent_energy_a; }
  void set_apparent_energy_sensor_b(sensor::Sensor *apparent_energy_b) { this->apparent_energy_b_ = apparent_energy_b; }
  void set_apparent_energy_sensor_c(sensor::Sensor *apparent_energy_c) { this->apparent_energy_c_ = apparent_energy_c; }

  // Combined sensor setters
  void set_frequency_sensor(sensor::Sensor *frequency) { this->frequency_ = frequency; }
  void set_total_active_power_sensor(sensor::Sensor *total_active_power) {
    this->total_active_power_ = total_active_power;
  }
  void set_total_reactive_power_sensor(sensor::Sensor *total_reactive_power) {
    this->total_reactive_power_ = total_reactive_power;
  }
  void set_total_apparent_power_sensor(sensor::Sensor *total_apparent_power) {
    this->total_apparent_power_ = total_apparent_power;
  }
  void set_total_power_factor_sensor(sensor::Sensor *total_power_factor) {
    this->total_power_factor_ = total_power_factor;
  }
  void set_total_active_energy_sensor(sensor::Sensor *total_active_energy) {
    this->total_active_energy_ = total_active_energy;
  }
  void set_total_reactive_energy_sensor(sensor::Sensor *total_reactive_energy) {
    this->total_reactive_energy_ = total_reactive_energy;
  }
  void set_total_apparent_energy_sensor(sensor::Sensor *total_apparent_energy) {
    this->total_apparent_energy_ = total_apparent_energy;
  }

  void update() override;

  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override;

  void on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) override;

  bool on_no_response(std::span<const uint8_t> request_pdu) override;

  void on_not_sent(std::span<const uint8_t> request_pdu) override;

  void dump_config() override;

 protected:
  template<typename... Ts> friend class ResetEnergyAction;

  void publish_values_(std::span<const uint8_t> data);
  void publish_(const uint8_t *data);
  void read_failed_();
  void read_finished_();

  // Register reads accepted by the hub but not yet resolved by a terminal callback. Lets update()
  // tell a genuine queue refusal (no answer is coming) from a duplicate absorbed into a read that is
  // already in flight (an answer is still owed).
  uint8_t reads_outstanding_{0};
  // Failed polls since the last good one, and polls the hub refused in a row. Both stop counting at
  // their threshold, and a successful poll resets them.
  uint8_t consecutive_failures_{0};
  uint8_t unqueued_polls_{0};

  // Per-phase sensors
  sensor::Sensor *voltage_a_{nullptr};
  sensor::Sensor *voltage_b_{nullptr};
  sensor::Sensor *voltage_c_{nullptr};

  sensor::Sensor *current_a_{nullptr};
  sensor::Sensor *current_b_{nullptr};
  sensor::Sensor *current_c_{nullptr};

  sensor::Sensor *active_power_a_{nullptr};
  sensor::Sensor *active_power_b_{nullptr};
  sensor::Sensor *active_power_c_{nullptr};

  sensor::Sensor *reactive_power_a_{nullptr};
  sensor::Sensor *reactive_power_b_{nullptr};
  sensor::Sensor *reactive_power_c_{nullptr};

  sensor::Sensor *apparent_power_a_{nullptr};
  sensor::Sensor *apparent_power_b_{nullptr};
  sensor::Sensor *apparent_power_c_{nullptr};

  sensor::Sensor *power_factor_a_{nullptr};
  sensor::Sensor *power_factor_b_{nullptr};
  sensor::Sensor *power_factor_c_{nullptr};

  sensor::Sensor *active_energy_a_{nullptr};
  sensor::Sensor *active_energy_b_{nullptr};
  sensor::Sensor *active_energy_c_{nullptr};

  sensor::Sensor *reactive_energy_a_{nullptr};
  sensor::Sensor *reactive_energy_b_{nullptr};
  sensor::Sensor *reactive_energy_c_{nullptr};

  sensor::Sensor *apparent_energy_a_{nullptr};
  sensor::Sensor *apparent_energy_b_{nullptr};
  sensor::Sensor *apparent_energy_c_{nullptr};

  // Combined sensors
  sensor::Sensor *frequency_{nullptr};
  sensor::Sensor *total_active_power_{nullptr};
  sensor::Sensor *total_reactive_power_{nullptr};
  sensor::Sensor *total_apparent_power_{nullptr};
  sensor::Sensor *total_power_factor_{nullptr};
  sensor::Sensor *total_active_energy_{nullptr};
  sensor::Sensor *total_reactive_energy_{nullptr};
  sensor::Sensor *total_apparent_energy_{nullptr};

  void reset_energy_(ResetPhase phase_option);
};

template<typename... Ts> class ResetEnergyAction final : public Action<Ts...> {
 public:
  explicit ResetEnergyAction(PZEM6L24 *parent) : parent_(parent) {}

  void set_phase(ResetPhase phase) { this->phase_ = phase; }

  void play(const Ts &...x) override { this->parent_->reset_energy_(this->phase_); }

 protected:
  PZEM6L24 *parent_;
  ResetPhase phase_{RESET_PHASE_ALL};
};

}  // namespace esphome::pzem6l24
