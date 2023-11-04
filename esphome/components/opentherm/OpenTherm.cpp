/*
OpenTherm.cpp - OpenTherm Communication Library For Arduino, ESP8266
Copyright 2018, Ihor Melnyk
*/

#include "OpenTherm.h"
#include "esphome/core/helpers.h"
#include <bitset>

namespace esphome {
namespace opentherm {

OpenTherm::OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, bool is_slave)
    : cur_status(OpenThermStatus::NOT_INITIALIZED),
      in_pin_(in_pin),
      out_pin_(out_pin),
      is_slave_(is_slave),
      response_(0),
      response_status_(OpenThermResponseStatus::NONE),
      response_timestamp_(0),
      process_response_callback_(nullptr) {
  isr_in_pin_ = in_pin->to_isr();
  isr_out_pin_ = out_pin->to_isr();
}

void OpenTherm::begin() { begin(nullptr); }

void OpenTherm::begin(void (*process_response_callback)(uint32_t, OpenThermResponseStatus)) {
  in_pin_->pin_mode(gpio::FLAG_INPUT);
  out_pin_->pin_mode(gpio::FLAG_OUTPUT);

  in_pin_->attach_interrupt(handle_interrupt, this, gpio::INTERRUPT_ANY_EDGE);

  activate_boiler_();
  cur_status = OpenThermStatus::READY;
  this->process_response_callback_ = process_response_callback;
}

bool IRAM_ATTR OpenTherm::is_ready() { return cur_status == OpenThermStatus::READY; }

int IRAM_ATTR OpenTherm::read_state_() { return isr_in_pin_.digital_read(); }

void OpenTherm::set_active_state_() { isr_out_pin_.digital_write(false); }

void OpenTherm::set_idle_state_() { isr_out_pin_.digital_write(true); }

void OpenTherm::activate_boiler_() {
  set_idle_state_();
  delay(1000);
}

void OpenTherm::send_bit_(bool high) {
  if (high) {
    set_active_state_();
  } else {
    set_idle_state_();
  }

  delayMicroseconds(500);

  if (high) {
    set_idle_state_();
  } else {
    set_active_state_();
  }

  delayMicroseconds(500);
}

bool OpenTherm::send_request_aync(uint32_t request) {
  // Serial.println("Request: " + String(request, HEX));
  bool ready;

  {
    InterruptLock const lock;
    ready = is_ready();
  }

  if (!ready)
    return false;

  cur_status = OpenThermStatus::REQUEST_SENDING;
  response_ = 0;
  response_status_ = OpenThermResponseStatus::NONE;
  protocol_error_ = OpenThermProtocolError::NO_ERROR;

  send_bit_(true);  // start bit
  for (int i = 31; i >= 0; i--) {
    send_bit_(bitRead(request, i));
  }
  send_bit_(true);  // stop bit
  set_idle_state_();

  cur_status = OpenThermStatus::RESPONSE_WAITING;
  response_timestamp_ = micros();
  return true;
}

uint32_t OpenTherm::send_request(uint32_t request) {
  if (!send_request_aync(request))
    return 0;
  while (!is_ready()) {
    process();
    yield();
  }
  return response_;
}

bool OpenTherm::send_response(uint32_t request) {
  cur_status = OpenThermStatus::REQUEST_SENDING;
  response_ = 0;
  response_status_ = OpenThermResponseStatus::NONE;

  send_bit_(true);  // start bit
  for (int i = 31; i >= 0; i--) {
    send_bit_(bitRead(request, i));
  }
  send_bit_(true);  // stop bit
  set_idle_state_();
  cur_status = OpenThermStatus::READY;
  return true;
}

uint32_t OpenTherm::get_last_response() { return response_; }

OpenThermResponseStatus OpenTherm::get_last_response_status() { return response_status_; }

void IRAM_ATTR OpenTherm::handle_interrupt(OpenTherm *arg) {
  if (arg->is_ready()) {
    if (arg->is_slave_ && arg->read_state_()) {
      arg->cur_status = OpenThermStatus::RESPONSE_WAITING;
    } else {
      return;
    }
  }

  uint32_t const new_ts = micros();
  if (arg->cur_status == OpenThermStatus::RESPONSE_WAITING) {
    if (arg->read_state_()) {
      arg->cur_status = OpenThermStatus::RESPONSE_START_BIT;
      arg->response_timestamp_ = new_ts;
    } else {
      arg->cur_status = OpenThermStatus::RESPONSE_INVALID;
      arg->protocol_error_ = OpenThermProtocolError::BEFORE_START_BIT;
      arg->response_timestamp_ = new_ts;
    }
  } else if (arg->cur_status == OpenThermStatus::RESPONSE_START_BIT) {
    if ((new_ts - arg->response_timestamp_ < 750) && !arg->read_state_()) {
      arg->cur_status = OpenThermStatus::RESPONSE_RECEIVING;
      arg->response_timestamp_ = new_ts;
      arg->response_bit_index_ = 0;
    } else {
      arg->cur_status = OpenThermStatus::RESPONSE_INVALID;
      arg->protocol_error_ = OpenThermProtocolError::AFTER_START_BIT;
      arg->response_timestamp_ = new_ts;
    }
  } else if (arg->cur_status == OpenThermStatus::RESPONSE_RECEIVING) {
    if ((new_ts - arg->response_timestamp_) > 750) {
      if (arg->response_bit_index_ < 32) {
        arg->response_ = (arg->response_ << 1) | !arg->read_state_();
        arg->response_timestamp_ = new_ts;
        arg->response_bit_index_++;
      } else {  // stop bit
        arg->cur_status = OpenThermStatus::RESPONSE_READY;
        arg->response_timestamp_ = new_ts;
      }
    }
  }
}

void OpenTherm::process() {
  OpenThermStatus st;
  uint32_t ts;

  {
    InterruptLock const lock;

    st = cur_status;
    ts = response_timestamp_;
  }

  if (st == OpenThermStatus::READY)
    return;
  auto new_ts = micros();
  if (st != OpenThermStatus::NOT_INITIALIZED && st != OpenThermStatus::DELAY && (new_ts - ts) > 1000000) {
    cur_status = OpenThermStatus::READY;
    response_status_ = OpenThermResponseStatus::TIMEOUT;
    if (process_response_callback_ != nullptr) {
      process_response_callback_(response_, response_status_);
    }
  } else if (st == OpenThermStatus::RESPONSE_INVALID) {
    cur_status = OpenThermStatus::DELAY;
    response_status_ = OpenThermResponseStatus::INVALID;
    if (process_response_callback_ != nullptr) {
      process_response_callback_(response_, response_status_);
    }
  } else if (st == OpenThermStatus::RESPONSE_READY) {
    cur_status = OpenThermStatus::DELAY;
    response_status_ = (is_slave_ ? is_valid_request(response_) : is_valid_response(response_))
                           ? OpenThermResponseStatus::SUCCESS
                           : OpenThermResponseStatus::INVALID;
    if (process_response_callback_ != nullptr) {
      process_response_callback_(response_, response_status_);
    }
  } else if (st == OpenThermStatus::DELAY) {
    if ((new_ts - ts) > 100000) {
      cur_status = OpenThermStatus::READY;
    }
  }
}

bool OpenTherm::parity(uint32_t frame)  // odd parity
{
  uint8_t p = 0;
  while (frame > 0) {
    if (frame & 1)
      p++;
    frame = frame >> 1;
  }
  return (p & 1);
}

OpenThermMessageType OpenTherm::get_message_type(uint32_t message) {
  auto msg_type = static_cast<OpenThermMessageType>((message >> 28) & 7);
  return msg_type;
}

OpenThermMessageID OpenTherm::get_data_id(uint32_t frame) { return (OpenThermMessageID) ((frame >> 16) & 0xFF); }

uint32_t OpenTherm::build_request(OpenThermMessageType type, OpenThermMessageID id, uint32_t data) {
  uint32_t request = data;
  if (type == OpenThermMessageType::WRITE_DATA) {
    request |= 1ul << 28;
  }
  request |= ((uint32_t) id) << 16;
  if (parity(request))
    request |= (1ul << 31);
  return request;
}

uint32_t OpenTherm::build_response(OpenThermMessageType type, OpenThermMessageID id, uint32_t data) {
  uint32_t response = data;
  response |= ((uint32_t) type) << 28;
  response |= ((uint32_t) id) << 16;
  if (parity(response))
    response |= (1ul << 31);
  return response;
}

bool OpenTherm::is_valid_response(uint32_t response) {
  if (parity(response))
    return false;
  uint8_t const msg_type = (response << 1) >> 29;
  return msg_type == READ_ACK || msg_type == WRITE_ACK;
}

bool OpenTherm::is_valid_request(uint32_t request) {
  if (parity(request))
    return false;
  uint8_t const msg_type = (request << 1) >> 29;
  return msg_type == READ_DATA || msg_type == WRITE_DATA;
}

void OpenTherm::end() { in_pin_->detach_interrupt(); }

const char *OpenTherm::status_to_string(OpenThermResponseStatus status) {
  switch (status) {
    case NONE:
      return "NONE";
    case SUCCESS:
      return "SUCCESS";
    case INVALID:
      return "INVALID";
    case TIMEOUT:
      return "TIMEOUT";
    default:
      return "UNKNOWN";
  }
}

const char *OpenTherm::message_type_to_string(OpenThermMessageType message_type) {
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

// building requests

uint32_t OpenTherm::build_set_boiler_status_request(bool enable_central_heating, bool enable_hot_water,
                                                    bool enable_cooling, bool enable_outside_temperature_compensation,
                                                    bool enable_central_heating_2) {
  unsigned int data = enable_central_heating | (enable_hot_water << 1) | (enable_cooling << 2) |
                      (enable_outside_temperature_compensation << 3) | (enable_central_heating_2 << 4);
  data <<= 8;
  return build_request(OpenThermMessageType::READ_DATA, OpenThermMessageID::Status, data);
}

uint32_t OpenTherm::build_set_boiler_temperature_request(float temperature) {
  uint32_t const data = temperature_to_data(temperature);
  return build_request(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TSet, data);
}

uint32_t OpenTherm::build_get_boiler_temperature_request() {
  return build_request(OpenThermMessageType::READ_DATA, OpenThermMessageID::Tboiler, 0);
}

// parsing responses
bool OpenTherm::is_fault(uint32_t response) { return response & 0x1; }

bool OpenTherm::is_central_heating_active(uint32_t response) { return response & 0x2; }

bool OpenTherm::is_hot_water_active(uint32_t response) { return response & 0x4; }

bool OpenTherm::is_flame_on(uint32_t response) { return response & 0x8; }

bool OpenTherm::is_cooling_active(uint32_t response) { return response & 0x10; }

bool OpenTherm::is_diagnostic(uint32_t response) { return response & 0x40; }

uint16_t OpenTherm::get_u_int(const uint32_t response) const {
  const uint16_t u88 = response & 0xffff;
  return u88;
}

float OpenTherm::get_float(const uint32_t response) const {
  const uint16_t u88 = get_u_int(response);
  const float f = (u88 & 0x8000) ? -(0x10000L - u88) / 256.0f : u88 / 256.0f;
  return f;
}

uint32_t OpenTherm::temperature_to_data(float temperature) {
  if (temperature < 0)
    temperature = 0;
  if (temperature > 100)
    temperature = 100;
  return (uint32_t) (temperature * 256);
}

// basic requests

uint32_t OpenTherm::set_boiler_status(bool enable_central_heating, bool enable_hot_water, bool enable_cooling,
                                      bool enable_outside_temperature_compensation, bool enable_central_heating_2) {
  return send_request(build_set_boiler_status_request(enable_central_heating, enable_hot_water, enable_cooling,
                                                      enable_outside_temperature_compensation,
                                                      enable_central_heating_2));
}

bool OpenTherm::set_boiler_temperature(float temperature) {
  uint32_t const response = send_request(build_set_boiler_temperature_request(temperature));
  return is_valid_response(response);
}

float OpenTherm::get_boiler_temperature() {
  uint32_t const response = send_request(build_get_boiler_temperature_request());
  return is_valid_response(response) ? get_float(response) : 0;
}

float OpenTherm::get_return_temperature() {
  uint32_t const response = send_request(build_request(OpenThermMessageType::READ, OpenThermMessageID::Tret, 0));
  return is_valid_response(response) ? get_float(response) : 0;
}

bool OpenTherm::set_dhw_setpoint(float temperature) {
  auto data = temperature_to_data(temperature);
  uint32_t const response =
      send_request(build_request(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TdhwSet, data));
  return is_valid_response(response);
}

float OpenTherm::get_dhw_temperature() {
  uint32_t const response = send_request(build_request(OpenThermMessageType::READ_DATA, OpenThermMessageID::Tdhw, 0));
  return is_valid_response(response) ? get_float(response) : 0;
}

float OpenTherm::get_modulation() {
  uint32_t const response = send_request(build_request(OpenThermMessageType::READ, OpenThermMessageID::RelModLevel, 0));
  return is_valid_response(response) ? get_float(response) : 0;
}

float OpenTherm::get_pressure() {
  uint32_t const response = send_request(build_request(OpenThermMessageType::READ, OpenThermMessageID::CHPressure, 0));
  return is_valid_response(response) ? get_float(response) : 0;
}

uint8_t OpenTherm::get_fault() {
  return ((send_request(build_request(OpenThermMessageType::READ, OpenThermMessageID::ASFflags, 0)) >> 8) & 0xff);
}
const char *OpenTherm::protocol_error_to_string(OpenThermProtocolError error) {
  switch (error) {
    case BEFORE_START_BIT:
      return "BEFORE_START_BIT";
    case AFTER_START_BIT:
      return "AFTER_START_BIT";
    case NO_ERROR:
      return "NO_ERROR";
    default:
      return "UNKNOWN";
  }
}

const char *OpenTherm::frame_msg_type_to_string(uint32_t frame) {
  auto msg_type = (OpenThermMessageType) ((frame << 1) >> 29);
  return message_type_to_string(msg_type);
}

string OpenTherm::debug_response(uint32_t response) {
  string result = int_to_hex(response);
  result += "[parity: ";
  result += (parity(response) ? "OK" : "FAIL");
  result += "; msg_type: ";
  result += frame_msg_type_to_string(response);
  result += "; data_id: ";
  result += std::to_string((response >> 16 & 0xFF));
  result += "value_int: ";
  result += std::to_string(get_u_int(response));
  result += "value_float: ";
  result += std::to_string(get_float(response));
  result += "value_bin: ";
  result += std::bitset<16>(get_u_int(response)).to_string();
  result += "]";

  return result;
}

}  // namespace opentherm
}  // namespace esphome