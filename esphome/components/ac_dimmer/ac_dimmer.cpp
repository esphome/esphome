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

/// Edges sooner than this after a crossing are noise or the second edge of the same zero-cross pulse.
/// A half cycle is 10 ms at 50 Hz and 8.33 ms at 60 Hz.
static constexpr uint32_t MIN_HALF_CYCLE_US = 6000;
/// A longer time between two edges is not a half cycle (20 Hz)
static constexpr uint32_t MAX_HALF_CYCLE_US = 25000;
/// Edges, each one half cycle after the one before, that confirm a new measurement of the half cycle
static constexpr uint8_t CONFIRMING_EDGES = 2;
/// Edges in a row that do not fit the timing before it is measured again
static constexpr uint8_t MAX_DOUBTFUL_EDGES = 8;

#ifdef USE_ESP8266
/// Timer interval while nothing is pending; the zero-cross interrupt restarts the timer anyway
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
  // No confirmed zero-cross timing (yet): nothing is scheduled. The zero-cross interrupt can cancel the events
  // while this one is switching the gate on (ESP32), so keep a partly-on output off here as well.
  if (!this->timing_locked()) {
    if (this->value != 65535)
      this->gate_pin.digital_write(false);
    return 0;
  }

  // A crossing taken at its expected time can still be ahead: the running half cycle keeps its events until then
  uint32_t until_crossing = 0;
  if (this->schedule_zero_at != this->crossed_zero_at) {
    const auto ahead = static_cast<int32_t>(this->crossed_zero_at - now);
    if (ahead > 0) {
      until_crossing = ahead;
    } else {
      this->start_half_cycle();
    }
  }

  uint32_t next = 0;
  uint32_t time_since_zc = now - this->schedule_zero_at;
  // Negative when the zero-cross interrupt started a half cycle after `now` was read (ESP32): nothing is due yet
  if (this->value != 65535 && this->value != 0 && static_cast<int32_t>(time_since_zc) >= 0) {
    if (this->enable_time_us != 0 && time_since_zc >= this->enable_time_us) {
      this->enable_time_us = 0;
      this->gate_pin.digital_write(true);
      // Prevent too short pulses
      this->disable_time_us = std::max(this->disable_time_us, time_since_zc + GATE_ENABLE_TIME);
    }
    if (this->disable_time_us != 0 && time_since_zc >= this->disable_time_us) {
      this->disable_time_us = 0;
      this->gate_pin.digital_write(false);
    }

    if (time_since_zc < this->enable_time_us) {
      // Next event is enable, return time until that event
      next = this->enable_time_us - time_since_zc;
    } else if (time_since_zc < this->disable_time_us) {
      // Next event is disable, return time until that event
      next = this->disable_time_us - time_since_zc;
    } else if (time_since_zc < this->cycle_time_us) {
      next = this->cycle_time_us - time_since_zc;
    }
    // else: past the last cycle time, nothing to do until the next zero crossing, which restarts the timer
  }
  if (until_crossing != 0 && (next == 0 || until_crossing < next))
    next = until_crossing;
  return next;
}

/// Run timer interrupt code and return in how many µs the next event is expected
uint32_t IRAM_ATTR HOT timer_interrupt() {
  // Only the ESP8266 uses the returned interval; with nothing pending it waits for the next zero crossing
#ifdef USE_ESP8266
  uint32_t min_dt_us = IDLE_TIMER_INTERVAL_US;
#else
  uint32_t min_dt_us = 1000;
#endif
  uint32_t now = micros();
  for (auto *dimmer : all_dimmers) {
    if (dimmer == nullptr) {
      // no more dimmers
      break;
    }
    uint32_t res = dimmer->timer_intr(now);
    if (res != 0 && res < min_dt_us)
      min_dt_us = res;
  }
  // return time until next timer interrupt in µs
  return min_dt_us;
}

bool IRAM_ATTR HOT AcDimmerDataStore::timing_locked() const { return this->confirmed_edges >= CONFIRMING_EDGES; }

void IRAM_ATTR HOT AcDimmerDataStore::restart_timing(uint32_t now) {
  this->crossed_zero_at = now;
  this->cycle_time_us = 0;
  this->confirmed_edges = 0;
  this->doubtful_edges = 0;
  this->crossing_early = false;
}

