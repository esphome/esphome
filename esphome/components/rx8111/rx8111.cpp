#include "rx8111.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::rx8111 {

ESPHOME_LOG_TAG(TAG, "rx8111");

static constexpr uint8_t RX8111_REG_SEC = 0x10;
static constexpr uint8_t RX8111_REG_MIN_ALARM = 0x17;
static constexpr uint8_t RX8111_REG_HOUR_ALARM = 0x18;
static constexpr uint8_t RX8111_REG_DAY_ALARM = 0x19;
static constexpr uint8_t RX8111_REG_EXTENSION = 0x1D;
static constexpr uint8_t RX8111_REG_FLAG = 0x1E;
static constexpr uint8_t RX8111_REG_TIMER0 = 0x1A;
static constexpr uint8_t RX8111_REG_CONTROL = 0x1F;
static constexpr uint8_t RX8111_REG_SEC_ALARM = 0x2C;
static constexpr uint8_t RX8111_ALARM_DISABLED = 0x80;
static constexpr uint8_t RX8111_EXTENSION_WADA = 0x08;
static constexpr uint8_t RX8111_CONTROL_AIE = 0x08;
static constexpr uint8_t RX8111_CONTROL_TIE = 0x10;
static constexpr uint8_t RX8111_EXTENSION_TE = 0x10;
static constexpr uint8_t RX8111_EXTENSION_TSEL = 0x03;
static constexpr uint8_t RX8111_FLAG_TF = 0x10;
static constexpr uint8_t RX8111_FLAG_AF = 0x08;
static constexpr uint8_t RX8111_FLAG_POR = 0x80;
static constexpr uint8_t RX8111_FLAG_VLF = 0x02;
static constexpr uint8_t RX8111_FLAG_XST = 0x01;
static constexpr uint8_t RX8111_CTRL_STOP = 0x01;
// FLAG bits are write-zero-to-clear; keep UF/TF/AF/EVF even if they latch during the write.
static constexpr uint8_t RX8111_KEEP_EVENT_FLAGS = 0x3C;
static constexpr uint32_t RX8111_EVENT_INTERVAL_ID = 0;
static constexpr uint32_t RX8111_BOOT_ALARM_RETRY_ID = 1;
static constexpr uint32_t RX8111_BOOT_TIMER_RETRY_ID = 2;
static constexpr uint32_t RX8111_BOOT_RETRY_DELAY_MS = 1000;
static constexpr uint8_t RX8111_BOOT_MAX_ATTEMPTS = 3;

void RX8111Component::setup() {
  this->read_time();
  this->configure_boot_alarm_();
  this->configure_boot_timer_();
  if (this->alarm_enabled_ || this->timer_enabled_ || this->timer_retained_event_pending_) {
    this->start_event_polling_();
  }
}

void RX8111Component::configure_boot_alarm_() {
  bool success = true;
  if (this->alarm_enabled_) {
    if (this->alarm_second_ != 0xFF || this->alarm_minute_ != 0xFF || this->alarm_hour_ != 0xFF ||
        this->alarm_weekday_ != 0xFF || this->alarm_day_ != 0xFF) {
      this->boot_alarm_in_progress_ = true;
      success = this->program_alarm_(this->alarm_second_, this->alarm_minute_, this->alarm_hour_, this->alarm_weekday_,
                                     this->alarm_day_);
      this->boot_alarm_in_progress_ = false;
      if (success) {
        // Only the first successful boot program can introduce a boot wake-up edge.
        if (!this->boot_alarm_event_seen_)
          this->alarm_flag_seen_ = false;
        this->start_event_polling_();
      }
    } else {
      // Preserve an alarm programmed by an action before a reboot or deep sleep.
      uint8_t control;
      success = this->read_byte(RX8111_REG_CONTROL, &control);
      if (success) {
        this->alarm_configured_ = (control & RX8111_CONTROL_AIE) != 0;
        if (this->alarm_configured_)
          this->start_event_polling_();
      }
    }
  } else if (this->alarm_explicitly_configured_) {
    success = this->set_alarm_interrupt_(false);
  }
  this->boot_config_result_(true, success);
}

void RX8111Component::configure_boot_timer_() {
  bool success = true;
  // An omitted timer setting leaves battery-backed state alone; explicit false stops it.
  if (this->timer_configured_) {
    const bool start_timer = this->timer_enabled_;
    this->boot_timer_in_progress_ = true;
    success = start_timer ? this->configure_timer_() : this->stop_timer();
    this->boot_timer_in_progress_ = false;
    if (success && !start_timer && this->timer_count_ != 0) {
      this->timer_retained_event_pending_ = true;
      this->start_event_polling_();
    }
  }
  this->boot_config_result_(false, success);
}

