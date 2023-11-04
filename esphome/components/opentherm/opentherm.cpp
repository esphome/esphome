#include "opentherm.h"
#include "esphome/core/helpers.h"
#include "driver/timer.h"

namespace esphome {
namespace opentherm {

OpenTherm::OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, int32_t slave_timeout)
    : in_pin_(in_pin),
      out_pin_(out_pin),
      mode_(OperationMode::IDLE),
      capture_(0),
      clock_(0),
      bit_pos_(0),
      data_(0),
      active_(false),
      timeout_counter_(-1),
      slave_timeout_(slave_timeout),
      timer_initialized_(false) {
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

  listen_();
}

void OpenTherm::listen_() {
  stop_timer_();
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

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantFunctionResult"
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
        // TODO: ADD ERROR
        arg->listen_();
      } else if (arg->clock_ == 1 || arg->capture_ > 0xF) {
        // transition in the middle of the bit OR no transition between two bit, both are valid data points
        if (arg->bit_pos_ == BitPositions::STOP_BIT) {
          // expecting stop bit
          if (arg->verify_stop_bit_(last)) {
            arg->mode_ = OperationMode::RECEIVED;
            arg->stop_();
          } else {
            // TODO: ADD ERROR
            // end of data not verified, invalid data
            arg->listen_();
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
      // TODO: ADD ERROR
      arg->listen_();
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
#pragma clang diagnostic pop

void IRAM_ATTR OpenTherm::bit_read_(uint8_t value) {
  data_ = (data_ << 1) | value;
  bit_pos_++;
}

bool IRAM_ATTR OpenTherm::verify_stop_bit_(uint8_t value) {
  if (value) {  // stop bit detected
    return check_parity_(data_);
  } else {  // no stop bit detected, error
    return false;
  }
}

void IRAM_ATTR OpenTherm::write_bit_(uint8_t high, uint8_t clock) {
  if (clock == 1) {                     // left part of manchester encoding
    isr_out_pin_.digital_write(!high);  // low means logical 1 to protocol
  } else {                              // right part of manchester encoding
    isr_out_pin_.digital_write(high);   // high means logical 0 to protocol
  }
}

bool OpenTherm::has_message() { return mode_ == OperationMode::RECEIVED; }

bool OpenTherm::is_sent() { return mode_ == OperationMode::SENT; }

bool OpenTherm::is_idle() { return mode_ == OperationMode::IDLE; }

bool OpenTherm::is_error() { return mode_ == OperationMode::ERROR_TIMEOUT; }

#ifdef ESP32

void IRAM_ATTR OpenTherm::init_timer_() {
  if (timer_initialized_)
    return;

  timer_config_t const config = {
      .alarm_en = TIMER_ALARM_DIS,
      .counter_en = TIMER_PAUSE,
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
  timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, reinterpret_cast<bool (*)(void *)>(timer_isr), this, 0);
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

#endif  // END ESP32

// https://stackoverflow.com/questions/21617970/how-to-check-if-value-has-even-parity-of-bits-or-odd
bool OpenTherm::check_parity_(uint32_t val) {
  val ^= val >> 16;
  val ^= val >> 8;
  val ^= val >> 4;
  val ^= val >> 2;
  val ^= val >> 1;
  return (~val) & 1;
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