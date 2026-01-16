#ifdef USE_ZEPHYR
#include "coredump.h"
#include "esphome/core/hal.h"
extern "C" {
#include <zephyr/debug/coredump.h>
}
namespace esphome::zephyr_coredump {

void print_coredump();

#define COREDUMP_BEGIN_STR "BEGIN#"
#define COREDUMP_END_STR "END#"
#define COREDUMP_ERROR_STR "ERROR CANNOT DUMP#"

/*
 * Need to prefix coredump strings to make it easier to parse
 * as log module adds its own prefixes.
 */
#define COREDUMP_PREFIX_STR "#CD:"

/* Length of buffer of printable size */
#define PRINT_BUF_SZ 64
#define BUF_SZ 32

/* Length of buffer of printable size plus null character */
#define PRINT_BUF_SZ_RAW (PRINT_BUF_SZ + 1)

/* Print buffer */
static char print_buf[PRINT_BUF_SZ_RAW];
static off_t print_buf_ptr;
static uint8_t buf[BUF_SZ];

static const char *const TAG = "coredump";

void Coredump::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Coredump\n"
                "  Has stored dump: %d\n"
                "  Size: %d\n"
                "  Error: %d\n",
                coredump_query(COREDUMP_QUERY_HAS_STORED_DUMP, nullptr),
                coredump_query(COREDUMP_QUERY_GET_STORED_DUMP_SIZE, nullptr),
                coredump_query(COREDUMP_QUERY_GET_ERROR, nullptr));
  print_coredump();
}

static void flush_print_buf() {
  ESP_LOGE(TAG, "%s%s", COREDUMP_PREFIX_STR, print_buf);
  print_buf_ptr = 0;
  (void) memset(print_buf, 0, sizeof(print_buf));
}

static void print_stored_dump() {
  /* If valid, start printing to shell */
  print_buf_ptr = 0;
  (void) memset(print_buf, 0, sizeof(print_buf));

  ESP_LOGE(TAG, "%s%s", COREDUMP_PREFIX_STR, COREDUMP_BEGIN_STR);

  size_t remaining = coredump_query(COREDUMP_QUERY_GET_STORED_DUMP_SIZE, nullptr);
  if (remaining == 0) {
    flush_print_buf();
  }
  size_t i = 0;
  off_t offset = 0;
  coredump_cmd_copy_arg arg{offset, buf, BUF_SZ};
  arch_feed_wdt();
  int ret = coredump_cmd(COREDUMP_CMD_COPY_STORED_DUMP, &arg);
  arch_feed_wdt();
  while (remaining > 0 && ret > 0) {
    print_buf[print_buf_ptr] = format_hex_char(buf[i] >> 4);
    print_buf_ptr++;
    print_buf[print_buf_ptr] = format_hex_char(buf[i] & 0xf);
    print_buf_ptr++;

    remaining--;
    offset++;
    i++;

    if (print_buf_ptr == PRINT_BUF_SZ) {
      i = 0;
      flush_print_buf();
      arg.offset = offset;
      arch_feed_wdt();
      ret = coredump_cmd(COREDUMP_CMD_COPY_STORED_DUMP, &arg);
      arch_feed_wdt();
    }
  }
  if (print_buf_ptr != 0) {
    flush_print_buf();
  }

  ESP_LOGE(TAG, "%s%s", COREDUMP_PREFIX_STR, COREDUMP_END_STR);

  if (ret > 0) {
    ESP_LOGE(TAG, "Stored coredump printed.");
  } else if (ret == 0) {
    ESP_LOGE(TAG, "Stored coredump verification failed "
                  "or there is no stored coredump.");
  } else {
    ESP_LOGE(TAG, "Failed to print: %d", ret);
  }
}

void print_coredump() {
  if (coredump_query(COREDUMP_QUERY_HAS_STORED_DUMP, nullptr) == 0) {
    return;
  }

  /* Verify first to see if stored dump is valid */
  int ret = coredump_cmd(COREDUMP_CMD_VERIFY_STORED_DUMP, nullptr);

  if (ret == 0) {
    ESP_LOGE(TAG, "Stored coredump verification failed");
  } else if (ret != 1) {
    ESP_LOGE(TAG, "Failed to perform verify command: %d", ret);
  } else {
    print_stored_dump();
  }
}
}  // namespace esphome::zephyr_coredump

// https://github.com/zephyrproject-rtos/zephyr/pull/79622
extern "C" {

#define EXC_RETURN_STACK_FRAME_TYPE_Pos 4
#define EXC_RETURN_STACK_FRAME_TYPE_Msk BIT(EXC_RETURN_STACK_FRAME_TYPE_Pos)
#define EXC_RETURN_STACK_FRAME_TYPE_EXTENDED 0
#define EXC_RETURN_STACK_FRAME_TYPE_STANDARD EXC_RETURN_STACK_FRAME_TYPE_Msk

uint32_t global_exc_return = 0;
extern void __real_z_arm_fault(uint32_t msp, uint32_t psp, uint32_t exc_return, _callee_saved_t *callee_regs);
void __wrap_z_arm_fault(uint32_t msp, uint32_t psp, uint32_t exc_return, _callee_saved_t *callee_regs) {
  global_exc_return = exc_return;
  __real_z_arm_fault(msp, psp, exc_return, callee_regs);
}

extern void __real_z_arm_fatal_error(unsigned int reason, const struct arch_esf *esf);
void __wrap_z_arm_fatal_error(unsigned int reason, const struct arch_esf *esf) {
#if defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE) || defined(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
  /* Gdb expects a stack pointer that does not include the exception stack frame in order to
   * unwind. So adjust the stack pointer accordingly.
   */
  z_arm_coredump_fault_sp += sizeof(esf->basic);

#if defined(CONFIG_FPU) && defined(CONFIG_FPU_SHARING)
  /* Assess whether thread had been using the FP registers and add size of additional
   * registers if necessary
   */
  if ((global_exc_return & EXC_RETURN_STACK_FRAME_TYPE_STANDARD) == EXC_RETURN_STACK_FRAME_TYPE_EXTENDED) {
    z_arm_coredump_fault_sp += sizeof(esf->fpu);
  }
#endif /* CONFIG_FPU && CONFIG_FPU_SHARING */

#ifndef CONFIG_ARMV8_M_MAINLINE
  if ((esf->basic.xpsr & SCB_CCR_STKALIGN_Msk) == SCB_CCR_STKALIGN_Msk) {
    /* Adjust stack alignment after PSR bit[9] detected */
    z_arm_coredump_fault_sp |= 0x4;
  }
#endif /* !CONFIG_ARMV8_M_MAINLINE */

#endif /* CONFIG_ARMV7_M_ARMV8_M_MAINLINE || CONFIG_ARMV6_M_ARMV8_M_BASELINE */
  __real_z_arm_fatal_error(reason, esf);
}
}

#endif
