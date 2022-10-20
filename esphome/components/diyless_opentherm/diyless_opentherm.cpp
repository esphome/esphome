#include "diyless_opentherm.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome {
namespace diyless_opentherm {

DiyLessOpenThermComponent *component;

void DiyLessOpenThermComponent::initialize(char pin_in, char pin_out) {
  ESP_LOGD(TAG, "Initialize connection with DIYLess board: in=%d, out=%d.", pin_in, pin_out);
  openTherm = new ihormelnyk::OpenTherm(pin_in, pin_out, false);
}

void DiyLessOpenThermComponent::setup() {
  component = this;
  openTherm->begin(handle_interrupt, response_callback);

  if (this->ch_enabled_) {
    this->ch_enabled_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_ch_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s CH", (enabled ? "Enabled" : "Disabled"));
        this->wanted_ch_enabled_ = enabled;
        this->process_status_();
      }
    });
  }
  if (this->dhw_enabled_) {
    this->dhw_enabled_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_dhw_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s DHW", (enabled ? "Enabled" : "Disabled"));
        this->wanted_dhw_enabled_ = enabled;
        this->process_status_();
      }
    });
  }
  if (this->cooling_enabled_) {
    this->cooling_enabled_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_cooling_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s cooling", (enabled ? "Enabled" : "Disabled"));
        this->wanted_cooling_enabled_ = enabled;
        this->process_status_();
      }
    });
  }
  if (this->ch_setpoint_temperature_) {
    this->ch_setpoint_temperature_->setup();
    this->ch_setpoint_temperature_->add_on_state_callback(
        [](float temperature) { ESP_LOGI(TAG, "Request updating CH setpoint to %f", temperature); });
  }
  if (this->dhw_setpoint_temperature_) {
    this->dhw_setpoint_temperature_->setup();
    this->dhw_setpoint_temperature_->add_on_state_callback(
        [](float temperature) { ESP_LOGI(TAG, "Request updating CH setpoint to %f", temperature); });
  }
}

void DiyLessOpenThermComponent::loop() {
  if (openTherm->is_ready() && !buffer_.empty()) {
    unsigned long request = buffer_.front();
    buffer_.pop();
    this->log_message(ESP_LOG_DEBUG, "Sending request", request);
    openTherm->send_request_async(request);
  }
  if (millis() - last_millis_ > 2000) {
    // The CH setpoint must be written at a fast interval or the boiler
    // might revert to a build-in default as a safety measure.
    last_millis_ = millis();
    this->enqueue_request_(openTherm->build_request(ihormelnyk::OpenThermMessageType::WRITE_DATA,
                                                    ihormelnyk::OpenThermMessageID::T_SET,
                                                    openTherm->temperature_to_data(this->ch_setpoint_temperature_->state)));
    if (this->confirmed_dhw_setpoint != this->dhw_setpoint_temperature_->state) {
      this->enqueue_request_(
          openTherm->build_request(ihormelnyk::OpenThermMessageType::WRITE_DATA, ihormelnyk::OpenThermMessageID::TDHW_SET,
                                   openTherm->temperature_to_data(this->dhw_setpoint_temperature_->state)));
    }
  }
  openTherm->process();
  yield();
}

void DiyLessOpenThermComponent::update() {
  this->enqueue_request_(
      openTherm->build_request(ihormelnyk::OpenThermMessageType::READ_DATA, ihormelnyk::OpenThermMessageID::TRET, 0));
  this->enqueue_request_(
      openTherm->build_request(ihormelnyk::OpenThermMessageType::READ_DATA, ihormelnyk::OpenThermMessageID::TBOILER, 0));
  this->enqueue_request_(openTherm->build_request(ihormelnyk::OpenThermMessageType::READ_DATA,
                                                  ihormelnyk::OpenThermMessageID::CH_PRESSURE, 0));
  this->enqueue_request_(openTherm->build_request(ihormelnyk::OpenThermMessageType::READ_DATA,
                                                  ihormelnyk::OpenThermMessageID::REL_MOD_LEVEL, 0));
  this->enqueue_request_(openTherm->build_request(ihormelnyk::OpenThermMessageType::READ_DATA,
                                                  ihormelnyk::OpenThermMessageID::RB_PFLAGS, 0));
  if (!this->dhw_min_max_read) {
    this->enqueue_request_(openTherm->build_request(ihormelnyk::OpenThermMessageType::READ_DATA,
                                                    ihormelnyk::OpenThermMessageID::TDHW_SET_UB_TDHW_SET_LB, 0));
  }
  if (!this->ch_min_max_read) {
    this->enqueue_request_(openTherm->build_request(ihormelnyk::OpenThermMessageType::READ_DATA,
                                                    ihormelnyk::OpenThermMessageID::MAX_T_SET_UB_MAX_T_SET_LB, 0));
  }
  this->process_status_();
}

void DiyLessOpenThermComponent::log_message(esp_log_level_t level, const char *pre_message, unsigned long message) {
  switch (level) {
    case ESP_LOG_DEBUG:
      ESP_LOGD(TAG, "%s: %s(%i, 0x%04hX)", pre_message, this->format_message_type_(message), openTherm->get_data_id(message),
               openTherm->get_u_int(message));
      break;
    default:
      ESP_LOGW(TAG, "%s: %s(%i, 0x%04hX)", pre_message, this->format_message_type_(message), openTherm->get_data_id(message),
               openTherm->get_u_int(message));
  }
}

void DiyLessOpenThermComponent::publish_sensor_state(sensor::Sensor *sensor, float state) {
  if (sensor) {
    sensor->publish_state(state);
  }
}

void DiyLessOpenThermComponent::publish_binary_sensor_state(binary_sensor::BinarySensor *sensor, bool state) {
  if (sensor) {
    sensor->publish_state(state);
  }
}

void DiyLessOpenThermComponent::process_status_() {
  // Fields: CH enabled | DHW enabled | cooling | outside temperature compensation | central heating 2
  unsigned int data =
      this->wanted_ch_enabled_ | (this->wanted_dhw_enabled_ << 1) | (this->wanted_cooling_enabled_ << 2) | (false << 3) | (false << 4);
  data <<= 8;
  this->enqueue_request_(openTherm->build_request(ihormelnyk::OpenThermMessageType::READ_DATA,
                                                  ihormelnyk::OpenThermMessageID::STATUS, data));
}

void DiyLessOpenThermComponent::enqueue_request_(unsigned long request) {
  if (this->buffer_.size() > 20) {
    this->log_message(ESP_LOG_WARN, "Queue full. Discarded request", request);
  } else {
    this->buffer_.push(request);
    this->log_message(ESP_LOG_DEBUG, "Enqueued request", request);
  }
}

const char *DiyLessOpenThermComponent::format_message_type_(unsigned long message) {
  return openTherm->message_type_to_string(openTherm->get_message_type(message));
}

}  // namespace diyless_opentherm
}  // namespace esphome
