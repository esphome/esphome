#ifdef ESP8266
#include "opentherm_esp8266.h"
#include "esphome/core/helpers.h"
#include <string>

namespace esphome {
namespace opentherm {

using std::string;
using std::to_string;

static const char *const TAG = "opentherm";

OpenTherm *OpenTherm::instance = nullptr;

OpenTherm::OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin) : OpenThermBase(in_pin, out_pin) {
  this->isr_in_pin_ = in_pin->to_isr();
  this->isr_out_pin_ = out_pin->to_isr();
}

bool OpenTherm::initialize() {
  auto base_result = OpenThermBase::initialize();
  if (!base_result)
    return false;

  OpenTherm::instance = this;
  this->out_pin_->digital_write(true);
  return true;
}

void OpenTherm::listen() {
  static constexpr int32_t DEVICE_TIMEOUT = 800;

  this->stop_timer_();
  OpenThermBase::listen();
  this->timeout_counter_ = DEVICE_TIMEOUT * 5;  // timer_ ticks at 5 ticks/ms
  this->bit_pos_ = 0;

  this->start_read_timer_();
}

void OpenTherm::send(OpenthermData &data) {
  this->stop_timer_();
  OpenThermBase::send(data);

  this->clock_ = 1;     // clock starts at HIGH
  this->bit_pos_ = 33;  // count down (33 == start bit, 32-1 data, 0 == stop bit)
  this->start_write_timer_();
}

void OpenTherm::stop() {
  this->stop_timer_();
  OpenThermBase::stop();
}

void OpenTherm::log_protocol_state() const {
  ESP_LOGD(TAG, "data: %s; clock: %s; capture: %s; bit_pos: %s", format_hex(this->data_).c_str(),
           to_string(clock_).c_str(), format_bin(this->capture_).c_str(), to_string(this->bit_pos_).c_str());
}

void IRAM_ATTR OpenTherm::read_() {
  this->data_ = 0;
  this->bit_pos_ = 0;
  this->mode_ = OperationMode::READ;
  this->capture_ = 1;         // reset counter and add as if read start bit
  this->clock_ = 1;           // clock is high at the start of comm
  this->start_read_timer_();  // get us into 1/4 of manchester code. 5 timer ticks constitute 1 ms, which is 1 bit
                              // period in OpenTherm.
}

bool IRAM_ATTR OpenTherm::timer_isr(OpenTherm *arg) {
  if (arg->mode_ == OperationMode::LISTEN) {
    if (arg->timeout_counter_ == 0) {
      arg->mode_ = OperationMode::ERROR_TIMEOUT;
      arg->stop_timer_();
      return false;
    }
    bool const value = arg->isr_in_pin_.digital_read();
    if (value) {  // incoming data (rising signal)
      arg->read_();
    }
    if (arg->timeout_counter_ > 0) {
      arg->timeout_counter_--;
    }
  } else if (arg->mode_ == OperationMode::READ) {
    bool const value = arg->isr_in_pin_.digital_read();
    uint8_t const last = (arg->capture_ & 1);
    if (value != last) {
      // transition of signal from last sampling
      if (arg->clock_ == 1 && arg->capture_ > 0xF) {
        // no transition in the middle of the bit
        arg->mode_ = OperationMode::ERROR_PROTOCOL;
        arg->error_type_ = ProtocolErrorType::NO_TRANSITION;
        arg->stop_timer_();
        return false;
      } else if (arg->clock_ == 1 || arg->capture_ > 0xF) {
        // transition in the middle of the bit OR no transition between two bit, both are valid data points
        if (arg->bit_pos_ == 33) {
          // expecting stop bit
          auto stop_bit_error = arg->verify_stop_bit_(last);
          if (stop_bit_error == ProtocolErrorType::NO_ERROR) {
            arg->mode_ = OperationMode::RECEIVED;
            arg->stop_timer_();
            return false;
          } else {
            // end of data not verified, invalid data
            arg->mode_ = OperationMode::ERROR_PROTOCOL;
            arg->error_type_ = stop_bit_error;
            arg->stop_timer_();
            return false;
          }
        } else {
          // normal data point at clock high
          arg->bit_read_(last);
          arg->clock_ = 0;
        }
      } else {
        // clock low, not a data point, switch clock
        arg->clock_ = 1;
      }
      arg->capture_ = 1;  // reset counter
    } else if (arg->capture_ > 0xFF) {
      // no change for too long, invalid manchester encoding
      arg->mode_ = OperationMode::ERROR_PROTOCOL;
      arg->error_type_ = ProtocolErrorType::NO_CHANGE_TOO_LONG;
      arg->stop_timer_();
      return false;
    }
    arg->capture_ = (arg->capture_ << 1) | value;
  } else if (arg->mode_ == OperationMode::WRITE) {
    // write data to pin
    if (arg->bit_pos_ == 33 || arg->bit_pos_ == 0) {  // start bit
      arg->write_bit_(1, arg->clock_);
    } else {  // data bits
      arg->write_bit_(read_bit(arg->data_, arg->bit_pos_ - 1), arg->clock_);
    }
    if (arg->clock_ == 0) {
      if (arg->bit_pos_ <= 0) {            // check termination
        arg->mode_ = OperationMode::SENT;  // all data written
        arg->stop_timer_();
      }
      arg->bit_pos_--;
      arg->clock_ = 1;
    } else {
      arg->clock_ = 0;
    }
  }

  return false;
}

void IRAM_ATTR OpenTherm::esp8266_timer_isr() { OpenTherm::timer_isr(OpenTherm::instance); }

void IRAM_ATTR OpenTherm::bit_read_(uint8_t value) {
  this->data_ = (this->data_ << 1) | value;
  this->bit_pos_++;
}

ProtocolErrorType IRAM_ATTR OpenTherm::verify_stop_bit_(uint8_t value) {
  if (value) {  // stop bit detected
    return check_parity(this->data_) ? ProtocolErrorType::NO_ERROR : ProtocolErrorType::PARITY_ERROR;
  } else {  // no stop bit detected, error
    return ProtocolErrorType::INVALID_START_STOP_BIT;
  }
}

void IRAM_ATTR OpenTherm::write_bit_(uint8_t high, uint8_t clock) {
  if (clock == 1) {                           // left part of manchester encoding
    this->isr_out_pin_.digital_write(!high);  // low means logical 1 to protocol
  } else {                                    // right part of manchester encoding
    this->isr_out_pin_.digital_write(high);   // high means logical 0 to protocol
  }
}

// 5 kHz timer_
void IRAM_ATTR OpenTherm::start_read_timer_() {
  InterruptLock const lock;
  timer1_attachInterrupt(OpenTherm::esp8266_timer_isr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  // 5MHz (5 ticks/us - 1677721.4 us max)
  timer1_write(1000);                            // 5kHz
}

// 2 kHz timer_
void IRAM_ATTR OpenTherm::start_write_timer_() {
  InterruptLock const lock;
  timer1_attachInterrupt(OpenTherm::esp8266_timer_isr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  // 5MHz (5 ticks/us - 1677721.4 us max)
  timer1_write(2500);                            // 2kHz
}

void IRAM_ATTR OpenTherm::stop_timer_() {
  InterruptLock const lock;
  timer1_disable();
  timer1_detachInterrupt();
}

}  // namespace opentherm
}  // namespace esphome

#endif
