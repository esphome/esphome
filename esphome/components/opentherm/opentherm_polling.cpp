#include "opentherm_polling.h"

#ifdef USE_ESP32
#include "esphome/core/hal.h"
#include <cinttypes>
#endif

#if !(defined(USE_ESP32) && SOC_RMT_SUPPORTED)

namespace esphome::opentherm {

ESPHOME_LOG_TAG(TAG, "opentherm");

#ifdef ESP8266
OpenThermPolling *OpenThermPolling::instance = nullptr;
#endif

bool OpenThermPolling::initialize_impl_() {
#ifdef ESP8266
  OpenThermPolling::instance = this;
  return true;
#else
  return this->init_esp32_timer_();
#endif
}

void OpenThermPolling::listen_impl_() {
  if (this->listening_) {
    return;  // Already sampling for the answer.
  }
  this->listening_ = true;
  this->stop_timer_();
  this->start_read_timer_();
}

void OpenThermPolling::send_impl_() {
  this->listening_ = false;
  this->stop_timer_();
  this->start_write_timer_();
}

void OpenThermPolling::stop_impl_() {
  this->listening_ = false;
  this->stop_timer_();
  this->out_pin_->digital_write(true);
}

bool OpenThermPolling::wait_until_done(uint32_t timeout) {
  // Progress comes from the sampling and bit-flipping interrupts, so this only has to wait. Servicing the loop here
  // would not advance anything, and not running other components is the whole point of the sync mode. Stop when the
  // stage ends: between transmitting and listening the receiver still has to be armed from the loop.
  const OperationMode stage = this->mode_;
  auto const start_time = millis();
  while (this->mode_ == stage) {
    yield();
    if (millis() - start_time >= timeout) {
      return false;
    }
  }
  return true;
}

void IRAM_ATTR OpenThermPolling::read_() {
  this->data_ = 0;
  this->bit_pos_ = 0;
  this->mode_ = OperationMode::READ;
  this->capture_ = 1;         // reset counter and add as if read start bit
  this->clock_ = 1;           // clock is high at the start of comm
  this->start_read_timer_();  // get us into 1/4 of manchester code. 5 timer ticks constitute 1 ms, which is 1 bit
                              // period in OpenTherm.
}

#ifdef USE_ESP32
bool IRAM_ATTR OpenThermPolling::timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata,
                                           void *user_ctx) {
  auto *arg = static_cast<OpenThermPolling *>(user_ctx);
#else
bool IRAM_ATTR OpenThermPolling::timer_isr(OpenThermPolling *arg) {
#endif
  if (arg->mode_ == OperationMode::LISTEN) {
    bool const value = arg->isr_in_pin_.digital_read();
    if (value) {  // incoming data (rising signal)
      arg->read_();
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
          auto stop_bit_error = arg->check_frame_(arg->data_, true, last != 0);
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
    // bit_pos_ counts the frame down from the start bit at 33 to the stop bit at 0; clock_ 1 is the left half.
    arg->write_half_bit_(FRAME_BITS - 1 - arg->bit_pos_, arg->clock_ == 1 ? 0 : 1);
    if (arg->clock_ == 1) {
      arg->clock_ = 0;  // the right half of this bit comes next
    } else if (arg->bit_pos_ > 0) {
      arg->bit_pos_--;  // this bit is complete
      arg->clock_ = 1;
    } else {
      // Right half of the stop bit: the request is on the line and the conversation moves to listening.
      arg->transmitted();
    }
  }

  return false;
}

#ifdef ESP8266
void IRAM_ATTR OpenThermPolling::esp8266_timer_isr() { OpenThermPolling::timer_isr(OpenThermPolling::instance); }
#endif

void IRAM_ATTR OpenThermPolling::bit_read_(uint8_t value) {
  this->data_ = (this->data_ << 1) | value;
  this->bit_pos_++;
}

void IRAM_ATTR OpenThermPolling::write_half_bit_(uint8_t bit, uint8_t half) {
  this->isr_out_pin_.digital_write(half_bit_level_(this->data_, bit, half));
}

#ifdef USE_ESP32
bool OpenThermPolling::init_esp32_timer_() {
  // 80MHz / 80 = 1MHz resolution (1µs per tick)
  gptimer_config_t config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,
  };

  esp_err_t result = gptimer_new_timer(&config, &this->timer_handle_);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(result));
    return false;
  }

  gptimer_event_callbacks_t cbs = {
      .on_alarm = OpenThermPolling::timer_isr,
  };
  result = gptimer_register_event_callbacks(this->timer_handle_, &cbs, this);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register timer callback: %s", esp_err_to_name(result));
    gptimer_del_timer(this->timer_handle_);
    this->timer_handle_ = nullptr;
    return false;
  }

  result = gptimer_enable(this->timer_handle_);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable timer: %s", esp_err_to_name(result));
    gptimer_del_timer(this->timer_handle_);
    this->timer_handle_ = nullptr;
    return false;
  }

  return true;
}
#endif

