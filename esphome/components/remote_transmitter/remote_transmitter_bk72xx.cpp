#include "remote_transmitter.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

// clang-tidy cannot parse the Beken SDK headers pulled in via ArduinoPrivate.h
#if defined(USE_BK72XX) && !defined(CLANG_TIDY)

// ArduinoPrivate.h = Arduino.h + the BDK SDK headers (pwm_pub.h, bk_timer_pub.h, icu_pub.h)
// with the core's fixes for type-name collisions between the two
#include <ArduinoPrivate.h>

// Only the BK7231N-style PWM block (shadow registers with a hardware CFG_UPDATA load bit)
// supports glitch-free per-edge duty updates; older SoCs compile the generic bit-bang
// implementation (remote_transmitter.cpp) instead, and this file compiles to nothing.
// REMOTE_TRANSMITTER_BK_PWM is set per-family in remote_transmitter.h.

namespace esphome::remote_transmitter {

static const char *const TAG = "remote_transmitter";

#ifdef REMOTE_TRANSMITTER_BK_PWM

// PWM peripheral carrier (26MHz block), envelope paced by a BKTIMER1 interrupt chain: each
// interrupt writes the next duty through the shadow registers (T1..T4 + CFG_UPDATA hardware
// load, glitch-free at the next carrier period). Direct register writes beat the driver's
// pwm_update_param() (~19us vs ~26us edge error) and have no shared state to race against.
// BKTIMER1 is the only free channel: TIMER0 = FreeRTOS tick, TIMER2 = SDK cal, TIMER4 = wdt.

static constexpr uint32_t REG_PWM_BASE = 0x00802B00UL;
static constexpr uint32_t REG_PWM_GROUP_STRIDE = 0x40;       // one register group per channel pair
static constexpr uint32_t REG_PWM_T_REGS[2] = {0x04, 0x14};  // T1..T4 offsets within a group
static constexpr uint32_t PWM_INT_STATUS_MASK = 3UL << 30;   // write-1-clear -- always write as zero
static constexpr uint8_t ENVELOPE_TIMER = BKTIMER1;

// The bk_timer handler receives only the channel number, so the chain resolves the instance
// that owns the timer. No IRAM_ATTR: hal.h makes it a no-op on BK72xx (the SDK masks IRQs
// around flash writes).
static void envelope_timer_isr(UINT8 channel) { RemoteTransmitterComponent::advance_active_isr(); }

// Channel <-> pin comes from the board variant's own PIN_PWMn defines rather than a
// family-wide assumption, so an unusual pinout maps correctly instead of silently
// driving another pad
struct PwmPinChannel {
  uint8_t pin;
  int8_t channel;
};
static constexpr PwmPinChannel PWM_PIN_CHANNELS[] = {
#ifdef PIN_PWM0
    {PIN_PWM0, 0},
#endif
#ifdef PIN_PWM1
    {PIN_PWM1, 1},
#endif
#ifdef PIN_PWM2
    {PIN_PWM2, 2},
#endif
#ifdef PIN_PWM3
    {PIN_PWM3, 3},
#endif
#ifdef PIN_PWM4
    {PIN_PWM4, 4},
#endif
#ifdef PIN_PWM5
    {PIN_PWM5, 5},
#endif
};

static int8_t pwm_channel_for_pin(uint8_t pin) {
  for (const auto &entry : PWM_PIN_CHANNELS) {
    if (entry.pin == pin)
      return entry.channel;
  }
  return -1;
}

void RemoteTransmitterComponent::setup() {
  // Deliberately no pin_->setup(): the pin must belong to the PWM function, not GPIO
  const int8_t channel = pwm_channel_for_pin(this->pin_->get_pin());
  if (channel < 0) {
    ESP_LOGE(TAG, "Pin %u is not PWM-capable", this->pin_->get_pin());
    this->mark_failed();
    return;
  }
  this->pwm_channel_ = channel;
  const uint32_t idle_t1 = this->pin_->is_inverted() ? this->isr_period_t4_ : 0;
  pwm_param_st param{};
  param.chan = channel;
  param.t1 = idle_t1;
  param.t4 = this->isr_period_t4_;
  param.init_level = idle_t1 ? 1 : 0;
  if (pwm_init_param(&param) != 0 || pwm_start(channel) != 0) {
    ESP_LOGE(TAG, "PWM init failed on pin %u", this->pin_->get_pin());
    this->pwm_channel_ = -1;
    this->mark_failed();
    return;
  }
  this->disable_loop();  // loop() is only needed while a non-blocking completion is pending
}

void RemoteTransmitterComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Remote Transmitter:\n"
                "  Carrier Duty: %u%%\n"
                "  Non-blocking: %s",
                this->carrier_duty_percent_, YESNO(this->non_blocking_));
  LOG_PIN("  Pin: ", this->pin_);
}

