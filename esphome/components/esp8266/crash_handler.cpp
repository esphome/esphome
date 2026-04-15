#ifdef USE_ESP8266

#include "esphome/core/defines.h"
#ifdef USE_ESP8266_CRASH_HANDLER

#include "crash_handler.h"
#include "esphome/core/log.h"

#include <cinttypes>

extern "C" {
#include <user_interface.h>

// Global reset info struct populated by SDK/Arduino core at boot
extern struct rst_info resetInfo;
}

// Xtensa windowed-ABI: bits[31:30] encode call type (CALL0=00, CALL4=01,
// CALL8=10, CALL12=11). Mask and force bit 30 to recover the real address.
static constexpr uint32_t XTENSA_ADDR_MASK = 0x3FFFFFFF;
static constexpr uint32_t XTENSA_CODE_BASE = 0x40000000;

// ESP8266 memory map boundaries for code regions
static constexpr uint32_t IRAM_START = 0x40100000;
static constexpr uint32_t IRAM_END = 0x40108000;  // 32KB

// Linker symbols for the actual firmware IROM section.
// Using these instead of a conservative upper bound (0x40400000) prevents
// false positives from stale stack values beyond the actual flash mapping.
extern "C" {
// NOLINTBEGIN(bugprone-reserved-identifier,readability-identifier-naming,readability-redundant-declaration)
extern void _irom0_text_start(void);
extern void _irom0_text_end(void);
// NOLINTEND(bugprone-reserved-identifier,readability-identifier-naming,readability-redundant-declaration)
}

// Check if a value looks like a code address in IRAM or flash-mapped IROM.
// IRAM_ATTR as safety net — normally inlined into custom_crash_callback, but
// ensures correctness if the compiler ever chooses not to inline.
static inline bool IRAM_ATTR is_code_addr(uint32_t val) {
  uint32_t addr = (val & XTENSA_ADDR_MASK) | XTENSA_CODE_BASE;
  return (addr >= IRAM_START && addr < IRAM_END) ||
         (addr >= (uint32_t) _irom0_text_start && addr < (uint32_t) _irom0_text_end);
}

// Recover the actual code address from a windowed-ABI return address on the stack.
static inline uint32_t IRAM_ATTR recover_code_addr(uint32_t val) { return (val & XTENSA_ADDR_MASK) | XTENSA_CODE_BASE; }

// RTC user memory layout for crash backtrace data.
// User-accessible RTC memory: blocks 64-191 (each block = 4 bytes).
// We use blocks 174-191 (last 18 blocks, 72 bytes) to minimize conflicts.
// Store 16 raw candidates, filter to real return addresses at log time.
static constexpr uint8_t RTC_CRASH_BASE = 174;
static constexpr size_t MAX_BACKTRACE = 16;

// Magic word packs sentinel, version, and count into one uint32_t:
//   bits[31:16] = sentinel
//   bits[15:8]  = version
//   bits[7:0]   = backtrace count
static constexpr uint8_t CRASH_SENTINEL_BITS = 16;
static constexpr uint8_t CRASH_VERSION_BITS = 8;

static constexpr uint16_t CRASH_SENTINEL_VALUE = 0xDEAD;
static constexpr uint8_t CRASH_VERSION_VALUE = 1;

static constexpr uint32_t CRASH_SENTINEL = static_cast<uint32_t>(CRASH_SENTINEL_VALUE) << CRASH_SENTINEL_BITS;
static constexpr uint32_t CRASH_VERSION = static_cast<uint32_t>(CRASH_VERSION_VALUE) << CRASH_VERSION_BITS;
static constexpr uint32_t CRASH_SENTINEL_MASK = static_cast<uint32_t>(0xFFFF) << CRASH_SENTINEL_BITS;
static constexpr uint32_t CRASH_VERSION_MASK = static_cast<uint32_t>(0xFF) << CRASH_VERSION_BITS;
static constexpr uint32_t CRASH_COUNT_MASK = 0xFF;

