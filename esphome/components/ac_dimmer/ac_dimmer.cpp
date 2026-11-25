#include "ac_dimmer.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <numbers>

#ifdef USE_ESP8266
#include <Arduino.h>
#endif

#ifdef USE_ESP32
#include "hw_timer_esp_idf.h"
#endif

namespace esphome::ac_dimmer {

ESPHOME_LOG_TAG(TAG, "ac_dimmer");

// Global array to store dimmer objects
static AcDimmerDataStore *all_dimmers[32];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/// Time in microseconds the gate should be held high
/// 10µs should be long enough for most triacs
/// For reference: BT136 datasheet says 2µs nominal (page 7)
/// However other factors like gate driver propagation time
/// are also considered and a really low value is not important
/// See also: https://github.com/esphome/issues/issues/1632
static constexpr uint32_t GATE_ENABLE_TIME = 50;

/// While acquiring, edges sooner than this after the last one taken are noise or the second edge of the same
/// zero-cross pulse. A half cycle is 10 ms at 50 Hz and 8.33 ms at 60 Hz.
static constexpr uint32_t MIN_HALF_CYCLE_US = 6000;
/// A longer gap between two edges is not a half cycle (20 Hz)
static constexpr uint32_t MAX_HALF_CYCLE_US = 25000;
/// Half cycles in a row without an edge near the predicted crossing before the lock is dropped
static constexpr uint8_t MAX_MISSES = 8;
/// taken_err when no correction is waiting (outside any window)
static constexpr int16_t NO_EDGE_ERR = INT16_MIN;
/// The zero-cross interrupt can be served this late on the ESP8266
static constexpr int32_t MAX_EDGE_LATENCY_US = 1500;
/// A late timer still needs this long after a pulse to switch the gate off before the crossing
static constexpr int32_t GATE_OFF_MARGIN_US = 100;
#ifdef USE_ESP32
/// The zero-cross interrupt runs at a higher level than the timer and updates the same timing
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static portMUX_TYPE timing_lock = portMUX_INITIALIZER_UNLOCKED;
#endif

#ifdef USE_ESP8266
/// Timer interval while no dimmer is locked; a zero-cross edge re-arms the timer anyway
static constexpr uint32_t IDLE_TIMER_INTERVAL_US = 20000;
static void esp8266_timer_in(uint32_t us);
#endif

#ifdef USE_ESP32
/// Timer frequency in Hz (1 MHz = 1µs resolution)
static constexpr uint32_t TIMER_FREQUENCY_HZ = 1000000;
/// Timer interrupt interval in microseconds
static constexpr uint64_t TIMER_INTERVAL_US = 50;
#endif

/// Function called from timer interrupt
/// Input is current time in microseconds (micros())
/// Returns when next "event" is expected in µs, or 0 if no such event known.
uint32_t IRAM_ATTR HOT AcDimmerDataStore::timer_intr(uint32_t now) {
  if (this->phase != ZERO_CROSS_PHASE_LOCKED) {
    if (this->value != 65535)
      this->gate_pin.digital_write(false);
    return 0;
  }

#ifdef USE_ESP32
  portENTER_CRITICAL_ISR(&timing_lock);
#endif
  const uint32_t next = this->timer_step(now);
#ifdef USE_ESP32
  portEXIT_CRITICAL_ISR(&timing_lock);
#endif
  return next;
}

uint32_t IRAM_ATTR HOT AcDimmerDataStore::timer_step(uint32_t now) {
  auto since = static_cast<int32_t>(now - this->zc_at);
  if (since >= this->period) {
    // The window of the crossing at zc_at closed long ago: its best edge is final
    if (this->taken_err != NO_EDGE_ERR && !this->edge_next)
      this->apply_edge_err();
    // The predicted crossing: the next half cycle starts here, whether or not its edge has been seen
    this->zc_at += this->period;
    since = static_cast<int32_t>(now - this->zc_at);
    if (this->edge_now) {
      this->misses = 0;
    } else if (++this->misses >= MAX_MISSES) {
      this->phase = ZERO_CROSS_PHASE_NONE;
      if (this->value != 65535)
        this->gate_pin.digital_write(false);
      return 0;
    }
    this->edge_now = this->edge_next;
    this->edge_next = false;
    this->start_half_cycle();
  }
  const int32_t period = this->period;
  // Before zc_at: a correction moved it ahead. Still past the end: the timer ran very late.
  if (since < 0)
    return -since;
  if (since >= period)
    return 1;

  uint32_t next = period - since;
  if (this->value != 65535 && this->value != 0) {
    if (this->enable_time_us != 0 && since >= this->enable_time_us) {
      this->enable_time_us = 0;
      // A late timer does not start a pulse that could last past the crossing: the triac would stay on
      if (since + static_cast<int32_t>(GATE_ENABLE_TIME) + GATE_OFF_MARGIN_US < period) {
        this->gate_pin.digital_write(true);
        // Prevent too short pulses
        this->disable_time_us = std::max<int32_t>(this->disable_time_us, since + GATE_ENABLE_TIME);
      }
    }
    if (this->disable_time_us != 0 && since >= this->disable_time_us) {
      this->disable_time_us = 0;
      this->gate_pin.digital_write(false);
    }

    if (since < this->enable_time_us) {
      next = std::min<uint32_t>(next, this->enable_time_us - since);
    } else if (since < this->disable_time_us) {
      next = std::min<uint32_t>(next, this->disable_time_us - since);
    }
  }
  return next;
}

void IRAM_ATTR HOT AcDimmerDataStore::zero_cross_edge(uint32_t now) {
  if (this->phase == ZERO_CROSS_PHASE_LOCKED) {
    this->track_edge(now);
  } else {
    this->acquire_edge(now);
  }
}

void IRAM_ATTR HOT AcDimmerDataStore::acquire_edge(uint32_t now) {
  const uint32_t gap = now - this->zc_at;
  if (this->phase == ZERO_CROSS_PHASE_NONE || gap > MAX_HALF_CYCLE_US) {
    this->zc_at = now;
    this->phase = ZERO_CROSS_PHASE_ONE_EDGE;
    return;
  }
  if (gap < MIN_HALF_CYCLE_US)
    return;
  const int32_t last_gap = this->period;
  this->zc_at = now;
  this->period = gap;
  const int32_t diff = static_cast<int32_t>(gap) - last_gap;
  if (this->phase == ZERO_CROSS_PHASE_ONE_EDGE || diff > last_gap / 4 || diff < -last_gap / 4) {
    this->phase = ZERO_CROSS_PHASE_CONFIRMING;
    this->enable_time_us = 0;
    this->disable_time_us = 0;
    return;
  }
  // Full cycles cancel unequal half cycles. A late edge changes at most two of the three, so the middle one is right.
  const uint32_t cycle = gap + last_gap;
  if (this->disable_time_us == 0) {
    this->disable_time_us = this->enable_time_us;
    this->enable_time_us = cycle;
    return;
  }
  const uint32_t a = this->disable_time_us;
  const uint32_t b = this->enable_time_us;
  // The three must also agree: the first edges after power up can drift, which moves two of them
  if (std::max(std::max(a, b), cycle) - std::min(std::min(a, b), cycle) > cycle / 128) {
    this->disable_time_us = b;
    this->enable_time_us = cycle;
    return;
  }
  this->period = std::max(std::min(a, b), std::min(std::max(a, b), cycle)) / 2;
  // Locked: the half cycle the edge before this one started counts as running
  this->zc_at = now - this->period;
  this->taken_err = NO_EDGE_ERR;
  this->misses = 0;
  this->edge_now = true;
  this->edge_next = true;
  this->early_steps = 0;
  this->late_last = false;
  this->late_before = false;
  this->phase = ZERO_CROSS_PHASE_LOCKED;
}

void IRAM_ATTR HOT AcDimmerDataStore::track_edge(uint32_t now) {
  // The edge belongs to the crossing at zc_at (served after the timer started it) or the one at zc_at + period
  const bool next = static_cast<int32_t>(now - this->zc_at) > this->period / 2;
  int32_t err = static_cast<int32_t>(now - this->zc_at) - (next ? this->period : 0);
  // Late edges get a wider window: the interrupt can be served late
  if (err > std::max<int32_t>(this->period / 6, MAX_EDGE_LATENCY_US) || err < -this->period / 8)
    return;
  if (next ? this->edge_next : this->edge_now) {
    // Another edge for the same crossing, such as noise from the triac firing just before it: the one closest to
    // the prediction wins. An edge well before it loses to any later one: noise from the gate, which fires relative
    // to the prediction, would otherwise drag the prediction along with it.
    const int32_t taken = this->taken_err;
    if (taken != NO_EDGE_ERR && (taken < -this->period / 32 || std::abs(err) < std::abs(taken)))
      this->taken_err = err;
    return;
  }
  if (this->taken_err != NO_EDGE_ERR) {
    // An edge for a later crossing: the earlier crossing's best edge is final
    this->apply_edge_err();
    err = static_cast<int32_t>(now - this->zc_at) - (next ? this->period : 0);
  }
  this->taken_err = err;
  if (next) {
    this->edge_next = true;
  } else {
    this->edge_now = true;
  }
}

void IRAM_ATTR HOT AcDimmerDataStore::apply_edge_err() {
  // Mains timing is steady, so follow slowly, by at most `close` per edge. A late edge may only have been served late:
  // beyond `close` it counts only to undo early steps, or as a lone late edge of a detector whose edges alternate
  // early and late around the crossing.
  const int32_t close = this->period / 32;
  const int32_t err = this->taken_err;
  const bool late = err > close;
  int32_t move = 0;
  if (!late) {
    move = std::max(err, -close);
    if (err < -close) {
      if (this->early_steps != 7)
        this->early_steps++;
    } else if (err >= 0) {
      this->early_steps = 0;
    }
  } else if (this->early_steps != 0) {
    // Early detector pulses moved the timing: the edges after them undo it
    this->early_steps--;
    move = close;
  } else if (this->late_before && !this->late_last) {
    move = close;
  }
  this->zc_at += move / 4;
  this->period += move / 16;
  this->late_before = this->late_last;
  this->late_last = late;
  this->taken_err = NO_EDGE_ERR;
}

/// GPIO interrupt routine, called when ZC pin triggers
void IRAM_ATTR HOT AcDimmerDataStore::gpio_intr([[maybe_unused]] uint32_t now) {
#if defined(USE_ESP8266) || defined(USE_ESP32)
  this->zero_cross_edge(now);
#else
  // No dimmer timer on this platform: only fully on and fully off work, switched at the crossing
  if (this->value == 65535) {
    this->gate_pin.digital_write(true);
  } else if (this->value == 0) {
    this->gate_pin.digital_write(false);
  }
#endif
}

void IRAM_ATTR HOT AcDimmerDataStore::start_half_cycle() {
  const uint32_t period = this->period;
  if (this->value == 65535) {
    // fully on, enable output immediately
    this->gate_pin.digital_write(true);
  } else if (this->init_cycle) {
    // send a full cycle
    this->init_cycle = false;
    this->enable_time_us = 0;
    this->disable_time_us = period;
  } else if (this->value == 0) {
    // fully off, disable output immediately
    this->gate_pin.digital_write(false);
  } else {
    const uint32_t min_us = period * this->min_power / 1000;
    if (this->method == DIM_METHOD_TRAILING) {
      this->enable_time_us = 1;  // cannot be 0
      // calculate time until disable in µs with integer arithmetic and take into account min_power
      this->disable_time_us = std::max<uint32_t>(10, this->value * (period - min_us) / 65535 + min_us);
    } else {
      // calculate time until enable in µs: (1.0-value)*cycle_time, but with integer arithmetic
      // also take into account min_power
      this->enable_time_us = std::max<uint32_t>(1, ((65535 - this->value) * (period - min_us)) / 65535);

      // A gate still on from the last half cycle (a pulse that started just before the crossing) would fire
      // this one at full power
      this->gate_pin.digital_write(false);
      if (this->method == DIM_METHOD_LEADING_PULSE) {
        // Minimum pulse time should be enough for the triac to trigger when it is close to the ZC zone
        // this is for brightness near 99%
        this->disable_time_us = std::max<uint32_t>(this->enable_time_us + GATE_ENABLE_TIME, period / 10);
      } else {
        this->disable_time_us = period;
      }
    }
  }
}

void IRAM_ATTR HOT AcDimmerDataStore::s_gpio_intr(AcDimmerDataStore *store) {
  // Attaching pin interrupts on the same pin will override the previous interrupt
  // However, the user expects that multiple dimmers sharing the same ZC pin will work.
  // We solve this in a bit of a hacky way: On each pin interrupt, we check all dimmers
  // if any of them are using the same ZC pin, and also trigger the interrupt for *them*.
  const uint32_t now = micros();
  for (auto *dimmer : all_dimmers) {
    if (dimmer == nullptr)
      break;
    if (dimmer->zero_cross_pin_number == store->zero_cross_pin_number) {
      dimmer->gpio_intr(now);
    }
  }
#ifdef USE_ESP8266
  // The edge can move the next event earlier: run the timer code right away
  esp8266_timer_in(2);
#endif
}

#if defined(USE_ESP8266) || defined(USE_ESP32)
/// Runs the timer work of every dimmer; returns the µs until the earliest next event, 0 for none
static uint32_t IRAM_ATTR HOT run_timers(uint32_t now) {
  uint32_t next = 0;
  for (auto *dimmer : all_dimmers) {
    if (dimmer == nullptr)
      break;
    const uint32_t res = dimmer->timer_intr(now);
    if (res != 0 && (next == 0 || res < next))
      next = res;
  }
  return next;
}
#endif

#ifdef USE_ESP8266
// ESP8266: a one-shot maskable interrupt on CCOMPARE0 (timer0), only at the gate events. Not timer1: the waveform
// generator runs it as an NMI at 1-10 kHz, too much load with WiFi. Masked sections can delay a gate event slightly.
// The cycle counter runs twice as fast during a CpuFrequencyBoost; the timer then fires early and re-arms.
/// Arms the timer `us` after the cycle count `from`
static void IRAM_ATTR esp8266_timer_at(uint32_t from, uint32_t us) {
  uint32_t target = from + us * (arch_get_cpu_freq_hz() / 1000000);
  timer0_write(target);
  // Already passed (a long handler, or an NMI between reading the counter and writing the compare): fire soon
  while (static_cast<int32_t>(target - arch_get_cpu_cycle_count()) <= 0) {
    target = arch_get_cpu_cycle_count() + 2 * (arch_get_cpu_freq_hz() / 1000000);
    timer0_write(target);
  }
}

static void IRAM_ATTR esp8266_timer_in(uint32_t us) { esp8266_timer_at(arch_get_cpu_cycle_count(), us); }

static void IRAM_ATTR HOT esp8266_timer_isr() {
  // The returned delays count from here, not from after the handler
  const uint32_t start = arch_get_cpu_cycle_count();
  const uint32_t next = run_timers(micros());
  esp8266_timer_at(start, next != 0 && next < IDLE_TIMER_INTERVAL_US ? next : IDLE_TIMER_INTERVAL_US);
}
#endif

#ifdef USE_ESP32
static HWTimer *dimmer_timer = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void IRAM_ATTR HOT AcDimmerDataStore::s_timer_intr() { run_timers(micros()); }
#endif

void AcDimmer::setup() {
  this->gate_pin_->setup();
  this->store_.gate_pin = this->gate_pin_->to_isr();
  this->store_.zero_cross_pin_number = this->zero_cross_pin_->get_pin();
  this->store_.min_power = static_cast<uint16_t>(this->min_power_ * 1000);
  this->min_power_ = 0;

  // The zero cross pin is set up only once: ESP8266 fails and ESP32 seems to fail silently on a second one.
  // The store is added only now, once the interrupts can use it.
  bool setup_zero_cross_pin = true;
  for (auto &dimmer : all_dimmers) {
    if (dimmer == nullptr) {
      dimmer = &this->store_;
      break;
    }
    if (dimmer->zero_cross_pin_number == this->store_.zero_cross_pin_number)
      setup_zero_cross_pin = false;
  }

  if (setup_zero_cross_pin) {
    this->zero_cross_pin_->setup();
    this->zero_cross_pin_->attach_interrupt(&AcDimmerDataStore::s_gpio_intr, &this->store_,
                                            this->zero_cross_interrupt_type_);
  }

#ifdef USE_ESP8266
  // One timer for all dimmers (see esp8266_timer_isr)
  if (all_dimmers[0] == &this->store_) {
    timer0_isr_init();
    timer0_attachInterrupt(&esp8266_timer_isr);
    esp8266_timer_in(IDLE_TIMER_INTERVAL_US);
  }
#endif
#ifdef USE_ESP32
  if (dimmer_timer == nullptr) {
    dimmer_timer = timer_begin(TIMER_FREQUENCY_HZ);
    if (dimmer_timer == nullptr) {
      ESP_LOGE(TAG, "Failed to create GPTimer for AC dimmer");
      this->mark_failed();
      return;
    }
    timer_attach_interrupt(dimmer_timer, &AcDimmerDataStore::s_timer_intr);
    // For ESP32, we can't use dynamic interval calculation because the timerX functions
    // are not callable from ISR (placed in flash storage).
    // Here we just use an interrupt firing every 50 µs.
    timer_alarm(dimmer_timer, TIMER_INTERVAL_US, true, 0);
  }
#endif
}

void AcDimmer::write_state(float state) {
  state = std::acos(1 - (2 * state)) / std::numbers::pi_v<float>;  // RMS power compensation
  auto new_value = static_cast<uint16_t>(roundf(state * 65535));
  if (new_value != 0 && this->store_.value == 0)
    this->store_.init_cycle = this->init_with_half_cycle_;
  this->store_.value = new_value;
}

void AcDimmer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "AcDimmer:\n"
                "  Min Power: %.1f%%\n"
                "  Init with half cycle: %s\n"
                "  Interrupt Type: %s\n"
                "  Method: %s",
                this->store_.min_power / 10.0f, YESNO(this->init_with_half_cycle_),
                this->zero_cross_interrupt_type_ == gpio::INTERRUPT_RISING_EDGE    ? LOG_STR_LITERAL("rising")
                : this->zero_cross_interrupt_type_ == gpio::INTERRUPT_FALLING_EDGE ? LOG_STR_LITERAL("falling")
                                                                                   : LOG_STR_LITERAL("any"),
                this->store_.method == DIM_METHOD_LEADING_PULSE ? LOG_STR_LITERAL("leading pulse")
                : this->store_.method == DIM_METHOD_LEADING     ? LOG_STR_LITERAL("leading")
                                                                : LOG_STR_LITERAL("trailing"));
  LOG_PIN("  Output Pin: ", this->gate_pin_);
  LOG_PIN("  Zero-Cross Pin: ", this->zero_cross_pin_);
  LOG_FLOAT_OUTPUT(this);
  ESP_LOGV(TAG, "  Estimated Frequency: %.3fHz", 1e6f / this->store_.period / 2);
}

}  // namespace esphome::ac_dimmer
