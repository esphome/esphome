#pragma once

#include "../mcp4461.h"
#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mcp4461 {

class Mcp4461Wiper : public output::FloatOutput, public Parented<Mcp4461Component> {
 public:
  Mcp4461Wiper(Mcp4461Component *parent, Mcp4461WiperIdx wiper) : parent_(parent), wiper_(wiper) {}
  /// @brief Set level of wiper
  /// @param[in] state - The desired float level in range 0-1.0
  void set_level(float state);
  /// @brief Enables/Disables current output using bool parameter
  /// @param[in] state boolean var representing desired state (true=ON, false=OFF)
  void set_state(bool state) override;
  /// @brief Enables current output
  void turn_on() override;
  /// @brief Disables current output
  void turn_off() override;
  /// @brief Read current device wiper state without updating internal output state
  /// @return float - current device state as float in range 0 - 1.0
  float read_state();
  /// @brief Update current output state using device wiper state
  /// @return float - current updated output state as float in range 0 - 1.0
  float update_state();
  /// @brief Increase wiper by 1 tap
  void increase_wiper();
  /// @brief Decrease wiper by 1 tap
  void decrease_wiper();
  /// @brief Enable given terminal
  /// @param[in] terminal single char parameter defining desired terminal to enable, one of { 'a', 'b', 'w', 'h' }
  void enable_terminal(char terminal);
  /// @brief Disable given terminal
  /// @param[in] terminal single char parameter defining desired terminal to disable, one of { 'a', 'b', 'w', 'h' }
  void disable_terminal(char terminal);

 protected:
  void write_state(float state) override;
  Mcp4461Component *parent_;
  Mcp4461WiperIdx wiper_;
  float state_;
};

}  // namespace mcp4461
}  // namespace esphome
