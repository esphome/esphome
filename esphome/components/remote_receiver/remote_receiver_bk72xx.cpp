#include "remote_receiver.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// clang-tidy cannot parse the Beken SDK headers pulled in via ArduinoPrivate.h
#if defined(USE_REMOTE_RECEIVER_TIMER_SAMPLING) && !defined(CLANG_TIDY)

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
// FCLK_DURATION_MS. Elapsed time is accumulated as counter deltas, so an interrupt delayed by up to one wrap
// costs nothing; a longer hold-off (a flash write) loses whole wraps, and every other clock stops with it.
static_assert(TIMER_TICKS_PER_US == CFG_XTAL_FREQUENCE / 1000000, "TIMER_TICKS_PER_US must match the crystal");
static constexpr uint32_t TICK_COUNTER_WRAP = FCLK_DURATION_MS * 1000 * TIMER_TICKS_PER_US;

// The latch takes a few cycles; give up after a bounded wait, like LibreTiny's getTicksCount()
static constexpr uint32_t TICK_READ_SPINS = 1000;

static inline uint32_t read_tick_counter() {
  REG_WRITE(TIMER0_2_READ_CTL, (BKTIMER0 << TIMER0_2_READ_INDEX_POSI) | TIMER0_2_READ_OP_BIT);
  for (uint32_t spins = 0; spins < TICK_READ_SPINS && (REG_READ(TIMER0_2_READ_CTL) & TIMER0_2_READ_OP_BIT); spins++) {
  }
  return REG_READ(TIMER0_2_READ_VALUE);
}

// The timer handler receives only the channel number, so it reaches the receiver through this pointer.
static RemoteReceiverComponent *s_receiver = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void RemoteReceiverComponent::sample_timer_isr(uint8_t channel) {
  if (s_receiver->store_.timer_isr())
    s_receiver->enable_loop_soon_any_context();
}

bool RemoteReceiverComponentStore::timer_isr() {
  // invert the level so marks are positive, like the edge path
  const bool level = !this->pin.digital_read();

  if (!this->capture_active) {
    // idle samples skip the tick counter read; the edge that starts a capture re-syncs it
    if (level == this->last_level)
      return false;
    this->last_counter = read_tick_counter();
    this->level_ticks = 0;
    this->last_level = level;
    // a frame starts with a mark
    if (level) {
      this->capture_active = true;
      this->run_active = false;
    }
    return false;
  }

  const uint32_t counter = read_tick_counter();
  uint32_t elapsed = counter - this->last_counter;
  if (counter < this->last_counter)
    elapsed += TICK_COUNTER_WRAP;
  this->last_counter = counter;
  this->level_ticks += elapsed;

  if (level == this->last_level) {
    if (this->level_ticks < this->idle_ticks)
      return false;
    // end the frame with one idle period, like the edge path, not the measured gap
    this->finish_run(level, this->idle_ticks);
    this->push_run();
    this->capture_active = false;
    const uint16_t count = this->count;
    this->count = 0;
    // a capture that held only a glitch the filter folded away leaves nothing to dispatch
    if (count == 0)
      return false;
    if (this->frame_overflow) {
      this->frame_overflow = false;
      this->overflow = true;
      return true;
    }
    if (this->ready != nullptr) {
      this->busy = true;
      return true;
    }
    this->ready = this->capture;
    this->ready_count = count;
    this->capture = this->capture == this->buffer ? this->buffer + this->frame_entries : this->buffer;
    return true;
  }

  const uint32_t ticks = this->level_ticks;
  const bool ended = this->last_level;
  this->level_ticks = 0;
  this->last_level = level;
  this->finish_run(ended, ticks);
  return false;
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
  const uint16_t count = this->count;
  if (count >= this->frame_entries) {
    // drop the rest of the frame; the frame end reports the overflow
    this->frame_overflow = true;
    return;
  }
  const auto run_us = static_cast<int32_t>(this->run_ticks / TIMER_TICKS_PER_US);
  this->capture[count] = this->run_level ? run_us : -run_us;
  this->count = count + 1;
}

void RemoteReceiverComponent::setup() {
  auto &s = this->store_;
  this->pin_->setup();
  s.pin = this->pin_->to_isr();
  s.capture = s.buffer;
  s.last_level = !s.pin.digital_read();
  s_receiver = this;

  timer_param_t param{};
  param.channel = SAMPLE_TIMER;
  param.div = 1;
  param.period = s.sample_period_us;
  param.t_Int_Handler = sample_timer_isr;
  // CMD_TIMER_INIT_PARAM_US also enables the channel and its interrupt
  if (sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_INIT_PARAM_US, &param) != 0) {
    this->mark_failed(LOG_STR("Timer init failed"));
    return;
  }
  // the timer interrupt wakes the loop when it has a frame
  this->disable_loop();
}

void RemoteReceiverComponent::loop() {
  auto &s = this->store_;
  const int32_t *frame;
  uint16_t count;
  bool overflow;
  bool busy;
  {
    InterruptLock lock;
    frame = s.ready;
    count = s.ready_count;
    overflow = s.overflow;
    busy = s.busy;
    s.overflow = false;
    s.busy = false;
  }
  if (overflow) {
    ESP_LOGW(TAG, "Buffer overflow");
  }
  if (busy) {
    ESP_LOGW(TAG, "Frame dropped, previous frame still being processed");
  }
  if (frame == nullptr) {
    this->disable_loop();
    return;
  }
  // the interrupt keeps capturing into the other half and publishes it once this one is released
  this->call_listeners_dumpers_({frame, count});
  s.ready = nullptr;
}

}  // namespace esphome::remote_receiver

#endif  // USE_REMOTE_RECEIVER_TIMER_SAMPLING && !CLANG_TIDY
