#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include "opentherm.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace esphome {
namespace opentherm {

// OpenTherm component for ESPHome
class OpenthermHub : public Component {
 protected:
  // Communication pins for the OpenTherm interface
  InternalGPIOPin *in_pin_, *out_pin_;
  // The OpenTherm interface
  std::unique_ptr<OpenTherm> opentherm_;

  // The set of initial messages to send on starting communication with the boiler
  std::unordered_set<MessageId> initial_messages_;
  // and the repeating messages which are sent repeatedly to update various sensors
  // and boiler parameters (like the setpoint).
  std::unordered_set<MessageId> repeating_messages_;
  // Indicates if we are still working on the initial requests or not
  bool sending_initial_ = true;
  // Index for the current request in one of the _requests sets.
  std::unordered_set<MessageId>::const_iterator current_message_iterator_;

  uint32_t last_conversation_start_ = 0;
  uint32_t last_conversation_end_ = 0;
  OperationMode last_mode_ = IDLE;
  OpenthermData last_request_;

  // Synchronous communication mode prevents other components from disabling interrupts while
  // we are talking to the boiler. Enable if you experience random intermittent invalid response errors.
  // Very likely to happen while using Dallas temperature sensors.
  bool sync_mode_ = false;

  // Create OpenTherm messages based on the message id
  OpenthermData build_request_(MessageId request_id);
  void handle_protocol_write_error_();
  void handle_protocol_read_error_();
  void handle_timeout_error_();
  void stop_opentherm_();
  void start_conversation_();
  void read_response_();
  bool check_timings_(uint32_t cur_time);
  bool should_skip_loop_(uint32_t cur_time) const;
  void sync_loop_();

  template<typename F> bool spin_wait_(uint32_t timeout, F func) {
    auto start_time = millis();
    while (func()) {
      yield();
      auto cur_time = millis();
      if (cur_time - start_time >= timeout) {
        return false;
      }
    }
    return true;
  }

 public:
  // Constructor with references to the global interrupt handlers
  OpenthermHub();

  // Handle responses from the OpenTherm interface
  void process_response(OpenthermData &data);

  // Setters for the input and output OpenTherm interface pins
  void set_in_pin(InternalGPIOPin *in_pin) { this->in_pin_ = in_pin; }
  void set_out_pin(InternalGPIOPin *out_pin) { this->out_pin_ = out_pin; }

  // Add a request to the set of initial requests
  void add_initial_message(MessageId message_id) { this->initial_messages_.insert(message_id); }
  // Add a request to the set of repeating requests. Note that a large number of repeating
  // requests will slow down communication with the boiler. Each request may take up to 1 second,
  // so with all sensors enabled, it may take about half a minute before a change in setpoint
  // will be processed.
  void add_repeating_message(MessageId message_id) { this->repeating_messages_.insert(message_id); }

  // There are five status variables, which can either be set as a simple variable,
  // or using a switch. ch_enable and dhw_enable default to true, the others to false.
  bool ch_enable = true, dhw_enable = true, cooling_enable = false, otc_active = false, ch2_active = false;

  // Setters for the status variables
  void set_ch_enable(bool value) { this->ch_enable = value; }
  void set_dhw_enable(bool value) { this->dhw_enable = value; }
  void set_cooling_enable(bool value) { this->cooling_enable = value; }
  void set_otc_active(bool value) { this->otc_active = value; }
  void set_ch2_active(bool value) { this->ch2_active = value; }
  void set_sync_mode(bool sync_mode) { this->sync_mode_ = sync_mode; }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  void on_shutdown() override;
  void loop() override;
  void dump_config() override;
};

}  // namespace opentherm
}  // namespace esphome
