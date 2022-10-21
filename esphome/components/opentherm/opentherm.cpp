/*
 * Notice:
 *   The code for actual communication with OpenTherm is heavily based on Ihor Melnyk's
 *   OpenTherm Library at https://github.com/ihormelnyk/opentherm_library, which is MIT licensed.
 */

#include "opentherm.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome {
namespace opentherm {

// Helpers

static const char *const TAG = "opentherm";

// All public method implementations

void OpenThermComponent::set_pins(InternalGPIOPin *read_pin, InternalGPIOPin *write_pin) {
  this->read_pin_ = read_pin;
  this->write_pin_ = write_pin;
}

void OpenThermComponent::setup() {
  this->read_pin_->setup();
  this->isr_read_pin_ = this->read_pin_->to_isr();
  this->read_pin_->attach_interrupt(&OpenThermComponent::handle_interrupt, this, gpio::INTERRUPT_ANY_EDGE);
  this->write_pin_->setup();

  this->active_boiler_();
  this->status_ = OpenThermStatus::READY;

  if (this->ch_enabled_switch_) {
    this->ch_enabled_switch_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_ch_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s CH", (enabled ? "Enabled" : "Disabled"));
        this->wanted_ch_enabled_ = enabled;
        this->set_boiler_status_();
      }
    });
  }
  if (this->dhw_enabled_switch_) {
    this->dhw_enabled_switch_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_dhw_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s DHW", (enabled ? "Enabled" : "Disabled"));
        this->wanted_dhw_enabled_ = enabled;
        this->set_boiler_status_();
      }
    });
  }
  if (this->cooling_enabled_switch_) {
    this->cooling_enabled_switch_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_cooling_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s cooling", (enabled ? "Enabled" : "Disabled"));
        this->wanted_cooling_enabled_ = enabled;
        this->set_boiler_status_();
      }
    });
  }
  if (this->ch_setpoint_temperature_number_) {
    this->ch_setpoint_temperature_number_->setup();
    this->ch_setpoint_temperature_number_->add_on_state_callback(
        [](float temperature) { ESP_LOGI(TAG, "Request updating CH setpoint to %f", temperature); });
  }
  if (this->dhw_setpoint_temperature_number_) {
    this->dhw_setpoint_temperature_number_->setup();
    this->dhw_setpoint_temperature_number_->add_on_state_callback(
        [](float temperature) { ESP_LOGI(TAG, "Request updating CH setpoint to %f", temperature); });
  }
}

void OpenThermComponent::loop() {
  if (this->status_ == OpenThermStatus::READY && !buffer_.empty()) {
    uint32_t request = buffer_.front();
    buffer_.pop();
    this->log_message_(0, "Sending request", request);
    this->send_request_async_(request);
  }
  if (millis() - last_millis_ > 2000) {
    // The CH setpoint must be written at a fast interval or the boiler
    // might revert to a build-in default as a safety measure.
    last_millis_ = millis();
    this->enqueue_request_(
        this->build_request_(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::T_SET,
                             this->temperature_to_data_(this->ch_setpoint_temperature_number_->state)));
    if (this->confirmed_dhw_setpoint_ != this->dhw_setpoint_temperature_number_->state) {
      this->enqueue_request_(
          this->build_request_(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TDHW_SET,
                               this->temperature_to_data_(this->dhw_setpoint_temperature_number_->state)));
    }
  }
  this->process_();
  yield();
}

void OpenThermComponent::update() {
  this->enqueue_request_(this->build_request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::TRET, 0));
  this->enqueue_request_(this->build_request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::TBOILER, 0));
  this->enqueue_request_(this->build_request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::CH_PRESSURE, 0));
  this->enqueue_request_(this->build_request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::REL_MOD_LEVEL, 0));
  this->enqueue_request_(this->build_request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::RB_PFLAGS, 0));
  if (!this->dhw_min_max_read_) {
    this->enqueue_request_(
        this->build_request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::TDHW_SET_UB_TDHW_SET_LB, 0));
  }
  if (!this->ch_min_max_read_) {
    this->enqueue_request_(
        this->build_request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::MAX_T_SET_UB_MAX_T_SET_LB, 0));
  }
  this->set_boiler_status_();
}

void IRAM_ATTR OpenThermComponent::handle_interrupt(OpenThermComponent *component) {
  if (component->status_ == OpenThermStatus::READY) {
    return;
  }

  unsigned long new_ts = micros();
  bool pin_state = component->isr_read_pin_.digital_read();
  if (component->status_ == OpenThermStatus::RESPONSE_WAITING) {
    if (pin_state) {
      component->status_ = OpenThermStatus::RESPONSE_START_BIT;
      component->response_timestamp_ = new_ts;
    } else {
      component->status_ = OpenThermStatus::RESPONSE_INVALID;
      component->response_timestamp_ = new_ts;
    }
  } else if (component->status_ == OpenThermStatus::RESPONSE_START_BIT) {
    if ((new_ts - component->response_timestamp_ < 750) && !pin_state) {
      component->status_ = OpenThermStatus::RESPONSE_RECEIVING;
      component->response_timestamp_ = new_ts;
      component->response_bit_index_ = 0;
    } else {
      component->status_ = OpenThermStatus::RESPONSE_INVALID;
      component->response_timestamp_ = new_ts;
    }
  } else if (component->status_ == OpenThermStatus::RESPONSE_RECEIVING) {
    if ((new_ts - component->response_timestamp_) > 750) {
      if (component->response_bit_index_ < 32) {
        component->response_ = (component->response_ << 1) | !pin_state;
        component->response_timestamp_ = new_ts;
        component->response_bit_index_++;
      } else {  // stop bit
        component->status_ = OpenThermStatus::RESPONSE_READY;
        component->response_timestamp_ = new_ts;
      }
    }
  }
}