void RX8111Component::boot_config_result_(bool alarm, bool success) {
  auto &attempts = alarm ? this->boot_alarm_attempts_ : this->boot_timer_attempts_;
  auto &warning = alarm ? this->boot_alarm_warning_ : this->boot_timer_warning_;
  ++attempts;
  if (success) {
    const bool had_warning = warning;
    warning = false;
    if (had_warning && !this->calendar_warning_ && !this->event_poll_warning_ && !this->boot_alarm_warning_ &&
        !this->boot_timer_warning_)
      this->status_clear_warning();
    return;
  }
  warning = true;
  this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  if (attempts >= RX8111_BOOT_MAX_ATTEMPTS)
    return;
  this->set_timeout(alarm ? RX8111_BOOT_ALARM_RETRY_ID : RX8111_BOOT_TIMER_RETRY_ID, RX8111_BOOT_RETRY_DELAY_MS,
                    [this, alarm]() {
                      if (alarm) {
                        this->configure_boot_alarm_();
                      } else {
                        this->configure_boot_timer_();
                      }
                    });
}

void RX8111Component::cancel_boot_retry_(bool alarm, bool success) {
  this->cancel_timeout(alarm ? RX8111_BOOT_ALARM_RETRY_ID : RX8111_BOOT_TIMER_RETRY_ID);
  if (alarm) {
    this->boot_alarm_attempts_ = RX8111_BOOT_MAX_ATTEMPTS;
  } else {
    this->boot_timer_attempts_ = RX8111_BOOT_MAX_ATTEMPTS;
  }
  if (success) {
    auto &warning = alarm ? this->boot_alarm_warning_ : this->boot_timer_warning_;
    const bool had_warning = warning;
    warning = false;
    if (had_warning && !this->calendar_warning_ && !this->event_poll_warning_ && !this->boot_alarm_warning_ &&
        !this->boot_timer_warning_)
      this->status_clear_warning();
  }
}

void RX8111Component::check_event_flags_() {
  if (!this->alarm_configured_ && !this->timer_enabled_ && !this->timer_retained_event_pending_)
    return;
  if (!this->poll_event_flags_()) {
    this->event_poll_warning_ = true;
    if (!this->calendar_warning_ && !this->status_has_warning()) {
      this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    }
  }
}

void RX8111Component::start_event_polling_() {
  if (this->event_polling_started_)
    return;
  this->set_interval(RX8111_EVENT_INTERVAL_ID, this->event_poll_interval_, [this]() { this->check_event_flags_(); });
  this->event_polling_started_ = true;
}

void RX8111Component::update() {
  this->read_time();
  this->check_event_flags_();
}

void RX8111Component::set_calendar_warning_(const LogString *message) {
  this->calendar_warning_ = true;
  this->status_set_warning(message);
}

void RX8111Component::dump_config() {
  ESP_LOGCONFIG(TAG, "RX8111:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG,
                "  Alarm interrupt: %s\n"
                "  Wake-up timer: %s",
                ONOFF(this->alarm_enabled_), ONOFF(this->timer_enabled_));
  if (this->timer_enabled_) {
    ESP_LOGCONFIG(TAG, "    Count: %" PRIu32 ", clock: %u", this->timer_count_, this->timer_clock_);
  }
  RealTimeClock::dump_config();
}

