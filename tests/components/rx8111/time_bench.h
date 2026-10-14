#pragma once

#include "esphome/components/rx8111/rx8111.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sys/time.h>

namespace rx8111_time_bench {
using namespace esphome;
using namespace esphome::rx8111;

// Time-only PR 1 bench: external GPIO/PCNT are not RX8111 child entity platforms.
class Runner {
 public:
  unsigned alarm_callbacks{0}, timer_callbacks{0}, event_callbacks{0};

  void begin(RX8111Component *rtc) {
    this->rtc_ = rtc;
    this->started_ = millis();
    this->phase_ = CLOCK;
    ESP_LOGW("rx8111_time_bench", "Destructive RTC test: keep independent USB power connected");
    uint8_t flags, power;
    if (!this->require(!rtc->is_failed(), "RTC setup") || !this->read(RX8111_REG_FLAG, flags) ||
        !this->read(RX8111_REG_PWRSWITCH, power) ||
        !this->require(!(power & RX8111_PWR_CHGEN), "Charging must be disabled"))
      return;
    ESP_LOGI("rx8111_time_bench", "Initial FLAG=%02X, charging off", flags);
    this->pass("RTC I2C and charging-off readback");
    id(evin_driver).turn_off();
    if (!this->write(RX8111_REG_CONTROL, 0) || !this->require(this->rtc_->unset_alarm(), "Unset boot alarm") ||
        !this->write(RX8111_REG_EXTENSION, RX8111_EXT_FSEL_MASK) || !this->write(RX8111_REG_TIMERCTRL, 0) ||
        !this->write(RX8111_REG_FLAG, 0) || !this->seed(1769903997))  // 2026-01-31 23:59:57 UTC, not real time.
      return;
    // Invalid one-hot weekday must not synchronize the MCU clock.
    for (uint8_t invalid : {0x00, 0x03}) {
      if (!this->write(RX8111_REG_WEEK, invalid))
        return;
      timeval tv{1769900397, 0};
      if (!this->require(settimeofday(&tv, nullptr) == 0, "Prepare invalid-weekday test"))
        return;
      this->rtc_->read_time();
      if (!this->require(this->rtc_->utcnow().timestamp < 1769900400, "Reject invalid clock weekday") ||
          !this->seed(1769903997))
        return;
    }
    this->pass("Invalid weekdays rejected and time write/read restored");
    this->enter(CLOCK, "Calendar rollover", 7000);
  }