// Writes the duty compare registers and sets the hardware CFG_UPDATA shadow-load bit;
// the new duty latches glitch-free at the next carrier period. ISR-safe: registers only.
// The group control word is shared with the paired channel, but every SDK write to it runs
// under GLOBAL_INT_DISABLE (bk_pwm), so it cannot be torn by this interrupt.
void RemoteTransmitterComponent::write_pwm_t1_(uint32_t t1_counts) {
  const uint32_t group = this->pwm_channel_ / 2;
  const uint32_t post = this->pwm_channel_ % 2;
  const uint32_t group_base = REG_PWM_BASE + REG_PWM_GROUP_STRIDE * group;
  auto *t_regs = (volatile uint32_t *) (group_base + REG_PWM_T_REGS[post]);
  auto *ctrl = (volatile uint32_t *) group_base;
  const uint32_t init_level_bit = 1UL << (8 * post + 6);  // output level while the counter is stopped
  const uint32_t cfg_updata_bit = 1UL << (8 * post + 7);  // 0->1 latches T1..T4 at the next period
  t_regs[0] = t1_counts;                                  // T1: high time
  t_regs[1] = 0;                                          // T2
  t_regs[2] = 0;                                          // T3
  t_regs[3] = this->isr_period_t4_;                       // T4: period
  uint32_t cfg = *ctrl;
  cfg &= ~(PWM_INT_STATUS_MASK | init_level_bit | cfg_updata_bit);
  if (t1_counts != 0)
    cfg |= init_level_bit;
  *ctrl = cfg;
  *ctrl = cfg | cfg_updata_bit;
}

// --- envelope chain hooks (see remote_transmitter_libretiny_isr.cpp) ---

bool RemoteTransmitterComponent::envelope_ready_() const { return this->pwm_channel_ >= 0; }

// Recomputes the carrier period in 26MHz counts and stages the per-item duties;
// unmodulated protocols drive the pin constantly during marks
void RemoteTransmitterComponent::prepare_carrier_(uint32_t carrier_frequency) {
  if (carrier_frequency > 0) {
    this->isr_period_t4_ = std::max(uint32_t(2), (26000000UL + carrier_frequency / 2) / carrier_frequency);
  }
  uint32_t mark_t1 = (carrier_frequency > 0 && this->carrier_duty_percent_ < 100)
                         ? std::max(uint32_t(1), this->isr_period_t4_ * this->carrier_duty_percent_ / 100)
                         : this->isr_period_t4_;
  uint32_t space_t1 = 0;
  if (this->pin_->is_inverted()) {
    mark_t1 = this->isr_period_t4_ - mark_t1;
    space_t1 = this->isr_period_t4_;
  }
  this->isr_mark_t1_ = mark_t1;
  this->isr_space_t1_ = space_t1;
}

void RemoteTransmitterComponent::write_envelope_level_(bool mark) {
  this->write_pwm_t1_(mark ? this->isr_mark_t1_ : this->isr_space_t1_);
}

// The driver's microsecond init path is register writes under a nested interrupt guard,
// so it is safe to call from the chain's own interrupt
void RemoteTransmitterComponent::arm_one_shot_(uint32_t duration_us) {
  timer_param_t param{};
  param.channel = ENVELOPE_TIMER;
  param.div = 1;
  param.period = duration_us;
  param.t_Int_Handler = envelope_timer_isr;
  sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_INIT_PARAM_US, &param);
}

void RemoteTransmitterComponent::stop_envelope_timer_() {
  UINT32 channel = ENVELOPE_TIMER;
  sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_UNIT_DISABLE, &channel);
}

void RemoteTransmitterComponent::digital_write(bool value) {
  if (this->pwm_channel_ < 0)
    return;
  // serialize behind an in-flight chain, matching the ESP32/RMT non-blocking behavior
  this->wait_until_idle_();
  this->write_pwm_t1_((value != this->pin_->is_inverted()) ? this->isr_period_t4_ : 0);
}

#endif  // REMOTE_TRANSMITTER_BK_PWM

}  // namespace esphome::remote_transmitter

#endif  // USE_BK72XX && !CLANG_TIDY
