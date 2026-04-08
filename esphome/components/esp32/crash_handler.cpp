#ifdef USE_ESP32

#include "esphome/core/defines.h"
#ifdef USE_ESP32_CRASH_HANDLER

#include "crash_handler.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstring>
#include <esp_attr.h>
#include <esp_private/panic_internal.h>
#include <soc/soc.h>

#if CONFIG_IDF_TARGET_ARCH_XTENSA
#include <esp_cpu_utils.h>
#include <esp_debug_helpers.h>
#include <xtensa_context.h>
#elif CONFIG_IDF_TARGET_ARCH_RISCV
#include <riscv/rvruntime-frames.h>
#endif

static constexpr uint32_t CRASH_MAGIC = 0xDEADBEEF;
static constexpr size_t MAX_BACKTRACE = 16;

// Check if an address looks like code (flash-mapped or IRAM).
// Must be safe to call from panic context (no flash access needed).
static inline bool IRAM_ATTR is_code_addr(uint32_t addr) {
  return (addr >= SOC_IROM_LOW && addr < SOC_IROM_HIGH) || (addr >= SOC_IRAM_LOW && addr < SOC_IRAM_HIGH);
}

#if CONFIG_IDF_TARGET_ARCH_RISCV
// Check if a code address is a real return address by verifying the preceding
// instruction is a JAL or JALR with rd=ra (x1). Called at log time (not during
// panic) so flash cache is available and both IRAM and IROM are safely readable.
static inline bool is_return_addr(uint32_t addr) {
  if (!is_code_addr(addr) || addr < 4)
    return false;
  // A return address on the stack points to the instruction after a call.
  // Check for 4-byte JAL/JALR call instruction before this address.
  // Use memcpy for alignment safety — RISC-V C extension means code addresses
  // are only 2-byte aligned, so addr-4 may not be 4-byte aligned.
  uint32_t inst;
  memcpy(&inst, (const void *) (addr - 4), sizeof(inst));
  // RISC-V instruction encoding: bits [6:0] = opcode, bits [11:7] = rd
  uint32_t opcode = inst & 0x7f;  // Extract 7-bit opcode
  uint32_t rd = inst & 0xf80;     // Extract rd field (bits 11:7)
  // Match JAL (0x6f) or JALR (0x67) with rd=ra (x1, encoded as 0x80 = 1<<7)
  if ((opcode == 0x6f || opcode == 0x67) && rd == 0x80)
    return true;
  // Check for 2-byte compressed c.jalr before this address (C extension).
  // c.jalr saves to ra implicitly: funct4=1001, rs1!=0, rs2=0, op=10
  if (addr >= 2) {
    uint16_t c_inst = *(uint16_t *) (addr - 2);
    if ((c_inst & 0xf07f) == 0x9002 && (c_inst & 0x0f80) != 0)
      return true;
  }
  return false;
}
#endif

// --- Architecture-specific backtrace helpers ---
// These run from IRAM during panic (no flash access).

#if CONFIG_IDF_TARGET_ARCH_XTENSA
// Walk Xtensa backtrace from an exception frame, writing PCs to out[].
// Returns number of entries written.
static uint8_t IRAM_ATTR walk_xtensa_backtrace(XtExcFrame *frame, uint32_t *out, uint8_t max) {
  esp_backtrace_frame_t bt_frame = {
      .pc = (uint32_t) frame->pc,
      .sp = (uint32_t) frame->a1,
      .next_pc = (uint32_t) frame->a0,
      .exc_frame = frame,
  };
  uint8_t count = 0;
  uint32_t first_pc = esp_cpu_process_stack_pc(bt_frame.pc);
  if (is_code_addr(first_pc)) {
    out[count++] = first_pc;
  }
  while (count < max && bt_frame.next_pc != 0) {
    if (!esp_backtrace_get_next_frame(&bt_frame))
      break;
    uint32_t pc = esp_cpu_process_stack_pc(bt_frame.pc);
    if (is_code_addr(pc)) {
      out[count++] = pc;
    }
  }
  return count;
}
#endif

