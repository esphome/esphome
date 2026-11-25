#include "ac_dimmer.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
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
/// The zero-cross interrupt can be served this late on the ESP8266
static constexpr int32_t MAX_EDGE_LATENCY_US = 1500;
/// A late timer still needs this long after a pulse to switch the gate off before the crossing
static constexpr int32_t GATE_OFF_MARGIN_US = 100;
/// An edge this soon after the gate goes on is noise from the triac firing
static constexpr uint32_t GATE_NOISE_US = 50;
/// Leading edge: the gate goes on at least this long after the predicted crossing, so an edge served a little late
/// comes before it and not in its noise window
static constexpr uint32_t MIN_ENABLE_TIME_US = 100;
/// The value that keeps the gate on for the whole half cycle
static constexpr uint16_t VALUE_FULL_ON = 65535;
/// While acquiring, a half cycle that differs from the one before by more than a quarter of it starts over
static constexpr int32_t ACQUIRE_HALF_CYCLE_DIV = 4;
/// The three full cycles taken for the lock must agree within 1/128 of a cycle
static constexpr uint32_t ACQUIRE_CYCLE_DIV = 128;
/// Locked, an edge counts from 1/12 of a half cycle early to 1/6 late (served late), or MAX_EDGE_LATENCY_US
static constexpr int32_t EDGE_EARLY_DIV = 12;
static constexpr int32_t EDGE_LATE_DIV = 6;
/// A gate that goes on within 1/32 of a half cycle before the crossing can hide the crossing's own edge
static constexpr int32_t GATE_NEAR_CROSSING_DIV = 32;
/// Crossings in a row that such gate noise alone may confirm; fits the 3-bit gate_edges
static constexpr uint8_t MAX_GATE_EDGES = 7;
/// One edge moves the timing by at most 1/24 of a half cycle
static constexpr int32_t FOLLOW_STEP_DIV = 24;
/// Steps taken for far early edges that later late edges may undo; fits the 3-bit early_steps
static constexpr uint8_t MAX_EARLY_STEPS = 7;
/// Each edge moves the crossing by a quarter of its step and the half cycle by a sixteenth
static constexpr int32_t PHASE_GAIN_DIV = 4;
static constexpr int32_t PERIOD_GAIN_DIV = 16;
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
    if (this->value != VALUE_FULL_ON)
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
    // The predicted crossing: the next half cycle starts here, whether or not its edge has been seen
    this->zc_at += this->period;
    since = static_cast<int32_t>(now - this->zc_at);
    if (this->edge_now) {
      this->misses = 0;
    } else if (++this->misses >= MAX_MISSES) {
      this->phase = ZERO_CROSS_PHASE_NONE;
      if (this->value != VALUE_FULL_ON)
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
  if (this->value != VALUE_FULL_ON && this->value != 0) {
    if (!this->gate_fired && this->enable_time_us != 0 && since >= this->enable_time_us) {
      // A late timer does not start a pulse that could last past the crossing: the triac would stay on
      if (since + static_cast<int32_t>(GATE_ENABLE_TIME) + GATE_OFF_MARGIN_US < period) {
        this->gate_pin.digital_write(true);
        this->gate_fired = true;
        // When it really went on, for the gate noise check; the timer can run up to a tick late
        this->enable_time_us = since;
        // Prevent too short pulses
        this->disable_time_us = std::max<int32_t>(this->disable_time_us, since + GATE_ENABLE_TIME);
      } else {
        this->enable_time_us = 0;
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

bool IRAM_ATTR HOT AcDimmerDataStore::gate_noise_at(uint32_t now) const {
  // With the trailing method the gate goes on at the crossing
  return this->phase == ZERO_CROSS_PHASE_LOCKED && this->gate_fired && this->method != DIM_METHOD_TRAILING &&
         now - this->zc_at - this->enable_time_us < GATE_NOISE_US;
}

void IRAM_ATTR HOT AcDimmerDataStore::zero_cross_edge(uint32_t now, const AcDimmerDataStore *noise_from) {
  if (this->phase == ZERO_CROSS_PHASE_LOCKED) {
    this->track_edge(now, noise_from);
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
  if (this->phase == ZERO_CROSS_PHASE_ONE_EDGE || diff > last_gap / ACQUIRE_HALF_CYCLE_DIV ||
      diff < -last_gap / ACQUIRE_HALF_CYCLE_DIV) {
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
  const uint32_t lo = std::min(a, b);
  const uint32_t hi = std::max(a, b);
  // The three must also agree: the first edges after power up can drift, which moves two of them
  if (std::max(hi, cycle) - std::min(lo, cycle) > cycle / ACQUIRE_CYCLE_DIV) {
    this->disable_time_us = b;
    this->enable_time_us = cycle;
    return;
  }
  this->period = std::max(lo, std::min(hi, cycle)) / 2;
  // Locked: the half cycle the edge before this one started counts as running. Its start is taken halfway between
  // what this edge and the one before it say, so a detector that alternates early and late starts centred.
  this->zc_at = now - (gap + this->period) / 2;
  this->misses = 0;
  this->edge_now = true;
  this->edge_next = true;
  this->early_steps = 0;
  this->gate_edges = 0;
  this->phase = ZERO_CROSS_PHASE_LOCKED;
}

void IRAM_ATTR HOT AcDimmerDataStore::track_edge(uint32_t now, const AcDimmerDataStore *noise_from) {
  // The edge belongs to the crossing at zc_at (served after the timer started it) or the one at zc_at + period
  const bool next = static_cast<int32_t>(now - this->zc_at) > this->period / 2;
  const int32_t err = static_cast<int32_t>(now - this->zc_at) - (next ? this->period : 0);
  // Late edges get a wider window: the interrupt can be served late
  if (err > std::max<int32_t>(this->period / EDGE_LATE_DIV, MAX_EDGE_LATENCY_US) ||
      err < -this->period / EDGE_EARLY_DIV)
    return;
  // The first edge of a crossing is taken, later ones are ignored
  if (next ? this->edge_next : this->edge_now)
    return;
  // Noise from a gate going on is not a crossing
  if (noise_from != nullptr) {
    // A gate this close to the crossing can hide the crossing itself: the edge confirms it without moving the timing,
    // so noise alone keeps the lock only for a few more half cycles
    if (noise_from->period - noise_from->enable_time_us > noise_from->period / GATE_NEAR_CROSSING_DIV ||
        this->gate_edges == MAX_GATE_EDGES)
      return;
    this->gate_edges++;
  } else {
    this->gate_edges = 0;
    // Mains timing is steady, so follow slowly, by at most `close` per edge. An edge late beyond `close` may only have
    // been served late: it only undoes an earlier step made by an edge far early.
    const int32_t close = this->period / FOLLOW_STEP_DIV;
    int32_t move = std::max(err, -close);
    if (err > close) {
      move = 0;
      if (this->early_steps != 0) {
        this->early_steps--;
        move = close;
      }
    } else if (err < -close) {
      if (this->early_steps != MAX_EARLY_STEPS)
        this->early_steps++;
    } else if (err >= 0) {
      this->early_steps = 0;
    }
    this->zc_at += move / PHASE_GAIN_DIV;
    this->period += move / PERIOD_GAIN_DIV;
  }
  if (next) {
    this->edge_next = true;
  } else {
    this->edge_now = true;
  }
}

/// GPIO interrupt routine, called when ZC pin triggers
void IRAM_ATTR HOT AcDimmerDataStore::gpio_intr([[maybe_unused]] uint32_t now,
                                                [[maybe_unused]] const AcDimmerDataStore *noise_from) {
#if defined(USE_ESP8266) || defined(USE_ESP32)
  this->zero_cross_edge(now, noise_from);
#else
  // No dimmer timer on this platform: only fully on and fully off work, switched at the crossing
  if (this->value == VALUE_FULL_ON) {
    this->gate_pin.digital_write(true);
  } else if (this->value == 0) {
    this->gate_pin.digital_write(false);
  }
#endif
}

void IRAM_ATTR HOT AcDimmerDataStore::start_half_cycle() {
  const uint32_t period = this->period;
  this->gate_fired = false;
  this->enable_time_us = 0;
  if (this->value == VALUE_FULL_ON) {
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
      this->disable_time_us = std::max<uint32_t>(10, this->value * (period - min_us) / VALUE_FULL_ON + min_us);
    } else {
      // calculate time until enable in µs: (1.0-value)*cycle_time, but with integer arithmetic
      // also take into account min_power
      this->enable_time_us =
          std::max<uint32_t>(MIN_ENABLE_TIME_US, ((VALUE_FULL_ON - this->value) * (period - min_us)) / VALUE_FULL_ON);

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
  // The detector also picks up the other dimmers' gates: noise after any of them is no crossing
  const AcDimmerDataStore *noise_from = nullptr;
  for (auto *dimmer : all_dimmers) {
    if (dimmer == nullptr)
      break;
    if (dimmer->zero_cross_pin_number == store->zero_cross_pin_number && dimmer->gate_noise_at(now))
      noise_from = dimmer;
  }
  for (auto *dimmer : all_dimmers) {
    if (dimmer == nullptr)
      break;
    if (dimmer->zero_cross_pin_number == store->zero_cross_pin_number) {
      dimmer->gpio_intr(now, noise_from);
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
  auto new_value = static_cast<uint16_t>(roundf(state * VALUE_FULL_ON));
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
