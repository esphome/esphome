#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/modbus/modbus.h"

#include <array>
#include <span>

namespace esphome::pzem6l24 {

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

// Input registers read per poll (0x0000 - 0x003F inclusive) and the payload size that yields.
static constexpr uint8_t PZEM_REGISTER_COUNT = 64;
static constexpr size_t PZEM_PAYLOAD_SIZE = PZEM_REGISTER_COUNT * 2;

// Failed polls tolerated before the readings are blanked; one collision on a shared RS-485 bus should
// not take every entity unavailable for a whole update interval.
static constexpr uint8_t MAX_CONSECUTIVE_READ_FAILURES = 3;

// The energy reset command as it goes on the wire: function code, reserved byte, phase selector. The
// hub adds the address and CRC. Split out so the phase byte can be pinned by a test.
constexpr std::array<uint8_t, 3> build_reset_pdu(ResetPhase phase) {
  return {PZEM_CMD_RESET_ENERGY, 0x00, static_cast<uint8_t>(phase)};
}

class PZEM6L24 final : public PollingComponent, public modbus::ModbusClientDevice {
 protected:
  // Ahead of the sensor pointers so the two bytes share the base class's tail padding.
  // Register reads accepted by the hub but not yet resolved by a terminal callback.
  uint8_t reads_outstanding_{0};
  // Failed polls since the last good one; stops counting at MAX_CONSECUTIVE_READ_FAILURES.
  uint8_t consecutive_failures_{0};

 public:
  SUB_SENSOR(voltage_a)
  SUB_SENSOR(voltage_b)
  SUB_SENSOR(voltage_c)
  SUB_SENSOR(current_a)
  SUB_SENSOR(current_b)
  SUB_SENSOR(current_c)
  SUB_SENSOR(active_power_a)
  SUB_SENSOR(active_power_b)
  SUB_SENSOR(active_power_c)
  SUB_SENSOR(reactive_power_a)
  SUB_SENSOR(reactive_power_b)
  SUB_SENSOR(reactive_power_c)
  SUB_SENSOR(apparent_power_a)
  SUB_SENSOR(apparent_power_b)
  SUB_SENSOR(apparent_power_c)
  SUB_SENSOR(power_factor_a)
  SUB_SENSOR(power_factor_b)
  SUB_SENSOR(power_factor_c)
  SUB_SENSOR(active_energy_a)
  SUB_SENSOR(active_energy_b)
  SUB_SENSOR(active_energy_c)
  SUB_SENSOR(reactive_energy_a)
  SUB_SENSOR(reactive_energy_b)
  SUB_SENSOR(reactive_energy_c)
  SUB_SENSOR(apparent_energy_a)
  SUB_SENSOR(apparent_energy_b)
  SUB_SENSOR(apparent_energy_c)
  SUB_SENSOR(frequency)
  SUB_SENSOR(total_active_power)
  SUB_SENSOR(total_reactive_power)
  SUB_SENSOR(total_apparent_power)
  SUB_SENSOR(total_power_factor)
  SUB_SENSOR(total_active_energy)
  SUB_SENSOR(total_reactive_energy)
  SUB_SENSOR(total_apparent_energy)

 public:
  // Queues the energy reset command for the selected phase(s); the pzem6l24.reset_energy action calls this.
  void reset_energy(ResetPhase phase_option);

  void update() override;

  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override;

  void on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) override;

  bool on_no_response(std::span<const uint8_t> request_pdu) override;

  void on_not_sent(std::span<const uint8_t> request_pdu) override;

  void dump_config() override;

 protected:
  void publish_(const uint8_t *data);
  void request_failed_(std::span<const uint8_t> request_pdu);
  void read_failed_();
  void read_finished_();
};

}  // namespace esphome::pzem6l24