  void tick() {
    if (this->rtc_ == nullptr || this->phase_ == DONE)
      return;
    const uint32_t elapsed = millis() - this->phase_started_;
    if (elapsed > this->timeout_ && this->phase_ != FOUT_MEASURE && this->phase_ != ALARM_CLEAR &&
        this->phase_ != EVENT_CLEAR) {
      this->fail("Phase deadline exceeded");
      return;
    }
    this->rtc_->update();
    uint8_t flags;
    if (!this->read(RX8111_REG_FLAG, flags))
      return;
    if (!this->require(!(flags & (RX8111_FLAG_POR | RX8111_FLAG_XST | RX8111_FLAG_VLF)),
                       "No power or oscillator fault"))
      return;
    const auto now = this->rtc_->utcnow();
    const time_t expected = this->seed_epoch_ + (millis() - this->seed_ms_) / 1000;
    if (!this->require(now.is_valid() && now.timestamp >= expected - 2 && now.timestamp <= expected + 2,
                       "Clock advancing at expected rate") ||
        !this->clock_registers(expected))
      return;

    if (this->timeout_ >= 7000 && elapsed / 5000 > this->last_wait_report_) {
      this->last_wait_report_ = elapsed / 5000;
      ESP_LOGI("rx8111_time_bench", "WAIT: %s elapsed=%lus deadline=%lus PASS=%u FAIL=%u", this->label_,
               static_cast<unsigned long>(elapsed / 1000), static_cast<unsigned long>(this->timeout_ / 1000),
               this->passed_, this->failed_);
    }

    switch (this->phase_) {
      case CLOCK: {
        if (elapsed < 5000)
          return;
        if (!this->require(now.month == 2 && now.day_of_month == 1 && now.day_of_week == 1,
                           "January to February weekday rollover"))
          return;
        this->pass("Clock read/write, ticking, calendar/weekday rollover");
        this->alarm_base_ = this->alarm_callbacks;
        const auto target = ESPTime::from_epoch_utc(now.timestamp + 3);
        SetAlarmAction<> alarm;
        alarm.set_parent(this->rtc_);
        alarm.set_second(target.second);
        alarm.set_minute(target.minute);
        alarm.set_hour(target.hour);
        alarm.set_day(target.day_of_month);
        alarm.play();
        if (!this->write(RX8111_REG_CONTROL, RX8111_CTRL_AIE))
          return;
        this->enter(ALARM_WAIT, "Set alarm and on_alarm", 7000);
        break;
      }
      case ALARM_WAIT:
      case ALARM_REARM:
        if ((flags & RX8111_FLAG_AF) && this->alarm_callbacks == this->alarm_base_ + 1) {
          if (!this->require(id(rtc_int).state, "INT LOW for AF"))
            return;
          this->pass(this->phase_ == ALARM_WAIT ? "Alarm register, AF, on_alarm, INT" : "Relative alarm rearm");
          this->enter(ALARM_HOLD, "Alarm latch and deduplication", 1000);
        } else if (elapsed > this->timeout_) {
          this->fail("Alarm did not trigger once");
        }
        break;
      case ALARM_HOLD:
        if (!this->require((flags & RX8111_FLAG_AF) && this->alarm_callbacks == this->alarm_base_ + 1,
                           "AF holds with no duplicate callback"))
          return;
        if (elapsed < 700)
          return;
        if (!this->require(this->rtc_->unset_alarm(), "Unset alarm preserves AF") ||
            !this->require(id(rtc_int).state, "INT held by AF after unset"))
          return;
        this->rtc_->clear_alarm_flag();
        this->enter(ALARM_CLEAR, "Wait for AF clear and INT release", 2000);
        break;
      case ALARM_CLEAR:
        // The GPIO binary sensor publishes in its own loop. Its .state still
        // contains the old asserted value immediately after the I2C write.
        if (elapsed < 300)
          return;
        if (!this->read(RX8111_REG_FLAG, flags) ||
            !this->require(!(flags & RX8111_FLAG_AF), "Explicit AF clear readback"))
          return;
        if (id(rtc_int).state) {
          if (elapsed >= this->timeout_) {
            uint8_t control = 0;
            if (this->read(RX8111_REG_CONTROL, control)) {
              ESP_LOGE("rx8111_time_bench", "INT stayed LOW: FLAG=%02X CONTROL=%02X", flags, control);
            }
            this->fail("INT remains LOW after AF clear; check other flags and wiring");
          }
          return;
        }
        this->pass("Unset, explicit AF clear and INT release");
        if (!this->rearmed_) {
          this->rearmed_ = true;
          this->alarm_base_ = this->alarm_callbacks;
          ScheduleAlarmInAction<> relative;
          relative.set_parent(this->rtc_);
          relative.set_seconds(3);
          relative.play();
          this->enter(ALARM_REARM, "Schedule alarm in 3 seconds", 7000);
        } else {
          this->timer_index_ = 0;
          this->start_timer();
        }
        break;
      case TIMER_WAIT:
      case TIMER_REARM:
        if ((flags & RX8111_FLAG_TF) && this->timer_callbacks == this->timer_base_ + 1) {
          if (this->phase_ == TIMER_REARM) {
            const uint32_t gap = millis() - this->first_timer_ms_;
            if (!this->require(gap + 600 >= this->timer_period_ && gap <= this->timer_period_ + 1300,
                               "Timer repeat period"))
              return;
            this->pass("Timer source, TF and repeat callback");
            if (++this->timer_index_ < 4)
              this->start_timer();
            else
              this->start_event();
          } else {
            this->first_timer_ms_ = millis();
            this->enter(TIMER_HOLD, "TF latch", 1000);
          }
        } else if (elapsed > this->timeout_) {
          this->fail("Timer TF/callback missing");
        }
        break;
      case TIMER_HOLD:
        if (!this->require((flags & RX8111_FLAG_TF) && this->timer_callbacks == this->timer_base_ + 1,
                           "TF held with no duplicate callback"))
          return;
        if (elapsed < 700)
          return;
        this->rtc_->clear_timer_flag();
        this->rtc_->update();
        if (!this->read(RX8111_REG_FLAG, flags) || !this->require(!(flags & RX8111_FLAG_TF), "Explicit TF clear"))
          return;
        this->timer_base_ = this->timer_callbacks;
        this->enter(TIMER_REARM, "Timer repeat", this->timer_period_ + 2500);
        break;
      case EVENT_IDLE: {
        uint8_t status;
        if (!this->read(RX8111_REG_STATUS, status) ||
            !this->require(!(status & RX8111_STATUS_EVIN), "EVIN inactive baseline"))
          return;
        this->event_base_ = this->event_callbacks;
        id(evin_driver).turn_on();
        this->enter(EVENT_WAIT, "EVIN HIGH event", 3500);
        break;
      }
      case EVENT_WAIT:
        if ((flags & RX8111_FLAG_EVF) && this->event_callbacks == this->event_base_ + 1) {
          uint8_t status;
          if (!this->read(RX8111_REG_STATUS, status) ||
              !this->require((status & RX8111_STATUS_EVIN) && id(rtc_int).state, "EVIN HIGH / INT LOW") ||
              !this->check_timestamp_registers())
            return;
          this->pass("EVIN, EVF, on_event, INT and raw timestamp capture");
          id(evin_driver).turn_off();
          this->enter(EVENT_HOLD, "EVF latch", 800);
        } else if (elapsed > this->timeout_) {
          this->fail("EVF/on_event missing");
        }
        break;
      case EVENT_HOLD:
        if (!this->require((flags & RX8111_FLAG_EVF) && this->event_callbacks == this->event_base_ + 1,
                           "EVF remains set without repeat callback"))
          return;
        if (elapsed < 500)
          return;
        this->rtc_->clear_event_flag();
        this->enter(EVENT_CLEAR, "Wait for EVF clear and INT release", 2000);
        break;
      case EVENT_CLEAR:
        if (elapsed < 300)
          return;
        if (!this->read(RX8111_REG_FLAG, flags) || !this->require(!(flags & RX8111_FLAG_EVF), "EVF clear readback"))
          return;
        if (id(rtc_int).state) {
          if (elapsed >= this->timeout_)
            this->fail("INT remains LOW after EVF clear; check other flags and wiring");
          return;
        }
        this->pass("Event latch/acknowledge and callback deduplication");
        this->start_fout();
        break;
      case FOUT_SETTLE:
        if (elapsed < 600)
          return;
        id(fout_counter).update();  // Discard switching transient.
        this->samples_ = 0;
        this->sum_ = 0;
        this->enter(FOUT_MEASURE, "Measure FOUT", 10000);
        break;
      case FOUT_MEASURE: {
        id(fout_counter).update();
        const float rate = id(fout_counter).state / 60.0f;
        if (!this->require(std::isfinite(rate), "FOUT counter finite"))
          return;
        this->sum_ += rate;
        this->samples_++;
        const uint32_t window = this->fout_index_ == 1 ? 8000 : 2000;
        if (elapsed < window)
          return;
        const float average = this->sum_ / this->samples_;
        const float expected_rate = FOUT_RATES[this->fout_index_];
        const float tolerance = this->fout_index_ == 0 ? 0.1f : (this->fout_index_ == 1 ? 0.3f : expected_rate * 0.05f);
        ESP_LOGI("rx8111_time_bench", "FOUT expected=%.0fHz measured=%.2fHz", expected_rate, average);
        if (!this->require(std::fabs(average - expected_rate) <= tolerance, "FOUT frequency"))
          return;
        this->pass("FOUT frequency selection and PCNT measurement");
        if (++this->fout_index_ < 4)
          this->start_fout();
        else
          this->finish();
        break;
      }
      default:
        break;
    }
  }

