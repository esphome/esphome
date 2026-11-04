#include "beken_irda.h"

#include <array>
#include <cinttypes>

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

static constexpr uint32_t NEC_HEADER_MARK_US = 9000;
static constexpr uint32_t NEC_HEADER_SPACE_US = 4500;
static constexpr uint32_t NEC_REPEAT_SPACE_US = 2250;
static constexpr uint32_t NEC_BIT_MARK_US = 560;
static constexpr uint32_t NEC_ONE_SPACE_US = 1690;
static constexpr uint32_t NEC_ZERO_SPACE_US = 560;
// Header mark and space, 32 bits of mark and space, and the final mark
static constexpr size_t NEC_FRAME_DURATIONS = 2 + 64 + 1;

// The ISR runs without an instance, so it reaches the component through this pointer.
static BekenIrdaComponent *s_irda = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void irda_isr() { s_irda->handle_isr(); }

void BekenIrdaComponent::handle_isr() {
  const uint32_t status = REG_READ(IRDA_INT_REG);
  REG_WRITE(IRDA_INT_REG, status);

  if (status & IRDA_END_INT) {
    const uint32_t code = REG_READ(IRDA_RX_FIFO_REG);
    const uint8_t command = code >> 16;
    const uint8_t inverse = code >> 24;
    if ((command ^ inverse) == 0xFF) {
      this->last_code_ = code;
      this->raw_ready_ = true;
      // a repeat still pending belongs to the previous code
      this->repeat_ready_ = false;
    } else {
      this->reject_count_ = this->reject_count_ + 1;
    }
  } else if (status & IRDA_REPEAT_INT) {
    this->repeat_ready_ = true;
  }
  if (status & (IRDA_FIFO_WREN_INT | IRDA_PARITY_ERR_INT | IRDA_STOP_ERR_INT))
    this->error_count_ = this->error_count_ + 1;
}

void BekenIrdaComponent::dispatch_full_(uint32_t code) {
  // Rebuild the NEC frame from the decoded address and command so the remote_base decoders and dumpers
  // can process it.
  std::array<int32_t, NEC_FRAME_DURATIONS> frame;
  size_t pos = 0;
  frame[pos++] = NEC_HEADER_MARK_US;
  frame[pos++] = -static_cast<int32_t>(NEC_HEADER_SPACE_US);
  for (uint32_t mask = 1; mask != 0; mask <<= 1) {
    frame[pos++] = NEC_BIT_MARK_US;
    frame[pos++] = -static_cast<int32_t>((code & mask) ? NEC_ONE_SPACE_US : NEC_ZERO_SPACE_US);
  }
  frame[pos] = NEC_BIT_MARK_US;
  this->call_listeners_dumpers_(frame);
}

void BekenIrdaComponent::dispatch_repeat_() {
  // remote_base's NEC decoder ignores standalone repeat frames, so they go out raw for on_raw listeners
  static constexpr int32_t REPEAT_FRAME[] = {NEC_HEADER_MARK_US, -static_cast<int32_t>(NEC_REPEAT_SPACE_US),
                                             NEC_BIT_MARK_US};
  this->call_listeners_dumpers_(REPEAT_FRAME);
}

void BekenIrdaComponent::setup() {
  s_irda = this;
  // irda_init() already registers the IRDA interrupt during the BDK driver init. Registering again with
  // the same priority fails and restores that handler, so replace it in place.
  intc_service_change_handler(IRQ_IRDA, irda_isr);
  // sets up P26 as the NEC input and enables the IRDA clock and interrupts
  Irda_init_app();
  if (REG_READ(IRDA_CTRL_REG) == 0)
    this->mark_failed(LOG_STR("IRDA controller did not initialize"));
}

void BekenIrdaComponent::loop() {
  // cheap while idle: the flags are only read here, the lock is taken once there is something to take
  if (!this->raw_ready_ && !this->repeat_ready_ && this->reject_count_ == this->last_reject_ &&
      this->error_count_ == this->last_error_)
    return;

  // Report decode errors once they change, at debug level so a noisy remote does not spam the log.
  if (this->reject_count_ != this->last_reject_ || this->error_count_ != this->last_error_) {
    this->last_reject_ = this->reject_count_;
    this->last_error_ = this->error_count_;
    ESP_LOGD(TAG, "%" PRIu32 " rejected, %" PRIu32 " errors", this->last_reject_, this->last_error_);
  }

  // Read the validated code together with clearing the flags, so a frame that arrives in between cannot
  // change what is dispatched.
  uint32_t code;
  bool have_code;
  bool have_repeat;
  {
    InterruptLock lock;
    have_code = this->raw_ready_;
    have_repeat = this->repeat_ready_;
    code = this->last_code_;
    this->raw_ready_ = false;
    this->repeat_ready_ = false;
  }
  if (have_code)
    this->dispatch_full_(code);
  if (have_repeat && code != 0)
    this->dispatch_repeat_();
}

void BekenIrdaComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Beken IRDA receiver (NEC):\n"
                     "  P26 is shared with PWM5; do not use PWM on it while receiving");
  LOG_PIN("  Pin: ", this->pin_);
}

}  // namespace esphome::beken_irda

#endif  // USE_BK72XX && !CLANG_TIDY
