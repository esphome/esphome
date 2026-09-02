#include "datalink.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32
#include "esp_err.h"
#endif
#ifdef ESP8266
#include "Arduino.h"
#endif

namespace esphome::opentherm42 {

#define TO_STRING_CASE(name) \
  case name: \
    return #name;

const char *data_link_error_to_string(DataLinkError error) {
  switch (error) {
    TO_STRING_CASE(DataLinkError::NONE)
    TO_STRING_CASE(DataLinkError::MANCHESTER_NO_TRANSITION)
    TO_STRING_CASE(DataLinkError::MANCHESTER_TIMEOUT)
    TO_STRING_CASE(DataLinkError::INVALID_STOP_BIT)
    TO_STRING_CASE(DataLinkError::PARITY_ERROR)
    TO_STRING_CASE(DataLinkError::RESPONSE_TIMEOUT)
    TO_STRING_CASE(DataLinkError::TIMER_ERROR)
    default:
      return "<INVALID>";
  }
}

const char *timer_error_to_string(TimerError error) {
  switch (error) {
    TO_STRING_CASE(TimerError::NONE)
    TO_STRING_CASE(TimerError::CREATE)
    TO_STRING_CASE(TimerError::REGISTER_CALLBACK)
    TO_STRING_CASE(TimerError::ENABLE)
    TO_STRING_CASE(TimerError::SET_ALARM)
    TO_STRING_CASE(TimerError::START)
    TO_STRING_CASE(TimerError::STOP)
    default:
      return "<INVALID>";
  }
}

#ifdef ESP8266
static OpenThermDataLink *instance = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

OpenThermDataLink::OpenThermDataLink(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin)
    : in_pin_(in_pin), out_pin_(out_pin) {
  this->isr_in_pin_ = in_pin->to_isr();
  this->isr_out_pin_ = out_pin->to_isr();
}

bool OpenThermDataLink::initialize() {
#ifdef ESP8266
  instance = this;
#endif
  this->in_pin_->pin_mode(gpio::FLAG_INPUT);
  this->in_pin_->setup();
  this->out_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->out_pin_->setup();
  this->out_pin_->digital_write(true);  // idle level

#ifdef USE_ESP32
  gptimer_config_t const config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,  // 1 µs per tick
  };
  if (gptimer_new_timer(&config, &this->timer_handle_) != ESP_OK) {
    this->timer_error_ = TimerError::CREATE;
    this->state_ = DataLinkState::ERROR;
    this->error_ = DataLinkError::TIMER_ERROR;
    return false;
  }
  gptimer_event_callbacks_t const callbacks = {.on_alarm = OpenThermDataLink::timer_isr};
  if (gptimer_register_event_callbacks(this->timer_handle_, &callbacks, this) != ESP_OK) {
    this->timer_error_ = TimerError::REGISTER_CALLBACK;
    this->state_ = DataLinkState::ERROR;
    this->error_ = DataLinkError::TIMER_ERROR;
    gptimer_del_timer(this->timer_handle_);
    this->timer_handle_ = nullptr;
    return false;
  }
  if (gptimer_enable(this->timer_handle_) != ESP_OK) {
    this->timer_error_ = TimerError::ENABLE;
    this->state_ = DataLinkState::ERROR;
    this->error_ = DataLinkError::TIMER_ERROR;
    gptimer_del_timer(this->timer_handle_);
    this->timer_handle_ = nullptr;
    return false;
  }
#endif
  return true;
}

