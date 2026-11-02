#pragma once

#include "esphome/core/defines.h"

#ifdef USE_CRASH_REPORT

#include <array>
#include <cstdint>

namespace esphome {

/// Per core
static constexpr uint8_t CRASH_REPORT_MAX_BACKTRACE = 16;

/// Sent as is over the native API
enum CrashReportFlag : uint8_t {
  CRASH_REPORT_FLAG_NONE = 0,
  CRASH_REPORT_FLAG_PRESENT = 1 << 0,        // Stored and not yet read
  CRASH_REPORT_FLAG_BUILD_MATCHES = 1 << 1,  // Recorded by the running firmware; assumed on ESP8266 and RP2
  CRASH_REPORT_FLAG_CAUSE = 1 << 2,          // cause is valid
  CRASH_REPORT_FLAG_FAULT_ADDRESS = 1 << 3,  // fault_address is valid
};

/// Sent as is over the native API
enum class CrashReason : uint8_t {
  CRASH_REASON_UNKNOWN = 0,
  CRASH_REASON_EXCEPTION = 1,
  CRASH_REASON_ABORT = 2,
  CRASH_REASON_TASK_WATCHDOG = 3,
  CRASH_REASON_INTERRUPT_WATCHDOG = 4,
  CRASH_REASON_SOFT_WATCHDOG = 5,
  CRASH_REASON_HARDWARE_WATCHDOG = 6,  // Only the reason is recorded
};

struct CrashReport {
  uint8_t flags{CRASH_REPORT_FLAG_NONE};
  CrashReason reason{CrashReason::CRASH_REASON_UNKNOWN};
  uint8_t crashed_core{0};
  uint8_t backtrace_count{0};
  uint8_t other_core_backtrace_count{0};
  uint32_t pc{0};
  uint32_t cause{0};
  uint32_t fault_address{0};
  uint32_t build_time{0};
  std::array<uint32_t, CRASH_REPORT_MAX_BACKTRACE> backtrace{};
  std::array<uint32_t, CRASH_REPORT_MAX_BACKTRACE> other_core_backtrace{};
};

/// The previous boot's crash; false when there is none or it was read. Implemented per platform.
bool crash_report_get(CrashReport &out);

/// Mark the report read; it also no longer survives a reboot.
void crash_report_clear();

}  // namespace esphome

#endif  // USE_CRASH_REPORT
