#include "nextion_sensor.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"

namespace esphome::nextion {

static const char *const TAG = "nextion_sensor";

void NextionSensor::process_sensor(const std::string &variable_name, int state) {
  if (!this->nextion_->is_setup())
    return;

#ifdef USE_NEXTION_WAVEFORM
  if (this->wave_chan_id_ == UINT8_MAX && this->variable_name_ == variable_name)
#else   // USE_NEXTION_WAVEFORM
  if (this->variable_name_ == variable_name)
#endif  // USE_NEXTION_WAVEFORM
  {
    this->publish_state(state);
    ESP_LOGD(TAG, "Sensor: %s=%d", variable_name.c_str(), state);
  }
}

#ifdef USE_NEXTION_WAVEFORM
void NextionSensor::add_to_wave_buffer(float state) {
  this->needs_to_send_update_ = true;
  int wave_state = (int) ((state / (float) this->wave_maxvalue_) * 100);
  this->wave_buffer_.push_back(wave_state);
  if (this->wave_buffer_.size() > (size_t) this->wave_max_length_) {
    this->wave_buffer_.erase(this->wave_buffer_.begin());
  }
}
#endif  // USE_NEXTION_WAVEFORM

void NextionSensor::update() {
  if (!this->nextion_->is_setup() || this->nextion_->is_updating())
    return;

#ifdef USE_NEXTION_WAVEFORM
  if (this->wave_chan_id_ == UINT8_MAX) {
    this->nextion_->add_to_get_queue(this);
  } else {
    if (this->send_last_value_) {
      this->add_to_wave_buffer(this->last_value_);
    }
    this->wave_update_();
  }
#else   // USE_NEXTION_WAVEFORM
  this->nextion_->add_to_get_queue(this);
#endif  // USE_NEXTION_WAVEFORM
}

void NextionSensor::set_state(float state, bool publish, bool send_to_nextion) {
  if (!this->nextion_->is_setup() || this->nextion_->is_updating())
    return;

  if (std::isnan(state))
    return;

#ifdef USE_NEXTION_WAVEFORM
  if (this->wave_chan_id_ != UINT8_MAX) {
    // Waveform sensor — buffer the value, don't send directly.
    if (this->send_last_value_) {
      this->last_value_ = state;  // Update will handle setting the buffer
    } else {
      this->add_to_wave_buffer(state);
    }
    this->update_component_settings();
    return;
  }
#endif  // USE_NEXTION_WAVEFORM

  if (send_to_nextion) {
    if (this->nextion_->is_sleeping() || !this->component_flags_.visible) {
      this->needs_to_send_update_ = true;
    } else {
      this->needs_to_send_update_ = false;
      if (this->precision_ > 0) {
        double to_multiply = pow(10, this->precision_);
        int state_value = (int) (state * to_multiply);
        this->nextion_->add_no_result_to_queue_with_set(this, (int) state_value);
      } else {
        this->nextion_->add_no_result_to_queue_with_set(this, (int) state);
      }
    }
  }

  float published_state = state;
  if (publish) {
    if (this->precision_ > 0) {
      double to_multiply = pow(10, -this->precision_);
      published_state = (float) (state * to_multiply);
    }
    this->publish_state(published_state);
  }
  this->update_component_settings();

  ESP_LOGN(TAG, "Write: %s=%lf", this->variable_name_.c_str(), published_state);
}

#ifdef USE_NEXTION_WAVEFORM
void NextionSensor::wave_update_() {
  if (this->nextion_->is_sleeping() || this->wave_buffer_.empty()) {
    return;
  }
#ifdef NEXTION_PROTOCOL_LOG
  size_t buffer_to_send =
      this->wave_buffer_.size() < 255 ? this->wave_buffer_.size() : 255;  // ADDT command can only send 255
  ESP_LOGN(TAG, "Wave update: %zu/%zu vals to comp %d ch %d", buffer_to_send, this->wave_buffer_.size(),
           this->component_id_, this->wave_chan_id_);
#endif  // NEXTION_PROTOCOL_LOG
  this->nextion_->add_addt_command_to_queue(this);
}
#endif  // USE_NEXTION_WAVEFORM

}  // namespace esphome::nextion