// Struct layout: 18 RTC blocks (72 bytes):
// [0] = magic (sentinel | version | count)
// [1..16] = up to 16 code addresses from stack scanning
// [17] = epc1 at crash time (to skip duplicates at log time)
struct RtcCrashData {
  uint32_t magic;
  uint32_t backtrace[MAX_BACKTRACE];
  uint32_t epc1;  // Fault PC, used to filter duplicates
};
static_assert(sizeof(RtcCrashData) == 72, "RtcCrashData must fit in 18 RTC blocks");

namespace esphome::esp8266 {

static const char *const TAG = "esp8266";

static inline bool is_crash_reason(uint32_t reason) {
  return reason == REASON_WDT_RST || reason == REASON_EXCEPTION_RST || reason == REASON_SOFT_WDT_RST;
}

bool crash_handler_has_data() { return is_crash_reason(resetInfo.reason); }

// Xtensa exception cause names for the LX106 core (ESP8266).
// Only includes causes that can actually occur on the LX106 — it has no MMU,
// no TLB, no PIF, and no privilege levels, so causes 12-18 and 24-26 are
// impossible and omitted. The numeric cause is always logged as fallback.
// Uses if-else with LOG_STR to avoid CSWTCH jump tables (RAM on ESP8266).
static const LogString *get_exception_cause(uint32_t cause) {
  if (cause == 0)
    return LOG_STR("IllegalInst");
  if (cause == 2)
    return LOG_STR("InstFetchErr");
  if (cause == 3)
    return LOG_STR("LoadStoreErr");
  if (cause == 4)
    return LOG_STR("Level1Int");
  if (cause == 6)
    return LOG_STR("DivByZero");
  if (cause == 9)
    return LOG_STR("Alignment");
  if (cause == 20)
    return LOG_STR("InstFetchProhibit");
  if (cause == 28)
    return LOG_STR("LoadProhibit");
  if (cause == 29)
    return LOG_STR("StoreProhibit");
  return nullptr;
}

static const LogString *get_reset_reason(uint32_t reason) {
  if (reason == REASON_WDT_RST)
    return LOG_STR("Hardware WDT");
  if (reason == REASON_EXCEPTION_RST)
    return LOG_STR("Exception");
  if (reason == REASON_SOFT_WDT_RST)
    return LOG_STR("Soft WDT");
  return LOG_STR("Unknown");
}

// Read backtrace from RTC user memory into caller-provided buffer.
// Returns the number of valid backtrace entries (0 if no data found).
static uint8_t read_rtc_backtrace(uint32_t *backtrace, size_t max_entries) {
  RtcCrashData rtc_data;
  if (!system_rtc_mem_read(RTC_CRASH_BASE, &rtc_data, sizeof(rtc_data)))
    return 0;
  uint32_t magic = rtc_data.magic;
  if ((magic & CRASH_SENTINEL_MASK) != CRASH_SENTINEL || (magic & CRASH_VERSION_MASK) != CRASH_VERSION)
    return 0;
  uint8_t raw_count = magic & CRASH_COUNT_MASK;
  if (raw_count > MAX_BACKTRACE)
    raw_count = MAX_BACKTRACE;
  // Skip any that match epc1 (already reported as the fault PC).
  // Note: we cannot verify CALL instructions at addr-3 on ESP8266 because
  // reading from IROM causes LoadStoreError due to flash cache conflicts
  // (the reading code and target can share a direct-mapped cache line).
  // The linker-symbol IROM bounds already eliminate most false positives.
  uint8_t out = 0;
  for (uint8_t i = 0; i < raw_count && out < max_entries; i++) {
    uint32_t addr = rtc_data.backtrace[i];
    if (addr != rtc_data.epc1)
      backtrace[out++] = addr;
  }
  return out;
}

// Intentionally uses separate ESP_LOGE calls per line instead of combining into
// one multi-line log message. This ensures each address appears as its own line
// on the serial console, making it possible to see partial output if the device
// crashes again during boot, and allowing the CLI's process_stacktrace to match
// and decode each address individually.
void crash_handler_log() {
  if (!is_crash_reason(resetInfo.reason))
    return;

  // Read and filter backtrace from RTC into stack-local buffer (no persistent RAM cost).
  // Both resetInfo and RTC data survive until the next reset, so this can be
  // called multiple times (logger init + API subscribe) with the same result.
  uint32_t backtrace[MAX_BACKTRACE];
  uint8_t bt_count = read_rtc_backtrace(backtrace, MAX_BACKTRACE);

  ESP_LOGE(TAG, "*** CRASH DETECTED ON PREVIOUS BOOT ***");
  // GCC's ROM divide routine triggers IllegalInstruction (exccause=0) at specific
  // ROM addresses instead of IntegerDivideByZero (exccause=6). Patch to match
  // the Arduino core's postmortem handler behavior.
  static constexpr uint32_t EXCCAUSE_ILLEGAL_INSTRUCTION = 0;
  static constexpr uint32_t EXCCAUSE_INTEGER_DIVIDE_BY_ZERO = 6;
  static constexpr uint32_t ROM_DIV_ZERO_ADDR_1 = 0x4000dce5;
  static constexpr uint32_t ROM_DIV_ZERO_ADDR_2 = 0x4000dd3d;
  uint32_t exccause = resetInfo.exccause;
  if (exccause == EXCCAUSE_ILLEGAL_INSTRUCTION &&
      (resetInfo.epc1 == ROM_DIV_ZERO_ADDR_1 || resetInfo.epc1 == ROM_DIV_ZERO_ADDR_2)) {
    exccause = EXCCAUSE_INTEGER_DIVIDE_BY_ZERO;
  }
  const LogString *cause = get_exception_cause(exccause);
  if (cause != nullptr) {
    ESP_LOGE(TAG, "  Reason: %s - %s (exccause=%" PRIu32 ")", LOG_STR_ARG(get_reset_reason(resetInfo.reason)),
             LOG_STR_ARG(cause), exccause);
  } else {
    ESP_LOGE(TAG, "  Reason: %s (exccause=%" PRIu32 ")", LOG_STR_ARG(get_reset_reason(resetInfo.reason)), exccause);
  }
  ESP_LOGE(TAG, "  PC: 0x%08" PRIX32, resetInfo.epc1);
  if (resetInfo.reason == REASON_EXCEPTION_RST) {
    ESP_LOGE(TAG, "  EXCVADDR: 0x%08" PRIX32, resetInfo.excvaddr);
  }
  for (uint8_t i = 0; i < bt_count; i++) {
    ESP_LOGE(TAG, "  BT%d: 0x%08" PRIX32, i, backtrace[i]);
  }
}

}  // namespace esphome::esp8266

