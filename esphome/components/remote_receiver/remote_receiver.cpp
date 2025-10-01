#include "remote_receiver.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#if defined(USE_LIBRETINY) || defined(USE_ESP8266)

namespace esphome {
namespace remote_receiver {

static const char *const TAG = "remote_receiver";

void IRAM_ATTR HOT RemoteReceiverComponentStore::gpio_intr(RemoteReceiverComponentStore *arg) {
  const uint32_t now = micros();
  const uint32_t next = (arg->buffer_write_at + 1) % arg->buffer_size;

  // If the lhs is 1 (rising edge) we should write to an uneven index and vice versa
  const bool level = arg->pin.digital_read();
  if (level != next % 2)
    return;

  // If next is buffer_read_at, we have hit an overflow
  if (next == arg->buffer_read_at) {
    arg->overflow = true;
    // Reset the write_at to the last good idle position corresponding to signal level,
    // droping some data since it's is already corrupted
    if (arg->buffer_write_at != arg->buffer_idle_at) {
      if (level == arg->buffer_idle_at % 2) {
        arg->buffer_write_at = arg->buffer_idle_at;
      } else {
        arg->buffer_write_at = (arg->buffer_idle_at + 1) % arg->buffer_size;
      }
    }
    arg->buffer[arg->buffer_write_at] = now;
    return;
  }

  const uint32_t last_change = arg->buffer[arg->buffer_write_at];
  const uint32_t time_since_change = now - last_change;
  const uint32_t prev = (arg->buffer_size + arg->buffer_write_at - 1) % arg->buffer_size;

  if (time_since_change >= arg->idle_us) {
    // Handle idle state
    arg->overflow = false;

    const uint32_t prev_delta = last_change - arg->buffer[prev];
    if (prev_delta <= arg->filter_us && prev_delta != 0 && arg->buffer[prev] != 0) {
      // If delta of the previous change is less than filter_us, we can just update the previous value
      arg->buffer_write_at = prev;
      arg->buffer[arg->buffer_write_at] = now;
      arg->buffer_idle_at = prev;
      return;
    }

    arg->buffer_idle_at = next;
  } else if (arg->overflow) {
    // Handle overflow condition by updating the time
    if (level == arg->buffer_idle_at % 2) {
      arg->buffer_write_at = arg->buffer_idle_at;
    } else {
      arg->buffer_write_at = (arg->buffer_idle_at + 1) % arg->buffer_size;
    }
    arg->buffer[arg->buffer_write_at] = now;
    return;
  } else if (time_since_change <= arg->filter_us && arg->buffer_write_at != arg->buffer_idle_at) {
    // Remove short pulse from the buffer
    arg->buffer_write_at = prev;
    return;
  }

  arg->buffer[arg->buffer_write_at = next] = now;  // NOLINT(clang-diagnostic-deprecated-volatile)
}

void RemoteReceiverComponent::setup() {
  this->pin_->setup();
  auto &s = this->store_;
  s.filter_us = this->filter_us_;
  s.idle_us = this->idle_us_;
  s.pin = this->pin_->to_isr();
  s.buffer_size = this->buffer_size_;

  this->high_freq_.start();
  if (s.buffer_size % 2 != 0) {
    // Make sure divisible by two. This way, we know that every 0bxxx0 index is a space and every 0bxxx1 index is a mark
    s.buffer_size++;
  }

  s.buffer = new uint32_t[s.buffer_size];
  void *buf = (void *) s.buffer;
  memset(buf, 0, s.buffer_size * sizeof(uint32_t));

  // First index is a space.
  if (this->pin_->digital_read()) {
    s.buffer_write_at = 1;
    s.buffer_idle_at = 1;
    s.buffer_read_at = 1;
  } else {
    s.buffer_write_at = 0;
    s.buffer_idle_at = 0;
    s.buffer_read_at = 0;
  }
  this->pin_->attach_interrupt(RemoteReceiverComponentStore::gpio_intr, &this->store_, gpio::INTERRUPT_ANY_EDGE);
}
void RemoteReceiverComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Remote Receiver:");
  LOG_PIN("  Pin: ", this->pin_);
  if (this->pin_->digital_read()) {
    ESP_LOGW(TAG, "Remote Receiver Signal starts with a HIGH value. Usually this means you have to "
                  "invert the signal using 'inverted: True' in the pin schema!");
  }
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
  auto &s = this->store_;

  // copy write at to local variables, as it's volatile
  const uint32_t now = micros();
  const uint32_t write_at = s.buffer_write_at;
  const uint32_t idle_at =
      (now - s.buffer[write_at] >= this->idle_us_) ? ((write_at + 1) % s.buffer_size) : s.buffer_idle_at;
  const uint32_t dist = (s.buffer_size + idle_at - s.buffer_read_at) % s.buffer_size;
  // signals must at least one rising and one leading edge
  if (dist <= 1)
    return;

  ESP_LOGVV(TAG, "read_at=%u idle_at=%u dist=%u now_us=%u end_us=%u", s.buffer_read_at, idle_at, dist, now,
            s.buffer[idle_at]);

  // Skip all consecutive idle pulses
  uint32_t read_at = (s.buffer_read_at + 1) % s.buffer_size;
  while (read_at != idle_at) {
    if ((s.buffer[read_at] - s.buffer[s.buffer_read_at]) < this->idle_us_)
      break;

    s.buffer_read_at = read_at;
    read_at = (read_at + 1) % s.buffer_size;
  }

  if (read_at == idle_at)
    return;  // No more data to read

  const uint32_t reserve_size = 1 + (s.buffer_size + idle_at - read_at) % s.buffer_size;
  this->temp_.clear();
  this->temp_.reserve(reserve_size);
  int32_t multiplier = read_at % 2 == 0 ? 1 : -1;

  for (uint32_t i = 0; read_at != idle_at; i++) {
    int32_t delta = s.buffer[read_at] - s.buffer[s.buffer_read_at];
    if (uint32_t(delta) >= this->idle_us_) {
      // already found a space longer than idle. There must have been more than one pulse
      break;
    }

    ESP_LOGVV(TAG, "  i=%u buffer[%u]=%u - buffer[%u]=%u -> %d", i, read_at, s.buffer[read_at], s.buffer_read_at,
              s.buffer[s.buffer_read_at], multiplier * delta);
    this->temp_.push_back(multiplier * delta);
    s.buffer_read_at = read_at;
    read_at = (read_at + 1) % s.buffer_size;
    multiplier *= -1;
  }

  if (s.overflow) {
    ESP_LOGVV(TAG, "Remote receiver buffer overflow! write_at=%u idle_at=%u read_at=%u", s.buffer_write_at,
              s.buffer_idle_at, s.buffer_read_at);
  }

  this->temp_.push_back(this->idle_us_ * multiplier);

  this->call_listeners_dumpers_();
}

}  // namespace remote_receiver
}  // namespace esphome

#endif