void RX8111Component::read_time() {
  ESPTime rtc_time;
  if (!this->read_calendar_(rtc_time))
    return;
  ESP_LOGD(TAG, "Read UTC time: %04d-%02d-%02d %02d:%02d:%02d", rtc_time.year, rtc_time.month, rtc_time.day_of_month,
           rtc_time.hour, rtc_time.minute, rtc_time.second);
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

bool RX8111Component::read_calendar_(ESPTime &rtc_time) {
  uint8_t flags;
  if (!this->read_byte(RX8111_REG_FLAG, &flags)) {
    this->set_calendar_warning_(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  if (flags & (RX8111_FLAG_VLF | RX8111_FLAG_XST | RX8111_FLAG_POR)) {
    ESP_LOGW(TAG, "RTC time invalidated by power loss or oscillator stop; write trusted time");
    this->set_calendar_warning_(LOG_STR("RTC time invalidated; write trusted time"));
    return false;
  }

  uint8_t buf[7];
  // I2C access freezes the RX8111 counter, yielding a coherent calendar snapshot.
  if (!this->read_bytes(RX8111_REG_SEC, buf, sizeof(buf))) {
    this->set_calendar_warning_(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  static constexpr uint8_t CALENDAR_MASKS[7] = {0x7F, 0x7F, 0x3F, 0x00, 0x3F, 0x1F, 0xFF};
  for (size_t i = 0; i < sizeof(buf); i++) {
    if (i == 3)
      continue;  // Weekday uses one-hot encoding, not BCD.
    buf[i] &= CALENDAR_MASKS[i];
    if ((buf[i] & 0x0F) > 9 || (buf[i] >> 4) > 9) {
      ESP_LOGW(TAG, "Invalid BCD in RTC time");
      this->set_calendar_warning_(LOG_STR("Invalid RTC time"));
      return false;
    }
  }
  const uint8_t onehot = buf[3];
  if (onehot == 0 || onehot > 0x40 || (onehot & (onehot - 1)) != 0) {
    ESP_LOGW(TAG, "Invalid RTC weekday");
    this->set_calendar_warning_(LOG_STR("Invalid RTC time"));
    return false;
  }
  rtc_time = time::decode_bcd_calendar(buf);
  if (!rtc_time.is_valid(true, false) || rtc_time.second >= 60) {
    ESP_LOGW(TAG, "Invalid RTC time, not syncing to system clock");
    this->set_calendar_warning_(LOG_STR("Invalid RTC time"));
    return false;
  }
  if (ESPTime::from_epoch_utc(rtc_time.timestamp).day_of_week != rtc_time.day_of_week) {
    ESP_LOGW(TAG, "RTC weekday does not match calendar date");
    this->set_calendar_warning_(LOG_STR("Invalid RTC time"));
    return false;
  }
  this->calendar_warning_ = false;
  if (!this->event_poll_warning_ && !this->boot_alarm_warning_ && !this->boot_timer_warning_)
    this->status_clear_warning();
  return true;
}

void RX8111Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid() || now.year > 2099) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC");
    return;
  }
  if (!this->set_stop_bit_(true)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  uint8_t buf[7];
  time::encode_bcd_calendar(now, buf);
  const bool written = this->write_bytes(RX8111_REG_SEC, buf, sizeof(buf));
  // Always try to restart the counter, even after a failed calendar write.
  const bool restarted = this->set_stop_bit_(false);
  if (!written || !restarted) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (!this->write_byte(RX8111_REG_FLAG, RX8111_KEEP_EVENT_FLAGS)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->calendar_warning_ = false;
  if (this->event_poll_warning_ || this->boot_alarm_warning_ || this->boot_timer_warning_) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  } else {
    this->status_clear_warning();
  }
  ESP_LOGD(TAG, "Wrote UTC time: %04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second);
}

bool RX8111Component::set_stop_bit_(bool stop) {
  uint8_t control;
  if (!this->read_byte(RX8111_REG_CONTROL, &control))
    return false;
  if (stop) {
    control |= RX8111_CTRL_STOP;
  } else {
    control &= ~RX8111_CTRL_STOP;
  }
  return this->write_byte(RX8111_REG_CONTROL, control);
}

bool RX8111Component::set_alarm_interrupt_(bool enabled) {
  uint8_t control;
  if (!this->read_byte(RX8111_REG_CONTROL, &control))
    return false;
  control = enabled ? (control | RX8111_CONTROL_AIE) : (control & ~RX8111_CONTROL_AIE);
  return this->write_byte(RX8111_REG_CONTROL, control);
}

bool RX8111Component::program_alarm_(uint8_t second, uint8_t minute, uint8_t hour, uint8_t weekday, uint8_t day) {
  uint8_t extension;
  if (!this->read_byte(RX8111_REG_EXTENSION, &extension))
    return false;
  if (weekday != 0xFF) {
    extension &= ~RX8111_EXTENSION_WADA;
  } else if (day != 0xFF) {
    extension |= RX8111_EXTENSION_WADA;
  }

  const uint8_t fields[] = {
      second == 0xFF ? RX8111_ALARM_DISABLED : uint8_to_bcd(second),
      minute == 0xFF ? RX8111_ALARM_DISABLED : uint8_to_bcd(minute),
      hour == 0xFF ? RX8111_ALARM_DISABLED : uint8_to_bcd(hour),
      weekday != 0xFF ? static_cast<uint8_t>(1U << (weekday - 1))
      : day != 0xFF   ? uint8_to_bcd(day)
                      : RX8111_ALARM_DISABLED,
  };
  // Disable the interrupt during partial programming; keep AF (past events) latched.
  if (!this->set_alarm_interrupt_(false))
    return false;
  this->alarm_configured_ = false;
  uint8_t flags;
  if (!this->read_byte(RX8111_REG_FLAG, &flags))
    return false;
  if (!this->boot_alarm_in_progress_)
    this->alarm_flag_seen_ = (flags & RX8111_FLAG_AF) != 0;
  if (!this->write_byte(RX8111_REG_SEC_ALARM, fields[0]) || !this->write_bytes(RX8111_REG_MIN_ALARM, &fields[1], 3) ||
      !this->write_byte(RX8111_REG_EXTENSION, extension))
    return false;
  if (!this->set_alarm_interrupt_(this->alarm_enabled_))
    return false;
  this->alarm_configured_ = true;
  return true;
}

bool RX8111Component::set_alarm(uint16_t second, uint16_t minute, uint16_t hour, uint16_t weekday, uint16_t day) {
  if (!this->alarm_enabled_) {
    ESP_LOGE(TAG, "Alarm interrupt disabled; set alarm_enabled: true");
    return false;
  }
  if ((second != 0xFF && second > 59) || (minute != 0xFF && minute > 59) || (hour != 0xFF && hour > 23) ||
      (weekday != 0xFF && (weekday < 1 || weekday > 7)) || (day != 0xFF && (day < 1 || day > 31)) ||
      (weekday != 0xFF && day != 0xFF) ||
      (second == 0xFF && minute == 0xFF && hour == 0xFF && weekday == 0xFF && day == 0xFF)) {
    ESP_LOGE(TAG, "Invalid alarm fields");
    return false;
  }
  // A newer runtime request supersedes pending boot programming.
  this->cancel_boot_retry_(true, false);
  if (!this->program_alarm_(second, minute, hour, weekday, day)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->boot_alarm_event_seen_ = true;
  return true;
}

bool RX8111Component::schedule_alarm_in(uint32_t seconds) {
  if (!this->alarm_enabled_ || seconds == 0 || seconds >= 28U * 86400U) {
    ESP_LOGE(TAG, "Relative alarm requires alarm_enabled and a duration of 1 second to less than 28 days");
    return false;
  }
  ESPTime now;
  if (!this->read_calendar_(now)) {
    ESP_LOGE(TAG, "Cannot read valid RTC time for relative alarm");
    return false;
  }
  auto target = ESPTime::from_epoch_utc(now.timestamp + seconds);
  if (!target.is_valid() || target.year > 2099) {
    ESP_LOGE(TAG, "Relative alarm target is outside RTC year range");
    return false;
  }
  return this->set_alarm(target.second, target.minute, target.hour, 0xFF, target.day_of_month);
}

bool RX8111Component::unset_alarm() {
  this->cancel_boot_retry_(true, false);
  if (!this->set_alarm_interrupt_(false)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->alarm_configured_ = false;
  if (!this->write_byte(RX8111_REG_SEC_ALARM, RX8111_ALARM_DISABLED) ||
      !this->write_byte(RX8111_REG_MIN_ALARM, RX8111_ALARM_DISABLED) ||
      !this->write_byte(RX8111_REG_HOUR_ALARM, RX8111_ALARM_DISABLED) ||
      !this->write_byte(RX8111_REG_DAY_ALARM, RX8111_ALARM_DISABLED)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->cancel_boot_retry_(true, true);
  return true;
}

bool RX8111Component::clear_alarm_flag() {
  // Write-zero-to-clear: ones protect every other flag, even one latched during this write.
  if (!this->write_byte(RX8111_REG_FLAG, static_cast<uint8_t>(~RX8111_FLAG_AF))) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->alarm_flag_seen_ = false;
  return true;
}

bool RX8111Component::configure_timer_(bool clear_stale_flag) {
  // Stop counting before updating the 24-bit reload value; preserve the boot flag unless explicitly starting.
  uint8_t extension;
  uint8_t control;
  if (!this->read_byte(RX8111_REG_EXTENSION, &extension) ||
      !this->write_byte(RX8111_REG_EXTENSION, extension & ~RX8111_EXTENSION_TE) ||
      !this->read_byte(RX8111_REG_CONTROL, &control) ||
      !this->write_byte(RX8111_REG_CONTROL, control & ~RX8111_CONTROL_TIE))
    return false;

  const uint8_t count[] = {static_cast<uint8_t>(this->timer_count_), static_cast<uint8_t>(this->timer_count_ >> 8),
                           static_cast<uint8_t>(this->timer_count_ >> 16)};
  if (!this->write_bytes(RX8111_REG_TIMER0, count, sizeof(count)) ||
      !this->write_byte(RX8111_REG_EXTENSION,
                        (extension & ~(RX8111_EXTENSION_TE | RX8111_EXTENSION_TSEL)) | this->timer_clock_) ||
      (clear_stale_flag && !this->clear_timer_flag()) ||
      !this->write_byte(RX8111_REG_CONTROL, control | RX8111_CONTROL_TIE))
    return false;
  return this->write_byte(RX8111_REG_EXTENSION,
                          (extension & ~RX8111_EXTENSION_TSEL) | this->timer_clock_ | RX8111_EXTENSION_TE);
}

bool RX8111Component::start_timer() {
  if (this->timer_count_ == 0 || this->timer_count_ > 0xFFFFFF) {
    ESP_LOGW(TAG, "Cannot start timer without a valid timer_count");
    return false;
  }
  this->cancel_boot_retry_(false, false);
  if (!this->configure_timer_(true)) {
    // A partial configuration can leave either enable bit set. Disable both if the bus permits it.
    this->stop_timer();
    this->timer_enabled_ = false;
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->cancel_boot_retry_(false, true);
  this->timer_enabled_ = true;
  this->timer_retained_event_pending_ = false;
  this->start_event_polling_();
  return true;
}

bool RX8111Component::clear_timer_flag() {
  // Write-zero-to-clear without clearing other flags latched concurrently.
  if (!this->write_byte(RX8111_REG_FLAG, static_cast<uint8_t>(~RX8111_FLAG_TF))) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->timer_flag_seen_ = false;
  this->timer_retained_event_pending_ = false;
  return true;
}

bool RX8111Component::stop_timer() {
  if (!this->boot_timer_in_progress_)
    this->cancel_boot_retry_(false, false);
  uint8_t extension;
  uint8_t control;
  if (!this->read_byte(RX8111_REG_EXTENSION, &extension) ||
      !this->write_byte(RX8111_REG_EXTENSION, extension & ~RX8111_EXTENSION_TE) ||
      !this->read_byte(RX8111_REG_CONTROL, &control) ||
      !this->write_byte(RX8111_REG_CONTROL, control & ~RX8111_CONTROL_TIE)) {
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->timer_enabled_ = false;
  if (!this->boot_timer_in_progress_)
    this->cancel_boot_retry_(false, true);
  return true;
}

bool RX8111Component::poll_event_flags_() {
  uint8_t flags;
  if (!this->read_byte(RX8111_REG_FLAG, &flags))
    return false;
  if (this->event_poll_warning_) {
    this->event_poll_warning_ = false;
    if (!this->calendar_warning_ && !this->boot_alarm_warning_ && !this->boot_timer_warning_)
      this->status_clear_warning();
  }
  const bool alarm_active = (flags & RX8111_FLAG_AF) != 0;
  const bool timer_active = (flags & RX8111_FLAG_TF) != 0;
  if (alarm_active && this->alarm_enabled_ && this->alarm_configured_)
    this->boot_alarm_event_seen_ = true;
  const bool alarm_rising = alarm_active && !this->alarm_flag_seen_ && this->alarm_enabled_ && this->alarm_configured_;
  const bool timer_rising =
      timer_active && !this->timer_flag_seen_ && (this->timer_enabled_ || this->timer_retained_event_pending_);
  const bool deliver_stopped_event = timer_rising && this->timer_retained_event_pending_;
  // Callbacks may clear flags synchronously: record both edges first.
  this->alarm_flag_seen_ = alarm_active;
  this->timer_flag_seen_ = timer_active;
  this->timer_retained_event_pending_ = false;
  if (alarm_rising)
    this->alarm_callbacks_.call();
  // An alarm callback may stop the timer before its callback is delivered.
  if (timer_rising && this->timer_flag_seen_ && (this->timer_enabled_ || deliver_stopped_event))
    this->timer_callbacks_.call();
  if (!this->alarm_configured_ && !this->timer_enabled_ && !this->timer_retained_event_pending_ &&
      this->event_polling_started_) {
    this->cancel_interval(RX8111_EVENT_INTERVAL_ID);
    this->event_polling_started_ = false;
  }
  return true;
}

}  // namespace esphome::rx8111
