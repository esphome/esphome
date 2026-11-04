#include "remote_receiver.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// clang-tidy cannot parse the Beken SDK headers pulled in via ArduinoPrivate.h
#if defined(REMOTE_RECEIVER_TIMER_SAMPLING) && !defined(CLANG_TIDY)

// ArduinoPrivate.h = Arduino.h + the BDK SDK headers (bk_timer.h, bk_timer_pub.h, sys_config.h)
#include <ArduinoPrivate.h>

extern "C" {
#include "fake_clock_pub.h"
}

// The FreeRTOS tick runs on BKTIMER0 only with LT_MICROS_HIGH_RES, the default everywhere but on the BK7231Q,
// which config validation rejects.
#if defined(LT_MICROS_HIGH_RES) && !LT_MICROS_HIGH_RES
#error "remote_receiver timer sampling needs LT_MICROS_HIGH_RES"
#endif

namespace esphome::remote_receiver {

ESPHOME_LOG_TAG(TAG, "remote_receiver");

// BKTIMER0 is the FreeRTOS tick and BKTIMER2 the SDK clock calibration, so the sampler runs on BKTIMER1. On
// BK7238 remote_transmitter uses it too, which config validation rejects.
static constexpr uint8_t SAMPLE_TIMER = BKTIMER1;

// Edge times come from the BKTIMER0 counter, which runs at the crystal frequency and wraps every
// FCLK_DURATION_MS. Elapsed time is accumulated as counter deltas, so a delayed interrupt costs nothing.
static constexpr uint32_t TICKS_PER_US = CFG_XTAL_FREQUENCE / 1000000;
static constexpr uint32_t TICK_COUNTER_WRAP = FCLK_DURATION_MS * 1000 * TICKS_PER_US;

static inline uint32_t read_tick_counter() {
  REG_WRITE(TIMER0_2_READ_CTL, (BKTIMER0 << TIMER0_2_READ_INDEX_POSI) | TIMER0_2_READ_OP_BIT);
  while (REG_READ(TIMER0_2_READ_CTL) & TIMER0_2_READ_OP_BIT) {
  }
  return REG_READ(TIMER0_2_READ_VALUE);
}

// The timer handler receives only the channel number, so it reaches the store through this pointer.
static RemoteReceiverComponentStore *s_store = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void sample_timer_isr(UINT8 channel) { s_store->timer_isr(); }

void RemoteReceiverComponentStore::timer_isr() {
  if (this->frame_ready || this->overflow)
    return;

  const uint32_t counter = read_tick_counter();
  uint32_t elapsed = counter - this->last_counter;
  if (counter < this->last_counter)
    elapsed += TICK_COUNTER_WRAP;
  this->last_counter = counter;
  this->level_ticks += elapsed;
  // invert the level so marks are positive, like the edge path
  const bool level = !this->pin.digital_read();

  if (level == this->last_level) {
    if (this->capture_active && this->level_ticks >= this->idle_ticks) {
      // end the frame with one idle period, like the edge path, not the measured gap
      this->finish_run(level, this->idle_ticks);
      this->push_run();
      this->capture_active = false;
      // a capture that held only a glitch the filter folded away leaves nothing to dispatch
      if (!this->overflow && this->count > 0) {
        this->frame_ready = true;
      } else {
        this->count = 0;
      }
    }
    return;
  }

  const uint32_t ticks = this->level_ticks;
  const bool ended = this->last_level;
  this->level_ticks = 0;
  this->last_level = level;

  if (this->capture_active) {
    this->finish_run(ended, ticks);
  } else if (level) {
    // a frame starts with a mark
    this->capture_active = true;
    this->run_active = false;
  }
}

void RemoteReceiverComponentStore::finish_run(bool level, uint32_t ticks) {
  if (!this->run_active) {
    this->run_level = level;
    this->run_ticks = ticks;
    this->run_active = true;
    return;
  }
  // A run that returns to the pending level continues it, and a run shorter than the filter is folded into
  // it, so the emitted durations keep alternating sign as the decoders expect.
  if (level == this->run_level || ticks < this->filter_ticks) {
    this->run_ticks += ticks;
    return;
  }
  this->push_run();
  this->run_level = level;
  this->run_ticks = ticks;
}

void RemoteReceiverComponentStore::push_run() {
  // drop a leading space, for example one left after a first mark that the filter removed
  if (!this->run_active || (this->count == 0 && !this->run_level) || this->run_ticks < this->filter_ticks)
    return;
  const uint32_t count = this->count;
  if (count >= this->frame_entries) {
    this->overflow = true;
    this->capture_active = false;
    return;
  }
  const auto run_us = static_cast<int32_t>(this->run_ticks / TICKS_PER_US);
  this->capture[count] = this->run_level ? run_us : -run_us;
  this->count = count + 1;
}

void RemoteReceiverComponentStore::reset(int32_t *capture) {
  this->capture = capture;
  this->count = 0;
  this->level_ticks = 0;
  this->last_counter = read_tick_counter();
  this->last_level = !this->pin.digital_read();
  this->capture_active = false;
  this->run_active = false;
  this->overflow = false;
  this->frame_ready = false;
}

void RemoteReceiverComponent::fail_(const LogString *reason) {
  RAMAllocator<int32_t> allocator;
  allocator.deallocate(this->store_.buffer, this->store_.frame_entries * 2);
  this->store_.buffer = nullptr;
  this->mark_failed(reason);
}

void RemoteReceiverComponent::setup() {
  auto &s = this->store_;
  this->pin_->setup();
  s.pin = this->pin_->to_isr();
  s.filter_ticks = this->filter_us_ * TICKS_PER_US;
  s.idle_ticks = this->idle_us_ * TICKS_PER_US;
  // buffer_size covers both frame buffers: one captures while the loop dispatches the other
  s.frame_entries = this->buffer_size_ / sizeof(int32_t) / 2;
  // allocate before the timer starts, since its interrupt writes the buffer as soon as it is enabled
  s.buffer = RAMAllocator<int32_t>().allocate(s.frame_entries * 2);
  if (s.buffer == nullptr) {
    this->mark_failed(LOG_STR("Buffer allocation failed"));
    return;
  }
  s.reset(s.buffer);
  s_store = &s;

  timer_param_t param{};
  param.channel = SAMPLE_TIMER;
  param.div = 1;
  param.period = this->sample_period_us_;
  param.t_Int_Handler = sample_timer_isr;
  // CMD_TIMER_INIT_PARAM_US enables the channel and its interrupt
  if (sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_INIT_PARAM_US, &param) != 0) {
    this->fail_(LOG_STR("Timer init failed"));
    return;
  }
  UINT32 channel = SAMPLE_TIMER;
  if (sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_UNIT_ENABLE, &channel) != 0) {
    // stop the interrupt before releasing the buffer it writes
    sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_UNIT_DISABLE, &channel);
    this->fail_(LOG_STR("Timer enable failed"));
  }
}

