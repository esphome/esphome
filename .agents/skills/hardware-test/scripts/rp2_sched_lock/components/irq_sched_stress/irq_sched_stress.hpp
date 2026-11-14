#pragma once
// Test only. run_defer(): N defer() calls from a 300 us repeating timer IRQ plus N from the main loop; every one must
// run. run_cancel(): NC set_timeout() calls on 10 shared names from each side inside one 150 ms window; exactly 10
// must run, the rest are implicit cancellations. Results are logged as "DEFER RESULT" and "CANCEL RESULT".
// Keep esphome: debug_scheduler off, it logs from the IRQ.
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <pico/time.h>

namespace esphome::irq_sched_stress {

class IrqSchedStress : public Component {
 public:
  void loop() override {
    if (this->mode_ == IDLE || this->finishing_)
      return;
    // Issue the main loop share in one tight burst while the IRQ issues its share, so every operation lands inside
    // the 150 ms timeout window and the IRQ interleaves with the main loop inside the scheduler.
    // The time limit keeps a stalled timer IRQ from wedging the main loop; the result then reports the shortfall.
    uint32_t lim = this->mode_ == DEFER ? N : NC;
    while ((this->main_issued_ < lim || this->irq_issued_ < lim) && millis() - this->start_ms_ < ISSUE_LIMIT_MS) {
      if (this->main_issued_ < lim)
        this->step_(false);
      delayMicroseconds(60);
    }
    cancel_repeating_timer(&this->timer_);
    this->issue_ms_ = millis() - this->start_ms_;
    this->finishing_ = true;
    this->set_timeout(1000, [this]() { this->finish_(); });
  }

  void run_defer() { this->start_(DEFER); }
  void run_cancel() { this->start_(CANCEL); }

 protected:
  enum Mode : uint8_t { IDLE, DEFER, CANCEL };
  static constexpr uint32_t N = 500;   // per source, defer mode
  static constexpr uint32_t NC = 200;  // per source, cancel mode
  static constexpr uint32_t ISSUE_LIMIT_MS = 2000;
  static constexpr const char *NAMES[10] = {"shared_0", "shared_1", "shared_2", "shared_3", "shared_4",
                                            "shared_5", "shared_6", "shared_7", "shared_8", "shared_9"};

  static bool irq_tick_(repeating_timer_t *t) {
    static_cast<IrqSchedStress *>(t->user_data)->step_(true);
    return true;
  }

  void start_(Mode mode) {
    if (this->mode_ != IDLE)
      return;
    this->irq_issued_ = this->main_issued_ = this->irq_exec_ = this->main_exec_ = 0;
    this->fifo_viol_ = this->cancel_exec_ = 0;
    this->irq_last_ = this->main_last_ = -1;
    this->finishing_ = false;
    this->start_ms_ = millis();
    ESP_LOGI("irq_sched_stress", "%s test start", mode == DEFER ? "defer" : "cancel");
    this->mode_ = mode;
    if (!add_repeating_timer_us(-300, &IrqSchedStress::irq_tick_, this, &this->timer_)) {
      this->mode_ = IDLE;
      ESP_LOGE("irq_sched_stress", "%s RESULT FAIL timer not started", mode == DEFER ? "DEFER" : "CANCEL");
    }
  }

  // One operation from the IRQ (irq true) or the main loop.
  void step_(bool irq) {
    volatile uint32_t &issued = irq ? this->irq_issued_ : this->main_issued_;
    if (this->mode_ == DEFER && issued < N) {
      int32_t id = issued;
      issued = issued + 1;
      if (irq) {
        this->defer([this, id]() { this->ran_(this->irq_last_, this->irq_exec_, id); });
      } else {
        this->defer([this, id]() { this->ran_(this->main_last_, this->main_exec_, id); });
      }
    } else if (this->mode_ == CANCEL && issued < NC) {
      uint32_t i = issued;
      issued = issued + 1;
      this->set_timeout(NAMES[i % 10], 150, [this]() { this->cancel_exec_ = this->cancel_exec_ + 1; });
    }
  }

  void ran_(volatile int32_t &last, volatile uint32_t &exec, int32_t id) {
    if (id != last + 1)
      this->fifo_viol_ = this->fifo_viol_ + 1;
    last = id;
    exec = exec + 1;
  }

  void finish_() {
    unsigned total = this->irq_issued_ + this->main_issued_;
    if (this->mode_ == DEFER) {
      bool ok = this->irq_exec_ == N && this->main_exec_ == N;
      ESP_LOGI("irq_sched_stress", "DEFER RESULT %s irq_exec=%u/%u main_exec=%u/%u out_of_order=%u issued_in=%u ms",
               ok ? "PASS" : "FAIL", (unsigned) this->irq_exec_, (unsigned) N, (unsigned) this->main_exec_,
               (unsigned) N, (unsigned) this->fifo_viol_, (unsigned) this->issue_ms_);
    } else {
      bool ok = this->cancel_exec_ == 10;
      ESP_LOGI("irq_sched_stress", "CANCEL RESULT %s scheduled=%u executed=%u (expect 10) issued_in=%u ms",
               ok ? "PASS" : "FAIL", total, (unsigned) this->cancel_exec_, (unsigned) this->issue_ms_);
    }
    this->mode_ = IDLE;
  }

  repeating_timer_t timer_{};
  volatile Mode mode_{IDLE};
  volatile uint32_t irq_issued_{0};
  volatile uint32_t main_issued_{0};
  volatile uint32_t irq_exec_{0};
  volatile uint32_t main_exec_{0};
  volatile int32_t irq_last_{-1};
  volatile int32_t main_last_{-1};
  volatile uint32_t fifo_viol_{0};
  volatile uint32_t cancel_exec_{0};
  uint32_t start_ms_{0};
  uint32_t issue_ms_{0};
  bool finishing_{false};
};

}  // namespace esphome::irq_sched_stress
