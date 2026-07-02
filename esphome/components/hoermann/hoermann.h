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
  void setup() override;
  void update() override;
  void dump_config() override;

  // Registered by child entities to be notified when the door state changes.
  template<typename F> void add_on_state_callback(F &&callback) {
    this->state_callback_.add(std::forward<F>(callback));
  }

  // Modbus server callbacks. The bus controller polls state and pushes commands with READ_WRITE_MULTIPLE_REGISTERS
  // (0x17) and broadcasts status with WRITE_MULTIPLE_REGISTERS (0x10).
  modbus::ServerResponseStatus on_modbus_read_write_registers(uint16_t read_start_address, uint16_t number_of_registers,
                                                              uint16_t write_start_address,
                                                              const modbus::RegisterValues &write_registers,
                                                              modbus::RegisterValues &read_registers) override;
  modbus::ServerResponseStatus on_modbus_write_registers(uint16_t start_address,
                                                         const modbus::RegisterValues &registers) override;

  // Control functions.
  void open_door();
  void close_door();
  void impulse_door();
  void stop_door();
  void set_position(int percent);
  void toggle_light();
  void turn_light(bool on);

  // State accessors used by child entities.
  DoorState get_door_state() const { return this->door_state_; }
  float get_current_position() const { return this->current_position_; }
  bool is_light_on() const { return this->light_on_; }
  bool is_valid() const { return this->valid_; }

 protected:
  void record_response_();
  void queue_command_(bool condition, const HoermannCommand &command);
  void get_command_values_to_read_(uint16_t &reg_plus2, uint16_t &reg_plus3);
  void on_door_position_changed_(uint16_t old_value, uint16_t new_value);
  void on_current_state_changed_(uint16_t old_value, uint16_t new_value);
  void on_light_changed_(uint16_t old_value, uint16_t new_value);

  void set_valid_(bool valid);
  void set_door_state_(DoorState state);
  void set_current_position_(float position);
  void set_light_on_(bool on);

  CallbackManager<void()> state_callback_;

  DoorState door_state_{DoorState::CLOSED};
  float current_position_{0.0f};
  float goto_position_{0.0f};
  bool light_on_{false};
  bool valid_{false};
  bool changed_{false};

  // Pending command / key-press state machine.
  const HoermannCommand *next_command_{nullptr};
  uint32_t command_written_at_{0};
  uint32_t last_response_{0};

  // Previous broadcast register values (to detect high/low byte changes).
  uint16_t prev_position_reg_{0};
  uint16_t prev_state_reg_{0};
  uint16_t prev_light_reg_{0};
};

}  // namespace esphome::hoermann
