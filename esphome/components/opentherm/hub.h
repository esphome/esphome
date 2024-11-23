#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <vector>

#include "opentherm.h"

#ifdef OPENTHERM_USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#ifdef OPENTHERM_USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef OPENTHERM_USE_SWITCH
#include "esphome/components/opentherm/switch/switch.h"
#endif

#ifdef OPENTHERM_USE_OUTPUT
#include "esphome/components/opentherm/output/output.h"
#endif

#ifdef OPENTHERM_USE_NUMBER
#include "esphome/components/opentherm/number/number.h"
#endif

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "opentherm_macros.h"

namespace esphome {
namespace opentherm {

// OpenTherm component for ESPHome
class OpenthermHub : public Component {
 protected:
  // Communication pins for the OpenTherm interface
  InternalGPIOPin *in_pin_, *out_pin_;
  // The OpenTherm interface
  std::unique_ptr<OpenTherm> opentherm_;

  OPENTHERM_SENSOR_LIST(OPENTHERM_DECLARE_SENSOR, )

  OPENTHERM_BINARY_SENSOR_LIST(OPENTHERM_DECLARE_BINARY_SENSOR, )

  OPENTHERM_SWITCH_LIST(OPENTHERM_DECLARE_SWITCH, )

  OPENTHERM_NUMBER_LIST(OPENTHERM_DECLARE_NUMBER, )

  OPENTHERM_OUTPUT_LIST(OPENTHERM_DECLARE_OUTPUT, )

  OPENTHERM_INPUT_SENSOR_LIST(OPENTHERM_DECLARE_INPUT_SENSOR, )

  // The set of initial messages to send on starting communication with the boiler
  std::vector<MessageId> initial_messages_;
  // and the repeating messages which are sent repeatedly to update various sensors
  // and boiler parameters (like the setpoint).
  std::vector<MessageId> repeating_messages_;
  // Indicates if we are still working on the initial requests or not
  bool sending_initial_ = true;
  // Index for the current request in one of the _requests sets.
  std::vector<MessageId>::const_iterator current_message_iterator_;

  uint32_t last_conversation_start_ = 0;
  uint32_t last_conversation_end_ = 0;
  OperationMode last_mode_ = IDLE;
  OpenthermData last_request_;

  // Synchronous communication mode prevents other components from disabling interrupts while
  // we are talking to the boiler. Enable if you experience random intermittent invalid response errors.
  // Very likely to happen while using Dallas temperature sensors.
  bool sync_mode_ = false;

  float opentherm_version_ = 0.0f;

  // Create OpenTherm messages based on the message id
  OpenthermData build_request_(MessageId request_id) const;
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

  OPENTHERM_SENSOR_LIST(OPENTHERM_SET_SENSOR, )

  OPENTHERM_BINARY_SENSOR_LIST(OPENTHERM_SET_BINARY_SENSOR, )

  OPENTHERM_SWITCH_LIST(OPENTHERM_SET_SWITCH, )

  OPENTHERM_NUMBER_LIST(OPENTHERM_SET_NUMBER, )

  OPENTHERM_OUTPUT_LIST(OPENTHERM_SET_OUTPUT, )

  OPENTHERM_INPUT_SENSOR_LIST(OPENTHERM_SET_INPUT_SENSOR, )

  // Add a request to the vector of initial requests
  void add_initial_message(MessageId message_id) { this->initial_messages_.push_back(message_id); }
  // Add a request to the set of repeating requests. Note that a large number of repeating
  // requests will slow down communication with the boiler. Each request may take up to 1 second,
  // so with all sensors enabled, it may take about half a minute before a change in setpoint
  // will be processed.
  void add_repeating_message(MessageId message_id) { this->repeating_messages_.push_back(message_id); }

  // There are seven status variables, which can either be set as a simple variable,
  // or using a switch. ch_enable and dhw_enable default to true, the others to false.
  bool ch_enable = true, dhw_enable = true, cooling_enable = false, otc_active = false, ch2_active = false,
       summer_mode_active = false, dhw_block = false;

  // Setters for the status variables
  void set_ch_enable(bool value) { this->ch_enable = value; }
  void set_dhw_enable(bool value) { this->dhw_enable = value; }
  void set_cooling_enable(bool value) { this->cooling_enable = value; }
  void set_otc_active(bool value) { this->otc_active = value; }
  void set_ch2_active(bool value) { this->ch2_active = value; }
  void set_summer_mode_active(bool value) { this->summer_mode_active = value; }
  void set_dhw_block(bool value) { this->dhw_block = value; }
  void set_sync_mode(bool sync_mode) { this->sync_mode_ = sync_mode; }
  void set_opentherm_version(float value) { this->opentherm_version_ = value; }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  void on_shutdown() override;
  void loop() override;
  void dump_config() override;
};

}  // namespace opentherm
}  // namespace esphome
