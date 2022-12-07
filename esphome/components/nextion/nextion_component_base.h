#pragma once
#include <utility>
#include <vector>
#include "esphome/core/defines.h"

namespace esphome {
namespace nextion {

enum NextionQueueType {
  NO_RESULT = 0,
  SENSOR = 1,
  BINARY_SENSOR = 2,
  SWITCH = 3,
  TEXT_SENSOR = 4,
  WAVEFORM_SENSOR = 5,
};

static const char *const NEXTION_QUEUE_TYPE_STRINGS[] = {"NO_RESULT", "SENSOR",      "BINARY_SENSOR",
                                                         "SWITCH",    "TEXT_SENSOR", "WAVEFORM_SENSOR"};

class NextionComponentBase;

class NextionQueue {
 public:
  virtual ~NextionQueue() = default;
  NextionComponentBase *component;
  uint32_t queue_time = 0;
};

class NextionComponentBase {
 public:
  virtual ~NextionComponentBase() = default;

  void set_variable_name(const std::string &variable_name, const std::string &variable_name_to_send = "") {
    variable_name_ = variable_name;
    if (variable_name_to_send.empty()) {
      variable_name_to_send_ = variable_name;
    } else {
      variable_name_to_send_ = variable_name_to_send;
    }
  }

  virtual void update_component_settings(){};
  virtual void update_component_settings(bool force_update){};

  virtual void update_component(){};
  virtual void process_sensor(const std::string &variable_name, int state){};
  virtual void process_touch(uint8_t page_id, uint8_t component_id, bool on){};
  virtual void process_text(const std::string &variable_name, const std::string &text_value){};
  virtual void process_bool(const std::string &variable_name, bool on){};

  virtual void set_state(float state){};
  virtual void set_state(float state, bool publish){};
  virtual void set_state(float state, bool publish, bool send_to_nextion){};

  virtual void set_state(bool state){};
  virtual void set_state(bool state, bool publish){};
  virtual void set_state(bool state, bool publish, bool send_to_nextion){};

  virtual void set_state(const std::string &state) {}
  virtual void set_state(const std::string &state, bool publish) {}
  virtual void set_state(const std::string &state, bool publish, bool send_to_nextion){};

  uint8_t get_component_id() { return this->component_id_; }
  void set_component_id(uint8_t component_id) { component_id_ = component_id; }

  uint8_t get_wave_channel_id() { return this->wave_chan_id_; }
  void set_wave_channel_id(uint8_t wave_chan_id) { this->wave_chan_id_ = wave_chan_id; }

  std::vector<uint8_t> get_wave_buffer() { return this->wave_buffer_; }
  size_t get_wave_buffer_size() { return this->wave_buffer_.size(); }

  std::string get_variable_name() { return this->variable_name_; }
  std::string get_variable_name_to_send() { return this->variable_name_to_send_; }
  virtual NextionQueueType get_queue_type() { return NextionQueueType::NO_RESULT; }
  virtual std::string get_queue_type_string() { return NEXTION_QUEUE_TYPE_STRINGS[this->get_queue_type()]; }
  virtual void set_state_from_int(int state_value, bool publish, bool send_to_nextion){};
  virtual void set_state_from_string(const std::string &state_value, bool publish, bool send_to_nextion){};
  virtual void send_state_to_nextion(){};
  bool get_needs_to_send_update() { return this->needs_to_send_update_; }
  uint8_t get_wave_chan_id() { return this->wave_chan_id_; }
  void set_wave_max_length(int wave_max_length) { this->wave_max_length_ = wave_max_length; }

 protected:
  std::string variable_name_;
  std::string variable_name_to_send_;

  uint8_t component_id_ = 0;
  uint8_t wave_chan_id_ = UINT8_MAX;
  std::vector<uint8_t> wave_buffer_;
  int wave_max_length_ = 255;

  bool needs_to_send_update_;
};
}  // namespace nextion
}  // namespace esphome