// Private

void OpenThermComponent::log_message_(uint8_t level, const char *pre_message, uint32_t message) {
  switch (level) {
    case 0:
      ESP_LOGD(TAG, "%s: %s(%i, 0x%04hX)", pre_message, this->format_message_type_(message),
               this->get_data_id_(message), this->get_uint16_(message));
      break;
    default:
      ESP_LOGW(TAG, "%s: %s(%i, 0x%04hX)", pre_message, this->format_message_type_(message),
               this->get_data_id_(message), this->get_uint16_(message));
  }
}

void OpenThermComponent::active_boiler_() {
  this->write_pin_->digital_write(true);
  delay(1000);
}

void OpenThermComponent::send_bit_(bool high) {
  if (high) {
    this->write_pin_->digital_write(false);
  } else {
    this->write_pin_->digital_write(true);
  }
  delayMicroseconds(500);
  if (high) {
    this->write_pin_->digital_write(true);
  } else {
    this->write_pin_->digital_write(false);
  }
  delayMicroseconds(500);
}

bool OpenThermComponent::send_request_async_(uint32_t request) {
  const bool ready = this->status_ == OpenThermStatus::READY;

  if (!ready)
    return false;

  this->status_ = OpenThermStatus::REQUEST_SENDING;
  this->response_ = 0;
  this->response_status_ = OpenThermResponseStatus::NONE;

  this->send_bit_(true);  // Start bit
  for (int i = 31; i >= 0; i--) {
    this->send_bit_(request >> i & 1);
  }
  this->send_bit_(true);  // Stop bit
  this->write_pin_->digital_write(true);

  this->status_ = OpenThermStatus::RESPONSE_WAITING;
  this->response_timestamp_ = micros();
  return true;
}

void OpenThermComponent::process_() {
  OpenThermStatus st = this->status_;
  uint32_t ts = this->response_timestamp_;

  if (st == OpenThermStatus::READY) {
    return;
  }

  uint32_t new_ts = micros();
  if (st != OpenThermStatus::NOT_INITIALIZED && st != OpenThermStatus::DELAY && (new_ts - ts) > 1000000) {
    this->status_ = OpenThermStatus::READY;
    this->response_status_ = OpenThermResponseStatus::TIMEOUT;
    this->process_response_(this->response_, this->response_status_);
  } else if (st == OpenThermStatus::RESPONSE_INVALID) {
    this->status_ = OpenThermStatus::DELAY;
    this->response_status_ = OpenThermResponseStatus::INVALID;
    this->process_response_(this->response_, this->response_status_);
  } else if (st == OpenThermStatus::RESPONSE_READY) {
    this->status_ = OpenThermStatus::DELAY;
    this->response_status_ =
        this->is_valid_response_(this->response_) ? OpenThermResponseStatus::SUCCESS : OpenThermResponseStatus::INVALID;
    this->process_response_(this->response_, this->response_status_);
  } else if (st == OpenThermStatus::DELAY) {
    if ((new_ts - ts) > 100000) {
      this->status_ = OpenThermStatus::READY;
    }
  }
}

bool OpenThermComponent::parity_(uint32_t frame)  // odd parity
{
  uint8_t p = 0;
  while (frame > 0) {
    if (frame & 1)
      p++;
    frame = frame >> 1;
  }
  return (p & 1);
}

OpenThermMessageType OpenThermComponent::get_message_type_(uint32_t message) {
  OpenThermMessageType msg_type = static_cast<OpenThermMessageType>((message >> 28) & 7);
  return msg_type;
}

OpenThermMessageID OpenThermComponent::get_data_id_(uint32_t frame) {
  return (OpenThermMessageID)((frame >> 16) & 0xFF);
}

uint32_t OpenThermComponent::build_request_(OpenThermMessageType type, OpenThermMessageID id, unsigned int data) {
  uint32_t request = data;
  if (type == OpenThermMessageType::WRITE_DATA) {
    request |= 1ul << 28;
  }
  request |= ((uint32_t) id) << 16;
  if (this->parity_(request))
    request |= (1ul << 31);
  return request;
}

uint32_t OpenThermComponent::build_response_(OpenThermMessageType type, OpenThermMessageID id, unsigned int data) {
  uint32_t response = data;
  response |= type << 28;
  response |= ((uint32_t) id) << 16;
  if (this->parity_(response))
    response |= (1ul << 31);
  return response;
}