// --- Custom crash callback ---
// Overrides the weak custom_crash_callback() from Arduino core's
// core_esp8266_postmortem.cpp. Called during exception handling before
// the device restarts. We scan the full stack for code addresses and store
// them in RTC user memory (which survives software reset).
extern "C" void IRAM_ATTR custom_crash_callback(struct rst_info *rst_info, uint32_t stack, uint32_t stack_end) {
  // No zero-init — only magic, epc1, and backtrace[0..count-1] are read.
  // Saves the IRAM cost of a 72-byte zero-init loop.
  RtcCrashData data;  // NOLINT(cppcoreguidelines-pro-type-member-init)
  uint8_t count = 0;

  // Stack pointer from the Xtensa exception frame is always 4-byte aligned.
  auto *scan = (uint32_t *) stack;     // NOLINT(performance-no-int-to-ptr)
  auto *end = (uint32_t *) stack_end;  // NOLINT(performance-no-int-to-ptr)
  uint32_t epc1 = rst_info->epc1;

  for (; scan < end && count < MAX_BACKTRACE; scan++) {
    uint32_t val = *scan;
    if (is_code_addr(val)) {
      uint32_t addr = recover_code_addr(val);
      // Skip epc1 — already reported as the fault PC
      if (addr != epc1)
        data.backtrace[count++] = addr;
    }
  }

  data.epc1 = epc1;
  data.magic = CRASH_SENTINEL | CRASH_VERSION | count;

  system_rtc_mem_write(RTC_CRASH_BASE, &data, sizeof(data));
}

#endif  // USE_ESP8266_CRASH_HANDLER
#endif  // USE_ESP8266