#if CONFIG_IDF_TARGET_ARCH_RISCV
// Capture RISC-V backtrace: MEPC + RA from registers, then stack scan.
// Returns total count; *reg_count receives number of register-sourced entries.
static uint8_t IRAM_ATTR capture_riscv_backtrace(RvExcFrame *frame, uint32_t *out, uint8_t max, uint8_t *reg_count) {
  uint8_t count = 0;
  if (is_code_addr(frame->mepc)) {
    out[count++] = frame->mepc;
  }
  if (is_code_addr(frame->ra) && frame->ra != frame->mepc) {
    out[count++] = frame->ra;
  }
  *reg_count = count;
  auto *scan_start = (uint32_t *) frame->sp;
  for (uint32_t i = 0; i < 64 && count < max; i++) {
    uint32_t val = scan_start[i];
    if (is_code_addr(val) && val != frame->mepc && val != frame->ra) {
      out[count++] = val;
    }
  }
  return count;
}
#endif

// Raw crash data written by the panic handler wrapper.
// Lives in .noinit so it survives software reset but contains garbage after power cycle.
// Validated by magic marker. Static linkage since it's only used within this file.
// Version field is first so future firmware can always identify the struct layout.
// Magic is second to validate the data. Remaining fields can change between versions.
// Version is uint32_t because it would be padded to 4 bytes anyway before the next
// uint32_t field, so we use the full width rather than wasting 3 bytes of padding.
static constexpr uint32_t CRASH_DATA_VERSION = 2;
struct RawCrashData {
  uint32_t version;
  uint32_t magic;
  uint32_t pc;
  uint8_t backtrace_count;
  uint8_t reg_frame_count;  // Number of entries from registers (not stack-scanned)
  uint8_t exception;        // panic_exception_t enum (FAULT/ABORT/IWDT/TWDT/DEBUG)
  uint8_t pseudo_excause;   // Whether cause is a pseudo exception (Xtensa SoC-level panic)
  uint32_t backtrace[MAX_BACKTRACE];
  uint32_t cause;  // Architecture-specific: exccause (Xtensa) or mcause (RISC-V)
  uint8_t crashed_core;
#if SOC_CPU_CORES_NUM > 1
  static_assert(SOC_CPU_CORES_NUM == 2, "Dual-core logic assumes exactly 2 cores");
  uint8_t other_backtrace_count;
  uint8_t other_reg_frame_count;
  uint32_t other_backtrace[MAX_BACKTRACE];
#endif
};
static RawCrashData __attribute__((section(".noinit")))
s_raw_crash_data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Whether crash data was found and validated this boot.
static bool s_crash_data_valid = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace esphome::esp32 {

static const char *const TAG = "esp32.crash";

void crash_handler_read_and_clear() {
  if (s_raw_crash_data.magic == CRASH_MAGIC && s_raw_crash_data.version == CRASH_DATA_VERSION) {
    s_crash_data_valid = true;
    // Clamp counts to prevent out-of-bounds reads from corrupt .noinit data
    if (s_raw_crash_data.backtrace_count > MAX_BACKTRACE)
      s_raw_crash_data.backtrace_count = MAX_BACKTRACE;
    if (s_raw_crash_data.reg_frame_count > s_raw_crash_data.backtrace_count)
      s_raw_crash_data.reg_frame_count = s_raw_crash_data.backtrace_count;
    if (s_raw_crash_data.exception > 4)  // panic_exception_t max value
      s_raw_crash_data.exception = 4;    // Default to PANIC_EXCEPTION_FAULT
    if (s_raw_crash_data.pseudo_excause > 1)
      s_raw_crash_data.pseudo_excause = 0;
    if (s_raw_crash_data.crashed_core >= SOC_CPU_CORES_NUM)
      s_raw_crash_data.crashed_core = 0;
#if SOC_CPU_CORES_NUM > 1
    if (s_raw_crash_data.other_backtrace_count > MAX_BACKTRACE)
      s_raw_crash_data.other_backtrace_count = MAX_BACKTRACE;
    if (s_raw_crash_data.other_reg_frame_count > s_raw_crash_data.other_backtrace_count)
      s_raw_crash_data.other_reg_frame_count = s_raw_crash_data.other_backtrace_count;
#endif
  }
  // Don't clear magic here — crash data must survive OTA rollback reboots.
  // Magic is cleared by crash_handler_clear() after an API client receives the data.
}

bool crash_handler_has_data() { return s_crash_data_valid; }

void crash_handler_clear() {
  // Only clear the magic so data doesn't survive the next reboot.
  // Keep s_crash_data_valid so crash_handler_log() still works for
  // additional API clients connecting during this boot session.
  s_raw_crash_data.magic = 0;
}

// Look up the exception cause as a human-readable string.
// Tables mirror ESP-IDF's panic_arch_fill_info() which uses local static arrays
// not exposed via any public API.
static const char *get_exception_reason() {
#if CONFIG_IDF_TARGET_ARCH_XTENSA
  if (s_raw_crash_data.pseudo_excause) {
    // SoC-level panic: watchdog, cache error, etc.
    // Keep in sync with ESP-IDF's PANIC_RSN_* defines
    static const char *const PSEUDO_REASON[] = {
        "Unknown reason",                 // 0
        "Unhandled debug exception",      // 1
        "Double exception",               // 2
        "Unhandled kernel exception",     // 3
        "Coprocessor exception",          // 4
        "Interrupt wdt timeout on CPU0",  // 5
        "Interrupt wdt timeout on CPU1",  // 6
        "Cache error",                    // 7
    };
    uint32_t cause = s_raw_crash_data.cause;
    if (cause < sizeof(PSEUDO_REASON) / sizeof(PSEUDO_REASON[0]))
      return PSEUDO_REASON[cause];
    return PSEUDO_REASON[0];
  }
  // Real Xtensa exception
  static const char *const REASON[] = {
      "IllegalInstruction",
      "Syscall",
      "InstructionFetchError",
      "LoadStoreError",
      "Level1Interrupt",
      "Alloca",
      "IntegerDivideByZero",
      "PCValue",
      "Privileged",
      "LoadStoreAlignment",
      nullptr,
      nullptr,
      "InstrPDAddrError",
      "LoadStorePIFDataError",
      "InstrPIFAddrError",
      "LoadStorePIFAddrError",
      "InstTLBMiss",
      "InstTLBMultiHit",
      "InstFetchPrivilege",
      nullptr,
      "InstrFetchProhibited",
      nullptr,
      nullptr,
      nullptr,
      "LoadStoreTLBMiss",
      "LoadStoreTLBMultihit",
      "LoadStorePrivilege",
      nullptr,
      "LoadProhibited",
      "StoreProhibited",
  };
  uint32_t cause = s_raw_crash_data.cause;
  if (cause < sizeof(REASON) / sizeof(REASON[0]) && REASON[cause] != nullptr)
    return REASON[cause];
#elif CONFIG_IDF_TARGET_ARCH_RISCV
  // For SoC-level panics (watchdog, cache error), mcause holds IDF-internal
  // interrupt numbers, not standard RISC-V cause codes. The exception type
  // field already identifies these, so just return null to use the type name.
  if (s_raw_crash_data.pseudo_excause)
    return nullptr;
  static const char *const REASON[] = {
      "Instruction address misaligned",
      "Instruction access fault",
      "Illegal instruction",
      "Breakpoint",
      "Load address misaligned",
      "Load access fault",
      "Store address misaligned",
      "Store access fault",
      "Environment call from U-mode",
      "Environment call from S-mode",
      nullptr,
      "Environment call from M-mode",
      "Instruction page fault",
      "Load page fault",
      nullptr,
      "Store page fault",
  };
  uint32_t cause = s_raw_crash_data.cause;
  if (cause < sizeof(REASON) / sizeof(REASON[0]) && REASON[cause] != nullptr)
    return REASON[cause];
#endif
  return "Unknown";
}

// Exception type names matching panic_exception_t enum
static const char *get_exception_type() {
  static const char *const TYPES[] = {
      "Debug exception",  // PANIC_EXCEPTION_DEBUG
      "Interrupt wdt",    // PANIC_EXCEPTION_IWDT
      "Task wdt",         // PANIC_EXCEPTION_TWDT
      "Abort",            // PANIC_EXCEPTION_ABORT
      "Fault",            // PANIC_EXCEPTION_FAULT
  };
  uint8_t exc = s_raw_crash_data.exception;
  if (exc < sizeof(TYPES) / sizeof(TYPES[0]))
    return TYPES[exc];
  return "Unknown";
}

// Log backtrace entries, filtering stack-scanned addresses on RISC-V.
static void log_backtrace(const uint32_t *addrs, uint8_t count, uint8_t reg_frame_count) {
  uint8_t bt_num = 0;
  for (uint8_t i = 0; i < count; i++) {
    uint32_t addr = addrs[i];
#if CONFIG_IDF_TARGET_ARCH_RISCV
    if (i >= reg_frame_count && !is_return_addr(addr))
      continue;
    const char *source = (i < reg_frame_count) ? "backtrace" : "stack scan";
#else
    const char *source = "backtrace";
#endif
    ESP_LOGE(TAG, "  BT%d: 0x%08" PRIX32 "  (%s)", bt_num++, addr, source);
  }
}

// Append backtrace addresses to the addr2line hint buffer.
static int append_addrs_to_hint(char *buf, int size, int pos, const uint32_t *addrs, uint8_t count,
                                uint8_t reg_frame_count) {
  for (uint8_t i = 0; i < count && pos < size - 12; i++) {
    uint32_t addr = addrs[i];
#if CONFIG_IDF_TARGET_ARCH_RISCV
    if (i >= reg_frame_count && !is_return_addr(addr))
      continue;
#endif
    pos += snprintf(buf + pos, size - pos, " 0x%08" PRIX32, addr);
  }
  return pos;
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
  const char *reason = get_exception_reason();
  if (reason != nullptr) {
    ESP_LOGE(TAG, "  Reason: %s - %s", get_exception_type(), reason);
  } else {
    ESP_LOGE(TAG, "  Reason: %s", get_exception_type());
  }
  ESP_LOGE(TAG, "  Crashed core: %d", s_raw_crash_data.crashed_core);
  ESP_LOGE(TAG, "  PC:  0x%08" PRIX32 "  (fault location)", s_raw_crash_data.pc);
  log_backtrace(s_raw_crash_data.backtrace, s_raw_crash_data.backtrace_count, s_raw_crash_data.reg_frame_count);

#if SOC_CPU_CORES_NUM > 1
  if (s_raw_crash_data.other_backtrace_count > 0) {
    int other_core = 1 - s_raw_crash_data.crashed_core;
    ESP_LOGE(TAG, "  Other core (%d) backtrace:", other_core);
    log_backtrace(s_raw_crash_data.other_backtrace, s_raw_crash_data.other_backtrace_count,
                  s_raw_crash_data.other_reg_frame_count);
  }
#endif

  // Build addr2line hint with all captured addresses for easy copy-paste
  char hint[256];
  int pos = snprintf(hint, sizeof(hint), "Use: addr2line -pfiaC -e firmware.elf 0x%08" PRIX32, s_raw_crash_data.pc);
  pos = append_addrs_to_hint(hint, sizeof(hint), pos, s_raw_crash_data.backtrace, s_raw_crash_data.backtrace_count,
                             s_raw_crash_data.reg_frame_count);
#if SOC_CPU_CORES_NUM > 1
  append_addrs_to_hint(hint, sizeof(hint), pos, s_raw_crash_data.other_backtrace,
                       s_raw_crash_data.other_backtrace_count, s_raw_crash_data.other_reg_frame_count);
#endif
  ESP_LOGE(TAG, "%s", hint);
}

}  // namespace esphome::esp32

