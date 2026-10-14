// Datasheets:
//   RX8111CE: Epson ETM61E application manual

#include "rx8111.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rx8111 {

ESPHOME_LOG_TAG(TAG, "rx8111");

namespace {

[[maybe_unused]] const char *timestamp_record_mode_to_string_(TimestampRecordMode mode) {
  switch (mode) {
    case TIMESTAMP_RECORD_LATEST:
      return "latest";
    case TIMESTAMP_RECORD_STOP_WHEN_FULL:
      return "stop_when_full";
    case TIMESTAMP_RECORD_OVERWRITE:
      return "overwrite";
    default:
      return "unknown";
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Runtime alarm helpers
// ---------------------------------------------------------------------------
bool RX8111Component::set_alarm_runtime(uint8_t second, uint8_t minute, uint8_t hour, uint8_t weekday, uint8_t day) {
  if (weekday != 0xFF && day != 0xFF) {
    ESP_LOGE(TAG, "set_alarm_runtime: cannot set both weekday and day");
    return false;
  }

  this->alarm_second_ = second;
  this->alarm_minute_ = minute;
  this->alarm_hour_ = hour;
  this->alarm_weekday_ = weekday;
  this->alarm_day_ = day;

  return this->configure_alarm_(false);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
bool RX8111Component::configure_fout_() {
  uint8_t ext = 0;
  if (!this->read_byte(RX8111_REG_EXTENSION, &ext)) {
    ESP_LOGE(TAG, "Can't read extension register");
    return false;
  }
  ext &= ~RX8111_EXT_FSEL_MASK;
  ext |= (static_cast<uint8_t>(this->fout_frequency_) << RX8111_EXT_FSEL_SHIFT) & RX8111_EXT_FSEL_MASK;
  if (!this->write_byte(RX8111_REG_EXTENSION, ext)) {
    ESP_LOGE(TAG, "Can't write extension register");
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
bool RX8111Component::clear_alarm_flag_() {
  uint8_t flags = 0;
  if (!this->read_byte(RX8111_REG_FLAG, &flags))
    return false;
  flags &= ~RX8111_FLAG_AF;
  if (!this->write_byte(RX8111_REG_FLAG, flags))
    return false;
  this->prev_alarm_flag_ = false;
  return true;
}

bool RX8111Component::clear_timer_flag_() {
  uint8_t flags = 0;
  if (!this->read_byte(RX8111_REG_FLAG, &flags))
    return false;
  flags &= ~RX8111_FLAG_TF;
  if (!this->write_byte(RX8111_REG_FLAG, flags))
    return false;
  this->prev_timer_flag_ = false;
  return true;
}

bool RX8111Component::clear_event_flag_() {
  const uint8_t evf_mask = RX8111_FLAG_EVF;
  uint8_t flags = 0;
  if (!this->read_byte(RX8111_REG_FLAG, &flags))
    return false;
  flags &= ~evf_mask;
  if (!this->write_byte(RX8111_REG_FLAG, flags))
    return false;
  this->prev_event_flag_ = false;
  return true;
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void RX8111Component::setup() {
  // 1. Report invalid-time flags without discarding them.
  if (!this->check_time_validity_()) {
    this->mark_failed();
    return;
  }

  // 2. Configure battery backup (INIEN, optionally CHGEN).
  if (!this->configure_battery_backup_()) {
    this->status_set_warning();
    ESP_LOGW(TAG, "Battery backup configuration failed");
  }

  // 3. Configure FOUT clock output.
  if (!this->configure_fout_()) {
    ESP_LOGW(TAG, "FOUT configuration failed");
  }

  // 4. Configure alarm registers, or explicitly disable alarm logic if omitted.
  if (this->alarm_enabled_) {
    if (!this->configure_alarm_()) {
      ESP_LOGW(TAG, "Alarm configuration failed");
    }
  } else if (!this->disable_alarm_()) {
    ESP_LOGW(TAG, "Alarm disable failed");
  }

  // 5. Configure wake-up timer, or explicitly disable it if omitted.
  if (this->timer_enabled_) {
    if (!this->configure_timer_()) {
      ESP_LOGW(TAG, "Timer configuration failed");
    }
  } else if (!this->disable_timer_()) {
    ESP_LOGW(TAG, "Timer disable failed");
  }

  // 6. Configure EVIN event interrupt, or explicitly disable it if omitted.
  if (this->event_enabled_) {
    if (!this->configure_event_()) {
      ESP_LOGW(TAG, "Event configuration failed");
    }
  } else if (!this->disable_event_()) {
    ESP_LOGW(TAG, "Event disable failed");
  }

  // 7. Apply timestamp routing and EVIN settings.
  if (!this->apply_runtime_options_()) {
    ESP_LOGW(TAG, "Runtime options failed");
  }

  // 8. Attempt an initial time read.
  this->read_time();
}

// ---------------------------------------------------------------------------
// update() - called periodically by the scheduler
//
// Design notes:
//   - AF (alarm flag) is NEVER auto-cleared here. AF holds /INT (or /IRQ) low
//     until the user explicitly calls rx8111.clear_alarm_flag, which both clears
//     the flag in software and releases the /INT pin to drive external circuits.
//   - TF (timer flag) is NOT auto-cleared here either, but note that the timer
//     /INT pin auto-releases in hardware after tRTN regardless of TF. Calling
//     rx8111.clear_timer_flag is only needed to re-arm the on_timer edge detection
//     for the next event — it has no effect on the /INT pin.
//   - EVF (event flag) behaves the same as AF: held until clear_event_flag is
//     called. /INT is held low while EVF is set (if EIE is enabled). Clearing
//     EVF releases /INT from the event, but not from AF or TF if those are set.
//   - on_alarm / on_timer / on_event callbacks fire only on the 0->1 rising
//     edge so they trigger exactly once per event, not on every poll while the
//     flag stays set.
// ---------------------------------------------------------------------------
void RX8111Component::update() {
  this->read_time();

  uint8_t flag_byte = 0;
  if (!this->read_byte(RX8111_REG_FLAG, &flag_byte)) {
    ESP_LOGW(TAG, "Failed to read flag register");
    return;
  }

  const bool alarm_set = (flag_byte & RX8111_FLAG_AF) != 0;
  const bool timer_set = (flag_byte & RX8111_FLAG_TF) != 0;
  const uint8_t evf_mask = RX8111_FLAG_EVF;
  const bool event_set = (flag_byte & evf_mask) != 0;
  const bool event_edge = event_set && !this->prev_event_flag_;
  const bool alarm_edge = alarm_set && !this->prev_alarm_flag_;
  const bool timer_edge = timer_set && !this->prev_timer_flag_;
  // Store the snapshot before any entity or automation callbacks can clear flags.
  this->prev_alarm_flag_ = alarm_set;
  this->prev_timer_flag_ = timer_set;
  this->prev_event_flag_ = event_set;

  // Fire trigger objects only on rising-edge transition (0->1).
  if (alarm_edge)
    for (auto *t : this->alarm_triggers_)
      t->trigger();
  if (timer_edge)
    for (auto *t : this->timer_triggers_)
      t->trigger();

  if (event_edge)
    for (auto *t : this->event_triggers_)
      t->trigger();
}

// ---------------------------------------------------------------------------
// dump_config()
// ---------------------------------------------------------------------------
void RX8111Component::dump_config() {
  ESP_LOGCONFIG(TAG, "%s:", "RX8111CE");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  ESP_LOGCONFIG(TAG, "  Battery Backup: %s", ONOFF(this->battery_backup_));
  if (this->battery_backup_) {
    ESP_LOGCONFIG(TAG, "  Battery Charging: %s", ONOFF(this->battery_charging_));
  }
  const char *fout_str = "OFF";
  switch (this->fout_frequency_) {
    case FoutFrequency::FOUT_32768_HZ:
      fout_str = "32768 Hz";
      break;
    case FoutFrequency::FOUT_1024_HZ:
      fout_str = "1024 Hz";
      break;
    case FoutFrequency::FOUT_1_HZ:
      fout_str = "1 Hz";
      break;
    case FoutFrequency::FOUT_OFF:
      fout_str = "OFF";
      break;
  }
  ESP_LOGCONFIG(TAG, "  FOUT Frequency: %s", fout_str);
  if (this->alarm_enabled_) {
    ESP_LOGCONFIG(TAG, "  Alarm: enabled");
  }
  if (this->timer_enabled_) {
    ESP_LOGCONFIG(TAG, "  Timer: enabled (count=%" PRIu32 ")", this->timer_count_);
  }
  if (this->event_enabled_) {
    ESP_LOGCONFIG(TAG, "  Event: enabled (%s level)", this->event_level_ == EVENT_LEVEL_HIGH ? "high" : "low");
  }
  ESP_LOGCONFIG(TAG, "  Timestamp Capture: %s", ONOFF(this->timestamp_capture_active_()));
  if (this->timestamp_capture_active_()) {
    ESP_LOGCONFIG(TAG, "  Timestamp Record Mode: %s", timestamp_record_mode_to_string_(this->timestamp_record_mode_));
  }
  if (this->event_enabled_ || this->timestamp_capture_active_()) {
    static const char *const FILTER_NAMES[] = {"off", "3.9ms", "15.6ms", "125ms"};
    ESP_LOGCONFIG(TAG, "  EVIN Event Level: %s", this->event_level_ == EVENT_LEVEL_HIGH ? "high" : "low");
    ESP_LOGCONFIG(TAG, "  EVIN Filter: %s (level %u)", FILTER_NAMES[this->evin_filter_], this->evin_filter_);
    const char *pull_str = "none (Hi-Z)";
    switch (this->evin_pull_) {
      case EVIN_PULL_UP_500K:
        pull_str = "pull-up 500k";
        break;
      case EVIN_PULL_UP_1M:
        pull_str = "pull-up 1M";
        break;
      case EVIN_PULL_UP_10M:
        pull_str = "pull-up 10M";
        break;
      case EVIN_PULL_DOWN_500K:
        pull_str = "pull-down 500k";
        break;
      default:
        break;
    }
    ESP_LOGCONFIG(TAG, "  EVIN Pull: %s", pull_str);
  }
  RealTimeClock::dump_config();
}

// ---------------------------------------------------------------------------
// read_time() - read 0x10-0x16, convert to ESPTime, sync system clock
// ---------------------------------------------------------------------------
void RX8111Component::read_time() {
  uint8_t flags;
  if (!this->read_byte(RX8111_REG_FLAG, &flags)) {
    ESP_LOGW(TAG, "Cannot verify RTC time validity");
    return;
  }
  if ((flags & (RX8111_FLAG_VLF | RX8111_FLAG_XST | RX8111_FLAG_POR)) != 0) {
    ESP_LOGW(TAG, "RTC time invalidated by power loss or oscillator stop; write trusted time");
    return;
  }
  uint8_t buf[7];
  // Burst read from SEC register. During I2C access the chip automatically
  // freezes the counter chain, so all 7 bytes form a coherent snapshot.
  if (!this->read_bytes(RX8111_REG_SEC, buf, sizeof(buf))) {
    ESP_LOGE(TAG, "Can't read time registers");
    return;
  }

  for (size_t i = 0; i < sizeof(buf); i++) {
    if (i != 3 && ((buf[i] & 0x0F) > 9 || (buf[i] >> 4) > 9)) {
      ESP_LOGW(TAG, "Invalid BCD in RTC time");
      return;
    }
  }
  const uint8_t weekday = onehot_to_weekday_(buf[3]);
  if (weekday == 0) {
    ESP_LOGW(TAG, "Invalid RTC weekday");
    return;
  }
  ESPTime rtc_time{
      .second = bcd_to_uint8_(buf[0] & 0x7F),
      .minute = bcd_to_uint8_(buf[1] & 0x7F),
      .hour = bcd_to_uint8_(buf[2] & 0x3F),
      .day_of_week = weekday,
      .day_of_month = bcd_to_uint8_(buf[4] & 0x3F),
      .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
      .month = bcd_to_uint8_(buf[5] & 0x1F),
      .year = static_cast<uint16_t>(bcd_to_uint8_(buf[6]) + 2000),
      .is_dst = false,
      .timestamp = 0,  // overwritten by recalc_timestamp_utc
  };
  rtc_time.recalc_timestamp_utc(false);

  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock");
    return;
  }
  const auto calendar = ESPTime::from_epoch_utc(rtc_time.timestamp);
  if (calendar.year != rtc_time.year || calendar.month != rtc_time.month ||
      calendar.day_of_month != rtc_time.day_of_month || calendar.day_of_week != weekday) {
    ESP_LOGW(TAG, "Invalid RTC calendar date or weekday");
    return;
  }

  ESP_LOGD(TAG, "Read %04d-%02d-%02d %02d:%02d:%02d DOW=%d", rtc_time.year, rtc_time.month, rtc_time.day_of_month,
           rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.day_of_week);

  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

// ---------------------------------------------------------------------------
// write_time() - freeze counter, write 0x10-0x16, restart counter
// ---------------------------------------------------------------------------
void RX8111Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid() || now.year > 2099) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC");
    return;
  }

  // Freeze the timekeeping counter while writing to prevent partial-update glitches.
  if (!this->set_stop_bit_(true)) {
    ESP_LOGE(TAG, "Failed to set STOP bit before time write");
    return;
  }

  uint8_t buf[7];
  buf[0] = uint8_to_bcd_(now.second);
  buf[1] = uint8_to_bcd_(now.minute);
  buf[2] = uint8_to_bcd_(now.hour);
  buf[3] = weekday_to_onehot_(now.day_of_week);
  buf[4] = uint8_to_bcd_(now.day_of_month);
  buf[5] = uint8_to_bcd_(now.month);
  buf[6] = uint8_to_bcd_(static_cast<uint8_t>((now.year - 2000) % 100));

  bool ok = this->write_bytes(RX8111_REG_SEC, buf, sizeof(buf));

  // Always attempt to clear STOP, even if the write failed.
  if (!this->set_stop_bit_(false)) {
    ESP_LOGE(TAG, "Failed to clear STOP bit after time write");
    return;
  }

  if (!ok) {
    ESP_LOGE(TAG, "Can't write time registers");
    return;
  }
  uint8_t flags;
  if (!this->read_byte(RX8111_REG_FLAG, &flags) ||
      !this->write_byte(RX8111_REG_FLAG, flags & ~(RX8111_FLAG_VLF | RX8111_FLAG_XST | RX8111_FLAG_POR))) {
    ESP_LOGW(TAG, "Time written but validity flags could not be cleared");
    return;
  }

  ESP_LOGD(TAG, "Wrote %04d-%02d-%02d %02d:%02d:%02d DOW=%d", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second, now.day_of_week);
}

// ---------------------------------------------------------------------------
// set_stop_bit_() - read-modify-write the STOP bit in the control register.
// Bit7 of the control register is always masked to 0 to prevent writing the
// ---------------------------------------------------------------------------
bool RX8111Component::set_stop_bit_(bool stop) {
  uint8_t ctrl = 0;
  if (!this->read_byte(RX8111_REG_CONTROL, &ctrl)) {
    return false;
  }
  if (stop) {
    ctrl |= RX8111_CTRL_STOP;
  } else {
    ctrl &= ~RX8111_CTRL_STOP;
  }
  return this->write_byte(RX8111_REG_CONTROL, ctrl);
}

// ---------------------------------------------------------------------------
// check_time_validity_
//
// Reads the flag register (0x1E) and warns on:
//   - VLF (bit1): oscillation was lost / power failure; time data invalid.
//   - XST (bit0): crystal oscillation stop detected.
//   - POR (bit7): power-on reset occurred.
// Preserve invalid-time flags until a trusted write and leave interrupt flags latched.
// Returns false only on I2C failure.
// ---------------------------------------------------------------------------
bool RX8111Component::check_time_validity_() {
  uint8_t flags = 0;
  if (!this->read_byte(RX8111_REG_FLAG, &flags)) {
    ESP_LOGE(TAG, "Can't read flag register");
    return false;
  }

  if (flags & RX8111_FLAG_VLF) {
    ESP_LOGW(TAG, "VLF set - oscillation was lost or power failed. "
                  "Time data invalid; write a valid time before use.");
  }
  if (flags & RX8111_FLAG_XST) {
    ESP_LOGW(TAG, "XST set - crystal oscillation stop was detected.");
  }
  if (flags & RX8111_FLAG_POR) {
    ESP_LOGD(TAG, "POR set - power-on reset occurred (expected on first boot).");
  }

  // Retain invalid-time flags across boots until write_time succeeds.

  return true;
}

// ---------------------------------------------------------------------------
// configure_battery_backup_
//
// Power Switch Control register (0x32):
//   bit7 = CHGEN  (trickle charge for rechargeable backup)
//   bit6 = INIEN  (automatic switchover enable)
//   bits5:4 = unused (0)
//   bits3:2 = SWSEL1:SWSEL0 (switchover voltage; preserve existing)
//   bits1:0 = SMPT1:SMPT0   (detection timing; preserve existing)
// ---------------------------------------------------------------------------
bool RX8111Component::configure_battery_backup_() {
  uint8_t reg = 0;
  if (!this->read_byte(RX8111_REG_PWRSWITCH, &reg)) {
    ESP_LOGE(TAG, "Can't read power switch register");
    return false;
  }

  if (this->battery_backup_)
    reg |= RX8111_PWR_INIEN;
  else
    reg &= ~RX8111_PWR_INIEN;

  if (this->battery_backup_ && this->battery_charging_) {
    reg |= RX8111_PWR_CHGEN;
  } else {
    reg &= ~RX8111_PWR_CHGEN;
  }

  if (!this->write_byte(RX8111_REG_PWRSWITCH, reg)) {
    ESP_LOGE(TAG, "Can't write power switch register");
    return false;
  }

  ESP_LOGD(TAG, "Battery backup: %s, charging: %s", ONOFF(this->battery_backup_), ONOFF(this->battery_charging_));
  return true;
}

// ---------------------------------------------------------------------------
// configure_alarm_
//
// Alarm registers:
//   0x17  MIN alarm  (AE bit7; BCD minutes 00-59)
//   0x18  HOUR alarm (AE bit7; BCD hours  00-23)
//   0x19  WEEK/DAY alarm (AE bit7; one-hot weekday OR BCD day, WADA selects)
//   0x2C  SEC alarm  (AE bit7; BCD seconds 00-59) - RX8111CE exclusive
//
// AE=1 means the field is disabled (don't-care).
// WADA bit (0x1D bit3): 0 = week alarm, 1 = day alarm.
//
// NOTE: This method does NOT clear the alarm flag (AF). The flag represents
// a prior alarm event that the user must explicitly clear via
// rx8111.clear_alarm_flag. Reconfiguring the alarm does not alter past events.
// ---------------------------------------------------------------------------
bool RX8111Component::unset_alarm() {
  uint8_t extension;
  if (!this->read_byte(RX8111_REG_EXTENSION, &extension)) {
    ESP_LOGE(TAG, "unset_alarm: cannot read extension register");
    return false;
  }
  // Empty comparison first: do not interpret the previous date as a weekday mask.
  // Keep AE=0; AE=1 would bypass comparison instead of preventing matches.
  if (!this->write_byte(RX8111_REG_WD_ALARM, 0x00) ||
      !this->write_byte(RX8111_REG_EXTENSION, extension & ~RX8111_EXT_WADA)) {
    ESP_LOGE(TAG, "unset_alarm: I2C write failed; alarm state may be partially updated");
    return false;
  }
  ESP_LOGD(TAG, "Alarm unset; AIE and AF unchanged");
  return true;
}

bool RX8111Component::configure_alarm_(bool enable_interrupt) {
  // Keep AIE asserted while updating alarm registers so runtime reprogramming
  // does not deassert /INT when AF is already latched.
  uint8_t ctrl = 0;
  if (!this->read_byte(RX8111_REG_CONTROL, &ctrl))
    return false;

  // --- Second alarm (0x2C) ---
  uint8_t sec_alarm = RX8111_ALARM_AE;  // AE=1 -> disabled
  if (this->alarm_second_ != 0xFF) {
    sec_alarm = uint8_to_bcd_(this->alarm_second_);  // AE=0 -> enabled
  }
  if (!this->write_byte(RX8111_REG_SEC_ALARM, sec_alarm))
    return false;

  // --- Minute alarm (0x17) ---
  uint8_t min_alarm = RX8111_ALARM_AE;
  if (this->alarm_minute_ != 0xFF) {
    min_alarm = uint8_to_bcd_(this->alarm_minute_);
  }
  if (!this->write_byte(RX8111_REG_MIN_ALARM, min_alarm))
    return false;

  // --- Hour alarm (0x18) ---
  uint8_t hour_alarm = RX8111_ALARM_AE;
  if (this->alarm_hour_ != 0xFF) {
    hour_alarm = uint8_to_bcd_(this->alarm_hour_);
  }
  if (!this->write_byte(RX8111_REG_HOUR_ALARM, hour_alarm))
    return false;

  // --- Week / Day alarm (0x19) and WADA bit ---
  uint8_t ext = 0;
  if (!this->read_byte(RX8111_REG_EXTENSION, &ext))
    return false;

  uint8_t wd_alarm = RX8111_ALARM_AE;
  if (this->alarm_weekday_ != 0xFF) {
    // WEEK alarm mode: one-hot weekday, WADA=0
    wd_alarm = weekday_to_onehot_(this->alarm_weekday_);
    ext &= ~RX8111_EXT_WADA;
  } else if (this->alarm_day_ != 0xFF) {
    // DAY alarm mode: BCD day, WADA=1
    wd_alarm = uint8_to_bcd_(this->alarm_day_);
    ext |= RX8111_EXT_WADA;
  }
  if (!this->write_byte(RX8111_REG_WD_ALARM, wd_alarm))
    return false;
  if (!this->write_byte(RX8111_REG_EXTENSION, ext))
    return false;

  // Only startup configuration enables AIE. Runtime programming preserves it.
  if (enable_interrupt) {
    ctrl |= RX8111_CTRL_AIE;
    if (!this->write_byte(RX8111_REG_CONTROL, ctrl))
      return false;
  }

  return true;
}

// ---------------------------------------------------------------------------
// configure_timer_
//
// Timer counter is 24-bit: 0x1A (LSB), 0x1B, 0x1C (MSB).
// Source clock: TSEL1:TSEL0 in extension register 0x1D bits 1:0.
// TE (0x1D bit4) starts the timer. TIE (0x1F bit4) enables interrupt.
//
// Per datasheet: clear TE first, write counter, then set TE.
//
// NOTE: This method does NOT clear the timer flag (TF). Reconfiguring the
// timer (e.g. changing the count) leaves past events intact for the user to
// acknowledge via rx8111.clear_timer_flag.
// ---------------------------------------------------------------------------
bool RX8111Component::configure_timer_() {
  // Step 1: stop timer and disable interrupt while reconfiguring.
  uint8_t ext = 0;
  if (!this->read_byte(RX8111_REG_EXTENSION, &ext))
    return false;
  ext &= ~RX8111_EXT_TE;
  if (!this->write_byte(RX8111_REG_EXTENSION, ext))
    return false;

  uint8_t ctrl = 0;
  if (!this->read_byte(RX8111_REG_CONTROL, &ctrl))
    return false;
  ctrl &= ~RX8111_CTRL_TIE;
  if (!this->write_byte(RX8111_REG_CONTROL, ctrl))
    return false;

  // Step 2: write 24-bit counter (LSB first).
  uint32_t count = this->timer_count_;
  uint8_t timer_buf[3] = {
      static_cast<uint8_t>(count & 0xFF),
      static_cast<uint8_t>((count >> 8) & 0xFF),
      static_cast<uint8_t>((count >> 16) & 0xFF),
  };
  if (!this->write_bytes(RX8111_REG_TIMER0, timer_buf, sizeof(timer_buf)))
    return false;

  // Step 3: set source clock.
  const uint8_t tsel = static_cast<uint8_t>(this->timer_clock_);
  ext &= ~RX8111_EXT_TSEL_MASK;
  ext |= (tsel & RX8111_EXT_TSEL_MASK);
  if (!this->write_byte(RX8111_REG_EXTENSION, ext))
    return false;

  // Step 4: enable interrupt and start timer.
  ctrl |= RX8111_CTRL_TIE;
  if (!this->write_byte(RX8111_REG_CONTROL, ctrl))
    return false;

  ext |= RX8111_EXT_TE;
  if (!this->write_byte(RX8111_REG_EXTENSION, ext))
    return false;

  return true;
}

// ---------------------------------------------------------------------------
// disable_alarm_
//
// Disable the alarm interrupt without touching AF. Matching can still set AF.
// ---------------------------------------------------------------------------
bool RX8111Component::disable_alarm_() {
  uint8_t ctrl = 0;
  if (!this->read_byte(RX8111_REG_CONTROL, &ctrl))
    return false;
  ctrl &= ~RX8111_CTRL_AIE;
  if (!this->write_byte(RX8111_REG_CONTROL, ctrl))
    return false;

  if (!this->write_byte(RX8111_REG_SEC_ALARM, RX8111_ALARM_AE))
    return false;
  if (!this->write_byte(RX8111_REG_MIN_ALARM, RX8111_ALARM_AE))
    return false;
  if (!this->write_byte(RX8111_REG_HOUR_ALARM, RX8111_ALARM_AE))
    return false;
  if (!this->write_byte(RX8111_REG_WD_ALARM, RX8111_ALARM_AE))
    return false;
  return true;
}

// ---------------------------------------------------------------------------
// disable_timer_
//
// Disables timer interrupt (TIE=0) and timer engine (TE=0) without touching TF.
// ---------------------------------------------------------------------------
bool RX8111Component::disable_timer_() {
  uint8_t ctrl = 0;
  if (!this->read_byte(RX8111_REG_CONTROL, &ctrl))
    return false;
  ctrl &= ~RX8111_CTRL_TIE;
  if (!this->write_byte(RX8111_REG_CONTROL, ctrl))
    return false;

  uint8_t ext = 0;
  if (!this->read_byte(RX8111_REG_EXTENSION, &ext))
    return false;
  ext &= ~RX8111_EXT_TE;
  if (!this->write_byte(RX8111_REG_EXTENSION, ext))
    return false;
  return true;
}

// ---------------------------------------------------------------------------
// configure_event_
//
// Enables EIE so that a latched EVF pulls /INT low. EVIN level/filter/pull and
// timestamp mode bits are applied centrally from apply_runtime_options_().
// ---------------------------------------------------------------------------
bool RX8111Component::configure_event_() {
  uint8_t ctrl = 0;
  if (!this->read_byte(RX8111_REG_CONTROL, &ctrl))
    return false;
  ctrl |= RX8111_CTRL_EIE;
  if (!this->write_byte(RX8111_REG_CONTROL, ctrl))
    return false;
  return true;
}

// ---------------------------------------------------------------------------
// disable_event_
//
// Disables EIE without touching EVF. ETS and EVIN routing are handled by the
// runtime timestamp configuration so timestamp capture can stay active without
// asserting /INT.
// ---------------------------------------------------------------------------
bool RX8111Component::disable_event_() {
  uint8_t ctrl = 0;
  if (!this->read_byte(RX8111_REG_CONTROL, &ctrl))
    return false;
  ctrl &= ~RX8111_CTRL_EIE;
  if (!this->write_byte(RX8111_REG_CONTROL, ctrl))
    return false;
  return true;
}

// ---------------------------------------------------------------------------
// apply_runtime_options_
//
// Applies RX8111-specific timestamp routing and EVIN settings. ETS must remain
// set if either event detection or timestamp capture is active.
// ---------------------------------------------------------------------------
bool RX8111Component::apply_runtime_options_() {
  uint8_t ext = 0;
  if (!this->read_byte(RX8111_REG_EXTENSION, &ext))
    return false;

  if (this->timestamp_capture_active_() || this->event_enabled_) {
    ext |= RX8111_EXT_ETS;
  } else {
    ext &= ~RX8111_EXT_ETS;
  }

  if (!this->write_byte(RX8111_REG_EXTENSION, ext))
    return false;
  return this->write_timestamp_mode_bits_();
}

bool RX8111Component::timestamp_capture_active_() const { return this->timestamp_enabled_; }

bool RX8111Component::buffered_timestamp_mode_() const {
  return this->timestamp_record_mode_ != TIMESTAMP_RECORD_LATEST;
}

bool RX8111Component::write_timestamp_mode_bits_() {
  uint8_t tsctrl1 = 0;
  if (!this->read_byte(RX8111_REG_TSCTRL1, &tsctrl1))
    return false;

  tsctrl1 &= ~(RX8111_TSCTRL1_EISEL | RX8111_TSCTRL1_TSCLR | RX8111_TSCTRL1_TSRAM);
  if (this->timestamp_capture_active_() && this->buffered_timestamp_mode_()) {
    tsctrl1 |= RX8111_TSCTRL1_TSRAM;
  }

  if (!this->write_byte(RX8111_REG_TSCTRL1, tsctrl1))
    return false;

  uint8_t evin_reg = 0;
  if (this->event_enabled_ || this->timestamp_capture_active_()) {
    if (this->event_level_ == EVENT_LEVEL_HIGH) {
      evin_reg |= RX8111_EVIN_EHL;
    }

    evin_reg |= (this->evin_filter_ << RX8111_EVIN_ET_SHIFT) & RX8111_EVIN_ET_MASK;

    switch (this->evin_pull_) {
      case EVIN_PULL_NONE:
        break;
      case EVIN_PULL_UP_500K:
        evin_reg |= RX8111_EVIN_PU0;
        break;
      case EVIN_PULL_UP_1M:
        evin_reg |= RX8111_EVIN_PU1;
        break;
      case EVIN_PULL_UP_10M:
        evin_reg |= RX8111_EVIN_PU1 | RX8111_EVIN_PU0;
        break;
      case EVIN_PULL_DOWN_500K:
        evin_reg |= RX8111_EVIN_PDN;
        break;
    }

    if (this->timestamp_capture_active_() && this->timestamp_record_mode_ == TIMESTAMP_RECORD_OVERWRITE) {
      evin_reg |= RX8111_EVIN_OVW;
    }
  }

  return this->write_byte(RX8111_REG_EVIN_SETTING, evin_reg);
}

bool RX8111Component::clear_timestamp_ram_() {
  uint8_t ext = 0;
  if (!this->read_byte(RX8111_REG_EXTENSION, &ext))
    return false;

  const bool restore_ets = (ext & RX8111_EXT_ETS) != 0;
  if (restore_ets) {
    ext &= ~RX8111_EXT_ETS;
    if (!this->write_byte(RX8111_REG_EXTENSION, ext))
      return false;
  }

  uint8_t tsctrl1 = 0;
  if (!this->read_byte(RX8111_REG_TSCTRL1, &tsctrl1))
    return false;
  tsctrl1 |= RX8111_TSCTRL1_TSCLR;
  if (!this->write_byte(RX8111_REG_TSCTRL1, tsctrl1))
    return false;

  if (restore_ets) {
    ext |= RX8111_EXT_ETS;
    if (!this->write_byte(RX8111_REG_EXTENSION, ext))
      return false;
  }

  return true;
}

}  // namespace rx8111
}  // namespace esphome
