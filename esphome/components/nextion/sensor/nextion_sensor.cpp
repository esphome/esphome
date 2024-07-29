#include "nextion_sensor.h"
#include "esphome/core/util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nextion {

static const char *const TAG = "nextion_sensor";

void NextionSensor::process_sensor(const std::string &variable_name, int state) {
  if (!this->nextion_->is_setup())
    return;

  if (this->wave_chan_id_ == UINT8_MAX && this->variable_name_ == variable_name) {
    this->publish_state(state);
    ESP_LOGD(TAG, "Processed sensor \"%s\" state %d", variable_name.c_str(), state);
  }
}

void NextionSensor::add_to_wave_buffer(float state) {
  this->needs_to_send_update_ = true;

  int wave_state = (int) ((state / (float) this->wave_maxvalue_) * 100);

  wave_buffer_.push_back(wave_state);

  if (this->wave_buffer_.size() > (size_t) this->wave_max_length_) {
    this->wave_buffer_.erase(this->wave_buffer_.begin());
  }
}

void NextionSensor::update() {
  if (!this->nextion_->is_setup() || this->nextion_->is_updating())
    return;

  if (this->wave_chan_id_ == UINT8_MAX) {
    this->nextion_->add_to_get_queue(this);
  } else {
    if (this->send_last_value_) {
      this->add_to_wave_buffer(this->last_value_);
    }

    this->wave_update_();
  }
}

void NextionSensor::set_state(float state, bool publish, bool send_to_nextion) {
  if (!this->nextion_->is_setup() || this->nextion_->is_updating())
    return;

  if (std::isnan(state))
    return;

  if (this->wave_chan_id_ == UINT8_MAX) {
    if (send_to_nextion) {
      if (this->nextion_->is_sleeping() || !this->visible_) {
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
  } else {
    if (this->send_last_value_) {
      this->last_value_ = state;  // Update will handle setting the buffer
    } else {
      this->add_to_wave_buffer(state);
    }
  }

  float published_state = state;
  if (this->wave_chan_id_ == UINT8_MAX) {
    if (publish) {
      if (this->precision_ > 0) {
        double to_multiply = pow(10, -this->precision_);
        published_state = (float) (state * to_multiply);
      }

      this->publish_state(published_state);
    } else {
      this->raw_state = state;
      this->state = state;
      this->has_state_ = true;
    }
  }
  this->update_component_settings();

  ESP_LOGN(TAG, "Wrote state for sensor \"%s\" state %lf", this->variable_name_.c_str(), published_state);
}

void NextionSensor::wave_update_() {
  if (this->nextion_->is_sleeping() || this->wave_buffer_.empty()) {
    return;
  }

#ifdef NEXTION_PROTOCOL_LOG
  size_t buffer_to_send =
      this->wave_buffer_.size() < 255 ? this->wave_buffer_.size() : 255;  // ADDT command can only send 255

  ESP_LOGN(TAG, "wave_update send %zu of %zu value(s) to wave nextion component id %d and wave channel id %d",
           buffer_to_send, this->wave_buffer_.size(), this->component_id_, this->wave_chan_id_);
#endif

  this->nextion_->add_addt_command_to_queue(this);
}

}  // namespace nextion
}  // namespace esphome
