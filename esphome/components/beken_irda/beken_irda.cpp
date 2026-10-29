#include "beken_irda.h"

#ifdef USE_BK72XX

#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

extern "C" {
// clang-format off
#include "drv_model_pub.h"
#include "bk_timer_pub.h"
// clang-format on
}

namespace esphome::beken_irda {

ESPHOME_LOG_TAG(TAG, "beken_irda");

// The casts to a fixed address are the point of these two helpers.
static inline uint32_t reg_read(uint32_t addr) {
  // NOLINTNEXTLINE(performance-no-int-to-ptr) - MMIO register at a fixed address
  return *reinterpret_cast<volatile uint32_t *>(addr);
}

static inline void reg_write(uint32_t addr, uint32_t value) {
  // NOLINTNEXTLINE(performance-no-int-to-ptr) - MMIO register at a fixed address
  *reinterpret_cast<volatile uint32_t *>(addr) = value;
}

// Beken BK72xx legacy IRDA hardware (NEC decoder) registers.
static constexpr uint32_t IRDA_CTRL_REG = 0x00802400;
static constexpr uint32_t IRDA_INT_MASK_REG = 0x00802404;
static constexpr uint32_t IRDA_INT_REG = 0x00802408;
static constexpr uint32_t IRDA_RX_FIFO_REG = 0x0080240C;

static constexpr uint32_t IRDA_END_INT = 1u << 0;
static constexpr uint32_t IRDA_RIGHT_INT = 1u << 1;
static constexpr uint32_t IRDA_REPEAT_INT = 1u << 2;
static constexpr uint32_t IRDA_FIFO_WREN_INT = 1u << 3;
static constexpr uint32_t IRDA_PARITY_ERR_INT = 1u << 4;
static constexpr uint32_t IRDA_STOP_ERR_INT = 1u << 5;

// BK7231U GPIO registers; BK7231T uses the same layout.
static constexpr uint32_t GPIO26_CFG_REG = 0x0802800 + 26 * 4;
static constexpr uint32_t GPIO_FUNC_CFG_2_REG = 0x0802800 + 46 * 4;
static constexpr uint32_t P26_FUNC_SHIFT = (26 - 16) * 2;

// Beken SDK entry points, already linked by LibreTiny.
extern "C" {
// NOLINTNEXTLINE(readability-identifier-naming) - name defined by the Beken SDK
void Irda_init_app();
// Implemented by the BDK but not declared in this SDK's intc_pub.h.
void intc_service_change_handler(unsigned char int_num, void (*isr)());
}

static constexpr unsigned char IRQ_IRDA_NUM = 3;
static constexpr unsigned char PRI_IRQ_IRDA_NUM = 23;

// Sampling timer registers. CMD_TIMER_INIT_PARAM_US only supports channels 0..2; channel 0 is
// the RTOS tick and channel 2 is the SDK calibration timer, so channel 1 is the default.
static constexpr uint32_t TIMER0_2_CTL_REG = 0x00802A0C;
static constexpr uint32_t TIMER0_CNT_REG = 0x00802A00;
static constexpr uint32_t TIMER1_CNT_REG = 0x00802A04;
static constexpr uint32_t TIMER2_CNT_REG = 0x00802A08;

static constexpr uint32_t NEC_HEADER_MARK_US = 9000;
static constexpr uint32_t NEC_HEADER_SPACE_US = 4500;
static constexpr uint32_t NEC_REPEAT_SPACE_US = 2250;
static constexpr uint32_t NEC_BIT_MARK_US = 560;
static constexpr uint32_t NEC_ONE_SPACE_US = 1690;
static constexpr uint32_t NEC_ZERO_SPACE_US = 560;

// The ISRs run without an instance, so they reach the component through this pointer.
static BekenIrdaComponent *s_irda = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static bool s_irda_app_initialized = false;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void irda_raw_isr() {
  if (s_irda != nullptr)
    s_irda->handle_isr();
}

static void gpio_timer_isr(UINT8 channel) {
  (void) channel;
  if (s_irda != nullptr)
    s_irda->handle_timer_isr();
}

void BekenIrdaComponent::handle_isr() {
  uint32_t status = reg_read(IRDA_INT_REG);
  reg_write(IRDA_INT_REG, status);
  this->raw_status_ = status;
  this->isr_count_ = this->isr_count_ + 1;

  if (status & IRDA_RIGHT_INT) {
    this->right_count_ = this->right_count_ + 1;
  }
  if (status & IRDA_END_INT) {
    uint32_t code = reg_read(IRDA_RX_FIFO_REG);
    this->raw_code_ = code;
    this->end_count_ = this->end_count_ + 1;
    // The command byte and its inverse must add up to 0xFF; reject frames that fail this.
    uint8_t command = (code >> 16) & 0xFFu;
    uint8_t inverse = (code >> 24) & 0xFFu;
    if ((command ^ inverse) == 0xFFu) {
      this->last_code_ = code;
      this->raw_ready_ = true;
    }
  }
  if ((status & IRDA_REPEAT_INT) && !(status & IRDA_END_INT)) {
    this->repeat_count_ = this->repeat_count_ + 1;
    this->repeat_ready_ = true;
  }
  if (status & (IRDA_FIFO_WREN_INT | IRDA_PARITY_ERR_INT | IRDA_STOP_ERR_INT)) {
    this->error_count_ = this->error_count_ + 1;
  }
}

void BekenIrdaComponent::handle_timer_isr() {
  if (this->frame_ready_ || this->overflow_)
    return;

  this->timer_ticks_ = this->timer_ticks_ + 1;
  // The receiver output idles high and goes low during a mark, so invert the sampled level to
  // match remote_base, which uses positive durations for marks.
  uint8_t level = this->isr_pin_.digital_read() ? 0 : 1;

  if (level == this->last_level_) {
    // No edge: finish the frame once the line has been idle long enough.
    if (this->capture_active_ && this->timer_ticks_ * this->sample_period_ >= this->idle_us_) {
      this->finish_run_(this->last_level_, this->timer_ticks_);
      this->push_run_();
      this->frame_ready_ = true;
      this->capture_active_ = false;
    }
    return;
  }

  // Edge: the run at `last_level_` lasted `timer_ticks_` ticks.
  uint32_t ticks = this->timer_ticks_;
  this->timer_ticks_ = 0;
  uint8_t ended = this->last_level_;
  this->last_level_ = level;
  this->timer_edges_ = this->timer_edges_ + 1;
  if (level) {
    this->timer_high_ = this->timer_high_ + 1;
  } else {
    this->timer_low_ = this->timer_low_ + 1;
  }

  if (!this->capture_active_) {
    // Start a frame on the first mark.
    if (level) {
      this->capture_active_ = true;
      this->buffer_count_ = 0;
      this->run_active_ = true;
      this->run_level_ = 1;
      this->run_ticks_ = 0;
    }
    return;
  }

  this->finish_run_(ended, ticks);
}

void BekenIrdaComponent::finish_run_(uint8_t level, uint32_t ticks) {
  if (!this->run_active_) {
    this->run_level_ = level;
    this->run_ticks_ = ticks;
    this->run_active_ = true;
    return;
  }
  // A run that returns to the pending level continues it, and a run shorter than the filter is
  // folded into it. Either way the samples that are emitted keep alternating sign, which is what
  // remote_base decoders expect.
  if (level == this->run_level_ || ticks * this->sample_period_ < this->filter_us_) {
    if (ticks * this->sample_period_ < this->filter_us_)
      this->filtered_count_ = this->filtered_count_ + 1;
    this->run_ticks_ = this->run_ticks_ + ticks;
    return;
  }
  this->push_run_();
  this->run_level_ = level;
  this->run_ticks_ = ticks;
}

void BekenIrdaComponent::push_run_() {
  if (!this->run_active_)
    return;
  uint32_t run_us = this->run_ticks_ * this->sample_period_;
  if (run_us >= this->filter_us_)
    this->push_sample_(this->run_level_ ? (int32_t) run_us : -(int32_t) run_us);
}

void BekenIrdaComponent::push_sample_(int32_t value) {
  if (this->buffer_ == nullptr)
    return;

  if (this->buffer_count_ >= this->buffer_size_) {
    this->overflow_ = true;
    this->capture_active_ = false;
    return;
  }

  this->buffer_[this->buffer_count_] = value;
  this->buffer_count_ = this->buffer_count_ + 1;
  if (this->buffer_count_ > this->buffer_high_water_)
    this->buffer_high_water_ = this->buffer_count_;
}

void BekenIrdaComponent::reset_capture_() {
  this->buffer_count_ = 0;
  this->timer_ticks_ = 0;
  this->frame_ready_ = false;
  this->overflow_ = false;
  this->capture_active_ = false;
  this->run_active_ = false;
  this->run_ticks_ = 0;
  this->last_level_ = this->isr_pin_.digital_read() ? 0 : 1;
}

void BekenIrdaComponent::dispatch_full_(uint32_t code) {
  // Rebuild the NEC frame from the decoded address and command so the standard remote_base
  // decoders and dumpers can process it.
  this->temp_.clear();
  this->temp_.reserve(2 + 64 + 1);
  this->temp_.push_back(NEC_HEADER_MARK_US);
  this->temp_.push_back(-static_cast<int32_t>(NEC_HEADER_SPACE_US));

  uint16_t address = code & 0xFFFFu;
  uint16_t command = (code >> 16) & 0xFFFFu;
  for (uint16_t value : {address, command}) {
    for (uint16_t mask = 1; mask != 0; mask <<= 1) {
      this->temp_.push_back(NEC_BIT_MARK_US);
      this->temp_.push_back((value & mask) ? -static_cast<int32_t>(NEC_ONE_SPACE_US)
                                           : -static_cast<int32_t>(NEC_ZERO_SPACE_US));
    }
  }
  this->temp_.push_back(NEC_BIT_MARK_US);

  this->call_listeners_dumpers_();
}

void BekenIrdaComponent::dispatch_repeat_() {
  if (this->last_code_ == 0) {
    this->dropped_repeat_count_ = this->dropped_repeat_count_ + 1;
    return;
  }

  // remote_base's NEC decoder ignores standalone repeat frames, so dispatch the repeat timings
  // as a raw frame for on_raw listeners.
  this->temp_.clear();
  this->temp_.reserve(3);
  this->temp_.push_back(NEC_HEADER_MARK_US);
  this->temp_.push_back(-static_cast<int32_t>(NEC_REPEAT_SPACE_US));
  this->temp_.push_back(NEC_BIT_MARK_US);

  this->call_listeners_dumpers_();
}

void BekenIrdaComponent::log_registers_(const LogString *where) {
  uint32_t ctrl = reg_read(IRDA_CTRL_REG);
  uint32_t mask = reg_read(IRDA_INT_MASK_REG);
  uint32_t ints = reg_read(IRDA_INT_REG);
  uint32_t gpio26 = reg_read(GPIO26_CFG_REG);
  uint32_t func2 = reg_read(GPIO_FUNC_CFG_2_REG);
  uint32_t p26_func = (func2 >> P26_FUNC_SHIFT) & 0x3u;
  ESP_LOGI(TAG,
           "%s regs: ctrl=0x%08X mask=0x%08X int=0x%08X gpio26=0x%08X "
           "func2=0x%08X p26_func=%u",
           LOG_STR_ARG(where), (unsigned) ctrl, (unsigned) mask, (unsigned) ints, (unsigned) gpio26, (unsigned) func2,
           (unsigned) p26_func);
}

void BekenIrdaComponent::setup_hardware_() {
  this->last_code_ = 0;
  this->raw_ready_ = false;
  this->repeat_ready_ = false;

  // irda_init() already registers IRQ 3 at priority 23 during the BDK DD init. Registering
  // again with the same priority fails and restores that handler, so replace it in place.
  ESP_LOGI(TAG, "setup: hardware mode, replacing IRDA ISR (vector=%u, priority=%u)", (unsigned) IRQ_IRDA_NUM,
           (unsigned) PRI_IRQ_IRDA_NUM);
  intc_service_change_handler(IRQ_IRDA_NUM, irda_raw_isr);

  // Configures P26 as the IRDA/NEC input and enables the IRDA clock and interrupts. Not
  // idempotent, so call it once per boot even if several components request hardware mode.
  if (!s_irda_app_initialized) {
    ESP_LOGI(TAG, "setup: calling Irda_init_app()");
    Irda_init_app();
    s_irda_app_initialized = true;
    ESP_LOGI(TAG, "setup: Irda_init_app() returned");
  } else {
    ESP_LOGI(TAG, "setup: Irda_init_app() already initialized");
  }

  if (this->debug_)
    this->log_registers_(LOG_STR("setup-after"));

  if (reg_read(IRDA_CTRL_REG) == 0) {
    this->mark_failed(LOG_STR("IRDA controller did not initialize"));
  }
}

void BekenIrdaComponent::setup_gpio_timer_() {
  ESP_LOGI(TAG, "setup: timer mode, channel=%u, sampling every %u us", (unsigned) this->timer_channel_,
           (unsigned) this->sample_period_);

  // Disable the hardware NEC decoder and configure the selected pin as a plain input.
  reg_write(IRDA_CTRL_REG, 0);
  this->pin_->setup();
  this->isr_pin_ = this->pin_->to_isr();

  this->buffer_ = new int32_t[this->buffer_size_];
  this->timer_edges_ = 0;
  this->timer_high_ = 0;
  this->timer_low_ = 0;
  this->frames_count_ = 0;
  this->overflow_count_ = 0;
  this->filtered_count_ = 0;
  this->buffer_high_water_ = 0;
  this->last_code_ = 0;
  this->dropped_repeat_count_ = 0;
  this->reset_capture_();

  timer_param_t param;
  param.channel = this->timer_channel_;
  param.div = 1;
  param.period = this->sample_period_;
  param.t_Int_Handler = gpio_timer_isr;

  UINT32 ret = sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_INIT_PARAM_US, &param);
  this->timer_init_ret_ = ret;
  ESP_LOGI(TAG, "setup: timer init ret=%u", (unsigned) ret);
  if (ret != 0) {
    this->mark_failed(LOG_STR("Timer init failed"));
    return;
  }

