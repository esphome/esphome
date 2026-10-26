#ifdef USE_LIBRETINY

#include "esphome/core/defines.h"
#ifdef USE_LIBRETINY_CRASH_HANDLER

#if !defined(USE_BK72XX) && !defined(USE_LIBRETINY_VARIANT_RTL8720C)
#error "The LibreTiny crash handler has no fault capture for this chip family"
#endif

#include "crash_handler.h"
#include "core.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#ifdef USE_BK72XX
extern "C" {
#include <arm_mcu_pub.h>  // MCU_REG_BACKUP_SP_*
#include <intc.h>         // struct arm_registers
}

// On a fault the SDK (boot_handlers.S, PUSH_SVC_REG) builds a struct arm_registers frame and branches
// to bk_trap_udef/pabt/dabt, which record the crash type, print a dump sdk_silent hides and spin for
// the watchdog. The wrappers (-Wl,--wrap) first save the fault location and a stack scan for the
// next boot. pc is the exception LR; PUSH_SVC_REG stores pc and spsr at these word offsets.
static_assert(sizeof(arm_registers) == 18 * sizeof(uint32_t), "SDK exception frame changed size");
static_assert(offsetof(arm_registers, pc) == 15 * 4 && offsetof(arm_registers, spsr) == 16 * 4,
              "SDK exception frame layout changed");
static_assert(LT_HW_WATCHDOG, "the crash reboot relies on the hardware watchdog");

// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {
void __real_bk_trap_udef(arm_registers *regs);
void __real_bk_trap_pabt(arm_registers *regs);
void __real_bk_trap_dabt(arm_registers *regs);
// Bounds of the app's flash image and its ITCM code (linker symbols).
extern char _vector_start[];
extern char _data_flash_begin[];
extern char _itcmcode_ram_begin[];
extern char _itcmcode_ram_end[];
}
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
#endif  // USE_BK72XX

#ifdef USE_LIBRETINY_VARIANT_RTL8720C
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
extern "C" {
// Bounds of the code in flash (XIP) and in RAM (linker symbols, rtl8710c_ram.ld).
extern char __xip_code_text_start__[];
extern char __xip_code_text_end__[];
extern char __ram_entry_text_start__[];
extern char __ram_code_text_end__[];
}
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
#endif  // USE_LIBRETINY_VARIANT_RTL8720C