bool IRAM_ATTR HOT AcDimmerDataStore::take_zero_cross_edge(uint32_t now) {
  if (this->crossed_zero_at == 0) {
    this->restart_timing(now);
    return false;
  }
  // Signed: the last crossing can be an expected one that is still ahead, by a quarter of a half cycle at most.
  // Further "ahead" means the time is stale: micros() got half way round (35.8 minutes) without an edge.
  const auto since = static_cast<int32_t>(now - this->crossed_zero_at);
  if (since < -static_cast<int32_t>(MIN_HALF_CYCLE_US)) {
    this->restart_timing(now);
    return false;
  }
  const auto cycle = static_cast<int32_t>(this->cycle_time_us);
  if (since < static_cast<int32_t>(MIN_HALF_CYCLE_US)) {
    // Noise, or another edge of the zero-cross pulse of the last crossing. If that crossing was taken from an
    // edge before its expected time (the pulse started early, or the triac fired just before the crossing and
    // the detector picked that up), an edge right at the crossing is the real one: it pulls the timing back and
    // confirms it. Without this an early edge in every half cycle would drag the timing along with it.
    if (this->crossing_early && since >= -cycle / 32 && since <= cycle / 32) {
      const bool started = this->schedule_zero_at == this->crossed_zero_at;
      this->crossed_zero_at += since / 2;
      this->cycle_time_us = cycle + since / 16;
      if (started)
        this->schedule_zero_at = this->crossed_zero_at;
      this->crossing_early = false;
      this->doubtful_edges = 0;
    }
    return false;
  }
  if (cycle == 0) {
    // The second edge of a measurement; the edges after it have to confirm the half cycle
    if (since > static_cast<int32_t>(MAX_HALF_CYCLE_US)) {
      this->restart_timing(now);
    } else {
      this->crossed_zero_at = now;
      this->cycle_time_us = since;
    }
    return false;
  }
  // A gap of more than four half cycles: the timing is measured again
  if (since > 4 * cycle + cycle / 2) {
    this->restart_timing(now);
    return false;
  }
  // Whole half cycles since the last crossing (a crossing can be missed) and how far this edge is from the
  // time expected from the measured half cycle
  const int32_t cycles = std::max<int32_t>((since + cycle / 2) / cycle, 1);
  const int32_t error = since - cycles * cycle;
  const bool fits = error >= -cycle / 32 && error <= cycle / 32;
  if (!this->timing_locked()) {
    // A measurement is confirmed only by edges one half cycle apart: a wrong one (taken from a noise edge or over
    // a missed crossing) can agree with every second or third real crossing
    if (!fits || cycles != 1) {
      this->restart_timing(now);
      return false;
    }
    this->crossed_zero_at = now - error / 2;
    this->cycle_time_us = cycle + error / 16;
    return ++this->confirmed_edges >= CONFIRMING_EDGES;
  }
  if (fits) {
    // The mains frequency is steady and single edges jitter: follow the edges slowly
    this->crossed_zero_at = now - error / 2;
    this->cycle_time_us = cycle + error / 16;
    this->crossing_early = error < 0;
    // Only an edge one half cycle after the last crossing shows that the timing is right; noise between the
    // crossings, however regular, then never adds up
    if (cycles == 1)
      this->doubtful_edges = 0;
    return true;
  }
  // The edge does not fit: noise if it is far too early, otherwise an early zero-cross pulse or one that
  // was served late. Too many of them in a row mean the timing is lost and is measured again.
  if (++this->doubtful_edges >= MAX_DOUBTFUL_EDGES) {
    this->restart_timing(now);
    return false;
  }
  if (error < -cycle / 4)
    return false;
  // Use the expected crossing time, not the time of this edge
  this->crossed_zero_at += cycles * cycle;
  this->crossing_early = error < 0;
  return true;
}

void IRAM_ATTR HOT AcDimmerDataStore::cancel_schedule() {
  this->enable_time_us = 0;
  this->disable_time_us = 0;
  this->schedule_zero_at = this->crossed_zero_at;
  if (this->value != 65535)
    this->gate_pin.digital_write(false);
}

void IRAM_ATTR HOT AcDimmerDataStore::zero_cross_edge(uint32_t now) {
  if (!this->take_zero_cross_edge(now)) {
    // Without a confirmed timing the events of the last half cycle mean nothing: they would be counted from the
    // edge the new measurement starts at, and could hold the gate on across a crossing
    if (!this->timing_locked())
      this->cancel_schedule();
    return;
  }
  // An early zero-cross pulse is taken at the expected crossing, which is still ahead: the running half cycle
  // keeps its events until then, and timer_intr() starts the new one
  if (static_cast<int32_t>(now - this->crossed_zero_at) >= 0)
    this->start_half_cycle();
}

/// GPIO interrupt routine, called when ZC pin triggers
void IRAM_ATTR HOT AcDimmerDataStore::gpio_intr() { this->zero_cross_edge(micros()); }

