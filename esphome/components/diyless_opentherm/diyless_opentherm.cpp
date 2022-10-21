#include "diyless_opentherm.h"
#include "esphome/core/log.h"
#include <vector>

namespace esphome {
namespace opentherm {

// Helpers

static const char *const TAG = "opentherm";
OpenThermComponent *component;

void IRAM_ATTR forward_interrupt() {
  component->handle_interrupt();
}

// All public method implementations

void OpenThermComponent::set_pins(char pin_in, char pin_out) {
  this->in_pin_ = pin_in;
  this->out_pin_ = pin_out;
}

void OpenThermComponent::setup() {
  component = this;
  pinMode(this->in_pin_, INPUT);
  pinMode(this->out_pin_, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(this->in_pin_), forward_interrupt, CHANGE);

  this->active_boiler();
  this->status = OpenThermStatus::READY;

  if (this->ch_enabled_switch_) {
    this->ch_enabled_switch_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_ch_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s CH", (enabled ? "Enabled" : "Disabled"));
        this->wanted_ch_enabled_ = enabled;
        this->process_status();
      }
    });
  }
  if (this->dhw_enabled_switch_) {
    this->dhw_enabled_switch_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_dhw_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s DHW", (enabled ? "Enabled" : "Disabled"));
        this->wanted_dhw_enabled_ = enabled;
        this->process_status();
      }
    });
  }
  if (this->cooling_enabled_switch_) {
    this->cooling_enabled_switch_->add_on_state_callback([this](bool enabled) {
      if (this->wanted_cooling_enabled_ != enabled) {
        ESP_LOGI(TAG, "%s cooling", (enabled ? "Enabled" : "Disabled"));
        this->wanted_cooling_enabled_ = enabled;
        this->process_status();
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
  if (this->is_ready() && !buffer_.empty()) {
    unsigned long request = buffer_.front();
    buffer_.pop();
    this->log_message(ESP_LOG_DEBUG, "Sending request", request);
    this->send_request_async(request);
  }
  if (millis() - last_millis_ > 2000) {
    // The CH setpoint must be written at a fast interval or the boiler
    // might revert to a build-in default as a safety measure.
    last_millis_ = millis();
    this->enqueue_request(
        this->build_request(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::T_SET,
                            this->temperature_to_data(this->ch_setpoint_temperature_number_->state)));
    if (this->confirmed_dhw_setpoint != this->dhw_setpoint_temperature_number_->state) {
      this->enqueue_request(this->build_request(
          OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TDHW_SET,
          this->temperature_to_data(this->dhw_setpoint_temperature_number_->state)));
    }
  }
  this->process();
  yield();
}

void OpenThermComponent::update() {
  this->enqueue_request(
      this->build_request(OpenThermMessageType::READ_DATA, OpenThermMessageID::TRET, 0));
  this->enqueue_request(this->build_request(OpenThermMessageType::READ_DATA,
                                             OpenThermMessageID::TBOILER, 0));
  this->enqueue_request(this->build_request(OpenThermMessageType::READ_DATA,
                                             OpenThermMessageID::CH_PRESSURE, 0));
  this->enqueue_request(this->build_request(OpenThermMessageType::READ_DATA,
                                             OpenThermMessageID::REL_MOD_LEVEL, 0));
  this->enqueue_request(this->build_request(OpenThermMessageType::READ_DATA,
                                             OpenThermMessageID::RB_PFLAGS, 0));
  if (!this->dhw_min_max_read) {
    this->enqueue_request(this->build_request(OpenThermMessageType::READ_DATA,
                                               OpenThermMessageID::TDHW_SET_UB_TDHW_SET_LB, 0));
  }
  if (!this->ch_min_max_read) {
    this->enqueue_request(this->build_request(OpenThermMessageType::READ_DATA,
                                               OpenThermMessageID::MAX_T_SET_UB_MAX_T_SET_LB, 0));
  }
  this->process_status();
}

// Private

void OpenThermComponent::log_message(esp_log_level_t level, const char *pre_message, unsigned long message) {
  switch (level) {
    case ESP_LOG_DEBUG:
      ESP_LOGD(TAG, "%s: %s(%i, 0x%04hX)", pre_message, this->format_message_type(message),
               this->get_data_id(message), this->get_u_int(message));
      break;
    default:
      ESP_LOGW(TAG, "%s: %s(%i, 0x%04hX)", pre_message, this->format_message_type(message),
               this->get_data_id(message), this->get_u_int(message));
  }
}

// TODO: ---- clean

bool IRAM_ATTR OpenThermComponent::is_ready() { return this->status == OpenThermStatus::READY; }

int IRAM_ATTR OpenThermComponent::read_state() { return digitalRead(this->in_pin_); }

void OpenThermComponent::set_active_state() { digitalWrite(this->out_pin_, LOW); }

void OpenThermComponent::set_idle_state() { digitalWrite(this->out_pin_, HIGH); }

void OpenThermComponent::active_boiler() {
  this->set_idle_state();
  delay(1000);
}

void OpenThermComponent::send_bit(bool high) {
  if (high) {
    this->set_active_state();
  } else {
    this->set_idle_state();
  }
  delayMicroseconds(500);
  if (high) {
    this->set_idle_state();
  } else {
    this->set_active_state();
  }
  delayMicroseconds(500);
}

bool OpenThermComponent::send_request_async(unsigned long request) {
  noInterrupts();
  const bool ready = this->is_ready();
  interrupts();

  if (!ready)
    return false;

  this->status = OpenThermStatus::REQUEST_SENDING;
  this->response_ = 0;
  this->response_status_ = OpenThermResponseStatus::NONE;

  this->send_bit(HIGH);  // Start bit
  for (int i = 31; i >= 0; i--) {
    this->send_bit(bitRead(request, i));
  }
  this->send_bit(HIGH);  // Stop bit
  this->set_idle_state();

  this->status = OpenThermStatus::RESPONSE_WAITING;
  this->response_timestamp_ = micros();
  return true;
}

bool OpenThermComponent::send_response(unsigned long request) {
  this->status = OpenThermStatus::REQUEST_SENDING;
  this->response_ = 0;
  this->response_status_ = OpenThermResponseStatus::NONE;

  this->send_bit(HIGH);  // Start bit
  for (int i = 31; i >= 0; i--) {
    this->send_bit(bitRead(request, i));
  }
  this->send_bit(HIGH);  // Stop bit
  this->set_idle_state();
  this->status = OpenThermStatus::READY;
  return true;
}

void IRAM_ATTR OpenThermComponent::handle_interrupt() {
  if (this->is_ready()) {
    if (this->is_slave_ && this->read_state() == HIGH) {
      this->status = OpenThermStatus::RESPONSE_WAITING;
    } else {
      return;
    }
  }

  unsigned long new_ts = micros();
  if (this->status == OpenThermStatus::RESPONSE_WAITING) {
    if (this->read_state() == HIGH) {
      this->status = OpenThermStatus::RESPONSE_START_BIT;
      this->response_timestamp_ = new_ts;
    } else {
      this->status = OpenThermStatus::RESPONSE_INVALID;
      this->response_timestamp_ = new_ts;
    }
  } else if (this->status == OpenThermStatus::RESPONSE_START_BIT) {
    if ((new_ts - this->response_timestamp_ < 750) && this->read_state() == LOW) {
      this->status = OpenThermStatus::RESPONSE_RECEIVING;
      this->response_timestamp_ = new_ts;
      this->response_bit_index_ = 0;
    } else {
      this->status = OpenThermStatus::RESPONSE_INVALID;
      this->response_timestamp_ = new_ts;
    }
  } else if (this->status == OpenThermStatus::RESPONSE_RECEIVING) {
    if ((new_ts - this->response_timestamp_) > 750) {
      if (this->response_bit_index_ < 32) {
        this->response_ = (this->response_ << 1) | !this->read_state();
        this->response_timestamp_ = new_ts;
        this->response_bit_index_++;
      } else {  // stop bit
        this->status = OpenThermStatus::RESPONSE_READY;
        this->response_timestamp_ = new_ts;
      }
    }
  }
}

void OpenThermComponent::process() {
  noInterrupts();
  OpenThermStatus st = this->status;
  unsigned long ts = this->response_timestamp_;
  interrupts();

  if (st == OpenThermStatus::READY) {
    return;
  }

  unsigned long new_ts = micros();
  if (st != OpenThermStatus::NOT_INITIALIZED && st != OpenThermStatus::DELAY && (new_ts - ts) > 1000000) {
    this->status = OpenThermStatus::READY;
    this->response_status_ = OpenThermResponseStatus::TIMEOUT;
    this->response_callback(this->response_, this->response_status_);
  } else if (st == OpenThermStatus::RESPONSE_INVALID) {
    this->status = OpenThermStatus::DELAY;
    this->response_status_ = OpenThermResponseStatus::INVALID;
    this->response_callback(this->response_, this->response_status_);
  } else if (st == OpenThermStatus::RESPONSE_READY) {
    this->status = OpenThermStatus::DELAY;
    this->response_status_ = (this->is_slave_ ? this->is_valid_request(response_) : this->is_valid_response(response_))
                           ? OpenThermResponseStatus::SUCCESS
                           : OpenThermResponseStatus::INVALID;
    this->response_callback(this->response_, this->response_status_);
  } else if (st == OpenThermStatus::DELAY) {
    if ((new_ts - ts) > 100000) {
      this->status = OpenThermStatus::READY;
    }
  }
}

bool OpenThermComponent::parity(unsigned long frame)  // odd parity
{
  byte p = 0;
  while (frame > 0) {
    if (frame & 1)
      p++;
    frame = frame >> 1;
  }
  return (p & 1);
}

OpenThermMessageType OpenThermComponent::get_message_type(unsigned long message) {
  OpenThermMessageType msg_type = static_cast<OpenThermMessageType>((message >> 28) & 7);
  return msg_type;
}

OpenThermMessageID OpenThermComponent::get_data_id(unsigned long frame) { return (OpenThermMessageID)((frame >> 16) & 0xFF); }

unsigned long OpenThermComponent::build_request(OpenThermMessageType type, OpenThermMessageID id, unsigned int data) {
  unsigned long request = data;
  if (type == OpenThermMessageType::WRITE_DATA) {
    request |= 1ul << 28;
  }
  request |= ((unsigned long) id) << 16;
  if (parity(request))
    request |= (1ul << 31);
  return request;
}

unsigned long OpenThermComponent::build_response(OpenThermMessageType type, OpenThermMessageID id, unsigned int data) {
  unsigned long response = data;
  response |= type << 28;
  response |= ((unsigned long) id) << 16;
  if (parity(response))
    response |= (1ul << 31);
  return response;
}

bool OpenThermComponent::is_valid_response(unsigned long response) {
  if (parity(response))
    return false;
  byte msg_type = (response << 1) >> 29;
  return msg_type == READ_ACK || msg_type == WRITE_ACK;
}

bool OpenThermComponent::is_valid_request(unsigned long request) {
  if (parity(request))
    return false;
  byte msg_type = (request << 1) >> 29;
  return msg_type == READ_DATA || msg_type == WRITE_DATA;
}

void OpenThermComponent::end() {
  detachInterrupt(digitalPinToInterrupt(in_pin_));
}

const char *OpenThermComponent::message_type_to_string(OpenThermMessageType message_type) {
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

void OpenThermComponent::response_callback(unsigned long response, OpenThermResponseStatus response_status) {
  if (response_status == OpenThermResponseStatus::SUCCESS) {
    this->log_message(ESP_LOG_DEBUG, "Received response", response);
    switch (this->get_data_id(response)) {
      case OpenThermMessageID::STATUS:
        this->publish_binary_sensor_state(this->ch_active_binary_sensor_,
                                          this->is_central_heating_active(response));
        this->publish_binary_sensor_state(this->dhw_active_binary_sensor_,
                                          this->is_hot_water_active(response));
        this->publish_binary_sensor_state(this->cooling_active_binary_sensor_,
                                          this->is_cooling_active(response));
        this->publish_binary_sensor_state(this->flame_active_binary_sensor_,
                                          this->is_flame_on(response));
        this->publish_binary_sensor_state(this->fault_binary_sensor_, this->is_fault(response));
        this->publish_binary_sensor_state(this->diagnostic_binary_sensor_,
                                          this->is_diagnostic(response));
        break;
      case OpenThermMessageID::TRET:
        this->publish_sensor_state(this->return_temperature_sensor_, this->get_float(response));
        break;
      case OpenThermMessageID::TBOILER:
        this->publish_sensor_state(this->boiler_temperature_sensor_, this->get_float(response));
        break;
      case OpenThermMessageID::CH_PRESSURE:
        this->publish_sensor_state(this->pressure_sensor_, this->get_float(response));
        break;
      case OpenThermMessageID::REL_MOD_LEVEL:
        this->publish_sensor_state(this->modulation_sensor_, this->get_float(response));
        break;
      case OpenThermMessageID::TDHW_SET_UB_TDHW_SET_LB:
        this->dhw_min_max_read = true;
        this->publish_sensor_state(this->dhw_max_temperature_sensor_, response >> 8 & 0xFF);
        this->publish_sensor_state(this->dhw_min_temperature_sensor_, response & 0xFF);
        break;
      case OpenThermMessageID::MAX_T_SET_UB_MAX_T_SET_LB:
        this->ch_min_max_read = true;
        this->publish_sensor_state(this->ch_max_temperature_sensor_, response >> 8 & 0xFF);
        this->publish_sensor_state(this->ch_min_temperature_sensor_, response & 0xFF);
        break;
      case OpenThermMessageID::TDHW_SET:
        if (this->get_message_type(response) == OpenThermMessageType::WRITE_ACK) {
          this->confirmed_dhw_setpoint = this->get_float(response);
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
    this->log_message(ESP_LOG_WARN, "Received invalid response", response);
  }
}

// TODO: ---- clean


void OpenThermComponent::publish_sensor_state(sensor::Sensor *sensor, float state) {
  if (sensor) {
    sensor->publish_state(state);
  }
}

void OpenThermComponent::publish_binary_sensor_state(binary_sensor::BinarySensor *sensor, bool state) {
  if (sensor) {
    sensor->publish_state(state);
  }
}

void OpenThermComponent::process_status() {
  // Fields: CH enabled | DHW enabled | cooling | outside temperature compensation | central heating 2
  unsigned int data = this->wanted_ch_enabled_ | (this->wanted_dhw_enabled_ << 1) |
                      (this->wanted_cooling_enabled_ << 2) | (false << 3) | (false << 4);
  data <<= 8;
  this->enqueue_request(this->build_request(OpenThermMessageType::READ_DATA,
                                             OpenThermMessageID::STATUS, data));
}

void OpenThermComponent::enqueue_request(unsigned long request) {
  if (this->buffer_.size() > 20) {
    this->log_message(ESP_LOG_WARN, "Queue full. Discarded request", request);
  } else {
    this->buffer_.push(request);
    this->log_message(ESP_LOG_DEBUG, "Enqueued request", request);
  }
}

const char *OpenThermComponent::format_message_type(unsigned long message) {
  return this->message_type_to_string(this->get_message_type(message));
}

}  // namespace opentherm
}  // namespace esphome
