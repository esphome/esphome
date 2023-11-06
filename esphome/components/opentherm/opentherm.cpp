#include "opentherm.h"
#include "esphome/core/helpers.h"
#include "driver/timer.h"
#include <string>
#include <sstream>
#include <bitset>

namespace esphome {
namespace opentherm {

using std::string;
using std::bitset;
using std::stringstream;
using std::to_string;

OpenTherm::OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, int32_t slave_timeout)
    : in_pin_(in_pin),
      out_pin_(out_pin),
      mode_(OperationMode::IDLE),
      error_type_(ProtocolErrorType::NO_ERROR),
      capture_(0),
      clock_(0),
      data_(0),
      bit_pos_(0),
      active_(false),
      timeout_counter_(-1),
      timer_initialized_(false),
      slave_timeout_(slave_timeout) {
  isr_in_pin_ = in_pin->to_isr();
  isr_out_pin_ = out_pin->to_isr();
}

void OpenTherm::begin() {
  in_pin_->pin_mode(gpio::FLAG_INPUT);
  out_pin_->pin_mode(gpio::FLAG_OUTPUT);
  out_pin_->digital_write(true);

  // delay(1000); // It was here in Igor Melnik's library, but there is nothing like this in arduino-opentherm
  // library example. Commenting out for now.
}

void OpenTherm::listen() {
  stop_();
  this->timeout_counter_ = slave_timeout_ * 5;  // timer_ ticks at 5 ticks/ms

  mode_ = OperationMode::LISTEN;
  active_ = true;
  data_ = 0;
  bit_pos_ = 0;

  start_read_timer_();
}

void OpenTherm::send(OpenthermData &data) {
  stop_();
  data_ = data.type;
  data_ = (data_ << 12) | data.id;
  data_ = (data_ << 8) | data.valueHB;
  data_ = (data_ << 8) | data.valueLB;
  if (!check_parity_(data_)) {
    data_ = data_ | 0x80000000;
  }

  clock_ = 1;     // clock starts at HIGH
  bit_pos_ = 33;  // count down (33 == start bit, 32-1 data, 0 == stop bit)
  mode_ = OperationMode::WRITE;

  active_ = true;
  start_write_timer_();
}

bool OpenTherm::get_message(OpenthermData &data) {
  if (mode_ == OperationMode::RECEIVED) {
    data.type = (data_ >> 28) & 0x7;
    data.id = (data_ >> 16) & 0xFF;
    data.valueHB = (data_ >> 8) & 0xFF;
    data.valueLB = data_ & 0xFF;
    return true;
  }
  return false;
}

bool OpenTherm::get_protocol_error(OpenThermError &error) {
  if (mode_ != OperationMode::ERROR_PROTOCOL) {
    return false;
  }

  error.error_type = error_type_;
  error.bit_pos = bit_pos_;
  error.capture = capture_;
  error.clock = clock_;
  error.data = data_;

  return true;
}

void OpenTherm::stop() {
  stop_();
  mode_ = OperationMode::IDLE;
}

void IRAM_ATTR OpenTherm::stop_() {
  if (active_) {
    stop_timer_();
    active_ = false;
  }
}

void IRAM_ATTR OpenTherm::read_() {
  data_ = 0;
  bit_pos_ = 0;
  mode_ = OperationMode::READ;
  capture_ = 1;         // reset counter and add as if read start bit
  clock_ = 1;           // clock is high at the start of comm
  start_read_timer_();  // get us into 1/4 of manchester code. 5 timer ticks constitute 1 ms, which is 1 bit period in
                        // OpenTherm.
}

bool IRAM_ATTR OpenTherm::timer_isr(OpenTherm *arg) {
  if (arg->mode_ == OperationMode::LISTEN) {
    if (arg->timeout_counter_ == 0) {
      arg->mode_ = OperationMode::ERROR_TIMEOUT;
      arg->stop_();
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
        arg->stop_();
      } else if (arg->clock_ == 1 || arg->capture_ > 0xF) {
        // transition in the middle of the bit OR no transition between two bit, both are valid data points
        if (arg->bit_pos_ == BitPositions::STOP_BIT) {
          // expecting stop bit
          auto stop_bit_error = arg->verify_stop_bit_(last);
          if (stop_bit_error == ProtocolErrorType::NO_ERROR) {
            arg->mode_ = OperationMode::RECEIVED;
            arg->stop_();
          } else {
            // end of data not verified, invalid data
            arg->mode_ = OperationMode::ERROR_PROTOCOL;
            arg->error_type_ = stop_bit_error;
            arg->stop_();
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
      // no change for too long, invalid mancheter encoding
      arg->mode_ = OperationMode::ERROR_PROTOCOL;
      arg->error_type_ = ProtocolErrorType::NO_CHANGE_TOO_LONG;
      arg->stop_();
    }
    arg->capture_ = (arg->capture_ << 1) | value;
  } else if (arg->mode_ == OperationMode::WRITE) {
    // write data to pin
    if (arg->bit_pos_ == 33 || arg->bit_pos_ == 0) {  // start bit
      arg->write_bit_(1, arg->clock_);
    } else {  // data bits
      arg->write_bit_(bitRead(arg->data_, arg->bit_pos_ - 1), arg->clock_);
    }
    if (arg->clock_ == 0) {
      if (arg->bit_pos_ <= 0) {            // check termination
        arg->mode_ = OperationMode::SENT;  // all data written
        arg->stop_();
      }
      arg->bit_pos_--;
      arg->clock_ = 1;
    } else {
      arg->clock_ = 0;
    }
  }

  return false;
}

void IRAM_ATTR OpenTherm::bit_read_(uint8_t value) {
  data_ = (data_ << 1) | value;
  bit_pos_++;
}

ProtocolErrorType OpenTherm::verify_stop_bit_(uint8_t value) {
  if (value) {  // stop bit detected
    return check_parity_(data_) ? ProtocolErrorType::NO_ERROR : ProtocolErrorType::PARITY_ERROR;
  } else {  // no stop bit detected, error
    return ProtocolErrorType::INVALID_STOP_BIT;
  }
}

void IRAM_ATTR OpenTherm::write_bit_(uint8_t high, uint8_t clock) {
  if (clock == 1) {                     // left part of manchester encoding
    isr_out_pin_.digital_write(!high);  // low means logical 1 to protocol
  } else {                              // right part of manchester encoding
    isr_out_pin_.digital_write(high);   // high means logical 0 to protocol
  }
}

// #ifdef ESP32

void IRAM_ATTR OpenTherm::init_timer_() {
  if (timer_initialized_)
    return;

  timer_config_t const config = {
      .alarm_en = TIMER_ALARM_DIS,
      .counter_en = TIMER_PAUSE,
      .intr_type = TIMER_INTR_LEVEL,
      .counter_dir = TIMER_COUNT_UP,
      .auto_reload = TIMER_AUTORELOAD_DIS,
      .divider = 80,
  };

  timer_init(TIMER_GROUP_0, TIMER_0, &config);
  timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
  timer_start(TIMER_GROUP_0, TIMER_0);

  timer_initialized_ = true;
}

void IRAM_ATTR OpenTherm::start_timer_(uint64_t alarm_value) {
  timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, reinterpret_cast<bool (*)(void *)>(timer_isr), this,
                         ESP_INTR_FLAG_IRAM);
  timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, alarm_value);
  timer_set_auto_reload(TIMER_GROUP_0, TIMER_0, TIMER_AUTORELOAD_EN);
  timer_set_alarm(TIMER_GROUP_0, TIMER_0, TIMER_ALARM_EN);
}

// 5 kHz timer_
void IRAM_ATTR OpenTherm::start_read_timer_() {
  {
    InterruptLock const lock;
    init_timer_();
    start_timer_(200);
  }
}

// 2 kHz timer_
void IRAM_ATTR OpenTherm::start_write_timer_() {
  {
    InterruptLock const lock;
    init_timer_();
    start_timer_(500);
  }
}

void IRAM_ATTR OpenTherm::stop_timer_() {
  {
    InterruptLock const lock;
    init_timer_();
    timer_set_alarm(TIMER_GROUP_0, TIMER_0, TIMER_ALARM_DIS);
    timer_isr_callback_remove(TIMER_GROUP_0, TIMER_0);
  }
}

// #endif  // END ESP32

// https://stackoverflow.com/questions/21617970/how-to-check-if-value-has-even-parity-of-bits-or-odd
bool OpenTherm::check_parity_(uint32_t val) {
  val ^= val >> 16;
  val ^= val >> 8;
  val ^= val >> 4;
  val ^= val >> 2;
  val ^= val >> 1;
  return (~val) & 1;
}

#define TO_STRING_MEMBER(name) \
  case name: \
    return #name;

const char *OpenTherm::operation_mode_to_str_(OperationMode mode) {
  switch (mode) {
    TO_STRING_MEMBER(IDLE)
    TO_STRING_MEMBER(LISTEN)
    TO_STRING_MEMBER(READ)
    TO_STRING_MEMBER(RECEIVED)
    TO_STRING_MEMBER(WRITE)
    TO_STRING_MEMBER(SENT)
    TO_STRING_MEMBER(ERROR_PROTOCOL)
    TO_STRING_MEMBER(ERROR_TIMEOUT)
    default:
      return "<INVALID>";
  }
}
const char *OpenTherm::protocol_error_to_to_str_(ProtocolErrorType error_type) {
  switch (error_type) {
    TO_STRING_MEMBER(NO_ERROR)
    TO_STRING_MEMBER(NO_TRANSITION)
    TO_STRING_MEMBER(INVALID_STOP_BIT)
    TO_STRING_MEMBER(PARITY_ERROR)
    TO_STRING_MEMBER(NO_CHANGE_TOO_LONG)
    default:
      return "<INVALID>";
  }
}
const char *OpenTherm::message_type_to_str_(MessageType message_type) {
  switch (message_type) {
    TO_STRING_MEMBER(READ_DATA)
    TO_STRING_MEMBER(READ_ACK)
    TO_STRING_MEMBER(WRITE_DATA)
    TO_STRING_MEMBER(WRITE_ACK)
    TO_STRING_MEMBER(INVALID_DATA)
    TO_STRING_MEMBER(DATA_INVALID)
    TO_STRING_MEMBER(UNKNOWN_DATAID)
    default:
      return "<INVALID>";
  }
}

string OpenTherm::debug_data(OpenthermData &data) {
  stringstream result;
  result << bitset<8>(data.type) << " " << bitset<8>(data.id) << " " << bitset<8>(data.valueHB) << " "
         << bitset<8>(data.valueLB) << "\n";
  result << "type: " << message_type_to_str_((MessageType) data.type) << "; ";
  result << "id: " << to_string(data.id) << "; ";
  result << "HB: " << to_string(data.valueHB) << "; ";
  result << "LB: " << to_string(data.valueLB) << "; ";
  result << "uint_16: " << to_string(data.u16()) << "; ";
  result << "float: " << to_string(data.f88());

  return result.str();
}

std::string OpenTherm::debug_error(OpenThermError &error) {
  stringstream result;
  result << "type: " << protocol_error_to_to_str_(error.error_type) << "; ";
  result << "data: ";
  int_to_hex(result, error.data);
  result << "; clock: " << to_string(clock_);
  result << "; capture: " << bitset<32>(error.capture);
  result << "; bit_pos: " << to_string(error.bit_pos);

  return result.str();
}

float OpenthermData::f88() {
  float const value = (int8_t) valueHB;
  return value + (float) valueLB / 256.0;
}

void OpenthermData::f88(float value) {
  if (value >= 0) {
    valueHB = (uint8_t) value;
    float const fraction = (value - valueHB);
    valueLB = fraction * 256.0;
  } else {
    valueHB = (uint8_t) (value - 1);
    float const fraction = (value - valueHB - 1);
    valueLB = fraction * 256.0;
  }
}

uint16_t OpenthermData::u16() {
  uint16_t const value = valueHB;
  return (value << 8) | valueLB;
}

void OpenthermData::u16(uint16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

int16_t OpenthermData::s16() {
  int16_t const value = valueHB;
  return (value << 8) | valueLB;
}

void OpenthermData::s16(int16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

// #ifdef ESP8266
//// 5 kHz timer_
// void OpenTherm::start_read_timer_() {
//   noInterrupts();
//   timer1_attachInterrupt(OpenTherm::timer_isr);
//   timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  // 5MHz (5 ticks/us - 1677721.4 us max)
//   timer1_write(1000);                            // 5kHz
//   interrupts();
// }
//
//// 2 kHz timer_
// void OpenTherm::start_write_timer_() {
//   noInterrupts();
//   timer1_attachInterrupt(OpenTherm::timer_isr);
//   timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  // 5MHz (5 ticks/us - 1677721.4 us max)
//   timer1_write(2500);                            // 2kHz
//   interrupts();
// }
//
//// 1 kHz timer_
// void OpenTherm::_startTimeoutTimer() {
//   noInterrupts();
//   timer1_attachInterrupt(OpenTherm::timer_isr);
//   timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  // 5MHz (5 ticks/us - 1677721.4 us max)
//   timer1_write(5000);                            // 1kHz
//   interrupts();
// }
//
// void OpenTherm::stop_timer_() {
//   noInterrupts();
//   timer1_disable();
//   timer1_detachInterrupt();
//   interrupts();
// }
//
// #endif  // END ESP8266

}  // namespace opentherm
}  // namespace esphome