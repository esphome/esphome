#pragma once

// HC32 MCU DP101/102 payload validation and MCU-owned sleep-mode epoch.
// This never chooses the platform power mode: only a fresh validated CMD01 may supply
// the MCU's 'sm' (standby/hibernation) selection.
#include <cstddef>
#include <cstdint>

namespace esphome::tuya {

class TuyaLowPowerSleepWindow {
 public:
  static constexpr uint8_t ENABLE_DP = 101;
  static constexpr uint8_t WINDOW_DP = 102;
  static constexpr uint8_t HEATING_SCHEDULE_DP = 107;
  static constexpr uint8_t BOOLEAN_TYPE = 1;
  static constexpr uint8_t RAW_TYPE = 0;

  // The original DP102 handler reads 4 bytes without checking its length.
  // Reject malformed commands/reports BEFORE they reach the MCU or DP cache.
  static bool valid_datapoint(uint8_t id, uint8_t type, const uint8_t *value, size_t size) {
    if (id == ENABLE_DP) {
      return type == BOOLEAN_TYPE && size == 1 && value != nullptr && value[0] <= 1;
    }
    if (id == WINDOW_DP) {
      return type == RAW_TYPE && size == 4 && value != nullptr && value[0] < 24 && value[1] < 60 && value[2] < 24 &&
             value[3] < 60;
    }
    if (id == HEATING_SCHEDULE_DP) {
      // A distinct HC32 handler reads eight 4-byte heating-program slots
      // without its own bounds checks: hour, minute, temperature BE16.
      if (type != RAW_TYPE || size != 32 || value == nullptr) {
        return false;
      }
      for (size_t i = 0; i < 8; ++i) {
        if (value[4 * i] >= 24 || value[4 * i + 1] >= 60) {
          return false;
        }
      }
    }
    return true;  // Other DPs are validated by the generic packet parser.
  }

  // Mirrors the emulated HC32 RTC service for host regression tests only.
  // The firmware NEVER evaluates this to fabricate a new product 'sm'.
  static bool rtc_marker(bool previous, bool time_valid, bool enabled, uint8_t hour, uint8_t minute, uint8_t start_hour,
                         uint8_t start_minute, uint8_t end_hour, uint8_t end_minute) {
    if (!time_valid) {
      return previous;
    }
    if (!enabled || hour >= 24 || minute >= 60 || start_hour >= 24 || start_minute >= 60 || end_hour >= 24 ||
        end_minute >= 60) {
      return false;
    }
    const unsigned now = static_cast<unsigned>(hour) * 60 + minute;
    const unsigned start = static_cast<unsigned>(start_hour) * 60 + start_minute;
    const unsigned end = static_cast<unsigned>(end_hour) * 60 + end_minute;
    if (start == end) {
      return false;  // No 24-hour interpretation of equal boundaries.
    }
    return start < end ? (now >= start && now < end) : (now >= start || now < end);
  }

  void reset_reports() {
    seen_enable_ = false;
    seen_window_ = false;
    enabled_ = false;
  }

  void product_validated() { product_epoch_valid_ = true; }
  void invalidate_product() { product_epoch_valid_ = false; }

  // A validated DP101/102 write can change the HC32's later RTC state.
  // The write ACK, the report and a guessed local clock are NOT a new sm.
  void on_write(uint8_t id) {
    if (id == ENABLE_DP || id == WINDOW_DP) {
      invalidate_product();
    }
  }

  void observe_report(uint8_t id, uint8_t type, const uint8_t *value, size_t len) {
    if (!valid_datapoint(id, type, value, len)) {
      return;
    }
    if (id == ENABLE_DP) {
      const bool new_enabled = value[0] != 0;
      if (seen_enable_ && enabled_ != new_enabled) {
        invalidate_product();
      }
      enabled_ = new_enabled;
      seen_enable_ = true;
    } else if (id == WINDOW_DP) {
      if (seen_window_ &&
          (value[0] != hours_[0] || value[1] != hours_[1] || value[2] != hours_[2] || value[3] != hours_[3])) {
        invalidate_product();
      }
      for (size_t i = 0; i < 4; ++i) {
        hours_[i] = value[i];
      }
      seen_window_ = true;
    }
  }

  bool enable_reported() const { return seen_enable_; }
  bool enabled() const { return seen_enable_ && enabled_; }
  bool window_reported() const { return seen_window_; }
  bool product_epoch_valid() const { return product_epoch_valid_; }

  // With DP101 enabled, the HC32 RTC task can change sm at minute boundaries
  // and may pulse PA3. Electrical connection to WXU reset is unproven.
  // Remain awake instead of treating the original CMD01 as perpetual truth.
  bool standby_mode_stable() const { return product_epoch_valid_ && seen_enable_ && !enabled_; }

 private:
  uint8_t hours_[4]{};
  bool enabled_{false};
  bool seen_enable_{false};
  bool seen_window_{false};
  bool product_epoch_valid_{false};
};

}  // namespace esphome::tuya