#ifdef USE_ESP32

void IRAM_ATTR OpenThermPolling::start_esp32_timer_(uint64_t alarm_value) {
  // We will report timer errors outside of interrupt handler
  this->timer_error_ = ESP_OK;
  this->timer_error_type_ = TimerErrorType::NO_TIMER_ERROR;

  this->alarm_config_.alarm_count = alarm_value;
  this->timer_error_ = gptimer_set_alarm_action(this->timer_handle_, &this->alarm_config_);
  if (this->timer_error_ != ESP_OK) {
    this->timer_error_type_ = TimerErrorType::SET_ALARM_VALUE_ERROR;
    return;
  }
  this->timer_error_ = gptimer_start(this->timer_handle_);
  if (this->timer_error_ != ESP_OK) {
    this->timer_error_type_ = TimerErrorType::TIMER_START_ERROR;
  }
}

void OpenThermPolling::report_and_reset_timer_error() {
  if (this->timer_error_ == ESP_OK) {
    return;
  }

  ESP_LOGE(TAG, "Error occurred while manipulating timer (%s): %s", this->timer_error_to_str(this->timer_error_type_),
           esp_err_to_name(this->timer_error_));

  this->timer_error_ = ESP_OK;
  this->timer_error_type_ = NO_TIMER_ERROR;
}

// 5 kHz timer_
void IRAM_ATTR OpenThermPolling::start_read_timer_() {
  InterruptLock const lock;
  this->start_esp32_timer_(200);
}

// 2 kHz timer_
void IRAM_ATTR OpenThermPolling::start_write_timer_() {
  InterruptLock const lock;
  this->start_esp32_timer_(500);
}

void IRAM_ATTR OpenThermPolling::stop_timer_() {
  InterruptLock const lock;
  // We will report timer errors outside of interrupt handler
  this->timer_error_ = ESP_OK;
  this->timer_error_type_ = TimerErrorType::NO_TIMER_ERROR;

  this->timer_error_ = gptimer_stop(this->timer_handle_);
  if (this->timer_error_ != ESP_OK) {
    this->timer_error_type_ = TimerErrorType::TIMER_PAUSE_ERROR;
    return;
  }
  this->timer_error_ = gptimer_set_raw_count(this->timer_handle_, 0);
  if (this->timer_error_ != ESP_OK) {
    this->timer_error_type_ = TimerErrorType::SET_COUNTER_VALUE_ERROR;
  }
}

#endif  // USE_ESP32

#ifdef ESP8266
// 5 kHz timer_
void IRAM_ATTR OpenThermPolling::start_read_timer_() {
  InterruptLock const lock;
  timer1_attachInterrupt(OpenThermPolling::esp8266_timer_isr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  // 5MHz (5 ticks/us - 1677721.4 us max)
  timer1_write(1000);                            // 5kHz
}

// 2 kHz timer_
void IRAM_ATTR OpenThermPolling::start_write_timer_() {
  InterruptLock const lock;
  timer1_attachInterrupt(OpenThermPolling::esp8266_timer_isr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  // 5MHz (5 ticks/us - 1677721.4 us max)
  timer1_write(2500);                            // 2kHz
}

void IRAM_ATTR OpenThermPolling::stop_timer_() {
  InterruptLock const lock;
  timer1_disable();
  timer1_detachInterrupt();
}

// There is nothing to report on ESP8266
void OpenThermPolling::report_and_reset_timer_error() {}

#endif  // END ESP8266

}  // namespace esphome::opentherm

#endif  // !(USE_ESP32 && SOC_RMT_SUPPORTED)
