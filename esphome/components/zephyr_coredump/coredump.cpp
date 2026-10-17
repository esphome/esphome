#include "coredump.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <zephyr/version.h>
// The header has no C++ linkage guards of its own
extern "C" {
#include <zephyr/debug/coredump.h>
}

namespace esphome::zephyr_coredump {

ESPHOME_LOG_TAG(TAG, "zephyr_coredump");

static constexpr size_t CHUNK_SIZE = 64;

// The "#CD:" prefix and BEGIN/END markers are what Zephyr's coredump_serial_log_parser.py looks for
static void print_stored_dump() {
  uint8_t buf[CHUNK_SIZE];
  char hex[format_hex_size(CHUNK_SIZE)];
  const size_t total = coredump_query(COREDUMP_QUERY_GET_STORED_DUMP_SIZE, nullptr);
  ESP_LOGE(TAG, "#CD:BEGIN#");
  int ret = 0;
  for (size_t offset = 0; offset < total; offset += CHUNK_SIZE) {
    coredump_cmd_copy_arg arg{static_cast<off_t>(offset), buf, std::min(CHUNK_SIZE, total - offset)};
    ret = coredump_cmd(COREDUMP_CMD_COPY_STORED_DUMP, &arg);
    if (ret <= 0)
      break;
    arch_feed_wdt();
    ESP_LOGE(TAG, "#CD:%s", format_hex_to(hex, buf, ret));
  }
  ESP_LOGE(TAG, "#CD:END#");
  if (ret < 0) {
    ESP_LOGE(TAG, "Failed to copy coredump: %d", ret);
  }
}

void print_coredump() {
  if (coredump_query(COREDUMP_QUERY_HAS_STORED_DUMP, nullptr) != 1)
    return;
  const int ret = coredump_cmd(COREDUMP_CMD_VERIFY_STORED_DUMP, nullptr);
  if (ret == 1) {
    print_stored_dump();
  } else {
    ESP_LOGE(TAG, "Stored coredump verification failed: %d", ret);
  }
  // Erase so the dump is not reported again on the next boot
  const int erase = coredump_cmd(COREDUMP_CMD_ERASE_STORED_DUMP, nullptr);
  if (erase < 0) {
    ESP_LOGE(TAG, "Failed to erase coredump: %d", erase);
  }
}

// Prints when the logger has no early messages; otherwise the logger already printed and erased it
void Coredump::setup() { print_coredump(); }

void Coredump::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Coredump:\n"
                "  Has stored dump: %d\n"
                "  Size: %d\n"
                "  Error: %d",
                coredump_query(COREDUMP_QUERY_HAS_STORED_DUMP, nullptr),
                coredump_query(COREDUMP_QUERY_GET_STORED_DUMP_SIZE, nullptr),
                coredump_query(COREDUMP_QUERY_GET_ERROR, nullptr));
}

}  // namespace esphome::zephyr_coredump

#if KERNEL_VERSION_MAJOR < 4
// Backport of https://github.com/zephyrproject-rtos/zephyr/pull/79622 (Zephyr 4.0): give GDB the stack
// pointer from before the exception frame so it can unwind
extern "C" {

#if KERNEL_VERSION_NUMBER >= 0x030700
using esf_t = struct arch_esf;
#else
using esf_t = z_arch_esf_t;
#endif

static constexpr uint8_t EXC_RETURN_STACK_FRAME_TYPE_POS = 4;
static constexpr uint32_t EXC_RETURN_STACK_FRAME_TYPE_EXTENDED = 0;

static uint32_t global_exc_return;

void __real_z_arm_fault(uint32_t msp, uint32_t psp, uint32_t exc_return, _callee_saved_t *callee_regs);
void __wrap_z_arm_fault(uint32_t msp, uint32_t psp, uint32_t exc_return, _callee_saved_t *callee_regs) {
  global_exc_return = exc_return;
  __real_z_arm_fault(msp, psp, exc_return, callee_regs);
}

void __real_z_arm_fatal_error(unsigned int reason, const esf_t *esf);
void __wrap_z_arm_fatal_error(unsigned int reason, const esf_t *esf) {
#if defined(CONFIG_ARMV7_M_ARMV8_M_MAINLINE) || defined(CONFIG_ARMV6_M_ARMV8_M_BASELINE)
  z_arm_coredump_fault_sp += sizeof(esf->basic);
#if defined(CONFIG_FPU) && defined(CONFIG_FPU_SHARING)
  // The thread used the FP registers, so the frame also holds them
  if ((global_exc_return & BIT(EXC_RETURN_STACK_FRAME_TYPE_POS)) == EXC_RETURN_STACK_FRAME_TYPE_EXTENDED) {
    z_arm_coredump_fault_sp += sizeof(esf->fpu);
  }
#endif
#ifndef CONFIG_ARMV8_M_MAINLINE
  // PSR bit 9 set: the frame was padded for alignment
  if ((esf->basic.xpsr & SCB_CCR_STKALIGN_Msk) == SCB_CCR_STKALIGN_Msk) {
    z_arm_coredump_fault_sp |= 0x4;
  }
#endif
#endif
  __real_z_arm_fatal_error(reason, esf);
}
}
#endif