void OpenThermDataLink::listen(uint32_t response_timeout_ms) {
  InterruptLock const lock;
  this->state_ = DataLinkState::LISTENING;
  this->listen_ticks_remaining_ = static_cast<int32_t>(response_timeout_ms) * 5;  // 5 ticks/ms at 5 kHz
  this->capture_ = 0;
  this->bit_pos_ = 0;

#ifdef USE_ESP32
  gptimer_alarm_config_t const alarm = {.alarm_count = 200, .flags = {.auto_reload_on_alarm = true}};
  if (gptimer_set_alarm_action(this->timer_handle_, &alarm) != ESP_OK) {
    this->timer_error_ = TimerError::SET_ALARM;
    this->state_ = DataLinkState::ERROR;
    this->error_ = DataLinkError::TIMER_ERROR;
    return;
  }
  if (gptimer_start(this->timer_handle_) != ESP_OK) {
    this->timer_error_ = TimerError::START;
    this->state_ = DataLinkState::ERROR;
    this->error_ = DataLinkError::TIMER_ERROR;
  }
#else
  timer1_attachInterrupt(OpenThermDataLink::timer_isr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  // 5 MHz base (5 ticks/µs)
  timer1_write(1000);                            // 5 kHz: fires every 200 µs
#endif
}

void OpenThermDataLink::send(const Frame &frame) {
  InterruptLock const lock;
  // §4.2: P(1) MSG-TYPE(3) SPARE(4) | DATA-ID(8) | DATA-VALUE(16), MSB-first.
  this->tx_data_ = (static_cast<uint32_t>(frame.type) << 28) | (static_cast<uint32_t>(frame.id) << 16) |
                   (static_cast<uint32_t>(frame.value_hb) << 8) | frame.value_lb;
  if (!check_parity_(this->tx_data_)) {
    this->tx_data_ |= 0x80000000;  // set P so the total '1' count across the 32 bits is even
  }

  this->tx_clock_ = 1;
  this->tx_bit_pos_ = 33;  // 33 = start bit, 32..1 = data bits, 0 = stop bit
  this->state_ = DataLinkState::SENDING;

#ifdef USE_ESP32
  gptimer_alarm_config_t const alarm = {.alarm_count = 500, .flags = {.auto_reload_on_alarm = true}};
  if (gptimer_set_alarm_action(this->timer_handle_, &alarm) != ESP_OK) {
    this->timer_error_ = TimerError::SET_ALARM;
    this->state_ = DataLinkState::ERROR;
    this->error_ = DataLinkError::TIMER_ERROR;
    return;
  }
  if (gptimer_start(this->timer_handle_) != ESP_OK) {
    this->timer_error_ = TimerError::START;
    this->state_ = DataLinkState::ERROR;
    this->error_ = DataLinkError::TIMER_ERROR;
  }
#else
  timer1_attachInterrupt(OpenThermDataLink::timer_isr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(2500);  // 2 kHz: fires every 500 µs
#endif
}

void OpenThermDataLink::stop_timer_() {
  InterruptLock const lock;
#ifdef USE_ESP32
  if (this->timer_handle_ != nullptr) {
    if (gptimer_stop(this->timer_handle_) != ESP_OK) {
      this->timer_error_ = TimerError::STOP;
    } else if (gptimer_set_raw_count(this->timer_handle_, 0) != ESP_OK) {
      this->timer_error_ = TimerError::STOP;
    }
  }
#else
  timer1_disable();
  timer1_detachInterrupt();
#endif
}

void OpenThermDataLink::stop() {
  this->stop_timer_();
  this->state_ = DataLinkState::IDLE;
  this->out_pin_->digital_write(true);  // idle level
}

void IRAM_ATTR OpenThermDataLink::set_error_(DataLinkError error) {
  this->state_ = DataLinkState::ERROR;
  this->error_ = error;
}

#ifdef USE_ESP32
bool IRAM_ATTR OpenThermDataLink::timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata,
                                            void *user_ctx) {
  static_cast<OpenThermDataLink *>(user_ctx)->on_timer_tick_();
  return false;
}
#else
void IRAM_ATTR OpenThermDataLink::timer_isr() { instance->on_timer_tick_(); }
#endif

void IRAM_ATTR OpenThermDataLink::on_timer_tick_() {
  if (this->state_ == DataLinkState::LISTENING) {
    if (this->listen_ticks_remaining_ == 0) {
      this->set_error_(DataLinkError::RESPONSE_TIMEOUT);
      this->stop_timer_();
      return;
    }
    bool const value = this->isr_in_pin_.digital_read();
    if (value) {  // start bit: rising edge from the idle-low line
      this->state_ = DataLinkState::RECEIVING;
      this->data_ = 0;
      this->bit_pos_ = 0;
      this->capture_ = 1;  // as if the start bit was already captured
      this->clock_ = 1;
    }
    if (this->listen_ticks_remaining_ > 0) {
      this->listen_ticks_remaining_--;
    }
  } else if (this->state_ == DataLinkState::RECEIVING) {
    bool const value = this->isr_in_pin_.digital_read();
    uint8_t const last = this->capture_ & 1;
    if (value != last) {
      // The line changed since the previous 200 µs sample -- a transition.
      if (this->clock_ == 1 && this->capture_ > 0xF) {
        // We were waiting for the mandatory mid-bit transition, but too long (>800 µs) passed: it never
        // came where Manchester encoding requires it.
        this->set_error_(DataLinkError::MANCHESTER_NO_TRANSITION);
        this->stop_timer_();
        return;
      }
      if (this->clock_ == 1 || this->capture_ > 0xF) {
        // Either the expected mid-bit transition arrived on time, or enough time passed (>800 µs) that a
        // transition now must be a data point rather than noise -- both are valid places to sample a bit.
        if (this->bit_pos_ == 33) {
          // 33 data+parity bits captured; this transition should be the stop bit.
          auto const stop_bit_error = this->check_stop_bit_(last);
          if (stop_bit_error == DataLinkError::NONE) {
            this->state_ = DataLinkState::RECEIVED;
            this->frame_.type = (this->data_ >> 28) & 0x7;
            this->frame_.id = (this->data_ >> 16) & 0xFF;
            this->frame_.value_hb = (this->data_ >> 8) & 0xFF;
            this->frame_.value_lb = this->data_ & 0xFF;
            this->stop_timer_();
          } else {
            this->set_error_(stop_bit_error);
            this->stop_timer_();
          }
          return;
        }
        this->record_bit_(last);
        this->clock_ = 0;
      } else {
        // A boundary transition (not a data point) -- now wait for the next mid-bit transition.
        this->clock_ = 1;
      }
      this->capture_ = 1;
    } else if (this->capture_ > 0xFF) {
      // No transition at all for >1600 µs: the line is stuck, well past even the loosest legal bit
      // period (§3.3.2: 900-1150 µs between mid-bit transitions).
      this->set_error_(DataLinkError::MANCHESTER_TIMEOUT);
      this->stop_timer_();
      return;
    }
    this->capture_ = (this->capture_ << 1) | value;
  } else if (this->state_ == DataLinkState::SENDING) {
    if (this->tx_bit_pos_ == 33 || this->tx_bit_pos_ == 0) {
      this->write_bit_(1, this->tx_clock_);  // start bit / stop bit: always '1'
    } else {
      this->write_bit_((this->tx_data_ >> (this->tx_bit_pos_ - 1)) & 1, this->tx_clock_);
    }
    if (this->tx_clock_ == 0) {
      if (this->tx_bit_pos_ <= 0) {
        this->state_ = DataLinkState::SENT;
        this->stop_timer_();
        return;
      }
      this->tx_bit_pos_--;
      this->tx_clock_ = 1;
    } else {
      this->tx_clock_ = 0;
    }
  }
}

void IRAM_ATTR OpenThermDataLink::record_bit_(uint8_t value) {
  this->data_ = (this->data_ << 1) | value;
  this->bit_pos_++;
}

DataLinkError IRAM_ATTR OpenThermDataLink::check_stop_bit_(uint8_t value) {
  if (!value) {
    return DataLinkError::INVALID_STOP_BIT;
  }
  return check_parity_(this->data_) ? DataLinkError::NONE : DataLinkError::PARITY_ERROR;
}

void IRAM_ATTR OpenThermDataLink::write_bit_(uint8_t high, uint8_t clock) {
  if (clock == 1) {                           // first half of the Manchester bit
    this->isr_out_pin_.digital_write(!high);  // line low means logical 1
  } else {                                    // second half
    this->isr_out_pin_.digital_write(high);   // line high means logical 0
  }
}

// §4.2.1: the parity bit is set/cleared such that the total number of '1' bits across the whole 32-bit
// frame is even. https://graphics.stanford.edu/~seander/bithacks.html#ParityParallel
bool OpenThermDataLink::check_parity_(uint32_t frame_bits) {
  frame_bits ^= frame_bits >> 16;
  frame_bits ^= frame_bits >> 8;
  frame_bits ^= frame_bits >> 4;
  frame_bits &= 0xF;
  return ((0x6996 >> frame_bits) & 1) == 0;
}

}  // namespace esphome::opentherm42
