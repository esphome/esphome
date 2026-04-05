#ifdef USE_ESP8266

#include "esphome/core/defines.h"
#ifdef USE_ESP8266_CRASH_HANDLER

#include "crash_handler.h"
#include "esphome/core/helpers.h"
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
static constexpr uint32_t IROM_START = 0x40200000;
static constexpr uint32_t IROM_END = 0x40400000;  // 2MB conservative upper bound

// Check if a value looks like a code address in IRAM or flash-mapped IROM.
// Must be IRAM_ATTR since it's called from custom_crash_callback (exception context).
static inline bool IRAM_ATTR is_code_addr(uint32_t val) {
  uint32_t addr = (val & XTENSA_ADDR_MASK) | XTENSA_CODE_BASE;
  return (addr >= IRAM_START && addr < IRAM_END) || (addr >= IROM_START && addr < IROM_END);
}

// Recover the actual code address from a windowed-ABI return address on the stack.
static inline uint32_t IRAM_ATTR recover_code_addr(uint32_t val) { return (val & XTENSA_ADDR_MASK) | XTENSA_CODE_BASE; }

// RTC user memory layout for crash backtrace data.
// User-accessible RTC memory: blocks 64-191 (each block = 4 bytes).
// We use blocks 183-191 (last 9 blocks, 36 bytes) to minimize conflicts.
static constexpr uint8_t RTC_CRASH_BASE = 183;
static constexpr size_t MAX_BACKTRACE = 8;

// Magic word packs sentinel, version, and count into one uint32_t:
//   bits[31:16] = 0xDEAD (sentinel)
//   bits[15:8]  = version (1)
//   bits[7:0]   = backtrace count
static constexpr uint32_t CRASH_SENTINEL = 0xDEAD0000;
static constexpr uint32_t CRASH_VERSION = 0x00000100;  // version 1 in bits[15:8]
static constexpr uint32_t CRASH_SENTINEL_MASK = 0xFFFF0000;
static constexpr uint32_t CRASH_VERSION_MASK = 0x0000FF00;
static constexpr uint32_t CRASH_COUNT_MASK = 0x000000FF;

// Struct layout: 9 RTC blocks (36 bytes):
// [0] = magic (sentinel | version | count)
// [1..8] = up to 8 code addresses from stack scanning
struct RtcCrashData {
  uint32_t magic;
  uint32_t backtrace[MAX_BACKTRACE];
};
static_assert(sizeof(RtcCrashData) == 36, "RtcCrashData must fit in 9 RTC blocks");

