#ifdef USE_ESP32

#include "crash_handler.h"
#include "esphome/core/log.h"

#include <cinttypes>
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
static constexpr size_t MAX_BACKTRACE = 8;

// Check if an address looks like code (flash-mapped or IRAM).
// Must be safe to call from panic context (no flash access needed).
static inline bool IRAM_ATTR is_code_addr(uint32_t addr) {
  return (addr >= SOC_IROM_LOW && addr < SOC_IROM_HIGH) || (addr >= SOC_IRAM_LOW && addr < SOC_IRAM_HIGH);
}

// Raw crash data written by the panic handler wrapper.
// Lives in .noinit so it survives software reset.
// Defined at file scope (outside any namespace) because both the namespace
// functions and the extern "C" panic handler wrapper need to access it.
struct RawCrashData {
  uint32_t magic;
  uint32_t pc;
  uint32_t backtrace[MAX_BACKTRACE];
  uint8_t backtrace_count;
};
extern RawCrashData s_raw_crash_data;

namespace esphome::esp32 {

static const char *const TAG = "esp32.crash";

// Validated crash data — populated by crash_handler_read_and_clear() from the
// raw NOINIT data written by the panic handler wrapper.
static struct {
  bool valid;
  uint32_t pc;
  uint32_t backtrace[MAX_BACKTRACE];
  uint8_t backtrace_count;
} s_crash_data;

void crash_handler_read_and_clear() {
  s_crash_data.valid = false;
  if (s_raw_crash_data.magic == CRASH_MAGIC) {
    s_crash_data.valid = true;
    s_crash_data.pc = s_raw_crash_data.pc;
    s_crash_data.backtrace_count = s_raw_crash_data.backtrace_count;
    if (s_crash_data.backtrace_count > MAX_BACKTRACE)
      s_crash_data.backtrace_count = MAX_BACKTRACE;
    for (uint8_t i = 0; i < s_crash_data.backtrace_count; i++) {
      s_crash_data.backtrace[i] = s_raw_crash_data.backtrace[i];
    }
  }
  // Clear magic regardless so we don't re-report on next normal reboot
  s_raw_crash_data.magic = 0;
}

bool crash_handler_has_data() { return s_crash_data.valid; }

// Intentionally uses separate ESP_LOGE calls per line instead of combining into
// one multi-line log message. This ensures each address appears as its own line
// on the serial console, making it possible to see partial output if the device
// crashes again during boot, and allowing the CLI's process_stacktrace to match
// and decode each address individually.
void crash_handler_log() {
  if (!s_crash_data.valid)
    return;

  ESP_LOGE(TAG, "*** CRASH DETECTED ON PREVIOUS BOOT ***");
  ESP_LOGE(TAG, "  PC:  0x%08" PRIX32 "  (fault location)", s_crash_data.pc);
  for (uint8_t i = 0; i < s_crash_data.backtrace_count; i++) {
    ESP_LOGE(TAG, "  BT%d: 0x%08" PRIX32 "  (backtrace)", i, s_crash_data.backtrace[i]);
  }
  // Build addr2line hint with all captured addresses for easy copy-paste
  char hint[256];
  int pos = snprintf(hint, sizeof(hint), "Use: addr2line -pfiaC -e firmware.elf 0x%08" PRIX32, s_crash_data.pc);
  for (uint8_t i = 0; i < s_crash_data.backtrace_count && pos < (int) sizeof(hint) - 12; i++) {
    pos += snprintf(hint + pos, sizeof(hint) - pos, " 0x%08" PRIX32, s_crash_data.backtrace[i]);
  }
  ESP_LOGE(TAG, "%s", hint);
}

}  // namespace esphome::esp32

// --- Panic handler wrapper ---
// Intercepts esp_panic_handler() via --wrap linker flag to capture crash data
// into NOINIT memory before the normal panic handler runs.
//
// The raw crash data struct must be separate from the read-side struct to avoid
// BSS initialization conflicts. It lives in .noinit so it survives software reset.

RawCrashData __attribute__((section(".noinit"))) s_raw_crash_data;

extern "C" {
extern void __real_esp_panic_handler(panic_info_t *info);

void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t *info) {
  // Save the faulting PC
  s_raw_crash_data.pc = (uint32_t) info->addr;
  s_raw_crash_data.backtrace_count = 0;

#if CONFIG_IDF_TARGET_ARCH_XTENSA
  // Xtensa: walk the backtrace using the public API
  if (info->frame != nullptr) {
    auto *xt_frame = (XtExcFrame *) info->frame;
    esp_backtrace_frame_t bt_frame = {
        .pc = (uint32_t) xt_frame->pc,
        .sp = (uint32_t) xt_frame->a1,
        .next_pc = (uint32_t) xt_frame->a0,
        .exc_frame = xt_frame,
    };

    uint8_t count = 0;
    // First frame PC
    if (is_code_addr(esp_cpu_process_stack_pc(bt_frame.pc))) {
      s_raw_crash_data.backtrace[count++] = esp_cpu_process_stack_pc(bt_frame.pc);
    }
    // Walk remaining frames
    while (count < MAX_BACKTRACE && bt_frame.next_pc != 0) {
      if (!esp_backtrace_get_next_frame(&bt_frame)) {
        break;
      }
      uint32_t pc = esp_cpu_process_stack_pc(bt_frame.pc);
      if (is_code_addr(pc)) {
        s_raw_crash_data.backtrace[count++] = pc;
      }
    }
    s_raw_crash_data.backtrace_count = count;
  }

#elif CONFIG_IDF_TARGET_ARCH_RISCV
  // RISC-V: capture MEPC + RA, then scan stack for code addresses
  if (info->frame != nullptr) {
    auto *rv_frame = (RvExcFrame *) info->frame;
    uint8_t count = 0;

    // Save MEPC (fault PC) and RA (return address)
    if (is_code_addr(rv_frame->mepc)) {
      s_raw_crash_data.backtrace[count++] = rv_frame->mepc;
    }
    if (is_code_addr(rv_frame->ra) && rv_frame->ra != rv_frame->mepc) {
      s_raw_crash_data.backtrace[count++] = rv_frame->ra;
    }

    // Scan stack for additional code addresses (like RP2040 approach)
    auto *scan_start = (uint32_t *) rv_frame->sp;
    for (uint32_t i = 0; i < 64 && count < MAX_BACKTRACE; i++) {
      uint32_t val = scan_start[i];
      if (is_code_addr(val) && val != rv_frame->mepc && val != rv_frame->ra) {
        s_raw_crash_data.backtrace[count++] = val;
      }
    }
    s_raw_crash_data.backtrace_count = count;
  }
#endif

  // Write magic last — ensures all data is written before we mark it valid
  s_raw_crash_data.magic = CRASH_MAGIC;

  // Call the real panic handler (prints to UART, does core dump, reboots, etc.)
  __real_esp_panic_handler(info);
}

}  // extern "C"

#endif  // USE_ESP32
