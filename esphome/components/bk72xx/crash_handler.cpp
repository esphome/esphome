#ifdef USE_BK72XX

#include "esphome/core/defines.h"
#ifdef USE_BK72XX_CRASH_HANDLER

#include "crash_handler.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdint>
#include <cstring>

// ARM968E-S register snapshot passed to the SDK trap handlers (matches the
// layout used by bk_show_register() in LibreTiny's intc.c fixup).
extern "C" struct arm_registers {
  uint32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10;
  uint32_t fp;
  uint32_t ip;
  uint32_t sp;
  uint32_t lr;
  uint32_t pc;
  uint32_t spsr;
  uint32_t cpsr;
};

static constexpr uint32_t CRASH_MAGIC = 0xDEADBEEF;
static constexpr uint32_t CRASH_DATA_VERSION = 1;
static constexpr size_t MAX_BACKTRACE = 16;

// BK72XX exception types (matches the order in which we wrap the SDK traps).
enum CrashException : uint8_t {
  CRASH_EXC_NONE = 0,
  CRASH_EXC_UNDEF = 1,  // Undefined instruction
  CRASH_EXC_PABT = 2,   // Prefetch abort
  CRASH_EXC_DABT = 3,   // Data abort
};

// Persistent crash record. Lives in .noinit so it survives the SDK's watchdog
// reset that follows bk_cpu_shutdown(). Validated by magic + version on the
// next boot.
struct CrashData {
  uint32_t magic;
  uint32_t version;
  uint32_t pc;
  uint32_t lr;
  uint32_t sp;
  uint32_t cpsr;
  uint8_t exception;
  uint8_t backtrace_count;
  uint16_t reserved;
  uint32_t backtrace[MAX_BACKTRACE];
};

// Placed in .noinit so it survives the watchdog reset that follows
// bk_cpu_shutdown(). The libretiny linker fragment (patch_bk72xx_noinit.py)
// inserts a .noinit section between .bss and _empty_ram so this region is
// not zeroed by the C runtime startup.
static CrashData s_raw_crash_data __attribute__((section(".noinit"), used));

// Whether crash data was found and validated this boot. Lives in .bss
// (zero-initialized at startup) — set by crash_handler_read_and_clear().
static bool s_crash_data_valid = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// BK72XX flash code is mapped at the chip-specific BKOFFSET_APP. Real code
// addresses always live above 0x00000000 in the flash region; SRAM ends at
// BK72XX_RAM_END. We accept any address in the flash window (broad bound)
// and reject obvious non-code values.
static constexpr uint32_t BK72XX_FLASH_START = 0x00010000;  // BKRBL header end (~min app offset)
static constexpr uint32_t BK72XX_FLASH_END = 0x00200000;    // 2MB cap (largest typical flash)

static inline bool is_code_addr(uint32_t addr) {
  // ARM968 instructions are 4-byte aligned; reject obviously bogus values.
  if ((addr & 0x3) != 0)
    return false;
  return addr >= BK72XX_FLASH_START && addr < BK72XX_FLASH_END;
}

// SRAM bounds for stack-scan validity. RAM origin is 0x00400000 across all
// BK72XX variants; the linker reserves the first 0x100 bytes for the ARM
// exception vector slots (see ORIGIN = 0x00400100 in bk7231{,n}_bsp.template.ld).
// Total RAM differs by variant: 192KB on BK7231N, 256KB on every other BK72XX.
// This split mirrors the SDK's own lt_heap_get_size() in
// libretiny/cores/beken-72xx/base/api/lt_mem.c — keep the values in sync if
// LibreTiny ever adjusts them.
static constexpr uint32_t BK72XX_RAM_BASE = 0x00400000;
#ifdef USE_LIBRETINY_VARIANT_BK7231N
static constexpr uint32_t BK72XX_RAM_SIZE = 192 * 1024;
#else
static constexpr uint32_t BK72XX_RAM_SIZE = 256 * 1024;
#endif
static constexpr uint32_t BK72XX_RAM_START = BK72XX_RAM_BASE + 0x100;
static constexpr uint32_t BK72XX_RAM_END = BK72XX_RAM_BASE + BK72XX_RAM_SIZE;

static inline bool is_valid_stack_ptr(uint32_t sp) {
  return (sp & 0x3) == 0 && sp >= BK72XX_RAM_START && sp < BK72XX_RAM_END;
}

// Walk the stack starting at `sp` and capture up to `max` code-looking
// 32-bit values into `out`. Skips entries equal to the fault PC so it isn't
// reported twice. Returns the number of entries captured.
static uint8_t scan_backtrace(uint32_t sp, uint32_t pc, uint32_t *out, uint8_t max) {
  if (!is_valid_stack_ptr(sp))
    return 0;
  uint8_t count = 0;
  // Limit the scan to 256 words (1KB) — covers typical nested call frames
  // without dredging up too many stale stack values.
  const auto *scan = reinterpret_cast<const uint32_t *>(sp);
  const auto *end = reinterpret_cast<const uint32_t *>(BK72XX_RAM_END);
  const uint32_t *limit = scan + 256;
  if (limit > end)
    limit = end;
  for (; scan < limit && count < max; scan++) {
    uint32_t val = *scan;
    if (is_code_addr(val) && val != pc) {
      out[count++] = val;
    }
  }
  return count;
}

