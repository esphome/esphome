#pragma once
#include "esphome/components/uart/uart.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <Arduino.h>
#include "esphome/components/uart/uart_component_libretiny.h"

namespace esphome::brkbench {

static const char *const TAG = "brk";

// Break bench: loopback check, break held ~500 ms with wire probes, release, loopback check again.
// Every 4th cycle does two breaks back to back before the loopback check.
class BrkBench : public Component, public uart::UARTDevice {
 public:
  void set_pins(InternalGPIOPin *tx, InternalGPIOPin *probe) {
    this->tx_ = tx->get_pin();
    this->probe_ = probe->get_pin();
  }
  void set_opts(bool probe_is_rx, bool loopback, int cycles) {
    this->probe_is_rx_ = probe_is_rx;
    this->loopback_ = loopback;
    this->cycles_ = cycles;
  }
  void set_inv(uart::UARTComponent *inv) { this->inv_ = inv; }
  void set_mode(int probe_from, bool fix, int rx_num) {
    this->probe_from_ = probe_from;
    this->fix_ = fix;
    this->rx_num_ = rx_num;
  }
  float get_setup_priority() const override { return setup_priority::LATE; }

  void setup() override { this->next_ = millis() + 8000; }

  void loop() override {
#ifdef USE_UART_TX_BREAK
    uint32_t now = millis();
    if ((int32_t) (now - this->next_) < 0)
      return;
    if (this->cycle_ == 0 && !this->did_api_) {
      this->did_api_ = true;
      this->api_checks_();
    }
    if (this->cycle_ >= this->cycles_) {
      if (!this->done_) {
        this->done_ = true;
        ESP_LOGI(TAG,
                 "SUMMARY cycles=%d breaks=%u lb_ok=%u lb_bad=%u ret_bad=%u low_ok=%u low_bad=%u high_ok=%u "
                 "high_bad=%u idle_ok=%u idle_bad=%u tx_low_ok=%u tx_low_bad=%u junk=%u",
                 this->cycles_, this->breaks_, this->lb_ok_, this->lb_bad_, this->ret_bad_, this->low_ok_,
                 this->low_bad_, this->high_ok_, this->high_bad_, this->idle_ok_, this->idle_bad_, this->txs_ok_,
                 this->txs_bad_, this->junk_);
      }
      this->next_ = now + 30000;
      ESP_LOGI(TAG, "alive done uptime=%u", now / 1000);
      return;
    }
    this->run_cycle_();
    this->next_ = millis() + 1500;
#endif
  }

 protected:
#ifdef USE_UART_TX_BREAK
  void api_checks_() {
    // release without a break: no-op, expect true
    bool r0 = this->parent_->set_tx_break(false);
    ESP_LOGI(TAG, "API release-without-break ret=%d", r0);
    if (!r0)
      this->ret_bad_++;
    if (this->inv_ != nullptr) {
      bool ri = this->inv_->set_tx_break(true);
      bool ri2 = this->inv_->set_tx_break(false);
      ESP_LOGI(TAG, "API inverted-tx set_tx_break(true)=%d (expect 0) release=%d", ri, ri2);
      if (ri)
        this->ret_bad_++;
    }
  }

  // reads the probe pin with a pull opposite to the expected level, so a floating pin fails
  int read_probe_(bool expect_high) {
    pinMode(this->probe_, expect_high ? INPUT_PULLDOWN : INPUT_PULLUP);
    delayMicroseconds(200);
    return digitalRead(this->probe_);
  }

  void drain_(const char *when) {
    uint32_t n = 0;
    uint8_t b, first[8];
    while (this->available() && this->read_byte(&b)) {
      if (n < 8)
        first[n] = b;
      n++;
    }
    if (n) {
      this->junk_ += n;
      ESP_LOGW(TAG, "  %s: %u stray rx bytes, first %02X %02X %02X", when, n, first[0], n > 1 ? first[1] : 0,
               n > 2 ? first[2] : 0);
    }
  }

  bool loopback_check_(const char *when) {
    if (!this->loopback_)
      return true;
    this->drain_(when);
    uint8_t out[24], in[24];
    for (int i = 0; i < 24; i++)
      out[i] = (uint8_t) (this->seq_++ * 37 + 0x55);
    out[0] = 0x00;
    out[1] = 0xFF;
    this->write_array(out, sizeof(out));
    this->flush();
    uint32_t start = millis();
    size_t got = 0;
    while (got < sizeof(in) && millis() - start < 200) {
      while (got < sizeof(in) && this->available() && this->read_byte(&in[got]))
        got++;
      delay(1);
    }
    bool ok = got == sizeof(in) && memcmp(in, out, sizeof(in)) == 0;
    if (ok)
      this->lb_ok_++;
    else
      this->lb_bad_++;
    ESP_LOGI(TAG, "  loopback %s: %s (%u/24 bytes)", when, ok ? "OK" : "BAD", (unsigned) got);
    return ok;
  }

