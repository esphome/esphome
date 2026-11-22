#pragma once
// One break cycle per run_next(): 500 ms hold sampled on the pads, then plain release, load_settings() or
// apply_settings_live() with a baud switch during the hold; loopback echo before and after. 21 cycles, then DONE.
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart_component.h"
#ifdef USE_ESP32
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "driver/gpio.h"
#include "hal/uart_ll.h"
#endif
#ifdef USE_ESP8266
#include <Arduino.h>
#include "esphome/components/uart/uart_component_esp8266.h"
#endif

namespace esphome::txbrk {
static const char *const TAG = "txbrk";

class TxBrk : public Component {
 public:
  TxBrk(uart::UARTComponent *u, int tx, int rx, const char *label, bool loopback, bool live, bool hw_uart1)
      : u_(u), tx_(tx), rx_(rx), label_(label), loopback_(loopback), live_(live), hw_uart1_(hw_uart1) {}
  void setup() override {
    this->rel_ok_ = this->u_->set_tx_break(false);
    ESP_LOGI(TAG, "[%s] release without break returned %d", this->label_, this->rel_ok_);
  }
  int level(int pin) {
    if (pin < 0)
      return -1;
#ifdef USE_ESP32
    gpio_input_enable((gpio_num_t) pin);
    return gpio_get_level((gpio_num_t) pin);
#else
    if (this->hw_uart1_ && pin == this->tx_) {
      // UART1 TX on GPIO2: GPI reads 0 while the pad has the UART function, so read the UART1 receiver
      // through the internal loopback (UCLBE): USS bit 15 is the RXD level the receiver sees
      USC0(UART1) |= BIT(UCLBE);
      return (USS(UART1) >> USRXD) & 1;
    }
    return (GPI >> pin) & 1;
#endif
  }
  void dbg(const char *when) {
#ifdef USE_ESP8266
    if (this->hw_uart1_)
      ESP_LOGI(TAG, "[%s] %s USC0=%08x USS=%08x INT_RAW=%08x", this->label_, when, (unsigned) USC0(UART1),
               (unsigned) USS(UART1), (unsigned) USIR(UART1));
#endif
  }
  int regbit() {
#ifdef USE_ESP32
    auto *idf = static_cast<uart::IDFUARTComponent *>(this->u_);
    return UART_LL_GET_HW(idf->get_hw_serial_number())->conf0.txd_inv;
#else
    // UCBRK of UART1 (Serial1, GPIO2) - only meaningful for the hw bus on GPIO2
    if (!this->hw_uart1_)
      return -1;
    return (USC0(UART1) >> UCBRK) & 1;
#endif
  }
  void drain(char *out, size_t outlen, int *count) {
    *count = 0;
    out[0] = 0;
    uint8_t b;
    size_t pos = 0;
    while (this->u_->available() > 0 && this->u_->read_array(&b, 1)) {
      if (pos + 4 < outlen)
        pos += snprintf(out + pos, outlen - pos, "%02X ", b);
      (*count)++;
    }
  }
  bool echo(int i) {
    if (!this->loopback_)
      return true;
    char junk[64];
    int n;
    this->drain(junk, sizeof(junk), &n);
    uint8_t msg[8] = {0x55, 0xA5, 0x00, 0xFF, 'h', 'i', (uint8_t) i, 0x0F};
    this->u_->write_array(msg, sizeof(msg));
    this->u_->flush();
    delay(20 + 8 * 11 * 1000 / this->u_->get_baud_rate());
    uint8_t got[16];
    size_t k = 0;
    while (this->u_->available() > 0 && k < sizeof(got) && this->u_->read_array(&got[k], 1))
      k++;
    bool ok = k == sizeof(msg) && memcmp(got, msg, sizeof(msg)) == 0;
    if (!ok)
      ESP_LOGW(TAG, "[%s] echo mismatch: got %u bytes", this->label_, (unsigned) k);
    return ok;
  }
  // samples pin every 10 ms for n samples, returns count at level `lvl`
  void sample(int n, int lvl, int *rxc, int *txc) {
    *rxc = this->rx_ < 0 ? -1 : 0;
    *txc = this->tx_ < 0 ? -1 : 0;
    for (int s = 0; s < n; s++) {
      if (this->rx_ >= 0 && this->level(this->rx_) == lvl)
        (*rxc)++;
      if (this->tx_ >= 0 && this->level(this->tx_) == lvl)
        (*txc)++;
      delay(10);
    }
  }
  // mode 0: plain hold; 1: load_settings with baud switch during hold; 2: apply_settings_live with baud switch
  void cycle(int i, int mode, bool inverted) {
    int brk = inverted ? 1 : 0, idle = inverted ? 0 : 1;
    if (mode == 2 && !this->live_)
      mode = 1;
    bool pre = this->echo(i);
    int pin_idle_pre = this->level(this->rx_ >= 0 ? this->rx_ : this->tx_);
    int tx_idle_pre = this->level(this->tx_);
    this->u_->flush();
#ifdef USE_ESP8266
    if (this->hw_uart1_) {
      this->level(this->tx_);
      USIC(UART1) = 0x1ff;
      delay(2);
      this->dbg("idle");
      USIC(UART1) = 0x1ff;
    }
#endif
    bool r1 = this->u_->set_tx_break(true);
    int reg_held = this->regbit();
    int rx_held, tx_held;
    this->sample(50, brk, &rx_held, &tx_held);  // 500 ms
    this->dbg("held");
    int rx_rel = -1, tx_rel = -1, reg_rel = -1;
    uint32_t newbaud = this->u_->get_baud_rate() == 9600 ? 115200 : 9600;
    bool live_ok = true;
    if (mode != 0) {
      this->u_->set_baud_rate(newbaud);
#ifdef USE_ESP32
      if (mode == 2) {
        esp_err_t err = static_cast<uart::IDFUARTComponent *>(this->u_)->apply_settings_live();
        live_ok = err == ESP_OK;
        if (!live_ok)
          ESP_LOGW(TAG, "[%s] apply_settings_live failed: %d", this->label_, err);
      } else
#endif
        this->u_->load_settings(false);
      this->sample(20, brk, &rx_rel, &tx_rel);
      reg_rel = this->regbit();
    }
    // no UART reads while the break is held (set_tx_break() contract): release first, then drain what came in
    bool r2 = this->u_->set_tx_break(false);
    char junk[64];
    int njunk;
    this->drain(junk, sizeof(junk), &njunk);  // anything during the hold
    char junk_hold[64];
    strncpy(junk_hold, junk, sizeof(junk_hold));
    int nhold = njunk;
    delay(5);
    int reg_after = this->regbit();
    this->dbg("released");
    int rx_after, tx_after;
    this->sample(5, idle, &rx_after, &tx_after);
    delay(20);
    this->drain(junk, sizeof(junk), &njunk);
    bool post = this->echo(i + 100);
    // the line must idle before the break too, or a wrong initial level could still pass with loopback off
    bool idle_ok = (pin_idle_pre < 0 || pin_idle_pre == idle) && (tx_idle_pre < 0 || tx_idle_pre == idle);
    bool pass = live_ok && idle_ok && r1 && r2 && pre && post && (this->rx_ < 0 || rx_held == 50) &&
                (this->tx_ < 0 || tx_held == 50) &&
                (mode == 0 || ((this->rx_ < 0 || rx_rel == 20) && (this->tx_ < 0 || tx_rel == 20))) &&
                (this->rx_ < 0 || rx_after == 5) && (this->tx_ < 0 || tx_after == 5);
    ESP_LOGI(TAG,
             "[%s] CYC %d mode=%d baud=%u inv=%d idle_pre rx=%d tx=%d | set(1)=%d reg=%d held rx=%d/50 tx=%d/50 | "
             "reload rx=%d/20 tx=%d/20 reg=%d | hold_rx_bytes=%d [%s] | set(0)=%d reg=%d after rx=%d/5 tx=%d/5 "
             "rel_bytes=%d [%s] | echo pre=%d post=%d => %s",
             this->label_, i, mode, (unsigned) this->u_->get_baud_rate(), inverted, pin_idle_pre, tx_idle_pre, r1,
             reg_held, rx_held, tx_held, rx_rel, tx_rel, reg_rel, nhold, junk_hold, r2, reg_after, rx_after, tx_after,
             njunk, junk, pre, post, pass ? "PASS" : "FAIL");
    if (pass)
      this->passes_++;
    this->runs_++;
  }
  void run_next(bool inverted) {
    if (this->runs_ >= 21) {
      if (this->runs_ == 21) {
        // a failed release without break in setup() costs one pass
        int passes = this->passes_ - (this->rel_ok_ || this->passes_ == 0 ? 0 : 1);
        ESP_LOGI(TAG, "[%s] DONE %d/%d passed%s", this->label_, passes, this->runs_,
                 this->rel_ok_ ? "" : ", release without break failed");
        this->runs_++;
      }
      return;
    }
    this->cycle(this->runs_, this->runs_ % 3, inverted);
  }
  int runs_{0}, passes_{0};

 protected:
  uart::UARTComponent *u_;
  int tx_, rx_;
  const char *label_;
  bool loopback_, live_, hw_uart1_;
  bool rel_ok_{true};
};
}  // namespace esphome::txbrk
