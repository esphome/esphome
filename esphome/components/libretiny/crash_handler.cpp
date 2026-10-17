#ifdef USE_BK72XX

#include "esphome/core/defines.h"
#ifdef USE_LIBRETINY_CRASH_HANDLER

#include "crash_handler.h"
#include "core.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>

extern "C" {
#include <intc.h>  // struct arm_registers
}

// The BK72xx SDK (driver/entry/boot_handlers.S) answers an undefined instruction, prefetch abort or
// data abort by building a struct arm_registers frame on the exception mode's stack (PUSH_SVC_REG)
// and branching to bk_trap_udef/pabt/dabt with a pointer to it. The SDK handler records the crash
// type in a retention register, prints a register dump that sdk_silent hides, and spins with
// interrupts off until the hardware watchdog resets the chip about 10 s later. The build wraps those
// three handlers (-Wl,--wrap) so the fault location and a stack scan are saved first, in RAM that
// survives the watchdog reset, and reported on the next boot.
//
// sp, lr and pc in the frame belong to the exception mode. PUSH_SVC_REG stores them at these word
// offsets, so the SDK header must keep describing that frame.
static_assert(sizeof(arm_registers) == 18 * sizeof(uint32_t), "SDK exception frame changed size");
static_assert(offsetof(arm_registers, sp) == 13 * 4 && offsetof(arm_registers, lr) == 14 * 4 &&
                  offsetof(arm_registers, pc) == 15 * 4 && offsetof(arm_registers, spsr) == 16 * 4 &&
                  offsetof(arm_registers, cpsr) == 17 * 4,
              "SDK exception frame layout changed");
static_assert(LT_HW_WATCHDOG, "the crash reboot relies on the hardware watchdog");

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {
void __real_bk_trap_udef(arm_registers *regs);
void __real_bk_trap_pabt(arm_registers *regs);
void __real_bk_trap_dabt(arm_registers *regs);
// Linker symbols bounding the app's flash image (code and rodata) and its ITCM code copy.
extern char _vector_start[];
extern char _data_flash_begin[];
extern char _itcmcode_ram_begin[];
extern char _itcmcode_ram_end[];
}
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