namespace esphome::libretiny {

ESPHOME_LOG_TAG(TAG, "libretiny.crash");

// 0xDEAD sentinel, then the record layout version.
static constexpr uint32_t CRASH_MAGIC_V2 = 0xDEAD0002;

// BK72xx: the ARM9 exception taken.
static constexpr uint32_t CRASH_REASON_UNDEFINED = 1;
static constexpr uint32_t CRASH_REASON_PREFETCH_ABORT = 2;
static constexpr uint32_t CRASH_REASON_DATA_ABORT = 3;
// RTL8720C: the Cortex-M exception number (3 to 7) plus this offset.
static constexpr uint32_t CRASH_REASON_CORTEX_M_OFFSET = 1;
PROGMEM_STRING_TABLE(CrashReasonStrings, "Unknown", "Undefined instruction", "Prefetch abort", "Data abort",
                     "Hard fault", "MemManage fault", "Bus fault", "Usage fault", "Secure fault");

static constexpr size_t MAX_BACKTRACE = 8;
// Words scanned above sp for return addresses.
static constexpr size_t STACK_SCAN_WORDS = 64;

struct CrashRecord {
  uint32_t magic;
  uint32_t reason;
  uint32_t pc;
  uint32_t lr;
  uint32_t sp;
  uint32_t fault_status;   // Cortex-M CFSR, 0 on BK72xx
  uint32_t fault_address;  // Cortex-M BFAR or MMFAR when valid, else 0
  uint32_t backtrace[MAX_BACKTRACE];
};

#ifdef USE_BK72XX
// Kept in ITCM, see patch_noinit.py.script.
#define CRASH_RECORD_SECTION ".noinit"
#else
// rtl8710c_ram.ld keeps .ram.bss.noinit* out of the startup zeroing.
#define CRASH_RECORD_SECTION ".ram.bss.noinit.esphome_crash"
#endif
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
__attribute__((section(CRASH_RECORD_SECTION))) static CrashRecord s_record;

static bool s_record_read = false;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_record_valid = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

bool crash_handler_has_data() {
  if (!s_record_read) {
    s_record_read = true;
    s_record_valid = s_record.magic == CRASH_MAGIC_V2;
    // Clear the magic so the record is reported once.
    s_record.magic = 0;
  }
  return s_record_valid;
}

// One ESP_LOGE per line: the CLI decodes per line and partial output still shows.
void crash_handler_log() {
  if (!crash_handler_has_data())
    return;

  ESP_LOGE(TAG, "*** CRASH DETECTED ON PREVIOUS BOOT ***");
  ESP_LOGE(TAG, "  Reason: %s", LOG_STR_ARG(CrashReasonStrings::get_log_str(s_record.reason, 0)));
  if (s_record.fault_status != 0) {
    ESP_LOGE(TAG, "  CFSR: 0x%08" PRIX32 "  Address: 0x%08" PRIX32, s_record.fault_status, s_record.fault_address);
  }
  ESP_LOGE(TAG, "  PC:  0x%08" PRIX32 "  (fault location)", s_record.pc);
  ESP_LOGE(TAG, "  LR:  0x%08" PRIX32 "  (return address)", s_record.lr);
  ESP_LOGE(TAG, "  SP:  0x%08" PRIX32, s_record.sp);
  uint8_t count = 0;
  for (; count < MAX_BACKTRACE && s_record.backtrace[count] != 0; count++) {
    ESP_LOGE(TAG, "  BT%d: 0x%08" PRIX32 "  (stack backtrace)", count, s_record.backtrace[count]);
  }
  // addr2line hint with every address
  char hint[160];
  int pos = snprintf(hint, sizeof(hint), "Use: addr2line -pfiaC -e raw_firmware.elf 0x%08" PRIX32 " 0x%08" PRIX32,
                     s_record.pc, s_record.lr);
  for (uint8_t i = 0; i < count && pos < (int) sizeof(hint) - 12; i++) {
    pos += snprintf(hint + pos, sizeof(hint) - pos, " 0x%08" PRIX32, s_record.backtrace[i]);
  }
  ESP_LOGE(TAG, "%s", hint);
}

// --- Crash time: exception mode, a few dozen bytes of stack: no printing, no large locals.

static inline bool is_code_addr(uint32_t val) {
  uint32_t addr = val & ~1u;  // clear the Thumb bit of a return address
#ifdef USE_BK72XX
  return (addr >= (uint32_t) _vector_start && addr < (uint32_t) _data_flash_begin) ||
         (addr >= (uint32_t) _itcmcode_ram_begin && addr < (uint32_t) _itcmcode_ram_end);
#else
  return (addr >= (uint32_t) __xip_code_text_start__ && addr < (uint32_t) __xip_code_text_end__) ||
         (addr >= (uint32_t) __ram_entry_text_start__ && addr < (uint32_t) __ram_code_text_end__);
#endif
}

// Return addresses above sp give the backtrace; then the magic marks the record complete.
static void finish_record(uint32_t sp, uint32_t sram_start, uint32_t sram_end) {
  uint32_t count = 0;
  if (sp >= sram_start && sp < sram_end && (sp & 3) == 0) {
    auto *scan = (const uint32_t *) sp;       // NOLINT(performance-no-int-to-ptr)
    auto *end = (const uint32_t *) sram_end;  // NOLINT(performance-no-int-to-ptr)
    for (size_t i = 0; i < STACK_SCAN_WORDS && scan < end && count < MAX_BACKTRACE; i++, scan++) {
      uint32_t val = *scan;
      if (is_code_addr(val) && val != s_record.lr && (val & ~1u) != s_record.pc)
        s_record.backtrace[count++] = val;
    }
  }
  if (count < MAX_BACKTRACE)
    s_record.backtrace[count] = 0;  // terminator, the reader stops at the first 0
  s_record.magic = CRASH_MAGIC_V2;
}

#ifdef USE_BK72XX

// With Wi-Fi up the boot-time watchdog no longer fires: any interrupts-off spin hangs until power is
// cycled (BK7238 and BK7231N; cause not in the open SDK). Re-armed here, long enough for the SDK's
// register dump when sdk_silent is off.
static constexpr uint32_t CRASH_REBOOT_WDT_MS = 500;

// SRAM: 192 KB on the BK7231N, 256 KB on the BK7238.
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

// PUSH_ALL_ARM_REG (boot_handlers.S) saves every mode's registers at the arm_mcu_pub.h addresses
// before bk_trap_*; lr (r14) follows sp (r13).
struct SavedBank {
  uint32_t mode;
  uint32_t sp_addr;
};
static constexpr SavedBank SAVED_BANKS[] = {
    {CPSR_MODE_SYS, MCU_REG_BACKUP_SP_SYS}, {CPSR_MODE_IRQ, MCU_REG_BACKUP_SP_IRQ},
    {CPSR_MODE_FIQ, MCU_REG_BACKUP_SP_FIQ}, {CPSR_MODE_ABT, MCU_REG_BACKUP_SP_ABT},
    {CPSR_MODE_UND, MCU_REG_BACKUP_SP_UND}, {CPSR_MODE_SVC, MCU_REG_BACKUP_SP_SVC},
};

// sp and lr stay 0 for an unknown mode (corrupt spsr).
static void read_saved_sp_lr(uint32_t mode, uint32_t *sp, uint32_t *lr) {
  for (const auto &bank : SAVED_BANKS) {
    if (bank.mode != mode)
      continue;
    const auto *regs = (const volatile uint32_t *) bank.sp_addr;  // NOLINT(performance-no-int-to-ptr)
    *sp = regs[0];
    *lr = regs[1];
    return;
  }
}

static void record_crash(arm_registers *regs, uint32_t reason, uint32_t pc_offset) {
  uint32_t mode = regs->spsr & CPSR_MODE_MASK;
  if (mode == CPSR_MODE_USR)
    mode = CPSR_MODE_SYS;  // same register bank
  uint32_t sp = 0;
  uint32_t lr = 0;
  read_saved_sp_lr(mode, &sp, &lr);

  s_record.reason = reason;
  s_record.pc = regs->pc - pc_offset;
  s_record.lr = lr;
  s_record.sp = sp;
  s_record.fault_status = 0;
  s_record.fault_address = 0;
  finish_record(sp, SRAM_START, SRAM_END);

  lt_wdt_enable(CRASH_REBOOT_WDT_MS);
}

// The exception LR is past the fault: one instruction (undefined), 4 bytes (prefetch), 8 (data abort).
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

#endif  // USE_BK72XX

#ifdef USE_LIBRETINY_VARIANT_RTL8720C

// The SDK points the fault vectors (3 HardFault to 7 SecureFault) of its RAM vector table (VTOR) at a
// ROM handler that prints a register dump and then spins. crash_handler_install() puts
// esphome_fault_entry() in front of it, as the SDK's own CONFIG_MIIO patch (app_start.c) does.
static_assert(LT_HW_WATCHDOG, "the crash reboot relies on the hardware watchdog");
// The ROM dump takes about 0.4 s at 115200 baud; without the re-arm the reset waits for the 10 s
// watchdog from arch_init().
static constexpr uint32_t CRASH_REBOOT_WDT_MS = 1000;
static constexpr uint32_t FIRST_FAULT_VECTOR = 3;
static constexpr uint32_t FAULT_VECTOR_COUNT = 5;

// Architectural System Control Block registers (ARMv8-M).
static constexpr uintptr_t SCB_VTOR = 0xE000ED08;
static constexpr uintptr_t SCB_CFSR = 0xE000ED28;
static constexpr uintptr_t SCB_MMFAR = 0xE000ED34;
static constexpr uintptr_t SCB_BFAR = 0xE000ED38;
static constexpr uint32_t CFSR_MMARVALID = 1u << 7;
static constexpr uint32_t CFSR_BFARVALID = 1u << 15;

// DTCM, where the app's stacks and heap live (rtl8710c_ram.ld).
static constexpr uint32_t SRAM_START = 0x10000000;
static constexpr uint32_t SRAM_END = 0x1003FA00;

// Exception frame: r0-r3, r12, lr, pc, xPSR (the RTL8720C has no FPU, so never the extended frame).
static constexpr uint32_t FRAME_LR = 5;
static constexpr uint32_t FRAME_PC = 6;
static constexpr uint32_t FRAME_XPSR = 7;
static constexpr uint32_t FRAME_WORDS = 8;
static constexpr uint32_t XPSR_STACK_ALIGNED = 1u << 9;

// The ROM handlers fault_entry() continues into, by vector.
static uint32_t s_rom_fault_vectors[FAULT_VECTOR_COUNT];  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static inline uint32_t read_reg(uintptr_t addr) {
  return *(const volatile uint32_t *) addr;  // NOLINT(performance-no-int-to-ptr)
}

// Returns the ROM handler to continue into. pc, lr and sp stay 0 when the frame is outside DTCM.
extern "C" __attribute__((used)) uint32_t esphome_record_fault(const uint32_t *frame, uint32_t vector) {
  uint32_t cfsr = read_reg(SCB_CFSR);
  uint32_t address = 0;
  if (cfsr & CFSR_BFARVALID) {
    address = read_reg(SCB_BFAR);
  } else if (cfsr & CFSR_MMARVALID) {
    address = read_reg(SCB_MMFAR);
  }
  s_record.reason = vector + CRASH_REASON_CORTEX_M_OFFSET;
  s_record.fault_status = cfsr;
  s_record.fault_address = address;

  auto frame_addr = (uint32_t) frame;
  uint32_t sp = 0;
  if (frame_addr >= SRAM_START && frame_addr + FRAME_WORDS * 4 <= SRAM_END && (frame_addr & 3) == 0) {
    s_record.pc = frame[FRAME_PC];
    s_record.lr = frame[FRAME_LR];
    sp = frame_addr + FRAME_WORDS * 4 + ((frame[FRAME_XPSR] & XPSR_STACK_ALIGNED) ? 4 : 0);
  } else {
    s_record.pc = 0;
    s_record.lr = 0;
  }
  s_record.sp = sp;
  finish_record(sp, SRAM_START, SRAM_END);

  lt_wdt_enable(CRASH_REBOOT_WDT_MS);
  return s_rom_fault_vectors[vector - FIRST_FAULT_VECTOR];
}

// Passes the faulting stack's frame and the vector number to esphome_record_fault, then
// branches to the ROM handler with r0-r3 and lr as they were on entry.
extern "C" __attribute__((naked, used)) void esphome_fault_entry() {
  __asm volatile("push {r0-r3, r4, lr}  \n"
                 "tst  lr, #4           \n"
                 "ite  eq               \n"
                 "mrseq r0, msp         \n"
                 "mrsne r0, psp         \n"
                 "it   eq               \n"
                 "addeq r0, r0, #24     \n"  // MSP: skip the six words pushed above
                 "mrs  r1, ipsr         \n"
                 "ubfx r1, r1, #0, #9   \n"
                 "bl   esphome_record_fault \n"
                 "mov  r12, r0          \n"
                 "pop  {r0-r3, r4, lr}  \n"
                 "bx   r12              \n");
}

void crash_handler_install() {
  auto *vectors = (uint32_t *) read_reg(SCB_VTOR);  // NOLINT(performance-no-int-to-ptr)
  for (uint32_t i = 0; i < FAULT_VECTOR_COUNT; i++) {
    s_rom_fault_vectors[i] = vectors[FIRST_FAULT_VECTOR + i];
    vectors[FIRST_FAULT_VECTOR + i] = (uint32_t) &esphome_fault_entry;
  }
}

#endif  // USE_LIBRETINY_VARIANT_RTL8720C

}  // namespace esphome::libretiny

#endif  // USE_LIBRETINY_CRASH_HANDLER
#endif  // USE_LIBRETINY
