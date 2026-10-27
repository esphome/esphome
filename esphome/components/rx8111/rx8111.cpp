#include "rx8111.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#ifdef USE_RX8111_TIMESTAMP
#include <cstdio>
#include <cstring>
#endif

namespace esphome::rx8111 {

ESPHOME_LOG_TAG(TAG, "rx8111");

// Indexed by the TSEL value stored in timer_clock_
PROGMEM_STRING_TABLE(TimerClockNames, "4096 Hz", "64 Hz", "1 Hz", "1/60 Hz");
PROGMEM_STRING_TABLE(FoutNames, "32768 Hz", "1024 Hz", "1 Hz", "off");
PROGMEM_STRING_TABLE(EvinPullNames, "none", "pullup_500k", "pullup_1m", "pullup_10m", "pulldown_500k");
#ifdef USE_BINARY_SENSOR
PROGMEM_STRING_TABLE(StatusSensorNames, "Voltage low flag", "Alarm flag", "Timer flag", "Oscillation stop flag",
                     "Event flag", "Battery low", "EVIN");
static constexpr uint8_t STATUS_SENSOR_MASKS[] PROGMEM = {0x02, 0x08, 0x10, 0x01, 0x04, 0x02, 0x40};
#endif
#ifdef USE_RX8111_TIMESTAMP
PROGMEM_STRING_TABLE(TimestampModeNames, "latest", "stop_when_full", "overwrite");
#endif
PROGMEM_STRING_TABLE(BootFailureNames, "RX8111 battery configuration failed", "RX8111 FOUT configuration failed",
                     "RX8111 EVIN configuration failed", ESP_LOG_MSG_COMM_FAIL, ESP_LOG_MSG_COMM_FAIL);

static constexpr uint8_t RX8111_REG_SEC = 0x10;
static constexpr uint8_t RX8111_REG_MIN_ALARM = 0x17;
static constexpr uint8_t RX8111_REG_EXTENSION = 0x1D;
static constexpr uint8_t RX8111_REG_FLAG = 0x1E;
static constexpr uint8_t RX8111_REG_TIMER0 = 0x1A;
static constexpr uint8_t RX8111_REG_CONTROL = 0x1F;
static constexpr uint8_t RX8111_REG_SEC_ALARM = 0x2C;
static constexpr uint8_t RX8111_REG_TIMER_CONTROL = 0x2D;
static constexpr uint8_t RX8111_REG_POWER_SWITCH = 0x32;
static constexpr uint8_t RX8111_REG_TIMESTAMP_CONTROL = 0x34;
#ifdef USE_RX8111_TIMESTAMP
static constexpr uint8_t RX8111_REG_TIMESTAMP_STATUS = 0x36;
static constexpr uint8_t RX8111_REG_TIMESTAMP_LATEST = 0x20;
static constexpr uint8_t RX8111_REG_TIMESTAMP_BUFFER = 0x40;
static constexpr uint8_t RX8111_TIMESTAMP_RAM = 0x01;
static constexpr uint8_t RX8111_TIMESTAMP_CLEAR = 0x02;
static constexpr uint8_t RX8111_TIMESTAMP_EMPTY = 0x08;
static constexpr uint8_t RX8111_TIMESTAMP_FULL = 0x10;
static constexpr uint8_t RX8111_EVIN_OVERWRITE = 0x02;  // Full buffer overwrites its oldest record
#endif
static constexpr uint8_t RX8111_TIMESTAMP_EISEL = 0x04;
static constexpr uint8_t RX8111_REG_EVIN = 0x2B;
static constexpr uint8_t RX8111_EXTENSION_ETS = 0x04;
static constexpr uint8_t RX8111_CONTROL_EIE = 0x04;
static constexpr uint8_t RX8111_FLAG_EVF = 0x04;
static constexpr uint8_t RX8111_POWER_CHGEN = 0x80;
static constexpr uint8_t RX8111_POWER_INIEN = 0x40;
static constexpr uint8_t RX8111_POWER_SWSEL = 0x0C;
static constexpr uint8_t RX8111_POWER_VDD_ONLY = 0x08;
static constexpr uint8_t RX8111_FOUT_FSEL = 0xC0;
static constexpr uint8_t RX8111_TIMER_TMPIN = 0x02;
static constexpr uint8_t RX8111_TIMER_MODE = 0x0F;  // TBKON, TBKE, TMPIN, TSTP
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
static constexpr uint32_t RX8111_BOOT_BATTERY_RETRY_ID = 3;
static constexpr uint32_t RX8111_BOOT_FOUT_RETRY_ID = 4;
static constexpr uint32_t RX8111_BOOT_EVENT_RETRY_ID = 5;
static constexpr uint32_t RX8111_DEFERRED_UPDATE_ID = 6;
static constexpr uint8_t RX8111_BOOT_MAX_ATTEMPTS = 3;

// Masks a 7-byte Epson calendar in place and checks its BCD digits; the one-hot weekday byte is skipped.
static bool mask_bcd_calendar(uint8_t *calendar) {
  static constexpr uint8_t MASKS[7] PROGMEM = {0x7F, 0x7F, 0x3F, 0x00, 0x3F, 0x1F, 0xFF};
  bool valid = true;
  for (size_t i = 0; i < sizeof(MASKS); i++) {
    if (i == 3)
      continue;
    calendar[i] &= progmem_read_byte(&MASKS[i]);
    valid = valid && (calendar[i] & 0x0F) <= 9 && (calendar[i] >> 4) <= 9;
  }
  return valid;
}

void RX8111Component::setup() {
  this->update_running_ = true;
  if (this->battery_configured_)
    this->configure_boot_battery_();
  if (this->fout_configured_)
    this->configure_boot_fout_();
#ifdef USE_RX8111_TIMESTAMP
  // Retained records are delivered after the alarm and timer are set up, so callbacks that change them win
  this->defer_timestamps_ = true;
#endif
  if (this->event_configured_)
    this->configure_boot_event_();
  this->configure_boot_alarm_();
  this->configure_boot_timer_();
#ifdef USE_RX8111_TIMESTAMP
  this->defer_timestamps_ = false;
  // A failed attempt keeps its staged records for the retries, which deliver them
  if (!this->event_boot_pending_)
    this->publish_staged_timestamps_();
#endif
  int16_t flags = -1;
  this->read_time_(flags);
#ifdef USE_BINARY_SENSOR
  // Publish initial status even when periodic calendar synchronization is disabled.
  for (auto *sensor : this->status_sensors_) {
    if (sensor != nullptr) {
      this->check_event_flags_(flags);
      break;
    }
  }
#endif
  if (this->watching_()) {
    this->start_event_polling_();
  }
  this->update_running_ = false;
}

void RX8111Component::configure_boot_event_() {
  bool success;
#ifdef USE_RX8111_TIMESTAMP
  if (this->timestamp_sensor_ != nullptr) {
    // Preserve startup validity and event evidence before any subsequent bus operation can fail.
    if (!this->retained_flags_seen_ && this->read_byte(RX8111_REG_FLAG, &this->retained_flags_))
      this->retained_flags_seen_ = true;
    uint8_t control;
    success = this->retained_flags_seen_ && this->read_byte(RX8111_REG_TIMESTAMP_CONTROL, &control) &&
              this->publish_timestamps_(this->retained_flags_, (control & RX8111_TIMESTAMP_RAM) != 0, true);
  } else
#endif
  {
    success = this->configure_event_();
  }
  this->event_boot_pending_ = !success;
  if (success && this->watching_())
    this->start_event_polling_();
  this->boot_config_result_(BOOT_EVENT, success);
#ifdef USE_RX8111_TIMESTAMP
  if (!success && this->timestamp_sensor_ != nullptr && this->boot_attempts_[BOOT_EVENT] >= RX8111_BOOT_MAX_ATTEMPTS)
    this->publish_staged_timestamps_();
#endif
}

bool RX8111Component::configure_event_() {
#ifdef USE_RX8111_TIMESTAMP
  if (this->timestamp_sensor_ != nullptr) {
    // Pause capture while changing storage. Enable it last so a retry cannot erase new records.
    uint8_t control;
    if (!this->update_register(RX8111_REG_EXTENSION, RX8111_EXTENSION_ETS, 0) ||
        !this->read_byte(RX8111_REG_TIMESTAMP_CONTROL, &control))
      return false;
    const bool buffered = this->timestamp_mode_ != TIMESTAMP_MODE_LATEST;
    const uint8_t mode = buffered ? RX8111_TIMESTAMP_RAM : 0;
    // A previously unused RAM area has no meaningful occupancy pointer.
    if (buffered && !(control & RX8111_TIMESTAMP_RAM))
      control |= RX8111_TIMESTAMP_CLEAR;  // TSCLR self-clears; ETS must be off.
    control = (control & ~(RX8111_TIMESTAMP_RAM | RX8111_TIMESTAMP_EISEL)) | mode;
    const uint8_t overwrite = this->timestamp_mode_ == TIMESTAMP_MODE_OVERWRITE ? RX8111_EVIN_OVERWRITE : 0;
    return this->update_register(RX8111_REG_EVIN, 0xFE, this->evin_settings_ | overwrite) &&
           this->write_byte(RX8111_REG_TIMESTAMP_CONTROL, control) &&
           this->update_register(RX8111_REG_CONTROL, RX8111_CONTROL_EIE,
                                 this->event_enabled_ ? RX8111_CONTROL_EIE : 0) &&
           this->update_register(RX8111_REG_EXTENSION, RX8111_EXTENSION_ETS, RX8111_EXTENSION_ETS);
  }
#endif
  // An explicit disable affects only /INT; leave retained capture settings untouched.
  if (!this->event_enabled_)
    return this->update_register(RX8111_REG_CONTROL, RX8111_CONTROL_EIE, 0);
  // ETS gates EVIN detection, not just timestamp publication. Preserve all other
  // extension bits, including FOUT, alarm and timer settings, and retained records.
  return this->update_register(RX8111_REG_EVIN, 0xFC, this->evin_settings_ & 0xFC) &&
         this->update_register(RX8111_REG_TIMESTAMP_CONTROL, RX8111_TIMESTAMP_EISEL, 0) &&
         this->update_register(RX8111_REG_EXTENSION, RX8111_EXTENSION_ETS, RX8111_EXTENSION_ETS) &&
         this->update_register(RX8111_REG_CONTROL, RX8111_CONTROL_EIE, RX8111_CONTROL_EIE);
}

bool RX8111Component::clear_event_flag() {
  // FLAG is write-zero-to-clear; ones preserve other events, even if they latch during the write.
  if (!this->write_byte(RX8111_REG_FLAG, static_cast<uint8_t>(~RX8111_FLAG_EVF))) {
    this->set_warning_(WARN_EVENT_FLAG_CLEAR, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->event_flag_seen_ = false;
  this->clear_warning_(WARN_EVENT_FLAG_CLEAR);
  return true;
}

void RX8111Component::configure_boot_battery_() {
  // Own SWSEL as well as INIEN/CHGEN: 10 selects VDD when automatic switching is disabled
  // and is legal when enabled (unlike 11). Preserve SMPT and reserved bits.
  const uint8_t value = RX8111_POWER_VDD_ONLY | (this->battery_backup_ ? RX8111_POWER_INIEN : 0) |
                        (this->battery_backup_ && this->battery_charging_ ? RX8111_POWER_CHGEN : 0);
  this->boot_config_result_(BOOT_BATTERY,
                            this->update_register(RX8111_REG_POWER_SWITCH,
                                                  RX8111_POWER_INIEN | RX8111_POWER_CHGEN | RX8111_POWER_SWSEL, value));
}

void RX8111Component::configure_boot_fout_() {
  // TMPIN routes timer interrupts to FOUT instead of /INT; do not silently change that routing.
  uint8_t timer_control;
  bool success = this->read_byte(RX8111_REG_TIMER_CONTROL, &timer_control);
  if (success && this->fout_frequency_ != 3 && (timer_control & RX8111_TIMER_TMPIN)) {
    ESP_LOGW(TAG, "FOUT is routed to the wake-up timer (TMPIN); cannot configure clock output");
    success = false;
  } else if (success) {
    success =
        this->update_register(RX8111_REG_EXTENSION, RX8111_FOUT_FSEL, static_cast<uint8_t>(this->fout_frequency_ << 6));
  }
  this->boot_config_result_(BOOT_FOUT, success);
}

// A timer start clears TMPIN, which may be what failed FOUT; give it a fresh set of attempts.
void RX8111Component::retry_fout_after_timer_start_() {
  if (!this->fout_configured_ || (this->warnings_ & WARN_FOUT) == 0)
    return;
  this->cancel_timeout(RX8111_BOOT_FOUT_RETRY_ID);
  this->boot_attempts_[BOOT_FOUT] = 0;
  this->configure_boot_fout_();
}

void RX8111Component::boot_config_result_(BootFeature feature, bool success) {
  struct BootConfig {
    uint32_t retry_id;
    void (RX8111Component::*configure)();
    uint16_t warning;
  };
  static constexpr BootConfig CONFIGS[BOOT_FEATURE_COUNT] PROGMEM = {
      {RX8111_BOOT_BATTERY_RETRY_ID, &RX8111Component::configure_boot_battery_, WARN_BATTERY},
      {RX8111_BOOT_FOUT_RETRY_ID, &RX8111Component::configure_boot_fout_, WARN_FOUT},
      {RX8111_BOOT_EVENT_RETRY_ID, &RX8111Component::configure_boot_event_, WARN_EVENT},
      {RX8111_BOOT_ALARM_RETRY_ID, &RX8111Component::configure_boot_alarm_, WARN_ALARM},
      {RX8111_BOOT_TIMER_RETRY_ID, &RX8111Component::configure_boot_timer_, WARN_TIMER},
  };
  BootConfig config;
  progmem_memcpy(&config, &CONFIGS[feature], sizeof(config));
  const auto attempts = ++this->boot_attempts_[feature];
  if (success) {
    this->clear_warning_(config.warning);
    return;
  }
  this->set_warning_(config.warning, BootFailureNames::get_log_str(feature, BootFailureNames::LAST_INDEX));
  if (attempts < RX8111_BOOT_MAX_ATTEMPTS) {
    this->set_timeout(config.retry_id, this->boot_retry_delay_ms_,
                      [this, configure = config.configure]() { (this->*configure)(); });
  }
}

void RX8111Component::configure_boot_alarm_() {
  bool success = true;
  if (this->alarm_enabled_) {
    if (this->alarm_second_ != ALARM_FIELD_UNSET || this->alarm_minute_ != ALARM_FIELD_UNSET ||
        this->alarm_hour_ != ALARM_FIELD_UNSET || this->alarm_weekday_ != ALARM_FIELD_UNSET ||
        this->alarm_day_ != ALARM_FIELD_UNSET) {
      success = this->program_alarm_(this->alarm_second_, this->alarm_minute_, this->alarm_hour_, this->alarm_weekday_,
                                     this->alarm_day_, true);
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
        this->alarm_armed_ = (control & RX8111_CONTROL_AIE) != 0;
        if (this->alarm_armed_)
          this->start_event_polling_();
      }
    }
  } else if (this->alarm_explicitly_configured_) {
    success = this->set_alarm_interrupt_(false);
  }
  this->boot_config_result_(BOOT_ALARM, success);
}

void RX8111Component::configure_boot_timer_() {
  bool success = true;
  // An omitted timer setting leaves battery-backed state alone; explicit false stops it.
  if (this->timer_configured_) {
    const bool start_timer = this->timer_enabled_;
    if (start_timer) {
      success = this->configure_timer_();
    } else {
      uint8_t extension, control;
      success = this->disable_timer_(extension, control);
    }
    if (success && !start_timer && this->timer_count_ != 0) {
      this->timer_retained_event_pending_ = true;
      this->start_event_polling_();
    }
    if (success && start_timer)
      this->retry_fout_after_timer_start_();
  }
  this->boot_config_result_(BOOT_TIMER, success);
}

// A runtime action supersedes pending boot programming of the same feature
void RX8111Component::cancel_boot_retry_(bool alarm) {
  this->cancel_timeout(alarm ? RX8111_BOOT_ALARM_RETRY_ID : RX8111_BOOT_TIMER_RETRY_ID);
}

void RX8111Component::check_event_flags_(int16_t flags) {
  if (this->poll_running_) {
    this->defer(RX8111_DEFERRED_UPDATE_ID, [this]() { this->update(); });
    return;
  }
  this->poll_running_ = true;
  if (this->watching_())
    this->poll_event_flags_(flags);
  this->poll_running_ = false;
  // A callback or a runtime action may have stopped the last watched feature
  if (!this->watching_() && this->event_polling_started_) {
    this->cancel_interval(RX8111_EVENT_INTERVAL_ID);
    this->event_polling_started_ = false;
  }
}

void RX8111Component::start_event_polling_() {
  if (this->event_polling_started_)
    return;
  this->set_interval(RX8111_EVENT_INTERVAL_ID, this->event_poll_interval_, [this]() { this->check_event_flags_(); });
  this->event_polling_started_ = true;
}

void RX8111Component::update() {
  // Entity and time-sync callbacks may request another update synchronously.
  // Coalesce them without interleaving snapshots or recursing on the stack.
  if (this->update_running_ || this->poll_running_) {
    this->defer(RX8111_DEFERRED_UPDATE_ID, [this]() { this->update(); });
    return;
  }
  this->update_running_ = true;
  this->cancel_defer(RX8111_DEFERRED_UPDATE_ID);
  int16_t flags = -1;
  this->read_time_(flags);
  this->check_event_flags_(flags);
  this->update_running_ = false;
}

void RX8111Component::set_warning_(uint16_t bit, const LogString *message) {
  this->warnings_ |= bit;
  this->status_set_warning(message);
}

void RX8111Component::clear_warning_(uint16_t bit) {
  if ((this->warnings_ & bit) == 0)
    return;
  this->warnings_ &= ~bit;
  if (this->warnings_ == 0)
    this->status_clear_warning();
}

void RX8111Component::dump_config() {
  ESP_LOGCONFIG(TAG, "RX8111:");
  LOG_I2C_DEVICE(this);
#ifdef USE_BINARY_SENSOR
  for (uint8_t i = 0; i < 7; i++) {
    if (this->status_sensors_[i] != nullptr) {
      log_binary_sensor(TAG, "  ", LOG_STR_ARG(StatusSensorNames::get_log_str(i, StatusSensorNames::LAST_INDEX)),
                        this->status_sensors_[i]);
    }
  }
#endif
#ifdef USE_RX8111_TIMESTAMP
  LOG_TEXT_SENSOR("  ", "Event timestamp", this->timestamp_sensor_);
  if (this->timestamp_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Timestamp record mode: %s",
                  LOG_STR_ARG(TimestampModeNames::get_log_str(this->timestamp_mode_, TimestampModeNames::LAST_INDEX)));
  }
#endif
  if (this->battery_configured_) {
    ESP_LOGCONFIG(TAG, "  Battery backup: %s, charging: %s", ONOFF(this->battery_backup_),
                  ONOFF(this->battery_charging_));
  }
  if (this->fout_configured_) {
    ESP_LOGCONFIG(TAG, "  FOUT: %s", LOG_STR_ARG(FoutNames::get_log_str(this->fout_frequency_, FoutNames::LAST_INDEX)));
  }
  if (this->event_configured_) {
    ESP_LOGCONFIG(TAG, "  EVIN interrupt: %s", ONOFF(this->event_enabled_));
    bool evin_used = this->event_enabled_;
#ifdef USE_RX8111_TIMESTAMP
    evin_used = evin_used || this->timestamp_sensor_ != nullptr;
#endif
    if (evin_used) {
      ESP_LOGCONFIG(
          TAG, "    EVIN: %s, filter: %u, pull: %s",
          (this->evin_settings_ & 0x80) ? LOG_STR_LITERAL("high") : LOG_STR_LITERAL("low"),
          (this->evin_settings_ >> 5) & 3,
          LOG_STR_ARG(EvinPullNames::get_log_str((this->evin_settings_ >> 2) & 7, EvinPullNames::LAST_INDEX)));
    }
  }
  ESP_LOGCONFIG(TAG,
                "  Alarm interrupt: %s\n"
                "  Wake-up timer: %s",
                ONOFF(this->alarm_enabled_), ONOFF(this->timer_enabled_));
  if (this->timer_enabled_) {
    ESP_LOGCONFIG(TAG, "    Count: %" PRIu32 ", clock: %s", this->timer_count_,
                  LOG_STR_ARG(TimerClockNames::get_log_str(this->timer_clock_, TimerClockNames::LAST_INDEX)));
  }
  RealTimeClock::dump_config();
}

void RX8111Component::read_time() {
  int16_t flags = -1;
  this->read_time_(flags);
}

void RX8111Component::read_time_(int16_t &flags) {
  ESPTime rtc_time;
  if (!this->read_calendar_(rtc_time, &flags))
    return;
  ESP_LOGD(TAG, "Read UTC time: %04d-%02d-%02d %02d:%02d:%02d", rtc_time.year, rtc_time.month, rtc_time.day_of_month,
           rtc_time.hour, rtc_time.minute, rtc_time.second);
  // Synchronization callbacks can clear flags or program the RTC. Only reuse
  // the snapshot when no user code will run between the read and the poll.
  if (!this->time_sync_callback_.empty())
    flags = -1;
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

bool RX8111Component::read_calendar_(ESPTime &rtc_time, int16_t *flags) {
  if (this->time_write_invalid_) {
    this->set_warning_(WARN_CALENDAR, LOG_STR("RTC write incomplete; write trusted time again"));
    return false;
  }
  uint8_t regs[2];  // Adjacent FLAG and CONTROL registers.
  if (flags != nullptr)
    *flags = -2;  // A failed shared read must not be repeated in the same update.
  if (!this->read_bytes(RX8111_REG_FLAG, regs, sizeof(regs))) {
    this->set_warning_(WARN_CALENDAR, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  if (flags != nullptr)
    *flags = regs[0];
  if (regs[1] & RX8111_CTRL_STOP) {
    this->set_warning_(WARN_CALENDAR, LOG_STR("RTC counter stopped; not synchronizing"));
    return false;
  }
  if (regs[0] & (RX8111_FLAG_VLF | RX8111_FLAG_XST | RX8111_FLAG_POR)) {
    ESP_LOGW(TAG, "RTC time invalidated by power loss or oscillator stop; write trusted time");
    this->set_warning_(WARN_CALENDAR, LOG_STR("RTC time invalidated; write trusted time"));
    return false;
  }

  uint8_t buf[7];
  // I2C access freezes the RX8111 counter, yielding a coherent calendar snapshot.
  if (!this->read_bytes(RX8111_REG_SEC, buf, sizeof(buf))) {
    this->set_warning_(WARN_CALENDAR, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  // decode_bcd_calendar() masks the same bits; the check here catches a corrupted register
  bool valid = mask_bcd_calendar(buf);
  const uint8_t onehot = buf[3];
  valid = valid && onehot != 0 && onehot <= 0x40 && (onehot & (onehot - 1)) == 0;
  if (valid) {
    rtc_time = time::decode_bcd_calendar<time::EPSON_BCD_CALENDAR>(buf);
    valid = rtc_time.is_valid(true, false) && rtc_time.second < 60 &&
            ESPTime::from_epoch_utc(rtc_time.timestamp).day_of_week == rtc_time.day_of_week;
  }
  if (!valid) {
    ESP_LOGW(TAG, "Invalid RTC time, not syncing to system clock");
    this->set_warning_(WARN_CALENDAR, LOG_STR("Invalid RTC time"));
    return false;
  }
  this->clear_warning_(WARN_CALENDAR);
  return true;
}

void RX8111Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid() || now.year > 2099) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC");
    return;
  }
  if (!this->set_stop_bit_(true)) {
    this->set_warning_(WARN_WRITE_TIME, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  uint8_t buf[7];
  time::encode_bcd_calendar<time::EPSON_BCD_CALENDAR>(now, buf);
  this->time_write_invalid_ = true;
  const bool calendar_written = this->write_bytes(RX8111_REG_SEC, buf, sizeof(buf));
  if (!this->finish_time_write_(calendar_written))
    return;
  ESP_LOGD(TAG, "Wrote UTC time: %04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second);
}

bool RX8111Component::finish_time_write_(bool calendar_written) {
  // STOP is the battery-backed invalidity marker, including across host resets.
  // Never release a partial or delayed snapshot; only a fresh trusted write can restart it.
  if (!calendar_written) {
    this->set_warning_(WARN_WRITE_TIME, LOG_STR("RTC write incomplete; write trusted time again"));
    return false;
  }
  if (!this->write_byte(RX8111_REG_FLAG, RX8111_KEEP_EVENT_FLAGS) || !this->set_stop_bit_(false)) {
    this->set_warning_(WARN_WRITE_TIME, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->time_write_invalid_ = false;
  this->clear_warning_(WARN_WRITE_TIME);
  this->clear_warning_(WARN_CALENDAR);
  return true;
}

bool RX8111Component::set_stop_bit_(bool stop) {
  return this->update_register(RX8111_REG_CONTROL, RX8111_CTRL_STOP, stop ? RX8111_CTRL_STOP : 0);
}

bool RX8111Component::set_alarm_interrupt_(bool enabled) {
  return this->update_register(RX8111_REG_CONTROL, RX8111_CONTROL_AIE, enabled ? RX8111_CONTROL_AIE : 0);
}

bool RX8111Component::program_alarm_(uint8_t second, uint8_t minute, uint8_t hour, uint8_t weekday, uint8_t day,
                                     bool boot) {
  uint8_t extension;
  if (!this->read_byte(RX8111_REG_EXTENSION, &extension))
    return false;
  if (weekday != ALARM_FIELD_UNSET) {
    extension &= ~RX8111_EXTENSION_WADA;
  } else if (day != ALARM_FIELD_UNSET) {
    extension |= RX8111_EXTENSION_WADA;
  }

  const uint8_t fields[] = {
      second == ALARM_FIELD_UNSET ? RX8111_ALARM_DISABLED : uint8_to_bcd(second),
      minute == ALARM_FIELD_UNSET ? RX8111_ALARM_DISABLED : uint8_to_bcd(minute),
      hour == ALARM_FIELD_UNSET ? RX8111_ALARM_DISABLED : uint8_to_bcd(hour),
      weekday != ALARM_FIELD_UNSET ? static_cast<uint8_t>(1U << (weekday - 1))
      : day != ALARM_FIELD_UNSET   ? uint8_to_bcd(day)
                                   : RX8111_ALARM_DISABLED,
  };
  // Disable the interrupt during partial programming; keep AF (past events) latched.
  if (!this->set_alarm_interrupt_(false))
    return false;
  this->alarm_armed_ = false;
  this->alarm_reprogrammed_ = true;
  uint8_t flags;
  if (!this->read_byte(RX8111_REG_FLAG, &flags))
    return false;
  if (!boot)
    this->alarm_flag_seen_ = (flags & RX8111_FLAG_AF) != 0;
  if (!this->write_byte(RX8111_REG_SEC_ALARM, fields[0]) || !this->write_bytes(RX8111_REG_MIN_ALARM, &fields[1], 3) ||
      !this->write_byte(RX8111_REG_EXTENSION, extension))
    return false;
  if (!this->set_alarm_interrupt_(this->alarm_enabled_))
    return false;
  this->alarm_armed_ = true;
  return true;
}

bool RX8111Component::set_alarm(uint16_t second, uint16_t minute, uint16_t hour, uint16_t weekday, uint16_t day) {
  if (!this->alarm_enabled_) {
    ESP_LOGE(TAG, "Alarm interrupt disabled; set alarm_enabled: true");
    return false;
  }
  constexpr uint16_t unset = ALARM_FIELD_UNSET;
  if ((second != unset && second > 59) || (minute != unset && minute > 59) || (hour != unset && hour > 23) ||
      (weekday != unset && (weekday < 1 || weekday > 7)) || (day != unset && (day < 1 || day > 31)) ||
      (weekday != unset && day != unset) ||
      (second == unset && minute == unset && hour == unset && weekday == unset && day == unset)) {
    ESP_LOGE(TAG, "Invalid alarm fields");
    return false;
  }
  this->cancel_boot_retry_(true);
  if (!this->program_alarm_(second, minute, hour, weekday, day)) {
    this->set_warning_(WARN_ALARM, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->clear_warning_(WARN_ALARM);
  this->boot_alarm_event_seen_ = true;
  this->start_event_polling_();
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
  return this->set_alarm(target.second, target.minute, target.hour, ALARM_FIELD_UNSET, target.day_of_month);
}

bool RX8111Component::unset_alarm() {
  this->cancel_boot_retry_(true);
  if (!this->set_alarm_interrupt_(false)) {
    this->set_warning_(WARN_ALARM, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->alarm_armed_ = false;
  // Minute, hour and day alarm registers are contiguous
  static constexpr uint8_t DISABLED_FIELDS[3] = {RX8111_ALARM_DISABLED, RX8111_ALARM_DISABLED, RX8111_ALARM_DISABLED};
  if (!this->write_byte(RX8111_REG_SEC_ALARM, RX8111_ALARM_DISABLED) ||
      !this->write_bytes(RX8111_REG_MIN_ALARM, DISABLED_FIELDS, sizeof(DISABLED_FIELDS))) {
    this->set_warning_(WARN_ALARM, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->clear_warning_(WARN_ALARM);
  return true;
}

bool RX8111Component::clear_alarm_flag() {
  if (!this->write_byte(RX8111_REG_FLAG, static_cast<uint8_t>(~RX8111_FLAG_AF))) {
    this->set_warning_(WARN_ALARM_FLAG_CLEAR, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->alarm_flag_seen_ = false;
  this->clear_warning_(WARN_ALARM_FLAG_CLEAR);
  return true;
}

// Clears TE and TIE; returns the register values read so callers can build on them
bool RX8111Component::disable_timer_(uint8_t &extension, uint8_t &control) {
  return this->read_byte(RX8111_REG_EXTENSION, &extension) &&
         this->write_byte(RX8111_REG_EXTENSION, extension & ~RX8111_EXTENSION_TE) &&
         this->read_byte(RX8111_REG_CONTROL, &control) &&
         this->write_byte(RX8111_REG_CONTROL, control & ~RX8111_CONTROL_TIE);
}

bool RX8111Component::configure_timer_(bool clear_stale_flag) {
  // Stop counting before updating the 24-bit reload value; preserve the boot flag unless explicitly starting.
  uint8_t extension;
  uint8_t control;
  if (!this->disable_timer_(extension, control))
    return false;

  // Explicit starts own the timer mode: count in both power modes, do not pause,
  // and route interrupt pulses to /INT rather than FOUT. Leave reserved bits alone.
  if (!this->update_register(RX8111_REG_TIMER_CONTROL, RX8111_TIMER_MODE, 0))
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
  this->cancel_boot_retry_(false);
  if (!this->configure_timer_(true)) {
    // A partial configuration can leave either enable bit set. Disable both if the bus permits it;
    // otherwise keep watching a timer that may still be running.
    uint8_t extension, control;
    if (this->disable_timer_(extension, control))
      this->timer_enabled_ = false;
    this->set_warning_(WARN_TIMER, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->clear_warning_(WARN_TIMER);
  this->timer_enabled_ = true;
  this->timer_retained_event_pending_ = false;
  this->start_event_polling_();
  this->retry_fout_after_timer_start_();
  return true;
}

bool RX8111Component::clear_timer_flag() {
  if (!this->write_byte(RX8111_REG_FLAG, static_cast<uint8_t>(~RX8111_FLAG_TF))) {
    this->set_warning_(WARN_TIMER_FLAG_CLEAR, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->timer_flag_seen_ = false;
  this->timer_retained_event_pending_ = false;
  this->clear_warning_(WARN_TIMER_FLAG_CLEAR);
  return true;
}

bool RX8111Component::stop_timer() {
  this->cancel_boot_retry_(false);
  uint8_t extension, control;
  if (!this->disable_timer_(extension, control)) {
    this->set_warning_(WARN_TIMER, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }
  this->timer_enabled_ = false;
  this->clear_warning_(WARN_TIMER);
  return true;
}

bool RX8111Component::poll_event_flags_(int16_t snapshot) {
  uint8_t flags = 0;
  const bool flags_valid = snapshot >= 0 || (snapshot == -1 && this->read_byte(RX8111_REG_FLAG, &flags));
  if (snapshot >= 0)
    flags = static_cast<uint8_t>(snapshot);
  bool success = flags_valid;
#ifdef USE_BINARY_SENSOR
  uint8_t status = 0;
  const bool monitor_requested = this->status_sensors_[5] != nullptr || this->status_sensors_[6] != nullptr;
  const bool status_valid = !monitor_requested || this->read_byte(0x33, &status);
  success = success && status_valid;
#endif
  // Settle this snapshot before callbacks, never after a callback's newer result.
  if (success) {
    this->clear_warning_(WARN_EVENT_POLL);
  } else {
    this->set_warning_(WARN_EVENT_POLL, LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  }
  // EVF is not cleared here: users may use it to hold /INT asserted across sleep.
  const bool event_active = (flags & RX8111_FLAG_EVF) != 0;
  const bool event_rising =
      flags_valid && event_active && !this->event_flag_seen_ && this->event_enabled_ && !this->event_boot_pending_;
  if (flags_valid && this->event_enabled_ && !this->event_boot_pending_)
    this->event_flag_seen_ = event_active;
  const bool alarm_active = flags_valid && (flags & RX8111_FLAG_AF) != 0;
  const bool timer_active = flags_valid && (flags & RX8111_FLAG_TF) != 0;
  // alarm_armed_ is only ever set while the alarm is enabled
  const bool timer_watched = this->timer_enabled_ || this->timer_retained_event_pending_;
  if (alarm_active && this->alarm_armed_)
    this->boot_alarm_event_seen_ = true;
  const bool alarm_rising = alarm_active && !this->alarm_flag_seen_ && this->alarm_armed_;
  const bool timer_rising = timer_active && !this->timer_flag_seen_ && timer_watched;
  const bool deliver_stopped_event = timer_rising && this->timer_retained_event_pending_;
  // Record eligible edges before callbacks, which may clear flags synchronously.
  // Leave flags unseen while boot configuration is still waiting to enable delivery.
  if (flags_valid && this->alarm_armed_)
    this->alarm_flag_seen_ = alarm_active;
  this->alarm_reprogrammed_ = false;
  if (flags_valid && timer_watched)
    this->timer_flag_seen_ = timer_active;
  if (flags_valid)
    this->timer_retained_event_pending_ = false;
#ifdef USE_BINARY_SENSOR
  // Read the live monitor only when requested. Its failure must not suppress
  // valid FLAG states, timestamp publication, or alarm/timer/event callbacks.
  for (uint8_t i = 0; i < 7; i++) {
    if (this->status_sensors_[i] != nullptr && (i < 5 ? flags_valid : status_valid)) {
      this->status_sensors_[i]->publish_state(((i < 5 ? flags : status) & progmem_read_byte(&STATUS_SENSOR_MASKS[i])) !=
                                              0);
    }
  }
#endif
#ifdef USE_RX8111_TIMESTAMP
  // Stage the event edges first; text callbacks may acknowledge any of the flags.
  if (flags_valid && this->timestamp_sensor_ != nullptr && !this->event_boot_pending_) {
    if (this->publish_timestamps_(flags, this->timestamp_mode_ != TIMESTAMP_MODE_LATEST)) {
      this->clear_warning_(WARN_TIMESTAMP);
    } else {
      this->set_warning_(WARN_TIMESTAMP, LOG_STR("RX8111 timestamp read failed"));
    }
  }
#endif
  // A timestamp callback that set a new alarm superseded the one that fired
  if (alarm_rising && this->alarm_flag_seen_ && this->alarm_armed_ && !this->alarm_reprogrammed_)
    this->alarm_callbacks_.call();
  // An alarm callback may stop the timer before its callback is delivered.
  if (timer_rising && this->timer_flag_seen_ && (this->timer_enabled_ || deliver_stopped_event))
    this->timer_callbacks_.call();
  if (event_rising && this->event_flag_seen_)
    this->event_callbacks_.call();
  return success;
}

#ifdef USE_RX8111_TIMESTAMP
void RX8111Component::publish_staged_timestamps_() {
  if (!this->timestamps_staged_ || this->defer_timestamps_)
    return;
  uint8_t records[64];
  std::memcpy(records, this->staged_timestamps_, sizeof(records));
  const uint8_t count = this->staged_count_;
  const bool buffered = this->staged_buffered_;
  // Consume the staging before callbacks, including callbacks that acknowledge flags synchronously.
  this->timestamps_staged_ = false;
  this->staged_count_ = 0;
  this->deliver_timestamps_(records, count, buffered);
}

// An unbuffered count is 0 or 1: the latest-bank record, which becomes the new baseline.
void RX8111Component::deliver_timestamps_(const uint8_t *records, uint8_t count, bool buffered) {
  if (!buffered && count != 0) {
    std::memcpy(this->last_timestamp_, records, 10);
    this->timestamp_seen_ = true;
  }
  for (uint8_t i = 0; i < count; i++)
    this->publish_timestamp_(records + i * 8, buffered);
}

void RX8111Component::publish_timestamp_(const uint8_t *raw, bool buffered) {
  // Buffered records omit weekday and the two least significant fractional bits.
  const uint8_t offset = buffered ? 1 : 2;
  uint8_t calendar[7] = {raw[offset],           raw[offset + 1],       raw[offset + 2],      1,
                         raw[buffered ? 4 : 6], raw[buffered ? 5 : 7], raw[buffered ? 6 : 8]};
  // Captured XST invalidates this record.
  const bool valid = mask_bcd_calendar(calendar) && (raw[buffered ? 7 : 9] & 2) == 0;
  const auto captured = time::decode_bcd_calendar<time::EPSON_BCD_CALENDAR>(calendar);
  if (!valid || !captured.is_valid(true, false) || captured.second > 59) {
    ESP_LOGW(TAG, "Skipping invalid RTC timestamp record");
    return;
  }
  const uint16_t fraction = buffered ? raw[0] * 4 : raw[1] * 4 + (raw[0] & 3);
  // Truncate rather than round: the last tick must not become an invalid .1000 fraction.
  const unsigned milliseconds = fraction * 1000U / 1024U;
  char value[25];
  const int len =
      std::snprintf(value, sizeof(value), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", captured.year, captured.month,
                    captured.day_of_month, captured.hour, captured.minute, captured.second, milliseconds);
  this->timestamp_sensor_->publish_state(value, len);
}

bool RX8111Component::publish_timestamps_(uint8_t flags, bool buffered, bool startup) {
  if (this->capture_restore_pending_) {
    if (!this->update_register(RX8111_REG_EXTENSION, RX8111_EXTENSION_ETS, RX8111_EXTENSION_ETS))
      return false;
    this->capture_restore_pending_ = false;
  }
  // EVF acknowledgement is independent of timestamp consumption. Read latest storage even after an acknowledgement.
  uint8_t records[64]{};
  if (!startup && !buffered && this->timestamp_seen_) {
    // Only use this active-capture read to skip unchanged storage, never to publish a potentially torn record.
    if (!this->read_bytes(RX8111_REG_TIMESTAMP_LATEST, records, 10))
      return false;
    if (std::memcmp(records, this->last_timestamp_, 10) == 0)
      return true;
  }
  if (!startup && buffered) {
    uint8_t status;
    if (!this->read_byte(RX8111_REG_TIMESTAMP_STATUS, &status))
      return false;
    if (status & RX8111_TIMESTAMP_EMPTY)
      return true;
  }
  uint8_t extension;
  if (!this->read_byte(RX8111_REG_EXTENSION, &extension))
    return false;
  const bool restore = (extension & RX8111_EXTENSION_ETS) != 0;
  if (startup && !this->retained_evidence_seen_) {
    this->retained_latest_qualified_ =
        (flags & (RX8111_FLAG_POR | RX8111_FLAG_VLF | RX8111_FLAG_XST)) == 0 && ((flags & RX8111_FLAG_EVF) || restore);
    this->retained_evidence_seen_ = true;
  }
  if (restore && !this->write_byte(RX8111_REG_EXTENSION, extension & ~RX8111_EXTENSION_ETS)) {
    this->capture_restore_pending_ = !startup;
    return false;
  }
  uint8_t count = 0;
  bool success = true;
  if (startup && !this->timestamps_staged_ && buffered &&
      !this->read_bytes(RX8111_REG_TIMESTAMP_LATEST, this->last_timestamp_, 10))
    return false;
  if (startup && this->timestamps_staged_) {
    buffered = this->staged_buffered_;
    count = this->staged_count_;
    std::memcpy(records, this->staged_timestamps_, sizeof(records));
  } else if (!buffered) {
    success = this->read_bytes(RX8111_REG_TIMESTAMP_LATEST, records, 10);
    // Across reboot there is no consumed marker. Recover clean retained capture, accepting possible redelivery.
    const bool publish_latest =
        success && (startup ? this->retained_latest_qualified_
                            : (this->timestamp_seen_ && std::memcmp(records, this->last_timestamp_, 10) != 0));
    count = publish_latest ? 1 : 0;
  } else {
    uint8_t status;
    success = this->read_byte(RX8111_REG_TIMESTAMP_STATUS, &status);
    if (success && !(status & RX8111_TIMESTAMP_EMPTY)) {
      const uint8_t latest = status & 7;
      const uint8_t available = (status & RX8111_TIMESTAMP_FULL) ? 8 : latest + 1;
      const uint8_t first = available == 8 ? (latest + 1) & 7 : 0;
      for (uint8_t i = 0; i < available && success; i++)
        success = this->read_bytes(RX8111_REG_TIMESTAMP_BUFFER + (((first + i) & 7) * 8), records + i * 8, 8);
      if (success) {
        count = available;
        if (startup) {
          this->timestamp_clear_pending_ = true;
        } else {
          success = this->update_register(RX8111_REG_TIMESTAMP_CONTROL, RX8111_TIMESTAMP_CLEAR, RX8111_TIMESTAMP_CLEAR);
        }
      }
    }
  }
  if (startup) {
    // Keep capture paused and retain staged data across configuration failures, including a failed final enable.
    if (!success)
      return false;
    if (!this->timestamps_staged_) {
      // Baseline the inactive latest bank too: switching from RAM must not publish old latest-bank contents.
      if (!buffered) {
        std::memcpy(this->last_timestamp_, records, 10);
      }
      this->timestamp_seen_ = true;
      std::memcpy(this->staged_timestamps_, records, count * (buffered ? 8 : 10));
      this->staged_count_ = count;
      this->staged_buffered_ = buffered;
      this->timestamps_staged_ = true;
    }
    // A failed acknowledgement does not prove TSCLR was rejected. Keep the host copy until delivery.
    if (this->timestamp_clear_pending_) {
      if (!this->update_register(RX8111_REG_TIMESTAMP_CONTROL, RX8111_TIMESTAMP_CLEAR, RX8111_TIMESTAMP_CLEAR))
        return false;
      this->timestamp_clear_pending_ = false;
    }
    if (!this->configure_event_())
      return false;
    this->publish_staged_timestamps_();
    return true;
  } else {
    const bool restored = !restore || this->write_byte(RX8111_REG_EXTENSION, extension);
    this->capture_restore_pending_ = !restored;
    success = success && restored;
  }
  // Deliver fully staged data even when runtime cleanup fails; buffered data may be redelivered.
  this->deliver_timestamps_(records, count, buffered);
  return success;
}
#endif

}  // namespace esphome::rx8111
