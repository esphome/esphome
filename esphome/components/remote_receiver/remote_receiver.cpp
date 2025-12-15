#include "remote_receiver.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#if defined(USE_LIBRETINY) || defined(USE_ESP8266) || defined(USE_RP2040)

namespace esphome::remote_receiver {

static const char *const TAG = "remote_receiver";

static void IRAM_ATTR HOT write_value(RemoteReceiverComponentStore *arg, uint32_t delta, bool level) {
  // convert level to -1 or +1 and write the delta to the buffer
  int32_t multiplier = ((int32_t) level << 1) - 1;
  uint32_t buffer_write = arg->buffer_write;
  arg->buffer[buffer_write++] = (int32_t) delta * multiplier;
  if (buffer_write >= arg->buffer_size) {
    buffer_write = 0;
  }

  // detect overflow and reset the write pointer
  if (buffer_write == arg->buffer_read) {
    buffer_write = arg->buffer_start;
    arg->overflow = true;
  }

  // detect idle and start a new sequence unless there is only idle in
  // which case reset the write pointer instead
  if (delta >= arg->idle_us) {
    if (arg->buffer_write == arg->buffer_start) {
      buffer_write = arg->buffer_start;
    } else {
      arg->buffer_start = buffer_write;
    }
  }
  arg->buffer_write = buffer_write;
}

static void IRAM_ATTR HOT commit_value(RemoteReceiverComponentStore *arg, uint32_t micros, bool level) {
  // commit value if the level is different from the last commit level
  if (level != arg->commit_level) {
    write_value(arg, micros - arg->commit_micros, level);
    arg->commit_micros = micros;
    arg->commit_level = level;
  }
}

void IRAM_ATTR HOT RemoteReceiverComponentStore::gpio_intr(RemoteReceiverComponentStore *arg) {
  // invert the level so it matches the level of the signal before the edge
  const bool curr_level = !arg->pin.digital_read();
  const uint32_t curr_micros = micros();
  const bool prev_level = arg->prev_level;
  const uint32_t prev_micros = arg->prev_micros;

  // commit the previous value if the pulse is not filtered and the level is different
  if (curr_micros - prev_micros >= arg->filter_us && prev_level != curr_level) {
    commit_value(arg, prev_micros, prev_level);
  }
  arg->prev_micros = curr_micros;
  arg->prev_level = curr_level;
}

void RemoteReceiverComponent::setup() {
  this->pin_->setup();
  this->store_.idle_us = this->idle_us_;
  this->store_.filter_us = this->filter_us_;
  this->store_.pin = this->pin_->to_isr();
  this->store_.buffer = new int32_t[this->buffer_size_];
  this->store_.buffer_size = this->buffer_size_;
  this->store_.prev_micros = micros();
  this->store_.commit_micros = this->store_.prev_micros;
  this->store_.prev_level = this->pin_->digital_read();
  this->store_.commit_level = this->store_.prev_level;
  this->pin_->attach_interrupt(RemoteReceiverComponentStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
  this->high_freq_.start();
}

void RemoteReceiverComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Remote Receiver:");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG,
                "  Buffer Size: %u\n"
                "  Tolerance: %u%s\n"
                "  Filter out pulses shorter than: %u us\n"
                "  Signal is done after %u us of no changes",
                this->buffer_size_, this->tolerance_,
                (this->tolerance_mode_ == remote_base::TOLERANCE_MODE_TIME) ? " us" : "%", this->filter_us_,
                this->idle_us_);
}

void RemoteReceiverComponent::loop() {
  // check for overflow
  auto &s = this->store_;
  if (s.overflow) {
    ESP_LOGW(TAG, "Buffer overflow");
    s.overflow = false;
  }

  // if no data is available check for uncommitted data stuck in the buffer and commit
  // the previous value if needed
  uint32_t last_index = s.buffer_start;
  if (last_index == s.buffer_read) {
    InterruptLock lock;
    if (s.buffer_read == s.buffer_start && s.buffer_write != s.buffer_start &&
        micros() - s.prev_micros >= this->idle_us_) {
      commit_value(&s, s.prev_micros, s.prev_level);
      write_value(&s, s.idle_us, !s.commit_level);
      last_index = s.buffer_start;
    }
  }
  if (last_index == s.buffer_read) {
    return;
  }

  // find the size of the packet and reserve the memory
  uint32_t temp_read = s.buffer_read;
  uint32_t reserve_size = 0;
  while (temp_read != last_index && (uint32_t) std::abs(s.buffer[temp_read]) < this->idle_us_) {
    reserve_size++;
    temp_read++;
    if (temp_read >= s.buffer_size) {
      temp_read = 0;
    }
  }
  this->temp_.clear();
  this->temp_.reserve(reserve_size + 1);

  // read the buffer
  for (uint32_t i = 0; i < reserve_size + 1; i++) {
    this->temp_.push_back((int32_t) s.buffer[s.buffer_read++]);
    if (s.buffer_read >= s.buffer_size) {
      s.buffer_read = 0;
    }
  }

  // call the listeners and dumpers
  this->call_listeners_dumpers_();
}

}  // namespace esphome::remote_receiver

#endif
