#ifdef USE_RP2040

#include "crash_handler.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <hardware/regs/addressmap.h>
#include <hardware/structs/watchdog.h>
#include <hardware/watchdog.h>

// Cortex-M0+ exception frame offsets (words)
// When a fault occurs, the CPU pushes: R0, R1, R2, R3, R12, LR, PC, xPSR
static constexpr uint32_t EF_LR = 5;
static constexpr uint32_t EF_PC = 6;

static constexpr uint32_t CRASH_MAGIC = 0xDEADBEEF;

// We only have 8 scratch registers (32 bytes) that survive watchdog reboot.
// Use them for the most important data, then scan the stack for code addresses.
//
// Scratch register layout:
// [0] = magic (CRASH_MAGIC)
// [1] = PC (program counter at fault)
// [2] = LR (link register from exception frame)
// [3] = SP (stack pointer at fault)
// [4..7] = up to 4 additional code addresses found by scanning the stack
//          (return addresses from callers, giving a deeper backtrace)

// Flash is mapped at XIP_BASE (0x10000000). We use a conservative upper bound
// to keep false positives low during stack scanning. Wider ranges would match
// more stale data on the stack that happens to look like code addresses.
#if defined(PICO_RP2350)
static constexpr uint32_t FLASH_SCAN_END = XIP_BASE + 0x400000;  // 4MB — RP2350 typical max
#else
static constexpr uint32_t FLASH_SCAN_END = XIP_BASE + 0x200000;  // 2MB — RP2040 typical max
#endif

static inline bool is_code_addr(uint32_t val) {
  uint32_t cleared = val & ~1u;  // Clear Thumb bit
  return cleared >= XIP_BASE && cleared < FLASH_SCAN_END;
}

static constexpr size_t MAX_BACKTRACE = 4;

namespace esphome::rp2040 {

static const char *const TAG = "rp2040.crash";

static struct {
  bool valid{false};
  uint32_t pc;
  uint32_t lr;
  uint32_t sp;
  uint32_t backtrace[MAX_BACKTRACE];
  uint8_t backtrace_count;
} s_crash_data;

void crash_handler_read_and_clear() {
  if (watchdog_hw->scratch[0] == CRASH_MAGIC) {
    s_crash_data.valid = true;
    s_crash_data.pc = watchdog_hw->scratch[1];
    s_crash_data.lr = watchdog_hw->scratch[2];
    s_crash_data.sp = watchdog_hw->scratch[3];
    s_crash_data.backtrace_count = 0;
    for (size_t i = 0; i < MAX_BACKTRACE; i++) {
      uint32_t addr = watchdog_hw->scratch[4 + i];
      if (addr == 0)
        break;
      s_crash_data.backtrace[i] = addr;
      s_crash_data.backtrace_count++;
    }
  }
  // Clear scratch registers regardless
  for (int i = 0; i < 8; i++) {
    watchdog_hw->scratch[i] = 0;
  }
}

// Intentionally uses separate ESP_LOGE calls per line instead of combining into
// one multi-line log message. This ensures each address appears as its own line
// on the serial console (miniterm), making it possible to see partial output if
// the device crashes again during boot, and allowing the CLI's process_stacktrace
// to match and decode each address individually.
void crash_handler_log() {
  if (!s_crash_data.valid)
    return;

  ESP_LOGE(TAG, "*** CRASH DETECTED ON PREVIOUS BOOT ***");
  ESP_LOGE(TAG, "  PC:  0x%08X  (fault location)", s_crash_data.pc);
  ESP_LOGE(TAG, "  LR:  0x%08X  (return address)", s_crash_data.lr);
  ESP_LOGE(TAG, "  SP:  0x%08X", s_crash_data.sp);
  for (uint8_t i = 0; i < s_crash_data.backtrace_count; i++) {
    ESP_LOGE(TAG, "  BT%d: 0x%08X  (stack backtrace)", i, s_crash_data.backtrace[i]);
  }
  // Build addr2line hint with all captured addresses for easy copy-paste
  char hint[160];
  int pos = snprintf(hint, sizeof(hint), "Use: addr2line -pfiaC -e firmware.elf 0x%08" PRIX32 " 0x%08" PRIX32,
                     s_crash_data.pc, s_crash_data.lr);
  for (uint8_t i = 0; i < s_crash_data.backtrace_count && pos < (int) sizeof(hint) - 12; i++) {
    pos += snprintf(hint + pos, sizeof(hint) - pos, " 0x%08" PRIX32, s_crash_data.backtrace[i]);
  }
  ESP_LOGE(TAG, "%s", hint);
}

}  // namespace esphome::rp2040