namespace esphome::esp8266 {

static const char *const TAG = "esp8266.crash";

// Whether the previous boot was a crash. Set once in crash_handler_read_and_clear().
// resetInfo and RTC backtrace data persist until the next reset, so no caching needed.
static bool s_crash_valid = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool crash_handler_has_data() { return s_crash_valid; }

void crash_handler_read_and_clear() {
  uint32_t reason = resetInfo.reason;
  s_crash_valid = (reason == REASON_WDT_RST || reason == REASON_EXCEPTION_RST || reason == REASON_SOFT_WDT_RST);
}

// Xtensa exception cause names (shared with ESP32, same ISA).
// Keep in sync with Xtensa ISA reference manual Table 4-64.
static const LogString *get_exception_cause(uint32_t cause) {
  switch (cause) {
    case 0:
      return LOG_STR("IllegalInstruction");
    case 1:
      return LOG_STR("Syscall");
    case 2:
      return LOG_STR("InstructionFetchError");
    case 3:
      return LOG_STR("LoadStoreError");
    case 4:
      return LOG_STR("Level1Interrupt");
    case 5:
      return LOG_STR("Alloca");
    case 6:
      return LOG_STR("IntegerDivideByZero");
    case 7:
      return LOG_STR("PCValue");
    case 8:
      return LOG_STR("Privileged");
    case 9:
      return LOG_STR("LoadStoreAlignment");
    case 12:
      return LOG_STR("InstrPDAddrError");
    case 13:
      return LOG_STR("LoadStorePIFDataError");
    case 14:
      return LOG_STR("InstrPIFAddrError");
    case 15:
      return LOG_STR("LoadStorePIFAddrError");
    case 16:
      return LOG_STR("InstTLBMiss");
    case 17:
      return LOG_STR("InstTLBMultiHit");
    case 18:
      return LOG_STR("InstFetchPrivilege");
    case 20:
      return LOG_STR("InstrFetchProhibited");
    case 24:
      return LOG_STR("LoadStoreTLBMiss");
    case 25:
      return LOG_STR("LoadStoreTLBMultihit");
    case 26:
      return LOG_STR("LoadStorePrivilege");
    case 28:
      return LOG_STR("LoadProhibited");
    case 29:
      return LOG_STR("StoreProhibited");
    default:
      return nullptr;
  }
}

static const LogString *get_reset_reason(uint32_t reason) {
  switch (reason) {
    case REASON_WDT_RST:
      return LOG_STR("Hardware Watchdog");
    case REASON_EXCEPTION_RST:
      return LOG_STR("Exception");
    case REASON_SOFT_WDT_RST:
      return LOG_STR("Software Watchdog");
    default:
      return LOG_STR("Unknown");
  }
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
  uint8_t count = magic & CRASH_COUNT_MASK;
  if (count > max_entries)
    count = max_entries;
  for (uint8_t i = 0; i < count; i++) {
    backtrace[i] = rtc_data.backtrace[i];
  }
  return count;
}

// Intentionally uses separate ESP_LOGE calls per line instead of combining into
// one multi-line log message. This ensures each address appears as its own line
// on the serial console, making it possible to see partial output if the device
// crashes again during boot, and allowing the CLI's process_stacktrace to match
// and decode each address individually.
void crash_handler_log() {
  if (!s_crash_valid)
    return;

  // Read backtrace from RTC into stack-local buffer (no persistent RAM cost).
  // Both resetInfo and RTC data survive until the next reset, so this can be
  // called multiple times (logger init + API subscribe) with the same result.
  uint32_t backtrace[MAX_BACKTRACE];
  uint8_t bt_count = read_rtc_backtrace(backtrace, MAX_BACKTRACE);

  ESP_LOGE(TAG, "*** CRASH DETECTED ON PREVIOUS BOOT ***");
  const LogString *cause = get_exception_cause(resetInfo.exccause);
  if (resetInfo.reason == REASON_EXCEPTION_RST && cause != nullptr) {
    ESP_LOGE(TAG, "  Reason: %s - %s (exccause=%" PRIu32 ")", LOG_STR_ARG(get_reset_reason(resetInfo.reason)),
             LOG_STR_ARG(cause), resetInfo.exccause);
  } else {
    ESP_LOGE(TAG, "  Reason: %s", LOG_STR_ARG(get_reset_reason(resetInfo.reason)));
  }
  ESP_LOGE(TAG, "  PC:  0x%08" PRIX32 "  (fault location)", resetInfo.epc1);
  if (resetInfo.epc2 != 0) {
    ESP_LOGE(TAG, "  EPC2: 0x%08" PRIX32, resetInfo.epc2);
  }
  if (resetInfo.epc3 != 0) {
    ESP_LOGE(TAG, "  EPC3: 0x%08" PRIX32, resetInfo.epc3);
  }
  if (resetInfo.excvaddr != 0) {
    ESP_LOGE(TAG, "  EXCVADDR: 0x%08" PRIX32 "  (faulting address)", resetInfo.excvaddr);
  }
  if (resetInfo.depc != 0) {
    ESP_LOGE(TAG, "  DEPC: 0x%08" PRIX32 "  (double exception)", resetInfo.depc);
  }
  for (uint8_t i = 0; i < bt_count; i++) {
    ESP_LOGE(TAG, "  BT%d: 0x%08" PRIX32 "  (stack scan)", i, backtrace[i]);
  }
  // Build addr2line hint with all captured addresses for easy copy-paste
  char hint[200];
  size_t pos =
      buf_append_printf(hint, sizeof(hint), 0, "Use: addr2line -pfiaC -e firmware.elf 0x%08" PRIX32, resetInfo.epc1);
  for (uint8_t i = 0; i < bt_count; i++) {
    pos = buf_append_printf(hint, sizeof(hint), pos, " 0x%08" PRIX32, backtrace[i]);
  }
  ESP_LOGE(TAG, "%s", hint);
}

}  // namespace esphome::esp8266

// --- Custom crash callback ---
// Overrides the weak custom_crash_callback() from Arduino core's
// core_esp8266_postmortem.cpp. Called during exception handling before
// the device restarts. We scan the stack for return addresses and store
// them in RTC user memory (which survives software reset).
extern "C" void IRAM_ATTR custom_crash_callback(struct rst_info * /*rst_info*/, uint32_t stack, uint32_t stack_end) {
  RtcCrashData data = {};
  uint8_t count = 0;

  auto *scan = reinterpret_cast<uint32_t *>(stack);
  auto *end = reinterpret_cast<uint32_t *>(stack_end);
  // Limit scan to 64 words (256 bytes) to avoid excessive scanning
  if (end > scan + 64)
    end = scan + 64;

  for (; scan < end && count < MAX_BACKTRACE; scan++) {
    uint32_t val = *scan;
    if (is_code_addr(val)) {
      data.backtrace[count++] = recover_code_addr(val);
    }
  }

  data.magic = CRASH_SENTINEL | CRASH_VERSION | count;

  system_rtc_mem_write(RTC_CRASH_BASE, &data, sizeof(data));
}

#endif  // USE_ESP8266_CRASH_HANDLER
#endif  // USE_ESP8266
