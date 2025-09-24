#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../nextion_component.h"
#include "../nextion_base.h"

namespace esphome {
namespace nextion {
class NextionSensor;

class NextionSensor : public NextionComponent, public sensor::Sensor, public PollingComponent {
 public:
  NextionSensor(NextionBase *nextion) { this->nextion_ = nextion; }
  void send_state_to_nextion() override { this->set_state(this->state, false, true); };

  void update_component() override { this->update(); }
  void update() override;
  void add_to_wave_buffer(float state);
  void set_precision(uint8_t precision) { this->precision_ = precision; }
  void set_component_id(uint8_t component_id) { component_id_ = component_id; }
  void set_wave_channel_id(uint8_t wave_chan_id) { this->wave_chan_id_ = wave_chan_id; }
  void set_wave_max_value(uint32_t wave_maxvalue) { this->wave_maxvalue_ = wave_maxvalue; }
  void process_sensor(const std::string &variable_name, int state) override;

  void set_state(float state) override { this->set_state(state, true, true); }
  void set_state(float state, bool publish) override { this->set_state(state, publish, true); }
  void set_state(float state, bool publish, bool send_to_nextion) override;

  void set_waveform_send_last_value(bool send_last_value) { this->send_last_value_ = send_last_value; }
  uint8_t get_wave_chan_id() { return this->wave_chan_id_; }
  void set_wave_max_length(int wave_max_length) { this->wave_max_length_ = wave_max_length; }
  NextionQueueType get_queue_type() override {
    return this->wave_chan_id_ == UINT8_MAX ? NextionQueueType::SENSOR : NextionQueueType::WAVEFORM_SENSOR;
  }
  void set_state_from_string(const std::string &state_value, bool publish, bool send_to_nextion) override {}
  void set_state_from_int(int state_value, bool publish, bool send_to_nextion) override {
    this->set_state(state_value, publish, send_to_nextion);
  }

 protected:
  uint8_t precision_ = 0;
  uint32_t wave_maxvalue_ = 255;

  float last_value_ = 0;
  bool send_last_value_ = true;
  void wave_update_();
};
}  // namespace nextion
}  // namespace esphome