namespace esphome::libretiny {

ESPHOME_LOG_TAG(TAG, "libretiny.crash");

// 0xDEAD sentinel in the upper half, record layout version in the lower half.
static constexpr uint32_t CRASH_MAGIC_V1 = 0xDEAD0001;

static constexpr uint32_t CRASH_REASON_UNDEFINED = 1;
static constexpr uint32_t CRASH_REASON_PREFETCH_ABORT = 2;
static constexpr uint32_t CRASH_REASON_DATA_ABORT = 3;
PROGMEM_STRING_TABLE(CrashReasonStrings, "Unknown", "Undefined instruction", "Prefetch abort", "Data abort");

// The watchdog armed at boot stops firing once Wi-Fi is up (measured on a BK7238 and a BK7231N: any
// spin with interrupts off, including the SDK's trap handler, hangs until power is cycled; the cause
// is not in the open SDK sources), so the wrapper arms it again before the SDK handler spins. Long
// enough for the SDK's register dump when sdk_silent is off.
static constexpr uint32_t CRASH_REBOOT_WDT_MS = 500;

static constexpr size_t MAX_BACKTRACE = 8;
// Words scanned above the faulting context's stack pointer for return addresses.
static constexpr size_t STACK_SCAN_WORDS = 64;

// SRAM range a task stack can be in: 192 KB on the BK7231N, 256 KB on the BK7238.
static constexpr uint32_t SRAM_START = 0x00400000;
#ifdef USE_LIBRETINY_VARIANT_BK7231N
static constexpr uint32_t SRAM_END = 0x00430000;
#else
static constexpr uint32_t SRAM_END = 0x00440000;
#endif

static constexpr uint32_t CPSR_MODE_MASK = 0x1F;
static constexpr uint32_t CPSR_MODE_USR = 0x10;
static constexpr uint32_t CPSR_MODE_FIQ = 0x11;
static constexpr uint32_t CPSR_MODE_IRQ = 0x12;
static constexpr uint32_t CPSR_MODE_SVC = 0x13;
static constexpr uint32_t CPSR_MODE_ABT = 0x17;
static constexpr uint32_t CPSR_MODE_UND = 0x1B;
static constexpr uint32_t CPSR_MODE_SYS = 0x1F;
static constexpr uint32_t CPSR_THUMB = 1u << 5;

struct CrashRecord {
  uint32_t magic;
  uint32_t reason;
  uint32_t pc;
  uint32_t lr;
  uint32_t sp;
  uint32_t backtrace[MAX_BACKTRACE];
};

// Kept in ITCM, see patch_noinit.py.script.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
__attribute__((section(".noinit"))) static CrashRecord s_record;

static bool s_record_read = false;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_record_valid = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool crash_handler_has_data() {
  if (!s_record_read) {
    s_record_read = true;
    s_record_valid = s_record.magic == CRASH_MAGIC_V1;
    // Only the magic is cleared so the data does not count again after the next reboot.
    s_record.magic = 0;
  }
  return s_record_valid;
}

// Separate ESP_LOGE calls per line so each address is its own line on the console, partial output
// survives a crash during boot, and the CLI's process_stacktrace decodes each address individually.
void crash_handler_log() {
  if (!crash_handler_has_data())
    return;

  ESP_LOGE(TAG, "*** CRASH DETECTED ON PREVIOUS BOOT ***");
  ESP_LOGE(TAG, "  Reason: %s", LOG_STR_ARG(CrashReasonStrings::get_log_str(s_record.reason, 0)));
  ESP_LOGE(TAG, "  PC:  0x%08" PRIX32 "  (fault location)", s_record.pc);
  ESP_LOGE(TAG, "  LR:  0x%08" PRIX32 "  (return address)", s_record.lr);
  ESP_LOGE(TAG, "  SP:  0x%08" PRIX32, s_record.sp);
  uint8_t count = 0;
  for (; count < MAX_BACKTRACE && s_record.backtrace[count] != 0; count++) {
    ESP_LOGE(TAG, "  BT%d: 0x%08" PRIX32 "  (stack backtrace)", count, s_record.backtrace[count]);
  }
  // addr2line hint with every captured address for easy copy-paste
  char hint[160];
  int pos = snprintf(hint, sizeof(hint), "Use: addr2line -pfiaC -e raw_firmware.elf 0x%08" PRIX32 " 0x%08" PRIX32,
                     s_record.pc, s_record.lr);
  for (uint8_t i = 0; i < count && pos < (int) sizeof(hint) - 12; i++) {
    pos += snprintf(hint + pos, sizeof(hint) - pos, " 0x%08" PRIX32, s_record.backtrace[i]);
  }
  ESP_LOGE(TAG, "%s", hint);
}

// --- Crash time ---
// Runs in the exception mode (UND or ABT) with IRQ and FIQ masked, on a stack of a few dozen bytes,
// so it keeps locals to registers and does no printing.

static inline bool is_code_addr(uint32_t val) {
  uint32_t addr = val & ~1u;  // clear the Thumb bit of a return address
  return (addr >= (uint32_t) _vector_start && addr < (uint32_t) _data_flash_begin) ||
         (addr >= (uint32_t) _itcmcode_ram_begin && addr < (uint32_t) _itcmcode_ram_end);
}

static inline bool is_banked_mode(uint32_t mode) {
  return mode == CPSR_MODE_SYS || mode == CPSR_MODE_FIQ || mode == CPSR_MODE_IRQ || mode == CPSR_MODE_SVC ||
         mode == CPSR_MODE_ABT || mode == CPSR_MODE_UND;
}

// Read the faulting context's own sp and lr from its register bank. The exception frame only holds
// the exception mode's, so switch to that mode (interrupts stay masked) and back. msr/mrs need ARM
// state, hence target("arm") in a Thumb build. Written in fixed registers: only r0-r3 are shared by
// every mode, so the saved CPSR and the return address must not sit in a banked register such as lr.
__attribute__((target("arm"), naked, noinline)) static void read_banked_sp_lr(uint32_t mode, uint32_t *sp,
                                                                              uint32_t *lr) {
  __asm__ volatile("orr r0, r0, #0xC0\n"  // keep IRQ and FIQ masked in the target mode
                   "mrs r3, cpsr\n"
                   "msr cpsr_c, r0\n"
                   "mov r0, sp\n"
                   "str r0, [r1]\n"
                   "mov r0, lr\n"
                   "str r0, [r2]\n"
                   "msr cpsr_c, r3\n"
                   "bx lr\n");
}

static void record_crash(arm_registers *regs, uint32_t reason, uint32_t pc_offset) {
  uint32_t mode = regs->spsr & CPSR_MODE_MASK;
  if (mode == CPSR_MODE_USR)
    mode = CPSR_MODE_SYS;  // same register bank
  uint32_t sp = regs->sp;
  uint32_t lr = regs->lr;
  if (is_banked_mode(mode))
    read_banked_sp_lr(mode, &sp, &lr);

  s_record.reason = reason;
  s_record.pc = regs->pc - pc_offset;
  s_record.lr = lr;
  s_record.sp = sp;

  // Scan up the faulting context's stack for return addresses to give a deeper backtrace.
  uint32_t count = 0;
  if (sp >= SRAM_START && sp < SRAM_END && (sp & 3) == 0) {
    auto *scan = (const uint32_t *) sp;       // NOLINT(performance-no-int-to-ptr)
    auto *end = (const uint32_t *) SRAM_END;  // NOLINT(performance-no-int-to-ptr)
    for (size_t i = 0; i < STACK_SCAN_WORDS && scan < end && count < MAX_BACKTRACE; i++, scan++) {
      uint32_t val = *scan;
      if (is_code_addr(val) && val != lr && (val & ~1u) != s_record.pc)
        s_record.backtrace[count++] = val;
    }
  }
  for (; count < MAX_BACKTRACE; count++)
    s_record.backtrace[count] = 0;
  s_record.magic = CRASH_MAGIC_V1;

  lt_wdt_enable(CRASH_REBOOT_WDT_MS);
}

// The exception LR points past the faulting instruction: by one instruction for an undefined
// instruction, 4 bytes for a prefetch abort and 8 bytes for a data abort.
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" void __wrap_bk_trap_udef(arm_registers *regs) {
  record_crash(regs, CRASH_REASON_UNDEFINED, (regs->spsr & CPSR_THUMB) ? 2 : 4);
  __real_bk_trap_udef(regs);
}

extern "C" void __wrap_bk_trap_pabt(arm_registers *regs) {
  record_crash(regs, CRASH_REASON_PREFETCH_ABORT, 4);
  __real_bk_trap_pabt(regs);
}

extern "C" void __wrap_bk_trap_dabt(arm_registers *regs) {
  record_crash(regs, CRASH_REASON_DATA_ABORT, 8);
  __real_bk_trap_dabt(regs);
}
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)

}  // namespace esphome::libretiny

#endif  // USE_LIBRETINY_CRASH_HANDLER
#endif  // USE_BK72XX