 protected:
  enum Phase {
    CLOCK,
    ALARM_WAIT,
    ALARM_REARM,
    ALARM_HOLD,
    ALARM_CLEAR,
    TIMER_WAIT,
    TIMER_HOLD,
    TIMER_REARM,
    EVENT_IDLE,
    EVENT_WAIT,
    EVENT_HOLD,
    EVENT_CLEAR,
    FOUT_SETTLE,
    FOUT_MEASURE,
    DONE
  };
  static constexpr float FOUT_RATES[4] = {0, 1, 1024, 32768};
  RX8111Component *rtc_{nullptr};
  Phase phase_{DONE};
  const char *label_{"startup"};
  unsigned passed_{0}, failed_{0}, skipped_{0};
  unsigned alarm_base_{0}, timer_base_{0}, event_base_{0};
  unsigned timer_index_{0}, fout_index_{0}, samples_{0};
  uint32_t started_{0}, phase_started_{0}, timeout_{0}, seed_ms_{0}, last_wait_report_{0};
  uint32_t timer_period_{0}, first_timer_ms_{0};
  time_t seed_epoch_{0};
  float sum_{0};
  bool rearmed_{false};

  void enter(Phase phase, const char *label, uint32_t timeout) {
    this->phase_ = phase;
    this->label_ = label;
    this->phase_started_ = millis();
    this->timeout_ = timeout;
    this->last_wait_report_ = 0;
    ESP_LOGI("rx8111_time_bench", "RUN: %s", label);
  }
  void pass(const char *label) {
    this->passed_++;
    ESP_LOGI("rx8111_time_bench", "PASS: %s", label);
  }
  bool require(bool ok, const char *label) {
    if (!ok)
      this->fail(label);
    return ok;
  }
  void fail(const char *label) {
    this->failed_++;
    ESP_LOGE("rx8111_time_bench", "FAIL: %s (phase %s)", label, this->label_);
    this->finish();
  }
  bool read(uint8_t reg, uint8_t &value) { return this->require(this->rtc_->read_byte(reg, &value), "I2C read"); }
  bool write(uint8_t reg, uint8_t value) { return this->require(this->rtc_->write_byte(reg, value), "I2C write"); }
  bool seed(time_t epoch) {
    timeval tv{epoch, 0};
    if (!this->require(settimeofday(&tv, nullptr) == 0, "Set synthetic clock"))
      return false;
    this->rtc_->write_time();
    tv.tv_sec -= 3600;
    if (!this->require(settimeofday(&tv, nullptr) == 0, "Disturb software clock"))
      return false;
    this->rtc_->read_time();
    const auto readback = this->rtc_->utcnow();
    if (!this->require(readback.is_valid() && readback.timestamp >= epoch && readback.timestamp <= epoch + 1,
                       "Clock read/write round trip"))
      return false;
    this->seed_epoch_ = epoch;
    this->seed_ms_ = millis();
    return true;
  }
  bool clock_registers(time_t expected) {
    uint8_t bytes[7];
    if (!this->require(this->rtc_->read_bytes(RX8111_REG_SEC, bytes, sizeof(bytes)), "Clock read"))
      return false;
    auto bcd = [](unsigned value) -> uint8_t { return ((value / 10) << 4) | (value % 10); };
    for (int offset = -2; offset <= 2; offset++) {
      const auto candidate = ESPTime::from_epoch_utc(expected + offset);
      const uint8_t match[7] = {bcd(candidate.second),       bcd(candidate.minute),
                                bcd(candidate.hour),         static_cast<uint8_t>(1U << (candidate.day_of_week - 1)),
                                bcd(candidate.day_of_month), bcd(candidate.month),
                                bcd(candidate.year - 2000)};
      if (!std::memcmp(bytes, match, sizeof(bytes)))
        return true;
    }
    return this->require(false, "Raw clock calendar/weekday mismatch");
  }
  bool check_timestamp_registers() {
    uint8_t extension, raw[10];
    if (!this->read(RX8111_REG_EXTENSION, extension) ||
        !this->require(extension & RX8111_EXT_ETS, "Timestamp capture enabled") ||
        !this->write(RX8111_REG_EXTENSION, extension & ~RX8111_EXT_ETS))
      return false;
    const bool read_ok = this->rtc_->read_bytes(RX8111_REG_TIMESTAMP_SINGLE, raw, sizeof(raw));
    if (!this->write(RX8111_REG_EXTENSION, extension) || !this->require(read_ok, "Raw timestamp read"))
      return false;
    auto bcd = [](unsigned value) -> uint8_t { return ((value / 10) << 4) | (value % 10); };
    for (int offset = -1; offset <= 0; offset++) {
      const auto candidate = ESPTime::from_epoch_utc(this->rtc_->utcnow().timestamp + offset);
      if (raw[2] == bcd(candidate.second) && raw[3] == bcd(candidate.minute) && raw[4] == bcd(candidate.hour) &&
          raw[6] == bcd(candidate.day_of_month) && raw[7] == bcd(candidate.month) &&
          raw[8] == bcd(candidate.year - 2000))
        return true;
    }
    return this->require(false, "Raw captured timestamp matches current UTC second");
  }
  void start_timer() {
    static constexpr uint32_t COUNTS[4] = {8192, 128, 2, 1};
    static constexpr const char *NAMES[4] = {"4096Hz", "64Hz", "1Hz", "1/60Hz"};
    this->timer_period_ = this->timer_index_ == 3 ? 60000 : 2000;
    if (!this->write(RX8111_REG_EXTENSION, RX8111_EXT_FSEL_MASK) || !this->write(RX8111_REG_CONTROL, RX8111_CTRL_AIE) ||
        !this->write(RX8111_REG_TIMERCTRL, 0))
      return;
    this->rtc_->clear_timer_flag();
    this->rtc_->update();
    const uint32_t count = COUNTS[this->timer_index_];
    const uint8_t bytes[3] = {static_cast<uint8_t>(count), static_cast<uint8_t>(count >> 8),
                              static_cast<uint8_t>(count >> 16)};
    if (!this->require(this->rtc_->write_bytes(RX8111_REG_TIMER0, bytes, sizeof(bytes)), "Timer count") ||
        !this->write(RX8111_REG_CONTROL, RX8111_CTRL_AIE | RX8111_CTRL_TIE) ||
        !this->write(RX8111_REG_EXTENSION, RX8111_EXT_FSEL_MASK | RX8111_EXT_TE | this->timer_index_))
      return;
    this->timer_base_ = this->timer_callbacks;
    ESP_LOGI("rx8111_time_bench", "Timer source=%s count=%lu", NAMES[this->timer_index_],
             static_cast<unsigned long>(count));
    this->enter(TIMER_WAIT, "Timer source/TF/on_timer", this->timer_period_ + 3000);
  }
  void start_event() {
    id(evin_driver).turn_off();
    if (!this->write(RX8111_REG_EXTENSION, RX8111_EXT_FSEL_MASK) || !this->write(RX8111_REG_CONTROL, 0))
      return;
    this->rtc_->clear_timer_flag();
    this->rtc_->clear_event_flag();
    if (!this->write(RX8111_REG_TSCTRL1, 0) ||
        !this->write(RX8111_REG_EVIN_SETTING, RX8111_EVIN_EHL | RX8111_EVIN_PDN | (2 << RX8111_EVIN_ET_SHIFT)) ||
        !this->write(RX8111_REG_CONTROL, RX8111_CTRL_EIE) ||
        !this->write(RX8111_REG_EXTENSION, RX8111_EXT_FSEL_MASK | RX8111_EXT_ETS))
      return;
    this->enter(EVENT_IDLE, "Wait for EVIN inactive", 1500);
  }
  void start_fout() {
    static constexpr uint8_t SELECTION[4] = {3, 2, 1, 0};
    if (!this->write(RX8111_REG_CONTROL, 0) ||
        !this->write(RX8111_REG_EXTENSION, SELECTION[this->fout_index_] << RX8111_EXT_FSEL_SHIFT))
      return;
    uint8_t extension;
    if (!this->read(RX8111_REG_EXTENSION, extension) ||
        !this->require((extension & RX8111_EXT_FSEL_MASK) == (SELECTION[this->fout_index_] << RX8111_EXT_FSEL_SHIFT),
                       "FOUT selection readback"))
      return;
    this->enter(FOUT_SETTLE, "FOUT off/1/1024/32768 Hz", 1500);
  }
  void finish() {
    this->phase_ = DONE;
    id(evin_driver).turn_off();
    const bool control_ok = this->rtc_->write_byte(RX8111_REG_CONTROL, 0);
    const bool timer_ok = this->rtc_->write_byte(RX8111_REG_TIMERCTRL, 0);
    const bool extension_ok = this->rtc_->write_byte(RX8111_REG_EXTENSION, RX8111_EXT_FSEL_MASK);
    const bool flags_ok = this->rtc_->write_byte(RX8111_REG_FLAG, 0);
    const bool ok = control_ok && timer_ok && extension_ok && flags_ok;
    if (!ok) {
      this->failed_++;
      ESP_LOGE("rx8111_time_bench", "FAIL: cleanup I2C writes; RTC state uncertain");
    }
    this->skipped_ += 3;
    ESP_LOGW("rx8111_time_bench", "SKIP: narrow INT pulses/glitches need a scope");
    ESP_LOGW("rx8111_time_bench", "SKIP: backup switchover/retention, thresholds and charging need equipment");
    ESP_LOGW("rx8111_time_bench", "SKIP: long-term accuracy and other EVIN levels/filters need separate testing");
    ESP_LOGI("rx8111_time_bench", "RESULT: %s PASS=%u FAIL=%u SKIP=%u elapsed=%lus",
             this->failed_ ? "FAILED" : "AUTOMATED CHECKS PASSED (SKIPS UNVERIFIED)", this->passed_, this->failed_,
             this->skipped_, static_cast<unsigned long>((millis() - this->started_) / 1000));
    ESP_LOGW("rx8111_time_bench", "Restore normal firmware and RTC time");
  }
};

inline Runner runner;
}  // namespace rx8111_time_bench