void RemoteReceiverComponent::dump_config() {
  ESP_LOGCONFIG(
      TAG,
      "Remote Receiver:\n"
      "  Sampling: timer, every %" PRIu32 " us\n"
      "  Buffer Size: %" PRIu32 " bytes (%" PRIu32 " pulses per frame)\n"
      "  Tolerance: %" PRIu32 "%s\n"
      "  Filter out pulses shorter than: %" PRIu32 " us\n"
      "  Signal is done after %" PRIu32 " us of no changes",
      this->sample_period_us_, this->buffer_size_, this->store_.frame_entries, this->tolerance_,
      (this->tolerance_mode_ == remote_base::TOLERANCE_MODE_TIME) ? LOG_STR_LITERAL(" us") : LOG_STR_LITERAL("%"),
      this->filter_us_, this->idle_us_);
  LOG_PIN("  Pin: ", this->pin_);
#ifdef REMOTE_BASE_DUMPER_COUNT
  this->warn_extra_secondaries_();
#endif
}

void RemoteReceiverComponent::loop() {
  auto &s = this->store_;
  if (s.overflow) {
    ESP_LOGW(TAG, "Buffer overflow");
    InterruptLock lock;
    s.reset(s.capture);
    return;
  }
  if (!s.frame_ready)
    return;

  // Hand the finished frame to the dispatch and re-arm capture into the other half, so a frame that starts
  // while the listeners and dumpers run is still captured. That half is free: its last frame was dispatched
  // on an earlier loop, and dispatch returns before the next swap.
  const int32_t *frame = s.capture;
  const uint32_t count = s.count;
  {
    InterruptLock lock;
    s.reset(frame == s.buffer ? s.buffer + s.frame_entries : s.buffer);
  }
  this->call_listeners_dumpers_({frame, count});
}

}  // namespace esphome::remote_receiver

#endif  // REMOTE_RECEIVER_TIMER_SAMPLING && !CLANG_TIDY