// --- Panic handler wrapper ---
// Intercepts esp_panic_handler() via --wrap linker flag to capture crash data
// into NOINIT memory before the normal panic handler runs.
//
extern "C" {
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
// Names are mandated by the --wrap linker mechanism
extern void __real_esp_panic_handler(panic_info_t *info);

void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t *info) {
  // Save the faulting PC and exception info
  s_raw_crash_data.pc = (uint32_t) info->addr;
  s_raw_crash_data.backtrace_count = 0;
  s_raw_crash_data.reg_frame_count = 0;
  s_raw_crash_data.exception = (uint8_t) info->exception;
  s_raw_crash_data.pseudo_excause = info->pseudo_excause ? 1 : 0;
  s_raw_crash_data.crashed_core = (uint8_t) info->core;
#if SOC_CPU_CORES_NUM > 1
  s_raw_crash_data.other_backtrace_count = 0;
  s_raw_crash_data.other_reg_frame_count = 0;
#endif

#if CONFIG_IDF_TARGET_ARCH_XTENSA
  // Xtensa: walk the backtrace using the public API
  if (info->frame != nullptr) {
    auto *xt_frame = (XtExcFrame *) info->frame;
    s_raw_crash_data.cause = xt_frame->exccause;
    s_raw_crash_data.backtrace_count = walk_xtensa_backtrace(xt_frame, s_raw_crash_data.backtrace, MAX_BACKTRACE);
  }

#if SOC_CPU_CORES_NUM > 1
  // Capture the other core's backtrace from the global frame array.
  // Both cores save their frames to g_exc_frames[] before esp_panic_handler
  // is called, so the other core's frame is available here.
  if (info->core >= 0 && info->core < SOC_CPU_CORES_NUM) {
    int other_core = 1 - info->core;
    auto *other_frame = (XtExcFrame *) g_exc_frames[other_core];
    if (other_frame != nullptr) {
      s_raw_crash_data.other_backtrace_count =
          walk_xtensa_backtrace(other_frame, s_raw_crash_data.other_backtrace, MAX_BACKTRACE);
    }
  }
#endif

#elif CONFIG_IDF_TARGET_ARCH_RISCV
  // RISC-V: capture MEPC + RA, then scan stack for code addresses
  if (info->frame != nullptr) {
    auto *rv_frame = (RvExcFrame *) info->frame;
    s_raw_crash_data.cause = rv_frame->mcause;
    s_raw_crash_data.backtrace_count =
        capture_riscv_backtrace(rv_frame, s_raw_crash_data.backtrace, MAX_BACKTRACE, &s_raw_crash_data.reg_frame_count);
  }

#if SOC_CPU_CORES_NUM > 1
  // Capture the other core's backtrace from the global frame array.
  if (info->core >= 0 && info->core < SOC_CPU_CORES_NUM) {
    int other_core = 1 - info->core;
    auto *other_frame = (RvExcFrame *) g_exc_frames[other_core];
    if (other_frame != nullptr) {
      s_raw_crash_data.other_backtrace_count = capture_riscv_backtrace(
          other_frame, s_raw_crash_data.other_backtrace, MAX_BACKTRACE, &s_raw_crash_data.other_reg_frame_count);
    }
  }
#endif
#endif

  // Write version and magic last — ensures all data is written before we mark it valid
  s_raw_crash_data.version = CRASH_DATA_VERSION;
  s_raw_crash_data.magic = CRASH_MAGIC;

  // Call the real panic handler (prints to UART, does core dump, reboots, etc.)
  __real_esp_panic_handler(info);
}

// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
}  // extern "C"

#endif  // USE_ESP32_CRASH_HANDLER
#endif  // USE_ESP32
