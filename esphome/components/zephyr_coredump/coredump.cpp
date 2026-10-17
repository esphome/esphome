#ifdef USE_ZEPHYR
#include "coredump.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <zephyr/version.h>
// The header has no C++ linkage guards of its own
extern "C" {
#include <zephyr/debug/coredump.h>
}

namespace esphome::zephyr_coredump {

ESPHOME_LOG_TAG(TAG, "zephyr_coredump");

// 64 bytes per line (128 hex characters) in the "#CD:" format that Zephyr's coredump_serial_log_parser.py reads
static constexpr size_t CHUNK_SIZE = 64;
// A USB host starts reading the port only some 100 ms after opening it; until then the CDC driver discards output
static constexpr uint32_t START_DELAY_MS = 1000;
static constexpr uint32_t START_DELAY_ID = 0;

// On a UART the logger reports the reader before this component exists, so the flag outlives the pointer
static Coredump *global_coredump = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool reader_present = false;          // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void print_coredump() {
  reader_present = true;
  if (global_coredump != nullptr)
    global_coredump->start_print_();
}

void stop_coredump() {
  reader_present = false;
  if (global_coredump != nullptr)
    global_coredump->stop_print_();
}

void Coredump::setup() {
  global_coredump = this;
  this->disable_loop();
  if (coredump_query(COREDUMP_QUERY_HAS_STORED_DUMP, nullptr) != 1) {
    return;
  }
  // A bad dump is reported and erased from loop() like a good one, once somebody can read the message
  this->stored_ = true;
  this->verify_ = coredump_cmd(COREDUMP_CMD_VERIFY_STORED_DUMP, nullptr);
  this->total_ = coredump_query(COREDUMP_QUERY_GET_STORED_DUMP_SIZE, nullptr);
  if (reader_present)
    this->start_print_();
}

// A new reader gets the whole dump: a print cut short by the port closing starts over
void Coredump::start_print_() {
  if (!this->stored_)
    return;
  this->offset_ = 0;
  this->disable_loop();
  this->set_timeout(START_DELAY_ID, START_DELAY_MS, [this]() { this->enable_loop(); });
}

void Coredump::stop_print_() {
  this->cancel_timeout(START_DELAY_ID);
  this->disable_loop();
}

void Coredump::loop() {
  if (this->verify_ != 1) {
    ESP_LOGE(TAG, "Stored coredump verification failed: %d", this->verify_);
    this->finish_(0);
    return;
  }
  if (this->offset_ == 0) {
    ESP_LOGE(TAG, "#CD:BEGIN#");
  }
  uint8_t buf[CHUNK_SIZE];
  char hex[format_hex_size(CHUNK_SIZE)];
  coredump_cmd_copy_arg arg{static_cast<off_t>(this->offset_), buf,
                            std::min<size_t>(CHUNK_SIZE, this->total_ - this->offset_)};
  const int ret = coredump_cmd(COREDUMP_CMD_COPY_STORED_DUMP, &arg);
  if (ret > 0) {
    ESP_LOGE(TAG, "#CD:%s", format_hex_to(hex, buf, ret));
    this->offset_ += ret;
  }
  if (ret <= 0 || this->offset_ >= this->total_) {
    ESP_LOGE(TAG, "#CD:END#");
    this->finish_(ret < 0 ? ret : 0);
  }
}

// Erase so the dump is not reported again; the next crash writes a new one. Erasing the partition halts the
// CPU for about 85 ms per 4 KB page, so this one call takes most of a second
void Coredump::finish_(int err) {
  if (err < 0) {
    ESP_LOGE(TAG, "Failed to copy coredump: %d", err);
  }
  const int erase = coredump_cmd(COREDUMP_CMD_ERASE_STORED_DUMP, nullptr);
  if (erase < 0) {
    ESP_LOGE(TAG, "Failed to erase coredump: %d", erase);
  }
  this->stored_ = false;
  this->disable_loop();
}

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

// Backport of https://github.com/zephyrproject-rtos/zephyr/pull/79622 (Zephyr 4.0): give GDB the stack
// pointer from before the exception frame so it can unwind. The Python side links it in with --wrap.
#ifdef USE_ZEPHYR_COREDUMP_SP_FIX
// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
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
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp,readability-identifier-naming)
#endif
#endif
