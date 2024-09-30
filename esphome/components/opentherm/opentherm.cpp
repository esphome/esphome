/*
 * OpenTherm protocol implementation. Originally taken from https://github.com/jpraus/arduino-opentherm, but
 * heavily modified to comply with ESPHome coding standards and provide better logging.
 * Original code is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International
 * Public License, which is compatible with GPLv3 license, which covers C++ part of ESPHome project.
 */

#include "opentherm.h"
#include "esphome/core/helpers.h"
#if defined(ESP32) || defined(USE_ESP_IDF)
#include "driver/timer.h"
#include "esp_err.h"
#endif
#ifdef ESP8266
#include "Arduino.h"
#endif
#include <string>
#include <sstream>
#include <bitset>

namespace esphome {
namespace opentherm {

using std::string;
using std::bitset;
using std::stringstream;
using std::to_string;

static const char *const TAG = "opentherm";

#ifdef ESP8266
OpenTherm *OpenTherm::instance_ = nullptr;
#endif

OpenTherm::OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, int32_t device_timeout)
    : in_pin_(in_pin),
      out_pin_(out_pin),
#if defined(ESP32) || defined(USE_ESP_IDF)
      timer_group_(TIMER_GROUP_0),
      timer_idx_(TIMER_0),
#endif
      mode_(OperationMode::IDLE),
      error_type_(ProtocolErrorType::NO_ERROR),
      capture_(0),
      clock_(0),
      data_(0),
      bit_pos_(0),
      timeout_counter_(-1),
      device_timeout_(device_timeout) {
  this->isr_in_pin_ = in_pin->to_isr();
  this->isr_out_pin_ = out_pin->to_isr();
}

bool OpenTherm::initialize() {
#ifdef ESP8266
  OpenTherm::instance_ = this;
#endif
  this->in_pin_->pin_mode(gpio::FLAG_INPUT);
  this->out_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->out_pin_->digital_write(true);

#if defined(ESP32) || defined(USE_ESP_IDF)
  return this->init_esp32_timer_();
#else
  return true;
#endif
}

void OpenTherm::listen() {
  this->stop_timer_();
  this->timeout_counter_ = this->device_timeout_ * 5;  // timer_ ticks at 5 ticks/ms

  this->mode_ = OperationMode::LISTEN;
  this->data_ = 0;
  this->bit_pos_ = 0;

  this->start_read_timer_();
}

void OpenTherm::send(OpenthermData &data) {
  this->stop_timer_();
  this->data_ = data.type;
  this->data_ = (this->data_ << 12) | data.id;
  this->data_ = (this->data_ << 8) | data.valueHB;
  this->data_ = (this->data_ << 8) | data.valueLB;
  if (!check_parity_(this->data_)) {
    this->data_ = this->data_ | 0x80000000;
  }

  this->clock_ = 1;     // clock starts at HIGH
  this->bit_pos_ = 33;  // count down (33 == start bit, 32-1 data, 0 == stop bit)
  this->mode_ = OperationMode::WRITE;

  this->start_write_timer_();
}

bool OpenTherm::get_message(OpenthermData &data) {
  if (this->mode_ == OperationMode::RECEIVED) {
    data.type = (this->data_ >> 28) & 0x7;
    data.id = (this->data_ >> 16) & 0xFF;
    data.valueHB = (this->data_ >> 8) & 0xFF;
    data.valueLB = this->data_ & 0xFF;
    return true;
  }
  return false;
}

bool OpenTherm::get_protocol_error(OpenThermError &error) {
  if (this->mode_ != OperationMode::ERROR_PROTOCOL) {
    return false;
  }

  error.error_type = this->error_type_;
  error.bit_pos = this->bit_pos_;
  error.capture = this->capture_;
  error.clock = this->clock_;
  error.data = this->data_;

  return true;
}

void OpenTherm::stop() {
  this->stop_timer_();
  this->mode_ = OperationMode::IDLE;
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
        if (arg->bit_pos_ == BitPositions::STOP_BIT) {
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
      // no change for too long, invalid mancheter encoding
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

#ifdef ESP8266
void IRAM_ATTR OpenTherm::esp8266_timer_isr() { OpenTherm::timer_isr(OpenTherm::instance_); }
#endif

void IRAM_ATTR OpenTherm::bit_read_(uint8_t value) {
  this->data_ = (this->data_ << 1) | value;
  this->bit_pos_++;
}

ProtocolErrorType OpenTherm::verify_stop_bit_(uint8_t value) {
  if (value) {  // stop bit detected
    return check_parity_(this->data_) ? ProtocolErrorType::NO_ERROR : ProtocolErrorType::PARITY_ERROR;
  } else {  // no stop bit detected, error
    return ProtocolErrorType::INVALID_STOP_BIT;
  }
}

void IRAM_ATTR OpenTherm::write_bit_(uint8_t high, uint8_t clock) {
  if (clock == 1) {                           // left part of manchester encoding
    this->isr_out_pin_.digital_write(!high);  // low means logical 1 to protocol
  } else {                                    // right part of manchester encoding
    this->isr_out_pin_.digital_write(high);   // high means logical 0 to protocol
  }
}

#if defined(ESP32) || defined(USE_ESP_IDF)

bool OpenTherm::init_esp32_timer_() {
  // Search for a free timer. Maybe unstable, we'll see.
  int cur_timer = 0;
  timer_group_t timer_group = TIMER_GROUP_0;
  timer_idx_t timer_idx = TIMER_0;
  bool timer_found = false;

  for (; cur_timer < SOC_TIMER_GROUP_TOTAL_TIMERS; cur_timer++) {
    timer_config_t temp_config;
    timer_group = cur_timer < 2 ? TIMER_GROUP_0 : TIMER_GROUP_1;
    timer_idx = cur_timer < 2 ? (timer_idx_t) cur_timer : (timer_idx_t) (cur_timer - 2);

    auto err = timer_get_config(timer_group, timer_idx, &temp_config);
    if (err == ESP_ERR_INVALID_ARG) {
      // Error means timer was not initialized (or other things, but we are careful with our args)
      timer_found = true;
      break;
    }

    ESP_LOGD(TAG, "Timer %d:%d seems to be occupied, will try another", timer_group, timer_idx);
  }

  if (!timer_found) {
    ESP_LOGE(TAG, "No free timer was found! OpenTherm cannot function without a timer.");
    return false;
  }

  ESP_LOGD(TAG, "Found free timer %d:%d", timer_group, timer_idx);
  this->timer_group_ = timer_group;
  this->timer_idx_ = timer_idx;

  timer_config_t const config = {
    .alarm_en = TIMER_ALARM_EN,
    .counter_en = TIMER_PAUSE,
    .intr_type = TIMER_INTR_LEVEL,
    .counter_dir = TIMER_COUNT_UP,
    .auto_reload = TIMER_AUTORELOAD_EN,
#if ESP_IDF_VERSION_MAJOR >= 5
    .clk_src = TIMER_SRC_CLK_DEFAULT,
#endif
    .divider = 80,
  };

  esp_err_t result;

  result = timer_init(this->timer_group_, this->timer_idx_, &config);
  if (result != ESP_OK) {
    const auto *error = esp_err_to_name(result);
    ESP_LOGE(TAG, "Failed to init timer. Error: %s", error);
    return false;
  }

  result = timer_set_counter_value(this->timer_group_, this->timer_idx_, 0);
  if (result != ESP_OK) {
    const auto *error = esp_err_to_name(result);
    ESP_LOGE(TAG, "Failed to set counter value. Error: %s", error);
    return false;
  }

  result = timer_isr_callback_add(this->timer_group_, this->timer_idx_, reinterpret_cast<bool (*)(void *)>(timer_isr),
                                  this, 0);
  if (result != ESP_OK) {
    const auto *error = esp_err_to_name(result);
    ESP_LOGE(TAG, "Failed to register timer interrupt. Error: %s", error);
    return false;
  }

  return true;
}

void IRAM_ATTR OpenTherm::start_esp32_timer_(uint64_t alarm_value) {
  esp_err_t result;

  result = timer_set_alarm_value(this->timer_group_, this->timer_idx_, alarm_value);
  if (result != ESP_OK) {
    const auto *error = esp_err_to_name(result);
    ESP_LOGE(TAG, "Failed to set alarm value. Error: %s", error);
    return;
  }

  result = timer_start(this->timer_group_, this->timer_idx_);
  if (result != ESP_OK) {
    const auto *error = esp_err_to_name(result);
    ESP_LOGE(TAG, "Failed to start the timer. Error: %s", error);
    return;
  }
}

// 5 kHz timer_
void IRAM_ATTR OpenTherm::start_read_timer_() {
  InterruptLock const lock;
  this->start_esp32_timer_(200);
}

// 2 kHz timer_
void IRAM_ATTR OpenTherm::start_write_timer_() {
  InterruptLock const lock;
  this->start_esp32_timer_(500);
}

void IRAM_ATTR OpenTherm::stop_timer_() {
  InterruptLock const lock;

  esp_err_t result;

  result = timer_pause(this->timer_group_, this->timer_idx_);
  if (result != ESP_OK) {
    const auto *error = esp_err_to_name(result);
    ESP_LOGE(TAG, "Failed to pause the timer. Error: %s", error);
    return;
  }

  result = timer_set_counter_value(this->timer_group_, this->timer_idx_, 0);
  if (result != ESP_OK) {
    const auto *error = esp_err_to_name(result);
    ESP_LOGE(TAG, "Failed to set timer counter to 0 after pausing. Error: %s", error);
    return;
  }
}

#endif  // END ESP32

#ifdef ESP8266
// 5 kHz timer_
void OpenTherm::start_read_timer_() {
  InterruptLock const lock;
  timer1_attachInterrupt(OpenTherm::esp8266_timer_isr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  // 5MHz (5 ticks/us - 1677721.4 us max)
  timer1_write(1000);                            // 5kHz
}

// 2 kHz timer_
void OpenTherm::start_write_timer_() {
  InterruptLock const lock;
  timer1_attachInterrupt(OpenTherm::esp8266_timer_isr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  // 5MHz (5 ticks/us - 1677721.4 us max)
  timer1_write(2500);                            // 2kHz
}

void OpenTherm::stop_timer_() {
  InterruptLock const lock;
  timer1_disable();
  timer1_detachInterrupt();
}

#endif  // END ESP8266

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

const char *OpenTherm::operation_mode_to_str(OperationMode mode) {
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
const char *OpenTherm::protocol_error_to_to_str(ProtocolErrorType error_type) {
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
const char *OpenTherm::message_type_to_str(MessageType message_type) {
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

const char *OpenTherm::message_id_to_str(MessageId id) {
  switch (id) {
    TO_STRING_MEMBER(STATUS)
    TO_STRING_MEMBER(CH_SETPOINT)
    TO_STRING_MEMBER(CONTROLLER_CONFIG)
    TO_STRING_MEMBER(DEVICE_CONFIG)
    TO_STRING_MEMBER(COMMAND_CODE)
    TO_STRING_MEMBER(FAULT_FLAGS)
    TO_STRING_MEMBER(REMOTE)
    TO_STRING_MEMBER(COOLING_CONTROL)
    TO_STRING_MEMBER(CH2_SETPOINT)
    TO_STRING_MEMBER(CH_SETPOINT_OVERRIDE)
    TO_STRING_MEMBER(TSP_COUNT)
    TO_STRING_MEMBER(TSP_COMMAND)
    TO_STRING_MEMBER(FHB_SIZE)
    TO_STRING_MEMBER(FHB_COMMAND)
    TO_STRING_MEMBER(MAX_MODULATION_LEVEL)
    TO_STRING_MEMBER(MAX_BOILER_CAPACITY)
    TO_STRING_MEMBER(ROOM_SETPOINT)
    TO_STRING_MEMBER(MODULATION_LEVEL)
    TO_STRING_MEMBER(CH_WATER_PRESSURE)
    TO_STRING_MEMBER(DHW_FLOW_RATE)
    TO_STRING_MEMBER(DAY_TIME)
    TO_STRING_MEMBER(DATE)
    TO_STRING_MEMBER(YEAR)
    TO_STRING_MEMBER(ROOM_SETPOINT_CH2)
    TO_STRING_MEMBER(ROOM_TEMP)
    TO_STRING_MEMBER(FEED_TEMP)
    TO_STRING_MEMBER(DHW_TEMP)
    TO_STRING_MEMBER(OUTSIDE_TEMP)
    TO_STRING_MEMBER(RETURN_WATER_TEMP)
    TO_STRING_MEMBER(SOLAR_STORE_TEMP)
    TO_STRING_MEMBER(SOLAR_COLLECT_TEMP)
    TO_STRING_MEMBER(FEED_TEMP_CH2)
    TO_STRING_MEMBER(DHW2_TEMP)
    TO_STRING_MEMBER(EXHAUST_TEMP)
    TO_STRING_MEMBER(FAN_SPEED)
    TO_STRING_MEMBER(FLAME_CURRENT)
    TO_STRING_MEMBER(DHW_BOUNDS)
    TO_STRING_MEMBER(CH_BOUNDS)
    TO_STRING_MEMBER(OTC_CURVE_BOUNDS)
    TO_STRING_MEMBER(DHW_SETPOINT)
    TO_STRING_MEMBER(MAX_CH_SETPOINT)
    TO_STRING_MEMBER(OTC_CURVE_RATIO)
    TO_STRING_MEMBER(HVAC_STATUS)
    TO_STRING_MEMBER(REL_VENT_SETPOINT)
    TO_STRING_MEMBER(DEVICE_VENT)
    TO_STRING_MEMBER(REL_VENTILATION)
    TO_STRING_MEMBER(REL_HUMID_EXHAUST)
    TO_STRING_MEMBER(SUPPLY_INLET_TEMP)
    TO_STRING_MEMBER(SUPPLY_OUTLET_TEMP)
    TO_STRING_MEMBER(EXHAUST_INLET_TEMP)
    TO_STRING_MEMBER(EXHAUST_OUTLET_TEMP)
    TO_STRING_MEMBER(NOM_REL_VENTILATION)
    TO_STRING_MEMBER(OVERRIDE_FUNC)
    TO_STRING_MEMBER(OEM_DIAGNOSTIC)
    TO_STRING_MEMBER(BURNER_STARTS)
    TO_STRING_MEMBER(CH_PUMP_STARTS)
    TO_STRING_MEMBER(DHW_PUMP_STARTS)
    TO_STRING_MEMBER(DHW_BURNER_STARTS)
    TO_STRING_MEMBER(BURNER_HOURS)
    TO_STRING_MEMBER(CH_PUMP_HOURS)
    TO_STRING_MEMBER(DHW_PUMP_HOURS)
    TO_STRING_MEMBER(DHW_BURNER_HOURS)
    TO_STRING_MEMBER(OT_VERSION_CONTROLLER)
    TO_STRING_MEMBER(OT_VERSION_DEVICE)
    TO_STRING_MEMBER(VERSION_CONTROLLER)
    TO_STRING_MEMBER(VERSION_DEVICE)
    default:
      return "<INVALID>";
  }
}

string OpenTherm::debug_data(OpenthermData &data) {
  stringstream result;
  result << bitset<8>(data.type) << " " << bitset<8>(data.id) << " " << bitset<8>(data.valueHB) << " "
         << bitset<8>(data.valueLB) << "\n";
  result << "type: " << this->message_type_to_str((MessageType) data.type) << "; ";
  result << "id: " << to_string(data.id) << "; ";
  result << "HB: " << to_string(data.valueHB) << "; ";
  result << "LB: " << to_string(data.valueLB) << "; ";
  result << "uint_16: " << to_string(data.u16()) << "; ";
  result << "float: " << to_string(data.f88());

  return result.str();
}
std::string OpenTherm::debug_error(OpenThermError &error) {
  stringstream result;
  result << "type: " << this->protocol_error_to_to_str(error.error_type) << "; ";
  result << "data: ";
  result << format_hex(error.data);
  result << "; clock: " << to_string(clock_);
  result << "; capture: " << bitset<32>(error.capture);
  result << "; bit_pos: " << to_string(error.bit_pos);

  return result.str();
}

float OpenthermData::f88() { return ((float) this->s16()) / 256.0; }

void OpenthermData::f88(float value) { this->s16((int16_t) (value * 256)); }

uint16_t OpenthermData::u16() {
  uint16_t const value = this->valueHB;
  return (value << 8) | this->valueLB;
}

void OpenthermData::u16(uint16_t value) {
  this->valueLB = value & 0xFF;
  this->valueHB = (value >> 8) & 0xFF;
}

int16_t OpenthermData::s16() {
  int16_t const value = this->valueHB;
  return (value << 8) | this->valueLB;
}

void OpenthermData::s16(int16_t value) {
  this->valueLB = value & 0xFF;
  this->valueHB = (value >> 8) & 0xFF;
}

}  // namespace opentherm
}  // namespace esphome