  // TX idles high: after the release, sample the probe while 0x00 bytes go out (non-rx probe only)
  void tx_activity_check_() {
    if (this->probe_is_rx_)
      return;
    int idle = this->read_probe_(true);
    if (idle)
      this->idle_ok_++;
    else
      this->idle_bad_++;
    pinMode(this->probe_, INPUT_PULLUP);
    uint8_t zeros[16] = {0};
    this->write_array(zeros, sizeof(zeros));
    uint32_t lows = 0, highs = 0, start = micros();
    while (micros() - start < 25000) {
      if (digitalRead(this->probe_))
        highs++;
      else
        lows++;
    }
    this->flush();
    bool ok = lows > 10;
    if (ok)
      this->txs_ok_++;
    else
      this->txs_bad_++;
    ESP_LOGI(TAG, "  after release: idle=%d, while sending 0x00: lows=%u highs=%u %s", idle, lows, highs,
             ok ? "TX-ACTIVE" : "TX-DEAD");
  }

  void one_break_(int hold_ms) {
    this->flush();
    bool r = this->parent_->set_tx_break(true);
    this->breaks_++;
    if (!r)
      this->ret_bad_++;
    bool probe = this->breaks_ >= (uint32_t) this->probe_from_;
    if (!probe) {
      delay(hold_ms);
      bool r2 = this->parent_->set_tx_break(false);
      if (!r2)
        this->ret_bad_++;
      ESP_LOGI(TAG, "  break #%u ret=%d (not probed) held=%ums release ret=%d", this->breaks_, r, hold_ms, r2);
      this->fix_reopen_();
      return;
    }
    // break level on the wire
    int l1 = this->read_probe_(false);
    // positive control: drive TX high through the GPIO record, probe must follow, then back low
    digitalWrite(this->tx_, HIGH);
    int h = this->read_probe_(true);
    digitalWrite(this->tx_, LOW);
    int l2 = this->read_probe_(false);
    delay(hold_ms);
    int l3 = this->read_probe_(false);
    bool low_ok = l1 == 0 && l2 == 0 && l3 == 0;
    if (low_ok)
      this->low_ok_++;
    else
      this->low_bad_++;
    if (h)
      this->high_ok_++;
    else
      this->high_bad_++;
    if (this->probe_is_rx_)
      pinModeRemove(this->probe_, PIN_GPIO);  // hand the RX pad back to the UART
    bool r2 = this->parent_->set_tx_break(false);
    if (!r2)
      this->ret_bad_++;
    ESP_LOGI(TAG, "  break #%u ret=%d probe low=%d/%d/%d (ctl high=%d) held=%ums release ret=%d %s", this->breaks_, r,
             l1, l2, l3, h, hold_ms, r2, low_ok ? "LOW" : "NOT-LOW");
    this->fix_reopen_();
  }

  // experiment: free the TX pad from the GPIO driver, then reopen the port so it can claim the pad
  void fix_reopen_() {
    if (!this->fix_)
      return;
    pinModeRemove(this->tx_, PIN_GPIO);
    auto *hw = static_cast<SerialClass *>(static_cast<uart::LibreTinyUARTComponent *>(this->parent_)->get_hw_serial());
    hw->end();
    hw->begin(this->parent_->get_baud_rate(), SERIAL_8N1, (pin_size_t) this->rx_num_, (pin_size_t) this->tx_);
  }

  void run_cycle_() {
    bool pair = (this->cycle_ % 4) == 3;
    ESP_LOGI(TAG, "cycle %d/%d %s", this->cycle_ + 1, this->cycles_, pair ? "(two breaks back to back)" : "");
    this->loopback_check_("before");
    this->one_break_(500);
    if (pair)
      this->one_break_(500);
    this->tx_activity_check_();
    this->loopback_check_("after");
    this->cycle_++;
  }
#endif

  uart::UARTComponent *inv_{nullptr};
  uint8_t tx_{0}, probe_{0};
  bool probe_is_rx_{false}, loopback_{true}, done_{false}, did_api_{false};
  int cycles_{16}, cycle_{0}, probe_from_{1}, rx_num_{0};
  bool fix_{false};
  uint32_t next_{0}, seq_{0};
  uint32_t breaks_{0}, lb_ok_{0}, lb_bad_{0}, ret_bad_{0}, low_ok_{0}, low_bad_{0}, high_ok_{0}, high_bad_{0};
  uint32_t idle_ok_{0}, idle_bad_{0}, txs_ok_{0}, txs_bad_{0}, junk_{0};
};

}  // namespace esphome::brkbench