// --- HardFault handler ---
// Overrides the weak isr_hardfault from arduino-pico's crt0.S.
// On Cortex-M0+, the CPU pushes {R0,R1,R2,R3,R12,LR,PC,xPSR} onto the
// active stack (MSP or PSP). We determine which stack was active,
// extract key registers, store them in watchdog scratch registers
// (which survive watchdog reboot), then trigger a reboot.

// C handler called from the asm wrapper with the exception frame pointer.
static void __attribute__((used, noreturn)) hard_fault_handler_c(uint32_t *frame, uint32_t /*exc_return*/) {
  // watchdog_reboot() overwrites scratch[4]-[7], so we must call it first
  // then write ALL our data after. The 10ms timeout gives us plenty of time.
  watchdog_reboot(0, 0, 10);

  // Write key registers
  watchdog_hw->scratch[0] = CRASH_MAGIC;
  watchdog_hw->scratch[1] = frame[EF_PC];
  watchdog_hw->scratch[2] = frame[EF_LR];
  watchdog_hw->scratch[3] = (uint32_t) frame;  // SP at fault

  // Scan stack for code addresses to build a deeper backtrace.
  // The exception frame is 8 words (32 bytes) at 'frame'. The pre-fault
  // stack starts at frame+8. Walk up to 64 words looking for return addresses.
  uint32_t *scan_start = frame + 8;  // Past exception frame
  // SRAM_END is chip-specific: 0x20042000 (RP2040) or 0x20082000 (RP2350)
  uint32_t *stack_top = (uint32_t *) SRAM_END;
  uint32_t bt_count = 0;

  for (uint32_t *p = scan_start; p < stack_top && p < scan_start + 64 && bt_count < MAX_BACKTRACE; p++) {
    uint32_t val = *p;
    // Check if this looks like a code address in flash
    // Skip if it's the same as PC or LR we already saved
    if (is_code_addr(val) && val != frame[EF_PC] && val != frame[EF_LR]) {
      watchdog_hw->scratch[4 + bt_count] = val;
      bt_count++;
    }
  }
  // Zero remaining slots
  for (uint32_t i = bt_count; i < MAX_BACKTRACE; i++) {
    watchdog_hw->scratch[4 + i] = 0;
  }

  while (true) {
    __asm volatile("nop");
  }
}

// Naked asm wrapper - Cortex-M0+ compatible (no ITE/conditional execution).
// Determines active stack pointer and branches to C handler.
// Uses literal pool (.word) for addresses since M0+ has limited immediate encoding.
//
// Based on the standard Cortex-M0+ HardFault handler pattern described in:
// - ARM Application Note AN209: "Using Cortex-M3/M4/M7 Fault Exceptions"
//   (adapted for M0+ which lacks conditional execution instructions)
// - Memfault: "How to debug a HardFault on an ARM Cortex-M MCU"
//   https://interrupt.memfault.com/blog/cortex-m-hardfault-debug
// - Raspberry Pi Forums: "Cortex-M0+ Hard Fault handler porting"
//   https://www.eevblog.com/forum/microcontrollers/cortex-m0-hard-fault-handler-porting/
//
// The key M0+ adaptation: replaces ITE/MRSEQ/MRSNE (Cortex-M3+) with
// MOVS+TST+BEQ branch sequence, and uses a literal pool for the C handler address.
extern "C" void __attribute__((naked, used)) isr_hardfault() {
  __asm volatile("movs r0, #4          \n"  // Prepare bit 2 mask
                 "mov  r1, lr          \n"  // r1 = EXC_RETURN
                 "tst  r1, r0          \n"  // Test bit 2
                 "beq  1f              \n"  // If 0, was using MSP
                 "mrs  r0, psp         \n"  // Bit 2 set = PSP was active
                 "b    2f              \n"
                 "1:                   \n"
                 "mrs  r0, msp         \n"  // Bit 2 clear = MSP was active
                 "2:                   \n"
                 // r0 = exception frame pointer, r1 = EXC_RETURN (still in r1)
                 "ldr  r2, 3f          \n"  // Load C handler address from literal pool
                 "bx   r2              \n"  // Branch to handler (r0=frame, r1=exc_return)
                 ".align 2             \n"
                 "3: .word %c0         \n"  // Literal pool: address of C handler
                 :
                 : "i"(hard_fault_handler_c));
}

#endif  // USE_RP2040