namespace esphome::bk72xx {

static const char *const TAG = "bk72xx.crash";

void crash_handler_read_and_clear() {
  if (s_raw_crash_data.magic == CRASH_MAGIC && s_raw_crash_data.version == CRASH_DATA_VERSION) {
    s_crash_data_valid = true;
    if (s_raw_crash_data.backtrace_count > MAX_BACKTRACE)
      s_raw_crash_data.backtrace_count = MAX_BACKTRACE;
    if (s_raw_crash_data.exception > CRASH_EXC_DABT)
      s_raw_crash_data.exception = CRASH_EXC_NONE;
  }
  // Clear magic so the next boot doesn't replay this crash. s_crash_data_valid
  // remains true so additional API clients connecting in this boot session
  // still see the log via crash_handler_log().
  s_raw_crash_data.magic = 0;
}

bool crash_handler_has_data() { return s_crash_data_valid; }

static const char *get_exception_name(uint8_t exc) {
  switch (exc) {
    case CRASH_EXC_UNDEF:
      return "Undefined instruction";
    case CRASH_EXC_PABT:
      return "Prefetch abort";
    case CRASH_EXC_DABT:
      return "Data abort";
    default:
      return "Unknown";
  }
}

// Intentionally uses separate ESP_LOGE calls per line instead of combining into
// one multi-line log message. This ensures each address appears as its own line
// on the serial console, making it possible to see partial output if the device
// crashes again during boot, and allowing the CLI's process_stacktrace to match
// and decode each address individually.
void crash_handler_log() {
  if (!s_crash_data_valid)
    return;

  ESP_LOGE(TAG, "*** CRASH DETECTED ON PREVIOUS BOOT ***");
  ESP_LOGE(TAG, "  Reason: %s", get_exception_name(s_raw_crash_data.exception));
  ESP_LOGE(TAG, "  PC:  0x%08" PRIX32 "  (fault location)", s_raw_crash_data.pc);
  ESP_LOGE(TAG, "  LR:  0x%08" PRIX32 "  (return address)", s_raw_crash_data.lr);
  ESP_LOGE(TAG, "  SP:  0x%08" PRIX32, s_raw_crash_data.sp);
  ESP_LOGE(TAG, "  CPSR:0x%08" PRIX32, s_raw_crash_data.cpsr);
  for (uint8_t i = 0; i < s_raw_crash_data.backtrace_count; i++) {
    ESP_LOGE(TAG, "  BT%d: 0x%08" PRIX32 "  (stack scan)", i, s_raw_crash_data.backtrace[i]);
  }
  // Build addr2line hint with all captured addresses for easy copy-paste.
  char hint[160];
  int pos = snprintf(hint, sizeof(hint), "Use: addr2line -pfiaC -e firmware.elf 0x%08" PRIX32 " 0x%08" PRIX32,
                     s_raw_crash_data.pc, s_raw_crash_data.lr);
  for (uint8_t i = 0; i < s_raw_crash_data.backtrace_count && pos < (int) sizeof(hint) - 12; i++) {
    pos += snprintf(hint + pos, sizeof(hint) - pos, " 0x%08" PRIX32, s_raw_crash_data.backtrace[i]);
  }
  ESP_LOGE(TAG, "%s", hint);
}

}  // namespace esphome::bk72xx

// --- BK72XX trap-handler wrappers ---
// LibreTiny's fixup intc.c defines bk_trap_udef/pabt/dabt; the closed-source
// Beken SDK calls them from do_undefined/pabort/dabort after pushing a full
// arm_registers frame. We intercept the call via -Wl,--wrap so the original
// behavior (register dump on UART + bk_cpu_shutdown → watchdog reset) still
// runs after we've snapshotted the registers and a stack-scan backtrace.
extern "C" {
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern void __real_bk_trap_udef(struct arm_registers *regs);
extern void __real_bk_trap_pabt(struct arm_registers *regs);
extern void __real_bk_trap_dabt(struct arm_registers *regs);

static void capture_crash(uint8_t exception, struct arm_registers *regs) {
  s_raw_crash_data.pc = regs->pc;
  s_raw_crash_data.lr = regs->lr;
  s_raw_crash_data.sp = regs->sp;
  s_raw_crash_data.cpsr = regs->cpsr;
  s_raw_crash_data.exception = exception;
  s_raw_crash_data.backtrace_count = scan_backtrace(regs->sp, regs->pc, s_raw_crash_data.backtrace, MAX_BACKTRACE);
  // Write version + magic last so a partial write is invalidated.
  s_raw_crash_data.version = CRASH_DATA_VERSION;
  s_raw_crash_data.magic = CRASH_MAGIC;
}

void __wrap_bk_trap_udef(struct arm_registers *regs) {
  capture_crash(CRASH_EXC_UNDEF, regs);
  __real_bk_trap_udef(regs);
}

void __wrap_bk_trap_pabt(struct arm_registers *regs) {
  capture_crash(CRASH_EXC_PABT, regs);
  __real_bk_trap_pabt(regs);
}

void __wrap_bk_trap_dabt(struct arm_registers *regs) {
  capture_crash(CRASH_EXC_DABT, regs);
  __real_bk_trap_dabt(regs);
}
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
}  // extern "C"

#endif  // USE_BK72XX_CRASH_HANDLER
#endif  // USE_BK72XX
