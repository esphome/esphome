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
#if !defined(LT_MICROS_HIGH_RES) || !LT_MICROS_HIGH_RES
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

// The latch completes in a few cycles; this bound only guards against a controller that never
// finishes the read. Give up and skip the sample rather than return a stale counter.
static constexpr uint32_t TICK_READ_SPINS = 100;

static inline bool read_tick_counter(uint32_t *value) {
  REG_WRITE(TIMER0_2_READ_CTL, (BKTIMER0 << TIMER0_2_READ_INDEX_POSI) | TIMER0_2_READ_OP_BIT);
  for (uint32_t spins = 0; spins < TICK_READ_SPINS && (REG_READ(TIMER0_2_READ_CTL) & TIMER0_2_READ_OP_BIT); spins++) {
  }
  if (REG_READ(TIMER0_2_READ_CTL) & TIMER0_2_READ_OP_BIT)
    return false;
  *value = REG_READ(TIMER0_2_READ_VALUE);
  return true;
}

// The timer handler receives only the channel number, so it reaches the receiver through this pointer.
static RemoteReceiverComponent *s_receiver = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void RemoteReceiverComponent::sample_timer_isr(uint8_t channel) {
  if (s_receiver->store_.timer_isr())
    s_receiver->enable_loop_soon_any_context();
}

bool RemoteReceiverComponentStore::timer_isr() {
  // digital_read() applies the pin's inverted: option, so a mark reads true, like the edge path
  const bool level = this->pin.digital_read();
  uint32_t counter;
  if (!this->capture_active) {
    // idle samples skip the tick counter read; the edge that starts a capture re-syncs it
    if (level == this->last_level)
      return false;
    if (!read_tick_counter(&counter)) {
      this->count_tick_read_error();
      // wake the loop so it reports the failure
      return true;
    }
    this->last_counter = counter;
    return this->sample(level, 0);
  }
  if (!read_tick_counter(&counter)) {
    // leave last_counter and the level alone, so the next read spans the gap
    this->count_tick_read_error();
    // wake the loop so it reports the failure
    return true;
  }
  uint32_t elapsed = counter - this->last_counter;
  if (counter < this->last_counter)
    elapsed += TICK_COUNTER_WRAP;
  this->last_counter = counter;
  return this->sample(level, elapsed);
}

void RemoteReceiverComponent::setup() {
  auto &s = this->store_;
  this->pin_->setup();
  s.pin = this->pin_->to_isr();
  s.capture = s.buffer;
  s.last_level = s.pin.digital_read();
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
  uint16_t tick_errors;
  {
    InterruptLock lock;
    frame = s.ready;
    count = s.ready_count;
    overflow = s.overflow;
    busy = s.busy;
    tick_errors = s.tick_read_errors;
    s.overflow = false;
    s.busy = false;
    s.tick_read_errors = 0;
  }
  if (overflow) {
    ESP_LOGW(TAG, "Buffer overflow");
  }
  if (busy) {
    ESP_LOGW(TAG, "Frame dropped, previous frame still being processed");
  }
  if (tick_errors != 0) {
    if (tick_errors == UINT16_MAX) {
      ESP_LOGW(TAG, "Counter read timed out at least %u times; timings may be wrong", tick_errors);
    } else {
      ESP_LOGW(TAG, "Counter read timed out %u times; timings may be wrong", tick_errors);
    }
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