  UINT32 channel = this->timer_channel_;
  ret = sddev_control((char *) TIMER_DEV_NAME, CMD_TIMER_UNIT_ENABLE, &channel);
  this->timer_enable_ret_ = ret;
  ESP_LOGI(TAG, "setup: timer enable ret=%u", (unsigned) ret);
  if (ret != 0) {
    this->mark_failed(LOG_STR("Timer enable failed"));
    return;
  }

  if (this->debug_)
    this->log_registers_(LOG_STR("setup-after"));
}

void BekenIrdaComponent::setup() {
  s_irda = this;
  ESP_LOGI(TAG, "setup: start, mode=%s, debug=%s",
           this->hardware_ ? LOG_STR_LITERAL("hardware") : LOG_STR_LITERAL("timer"),
           this->debug_ ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("no"));
  if (this->debug_)
    this->log_registers_(LOG_STR("setup-before"));

  if (this->hardware_) {
    this->setup_hardware_();
  } else {
    this->setup_gpio_timer_();
  }
}

void BekenIrdaComponent::loop() {
  if (this->debug_ && !this->logged_first_loop_) {
    this->logged_first_loop_ = true;
    ESP_LOGI(TAG, "loop: first iteration");
    this->log_registers_(LOG_STR("loop-first"));
  }

  this->loop_count_++;
  if (this->debug_) {
    uint32_t now = App.get_loop_component_start_time();
    if (now - this->last_heartbeat_ >= 3000) {
      this->last_heartbeat_ = now;
      if (!this->hardware_) {
        ESP_LOGI(TAG,
                 "timer heartbeat: loops=%u edges=%u high=%u low=%u frames=%u overflow=%u "
                 "filtered=%u high_water=%u capture=%u level=%u ticks=%u timer_init=%u timer_enable=%u",
                 (unsigned) this->loop_count_, (unsigned) this->timer_edges_, (unsigned) this->timer_high_,
                 (unsigned) this->timer_low_, (unsigned) this->frames_count_, (unsigned) this->overflow_count_,
                 (unsigned) this->filtered_count_, (unsigned) this->buffer_high_water_,
                 (unsigned) this->capture_active_, (unsigned) this->last_level_, (unsigned) this->timer_ticks_,
                 (unsigned) this->timer_init_ret_, (unsigned) this->timer_enable_ret_);
        ESP_LOGI(TAG, "timer regs: ctl=0x%08X t0=%u t1=%u t2=%u", (unsigned) reg_read(TIMER0_2_CTL_REG),
                 (unsigned) reg_read(TIMER0_CNT_REG), (unsigned) reg_read(TIMER1_CNT_REG),
                 (unsigned) reg_read(TIMER2_CNT_REG));
      } else {
        ESP_LOGI(TAG,
                 "hardware heartbeat: loops=%u isr=%u right=%u end=%u "
                 "repeat=%u err=%u status=0x%X raw=0x%08X",
                 (unsigned) this->loop_count_, (unsigned) this->isr_count_, (unsigned) this->right_count_,
                 (unsigned) this->end_count_, (unsigned) this->repeat_count_, (unsigned) this->error_count_,
                 (unsigned) this->raw_status_, (unsigned) this->raw_code_);
      }
      this->log_registers_(LOG_STR("heartbeat"));
    }
  }

  if (this->hardware_) {
    // Read the validated code together with clearing the flags, so a frame that arrives in
    // between cannot change what is dispatched.
    uint32_t code = 0;
    bool have_code = false;
    bool have_repeat = false;
    {
      InterruptLock lock;
      have_code = this->raw_ready_;
      this->raw_ready_ = false;
      code = this->last_code_;
      have_repeat = this->repeat_ready_;
      this->repeat_ready_ = false;
    }
    if (have_code)
      this->dispatch_full_(code);
    if (have_repeat)
      this->dispatch_repeat_();
    return;
  }

  if (this->overflow_) {
    {
      InterruptLock lock;
      this->overflow_count_ = this->overflow_count_ + 1;
      this->reset_capture_();
    }
    ESP_LOGW(TAG, "Timer capture overflow; dropping frame");
    return;
  }

  if (!this->frame_ready_)
    return;

  this->temp_.clear();
  this->temp_.reserve(this->buffer_size_);
  {
    InterruptLock lock;
    uint32_t count = this->buffer_count_;
    for (uint32_t index = 0; index < count; index++)
      this->temp_.push_back(this->buffer_[index]);
    this->frames_count_ = this->frames_count_ + 1;
    this->reset_capture_();
  }
  this->call_listeners_dumpers_();
}

void BekenIrdaComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Beken IR receiver:");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Mode: %s",
                this->hardware_ ? LOG_STR_LITERAL("hardware IRDA (NEC)") : LOG_STR_LITERAL("timer (raw capture)"));
  if (!this->hardware_) {
    ESP_LOGCONFIG(TAG, "  Timer channel: %u", (unsigned) this->timer_channel_);
    ESP_LOGCONFIG(TAG, "  Sample period: %u us", (unsigned) this->sample_period_);
    ESP_LOGCONFIG(TAG, "  Filter: %u us", (unsigned) this->filter_us_);
    ESP_LOGCONFIG(TAG, "  Idle: %u us", (unsigned) this->idle_us_);
    ESP_LOGCONFIG(TAG, "  Buffer size: %u durations", (unsigned) this->buffer_size_);
  }
  ESP_LOGCONFIG(TAG, "  Debug: %s", this->debug_ ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));
  ESP_LOGCONFIG(
      TAG, "  Tolerance: %" PRIu32 "%s", this->tolerance_,
      this->tolerance_mode_ == remote_base::TOLERANCE_MODE_TIME ? LOG_STR_LITERAL(" us") : LOG_STR_LITERAL("%"));
  if (this->pin_->get_pin() == 26) {
    ESP_LOGW(TAG, "P26 is shared with PWM5; do not use PWM on this pin while IR reception is active");
  }
}

}  // namespace esphome::beken_irda

#endif  // USE_BK72XX