void IRAM_ATTR HOT AcDimmerDataStore::start_half_cycle() {
  this->schedule_zero_at = this->crossed_zero_at;

  if (this->value == 65535) {
    // fully on, enable output immediately
    this->gate_pin.digital_write(true);
  } else if (this->init_cycle) {
    // send a full cycle
    this->init_cycle = false;
    this->enable_time_us = 0;
    this->disable_time_us = cycle_time_us;
  } else if (this->value == 0) {
    // fully off, disable output immediately
    this->gate_pin.digital_write(false);
  } else {
    auto min_us = this->cycle_time_us * this->min_power / 1000;
    if (this->method == DIM_METHOD_TRAILING) {
      this->enable_time_us = 1;  // cannot be 0
      // calculate time until disable in µs with integer arithmetic and take into account min_power
      this->disable_time_us = std::max((uint32_t) 10, this->value * (this->cycle_time_us - min_us) / 65535 + min_us);
    } else {
      // calculate time until enable in µs: (1.0-value)*cycle_time, but with integer arithmetic
      // also take into account min_power
      this->enable_time_us = std::max((uint32_t) 1, ((65535 - this->value) * (this->cycle_time_us - min_us)) / 65535);

      // A gate still on from the last half cycle (a pulse that started just before the crossing) would fire
      // this one at full power
      this->gate_pin.digital_write(false);
      if (this->method == DIM_METHOD_LEADING_PULSE) {
        // Minimum pulse time should be enough for the triac to trigger when it is close to the ZC zone
        // this is for brightness near 99%
        this->disable_time_us = std::max(this->enable_time_us + GATE_ENABLE_TIME, (uint32_t) cycle_time_us / 10);
      } else {
        this->disable_time_us = this->cycle_time_us;
      }
    }
  }
}

void IRAM_ATTR HOT AcDimmerDataStore::s_gpio_intr(AcDimmerDataStore *store) {
  // Attaching pin interrupts on the same pin will override the previous interrupt
  // However, the user expects that multiple dimmers sharing the same ZC pin will work.
  // We solve this in a bit of a hacky way: On each pin interrupt, we check all dimmers
  // if any of them are using the same ZC pin, and also trigger the interrupt for *them*.
  for (auto *dimmer : all_dimmers) {
    if (dimmer == nullptr)
      break;
    if (dimmer->zero_cross_pin_number == store->zero_cross_pin_number) {
      dimmer->gpio_intr();
    }
  }
#ifdef USE_ESP8266
  // Run the timer code right away: it schedules the gate events of this half cycle
  esp8266_timer_in(2);
#endif
}

#ifdef USE_ESP8266
// ESP8266: a one-shot maskable interrupt on CCOMPARE0 (timer0), only at the gate events. Not timer1: the waveform
// generator runs it as an NMI at 1-10 kHz, too much load with WiFi. Masked sections can delay a gate event slightly.
// The cycle counter runs twice as fast during a CpuFrequencyBoost; the timer then fires early and re-arms.
static void IRAM_ATTR esp8266_timer_in(uint32_t us) {
  const uint32_t cycles = us * (arch_get_cpu_freq_hz() / 1000000);
  uint32_t target;
  do {
    target = arch_get_cpu_cycle_count() + cycles;
    timer0_write(target);
    // An NMI between reading the counter and writing the compare can leave the target in the past
  } while (static_cast<int32_t>(target - arch_get_cpu_cycle_count()) <= 0);
}
static void IRAM_ATTR HOT esp8266_timer_isr() { esp8266_timer_in(timer_interrupt()); }
#endif

#ifdef USE_ESP32
// ESP32 implementation, uses basically the same code but needs to wrap
// timer_interrupt() function to auto-reschedule
static HWTimer *dimmer_timer = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
void IRAM_ATTR HOT AcDimmerDataStore::s_timer_intr() { timer_interrupt(); }
#endif

void AcDimmer::setup() {
  // extend all_dimmers array with our dimmer

  // Need to be sure the zero cross pin is setup only once, ESP8266 fails and ESP32 seems to fail silently
  auto setup_zero_cross_pin = true;

  for (auto &all_dimmer : all_dimmers) {
    if (all_dimmer == nullptr) {
      all_dimmer = &this->store_;
      break;
    }
    if (all_dimmer->zero_cross_pin_number == this->zero_cross_pin_->get_pin()) {
      setup_zero_cross_pin = false;
    }
  }

  this->gate_pin_->setup();
  this->store_.gate_pin = this->gate_pin_->to_isr();
  this->store_.zero_cross_pin_number = this->zero_cross_pin_->get_pin();
  this->store_.min_power = static_cast<uint16_t>(this->min_power_ * 1000);
  this->min_power_ = 0;

  if (setup_zero_cross_pin) {
    this->zero_cross_pin_->setup();
    this->zero_cross_pin_->attach_interrupt(&AcDimmerDataStore::s_gpio_intr, &this->store_,
                                            this->zero_cross_interrupt_type_);
  }

#ifdef USE_ESP8266
  // One timer for all dimmers (see esp8266_timer_isr)
  static bool timer_started = false;
  if (!timer_started) {
    timer_started = true;
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
  ESP_LOGV(TAG, "  Estimated Frequency: %.3fHz", 1e6f / this->store_.cycle_time_us / 2);
}

}  // namespace esphome::ac_dimmer
