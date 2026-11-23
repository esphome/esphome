#include "beken_irda.h"

#include <array>

#include "esphome/components/remote_base/nec_protocol.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

// clang-tidy cannot parse the Beken SDK headers pulled in via ArduinoPrivate.h
#if defined(USE_BK72XX) && !defined(CLANG_TIDY)

// ArduinoPrivate.h = Arduino.h + the BDK SDK headers (arm_arch.h, intc_pub.h)
#include <ArduinoPrivate.h>

extern "C" {
#include "intc_pub.h"
#include "irda_pub.h"
// Implemented by the BDK but not declared in this SDK's intc_pub.h.
void intc_service_change_handler(unsigned char int_num, void (*isr)());
}

namespace esphome::beken_irda {

ESPHOME_LOG_TAG(TAG, "beken_irda");

// Legacy IRDA block registers, from the BDK's driver/irda/irda.h, which is not on the include path.
static constexpr uint32_t IRDA_CTRL_REG = 0x00802400;
static constexpr uint32_t IRDA_INT_REG = 0x00802408;
static constexpr uint32_t IRDA_RX_FIFO_REG = 0x0080240C;

static constexpr uint32_t IRDA_END_INT = 1u << 0;
static constexpr uint32_t IRDA_REPEAT_INT = 1u << 2;
static constexpr uint32_t IRDA_FIFO_WREN_INT = 1u << 3;
static constexpr uint32_t IRDA_PARITY_ERR_INT = 1u << 4;
static constexpr uint32_t IRDA_STOP_ERR_INT = 1u << 5;
// The BDK ignores all three; parity and stop errors mark a bad frame, FIFO_WREN is only counted
static constexpr uint32_t IRDA_FRAME_ERROR_INTS = IRDA_PARITY_ERR_INT | IRDA_STOP_ERR_INT;
static constexpr uint32_t IRDA_ERROR_INTS = IRDA_FIFO_WREN_INT | IRDA_FRAME_ERROR_INTS;

using NEC = remote_base::NECProtocol;
// Header mark and space, 32 bits of mark and space, and the final mark
static constexpr size_t NEC_FRAME_DURATIONS = 2 + 64 + 1;

// The counters are read and reset once per loop, so a flood between passes must not wrap them
static inline void saturating_increment(volatile uint8_t &value) {
  if (value != UINT8_MAX)
    value = value + 1;
}

// The ISR runs without an instance, so it reaches the component through this pointer.
static BekenIrdaComponent *s_irda = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void BekenIrdaComponent::irda_isr() { s_irda->handle_isr_(); }

void BekenIrdaComponent::handle_isr_() {
  uint32_t status = REG_READ(IRDA_INT_REG);
  REG_WRITE(IRDA_INT_REG, status);

  if (status & IRDA_END_INT) {
    const uint32_t code = REG_READ(IRDA_RX_FIFO_REG);
    const uint8_t command = code >> 16;
    const uint8_t inverse = code >> 24;
    // a frame the hardware flagged as corrupt is dropped even when the command check passes
    if ((status & IRDA_FRAME_ERROR_INTS) == 0 && (command ^ inverse) == 0xFF) {
      if (this->raw_ready_)
        saturating_increment(this->dropped_count_);
      this->last_code_ = code;
      this->raw_ready_ = true;
      this->repeats_valid_ = true;
      // a repeat still pending belongs to the previous code
      this->repeat_ready_ = false;
    } else {
      saturating_increment(this->error_count_);
      // counted once
      status &= ~IRDA_ERROR_INTS;
      // the repeats that follow belong to the rejected frame; a code still pending is dispatched
      this->repeat_ready_ = false;
      this->repeats_valid_ = false;
    }
  } else if ((status & IRDA_REPEAT_INT) && (status & IRDA_FRAME_ERROR_INTS) == 0 && this->repeats_valid_) {
    this->repeat_ready_ = true;
  }
  if (status & IRDA_ERROR_INTS)
    saturating_increment(this->error_count_);
  this->enable_loop_soon_any_context();
}

void BekenIrdaComponent::dispatch_full_(uint32_t code) {
  // Rebuild the NEC frame from the decoded address and command so the remote_base decoders and dumpers
  // can process it.
  std::array<int32_t, NEC_FRAME_DURATIONS> frame;
  size_t pos = 0;
  frame[pos++] = NEC::HEADER_HIGH_US;
  frame[pos++] = -NEC::HEADER_LOW_US;
  for (uint32_t mask = 1; mask != 0; mask <<= 1) {
    frame[pos++] = NEC::BIT_HIGH_US;
    frame[pos++] = (code & mask) ? -NEC::BIT_ONE_LOW_US : -NEC::BIT_ZERO_LOW_US;
  }
  frame[pos] = NEC::BIT_HIGH_US;
  this->call_listeners_dumpers_(frame);
}

void BekenIrdaComponent::dispatch_repeat_() {
  // remote_base's NEC decoder ignores standalone repeat frames, so they go out raw for on_raw listeners
  static constexpr int32_t REPEAT_FRAME[] = {NEC::HEADER_HIGH_US, -NEC::REPEAT_HEADER_LOW_US, NEC::BIT_HIGH_US};
  this->call_listeners_dumpers_(REPEAT_FRAME);
}

void BekenIrdaComponent::setup() {
  s_irda = this;
  // irda_init() normally registers the IRDA interrupt during the BDK driver init. Registering again with the
  // same priority then does nothing, and the change call replaces that handler in place; without it, the
  // register call installs ours and the change call, which would otherwise do nothing, keeps it.
  intc_service_register(IRQ_IRDA, PRI_IRQ_IRDA, irda_isr);
  intc_service_change_handler(IRQ_IRDA, irda_isr);
  // sets up P26 as the NEC input and enables the IRDA clock and interrupts
  Irda_init_app();
  if (REG_READ(IRDA_CTRL_REG) == 0) {
    this->mark_failed(LOG_STR("IRDA controller did not initialize"));
    return;
  }
  // the interrupt wakes the loop
  this->disable_loop();
}

void BekenIrdaComponent::loop() {
  // Read the validated code together with clearing the flags, so a frame that arrives in between cannot
  // change what is dispatched.
  uint32_t code;
  uint8_t errors;
  uint8_t dropped;
  bool have_code;
  bool have_repeat;
  {
    InterruptLock lock;
    have_code = this->raw_ready_;
    have_repeat = this->repeat_ready_;
    code = this->last_code_;
    errors = this->error_count_;
    dropped = this->dropped_count_;
    this->raw_ready_ = false;
    this->repeat_ready_ = false;
    this->error_count_ = 0;
    this->dropped_count_ = 0;
  }
  // at debug level so a noisy remote does not spam the log
  if (errors != 0) {
    ESP_LOGD(TAG, "%u decode errors", errors);
  }
  if (dropped != 0) {
    ESP_LOGW(TAG, "%u codes dropped, the loop did not keep up", dropped);
  }
  if (!have_code && !have_repeat) {
    this->disable_loop();
    return;
  }
  if (have_code)
    this->dispatch_full_(code);
  if (have_repeat)
    this->dispatch_repeat_();
}

void BekenIrdaComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Beken IRDA receiver (NEC):");
  LOG_PIN("  Pin: ", this->pin_);
}

}  // namespace esphome::beken_irda

#endif  // USE_BK72XX && !CLANG_TIDY
