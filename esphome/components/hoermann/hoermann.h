#pragma once

#include <utility>

#include "esphome/components/modbus/modbus.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::hoermann {

// Door state as reported by the Hoermann bus controller.
enum class DoorState : uint8_t {
  OPEN,
  OPENING,
  CLOSED,
  CLOSING,
  HALF_OPEN,
  MOVE_VENTING,
  VENT,
  MOVE_HALF,
  STOPPED,
};

// A HCP command is a simulated key press: the "start" values are presented to the bus controller, then after a
// short delay the "end" values, mimicking a button being pressed and released.
struct HoermannCommand {
  const char *name;
  uint16_t reg_plus2_start;
  uint16_t reg_plus2_end;
  uint16_t reg_plus3_start;
  uint16_t reg_plus3_end;
};

class Hoermann : public PollingComponent, public modbus::ModbusServerDevice {
 public:
  void update() override;
  void dump_config() override;

  // Registered by child entities to be notified when the door state changes.
  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }

  // Modbus server callbacks. The bus controller polls state and pushes commands with READ_WRITE_MULTIPLE_REGISTERS
  // (0x17) and broadcasts status with WRITE_MULTIPLE_REGISTERS (0x10). For 0x17 the hub calls the write half first
  // (which stores the command register), then the read half (which echoes it back from STATE_REG).
  modbus::ResponseStatus on_write_registers(uint16_t start_address, const modbus::RegisterValues &registers) override;
  modbus::ResponseStatus on_read_holding_registers(uint16_t start_address, uint16_t number_of_registers,
                                                   modbus::RegisterValues &registers) override;

  // Control functions. Positions follow the cover convention: 0.0 is fully closed, 1.0 fully open.
  void open_door();
  void close_door();
  void impulse_door();
  void stop_door();
  void set_position(float position);

  // State accessors used by child entities.
  DoorState get_door_state() const { return this->door_state_; }
  float get_current_position() const { return this->current_position_; }
  bool is_valid() const { return this->valid_; }

 protected:
  void record_response_();
  // Returns false when the bus controller has not fetched the previous command yet.
  bool queue_command_(const HoermannCommand &command);
  void get_command_values_to_read_(uint16_t &reg_plus2, uint16_t &reg_plus3);
  void on_door_position_changed_(uint16_t old_value, uint16_t new_value);
  void on_current_state_changed_(uint16_t old_value, uint16_t new_value);

  void set_valid_(bool valid);
  void set_door_state_(DoorState state);
  void set_current_position_(float position);

  CallbackManager<void()> state_callback_;

  DoorState door_state_{DoorState::CLOSED};
  float current_position_{0.0f};
  // Position the door was told to travel to; 0.0 means no target is armed.
  float goto_position_{0.0f};
  bool valid_{false};
  bool changed_{false};

  // Pending command / key-press state machine.
  const HoermannCommand *next_command_{nullptr};
  uint32_t command_written_at_{0};
  uint32_t last_response_{0};

  // Previous broadcast register values (to detect high/low byte changes).
  uint16_t prev_position_reg_{0};
  uint16_t prev_state_reg_{0};

  // 0x17 write half: command register last written to COMMAND_REG by the bus controller. The read half echoes
  // its high-byte message counter and low-byte command back from STATE_REG.
  uint16_t command_reg_value_{0};
};

}  // namespace esphome::hoermann