bool OpenThermComponent::is_valid_response_(uint32_t response) {
  if (this->parity_(response))
    return false;
  uint8_t msg_type = (response << 1) >> 29;
  return msg_type == READ_ACK || msg_type == WRITE_ACK;
}

const char *OpenThermComponent::message_type_to_string_(OpenThermMessageType message_type) {
  switch (message_type) {
    case READ_DATA:
      return "READ_DATA";
    case WRITE_DATA:
      return "WRITE_DATA";
    case INVALID_DATA:
      return "INVALID_DATA";
    case RESERVED:
      return "RESERVED";
    case READ_ACK:
      return "READ_ACK";
    case WRITE_ACK:
      return "WRITE_ACK";
    case DATA_INVALID:
      return "DATA_INVALID";
    case UNKNOWN_DATA_ID:
      return "UNKNOWN_DATA_ID";
    default:
      return "UNKNOWN";
  }
}

void OpenThermComponent::process_response_(uint32_t response, OpenThermResponseStatus response_status) {
  if (response_status == OpenThermResponseStatus::SUCCESS) {
    this->log_message_(0, "Received response", response);
    switch (this->get_data_id_(response)) {
      case OpenThermMessageID::STATUS:
        this->publish_binary_sensor_state_(this->ch_active_binary_sensor_, this->is_central_heating_active_(response));
        this->publish_binary_sensor_state_(this->dhw_active_binary_sensor_, this->is_hot_water_active_(response));
        this->publish_binary_sensor_state_(this->cooling_active_binary_sensor_, this->is_cooling_active_(response));
        this->publish_binary_sensor_state_(this->flame_active_binary_sensor_, this->is_flame_on_(response));
        this->publish_binary_sensor_state_(this->fault_binary_sensor_, this->is_fault_(response));
        this->publish_binary_sensor_state_(this->diagnostic_binary_sensor_, this->is_diagnostic_(response));
        break;
      case OpenThermMessageID::TRET:
        this->publish_sensor_state_(this->return_temperature_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::TBOILER:
        this->publish_sensor_state_(this->boiler_temperature_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::CH_PRESSURE:
        this->publish_sensor_state_(this->pressure_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::REL_MOD_LEVEL:
        this->publish_sensor_state_(this->modulation_sensor_, this->get_float_(response));
        break;
      case OpenThermMessageID::TDHW_SET_UB_TDHW_SET_LB:
        this->dhw_min_max_read_ = true;
        this->publish_sensor_state_(this->dhw_max_temperature_sensor_, response >> 8 & 0xFF);
        this->publish_sensor_state_(this->dhw_min_temperature_sensor_, response & 0xFF);
        break;
      case OpenThermMessageID::MAX_T_SET_UB_MAX_T_SET_LB:
        this->ch_min_max_read_ = true;
        this->publish_sensor_state_(this->ch_max_temperature_sensor_, response >> 8 & 0xFF);
        this->publish_sensor_state_(this->ch_min_temperature_sensor_, response & 0xFF);
        break;
      case OpenThermMessageID::TDHW_SET:
        if (this->get_message_type_(response) == OpenThermMessageType::WRITE_ACK) {
          this->confirmed_dhw_setpoint_ = this->get_float_(response);
        }
        break;
      default:
        break;
    }
  } else if (response_status == OpenThermResponseStatus::NONE) {
    ESP_LOGW(TAG, "OpenTherm is not initialized");
  } else if (response_status == OpenThermResponseStatus::TIMEOUT) {
    ESP_LOGW(TAG, "Request timeout");
  } else if (response_status == OpenThermResponseStatus::INVALID) {
    this->log_message_(2, "Received invalid response", response);
  }
}

void OpenThermComponent::publish_sensor_state_(sensor::Sensor *sensor, float state) {
  if (sensor) {
    sensor->publish_state(state);
  }
}

void OpenThermComponent::publish_binary_sensor_state_(binary_sensor::BinarySensor *sensor, bool state) {
  if (sensor) {
    sensor->publish_state(state);
  }
}

void OpenThermComponent::set_boiler_status_() {
  // Fields: CH enabled | DHW enabled | cooling | outside temperature compensation | central heating 2
  unsigned int data = this->wanted_ch_enabled_ | (this->wanted_dhw_enabled_ << 1) |
                      (this->wanted_cooling_enabled_ << 2) | (false << 3) | (false << 4);
  data <<= 8;
  this->enqueue_request_(this->build_request_(OpenThermMessageType::READ_DATA, OpenThermMessageID::STATUS, data));
}

void OpenThermComponent::enqueue_request_(uint32_t request) {
  if (this->buffer_.size() > 20) {
    this->log_message_(2, "Queue full. Discarded request", request);
  } else {
    this->buffer_.push(request);
    this->log_message_(0, "Enqueued request", request);
  }
}

const char *OpenThermComponent::format_message_type_(uint32_t message) {
  return this->message_type_to_string_(this->get_message_type_(message));
}

}  // namespace opentherm
}  // namespace esphome
