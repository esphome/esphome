/*
OpenTherm.cpp - OpenTherm Library for the ESP8266/Arduino platform
Licensed under MIT license
Copyright 2018, Ihor Melnyk

Original:
  https://github.com/ihormelnyk/OpenTherm
  http://ihormelnyk.com/pages/OpenTherm
Adapted by:
  * Kenneth Henderick - For ESPHome integration
    Adaptations:
      * Namespacing
      * Minor cleanups (typo, formatting, ...)
      * Remove some helper methods
      * Remove non-async code
*/

#include "opentherm.h"

namespace esphome {
namespace diyless_opentherm {
namespace ihormelnyk {

OpenTherm::OpenTherm(int in_pin, int out_pin, bool is_slave)
    : status(OpenThermStatus::NOT_INITIALIZED),
      in_pin_(in_pin),
      out_pin_(out_pin),
      is_slave_(is_slave),
      response_(0),
      response_status_(OpenThermResponseStatus::NONE),
      response_timestamp_(0),
      handle_interrupt_callback_(nullptr),
      process_response_callback_(nullptr) {}

void OpenTherm::begin(void (*handle_interrupt_callback)(),
                      void (*process_response_callback)(unsigned long, OpenThermResponseStatus)) {
  pinMode(in_pin_, INPUT);
  pinMode(out_pin_, OUTPUT);
  if (handle_interrupt_callback != nullptr) {
    this->handle_interrupt_callback_ = handle_interrupt_callback;
    attachInterrupt(digitalPinToInterrupt(in_pin_), handle_interrupt_callback, CHANGE);
  }
  activate_boiler_();
  status = OpenThermStatus::READY;
  this->process_response_callback_ = process_response_callback;
}

void OpenTherm::begin(void (*handle_interrupt_callback)()) { begin(handle_interrupt_callback, nullptr); }

bool IRAM_ATTR OpenTherm::is_ready() { return status == OpenThermStatus::READY; }

int IRAM_ATTR OpenTherm::read_state_() { return digitalRead(in_pin_); }

void OpenTherm::set_active_state_() { digitalWrite(out_pin_, LOW); }

void OpenTherm::set_idle_state_() { digitalWrite(out_pin_, HIGH); }

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

bool OpenTherm::send_request_async(unsigned long request) {
  noInterrupts();
  const bool ready = is_ready();
  interrupts();

  if (!ready)
    return false;

  status = OpenThermStatus::REQUEST_SENDING;
  response_ = 0;
  response_status_ = OpenThermResponseStatus::NONE;

  send_bit_(HIGH);  // Start bit
  for (int i = 31; i >= 0; i--) {
    send_bit_(bitRead(request, i));
  }
  send_bit_(HIGH);  // Stop bit
  set_idle_state_();

  status = OpenThermStatus::RESPONSE_WAITING;
  response_timestamp_ = micros();
  return true;
}

bool OpenTherm::send_response(unsigned long request) {
  status = OpenThermStatus::REQUEST_SENDING;
  response_ = 0;
  response_status_ = OpenThermResponseStatus::NONE;

  send_bit_(HIGH);  // Start bit
  for (int i = 31; i >= 0; i--) {
    send_bit_(bitRead(request, i));
  }
  send_bit_(HIGH);  // Stop bit
  set_idle_state_();
  status = OpenThermStatus::READY;
  return true;
}

unsigned long OpenTherm::get_last_response() { return response_; }

OpenThermResponseStatus OpenTherm::get_last_response_status() { return response_status_; }

void IRAM_ATTR OpenTherm::handle_interrupt() {
  if (is_ready()) {
    if (is_slave_ && read_state_() == HIGH) {
      status = OpenThermStatus::RESPONSE_WAITING;
    } else {
      return;
    }
  }

  unsigned long new_ts = micros();
  if (status == OpenThermStatus::RESPONSE_WAITING) {
    if (read_state_() == HIGH) {
      status = OpenThermStatus::RESPONSE_START_BIT;
      response_timestamp_ = new_ts;
    } else {
      status = OpenThermStatus::RESPONSE_INVALID;
      response_timestamp_ = new_ts;
    }
  } else if (status == OpenThermStatus::RESPONSE_START_BIT) {
    if ((new_ts - response_timestamp_ < 750) && read_state_() == LOW) {
      status = OpenThermStatus::RESPONSE_RECEIVING;
      response_timestamp_ = new_ts;
      response_bit_index_ = 0;
    } else {
      status = OpenThermStatus::RESPONSE_INVALID;
      response_timestamp_ = new_ts;
    }
  } else if (status == OpenThermStatus::RESPONSE_RECEIVING) {
    if ((new_ts - response_timestamp_) > 750) {
      if (response_bit_index_ < 32) {
        response_ = (response_ << 1) | !read_state_();
        response_timestamp_ = new_ts;
        response_bit_index_++;
      } else {  // stop bit
        status = OpenThermStatus::RESPONSE_READY;
        response_timestamp_ = new_ts;
      }
    }
  }
}

void OpenTherm::process() {
  noInterrupts();
  OpenThermStatus st = status;
  unsigned long ts = response_timestamp_;
  interrupts();

  if (st == OpenThermStatus::READY)
    return;
  unsigned long new_ts = micros();
  if (st != OpenThermStatus::NOT_INITIALIZED && st != OpenThermStatus::DELAY && (new_ts - ts) > 1000000) {
    status = OpenThermStatus::READY;
    response_status_ = OpenThermResponseStatus::TIMEOUT;
    if (process_response_callback_ != nullptr) {
      process_response_callback_(response_, response_status_);
    }
  } else if (st == OpenThermStatus::RESPONSE_INVALID) {
    status = OpenThermStatus::DELAY;
    response_status_ = OpenThermResponseStatus::INVALID;
    if (process_response_callback_ != nullptr) {
      process_response_callback_(response_, response_status_);
    }
  } else if (st == OpenThermStatus::RESPONSE_READY) {
    status = OpenThermStatus::DELAY;
    response_status_ = (is_slave_ ? is_valid_request(response_) : is_valid_response(response_))
                           ? OpenThermResponseStatus::SUCCESS
                           : OpenThermResponseStatus::INVALID;
    if (process_response_callback_ != nullptr) {
      process_response_callback_(response_, response_status_);
    }
  } else if (st == OpenThermStatus::DELAY) {
    if ((new_ts - ts) > 100000) {
      status = OpenThermStatus::READY;
    }
  }
}

bool OpenTherm::parity(unsigned long frame)  // odd parity
{
  byte p = 0;
  while (frame > 0) {
    if (frame & 1)
      p++;
    frame = frame >> 1;
  }
  return (p & 1);
}

OpenThermMessageType OpenTherm::get_message_type(unsigned long message) {
  OpenThermMessageType msg_type = static_cast<OpenThermMessageType>((message >> 28) & 7);
  return msg_type;
}

OpenThermMessageID OpenTherm::get_data_id(unsigned long frame) { return (OpenThermMessageID)((frame >> 16) & 0xFF); }

unsigned long OpenTherm::build_request(OpenThermMessageType type, OpenThermMessageID id, unsigned int data) {
  unsigned long request = data;
  if (type == OpenThermMessageType::WRITE_DATA) {
    request |= 1ul << 28;
  }
  request |= ((unsigned long) id) << 16;
  if (parity(request))
    request |= (1ul << 31);
  return request;
}

unsigned long OpenTherm::build_response(OpenThermMessageType type, OpenThermMessageID id, unsigned int data) {
  unsigned long response = data;
  response |= type << 28;
  response |= ((unsigned long) id) << 16;
  if (parity(response))
    response |= (1ul << 31);
  return response;
}

bool OpenTherm::is_valid_response(unsigned long response) {
  if (parity(response))
    return false;
  byte msg_type = (response << 1) >> 29;
  return msg_type == READ_ACK || msg_type == WRITE_ACK;
}

bool OpenTherm::is_valid_request(unsigned long request) {
  if (parity(request))
    return false;
  byte msg_type = (request << 1) >> 29;
  return msg_type == READ_DATA || msg_type == WRITE_DATA;
}

void OpenTherm::end() {
  if (this->handle_interrupt_callback_ != nullptr) {
    detachInterrupt(digitalPinToInterrupt(in_pin_));
  }
}

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

// Parsing responses
bool OpenTherm::is_fault(unsigned long response) { return response & 0x1; }

bool OpenTherm::is_central_heating_active(unsigned long response) { return response & 0x2; }

bool OpenTherm::is_hot_water_active(unsigned long response) { return response & 0x4; }

bool OpenTherm::is_flame_on(unsigned long response) { return response & 0x8; }

bool OpenTherm::is_cooling_active(unsigned long response) { return response & 0x10; }

bool OpenTherm::is_diagnostic(unsigned long response) { return response & 0x40; }

uint16_t OpenTherm::get_u_int(const unsigned long response) const {
  const uint16_t u88 = response & 0xffff;
  return u88;
}

float OpenTherm::get_float(const unsigned long response) const {
  const uint16_t u88 = get_u_int(response);
  const float f = (u88 & 0x8000) ? -(0x10000L - u88) / 256.0f : u88 / 256.0f;
  return f;
}

unsigned int OpenTherm::temperature_to_data(float temperature) {
  if (temperature < 0)
    temperature = 0;
  if (temperature > 100)
    temperature = 100;
  unsigned int data = (unsigned int) (temperature * 256);
  return data;
}

}  // namespace ihormelnyk
}  // namespace diyless_opentherm
}  // namespace esphome
