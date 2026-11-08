#pragma once
// BMP390 bench helpers: register dump, INT data-ready edge counter, start write fault injection (software NACK or
// SDA held through the GPIO matrix) and a bus probe. ESP32-S3, ESP-IDF.
#include <cmath>
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"
#include "esp_heap_caps.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_periph.h"

namespace bmpt {
static const char *const TAG = "bmpt";

inline volatile uint32_t g_int_edges = 0;
inline void int_isr(void *) { g_int_edges = g_int_edges + 1; }
inline void int_counter_begin(int pin) {
  gpio_config_t c{};
  c.pin_bit_mask = 1ULL << pin;
  c.mode = GPIO_MODE_INPUT;
  c.pull_down_en = GPIO_PULLDOWN_ENABLE;
  c.intr_type = GPIO_INTR_POSEDGE;
  gpio_config(&c);
  esp_err_t e = gpio_install_isr_service(0);
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE)
    ESP_LOGE(TAG, "isr service %d", e);
  gpio_isr_handler_add((gpio_num_t) pin, int_isr, nullptr);
}

inline int g_sda = -1;
inline int g_port = 0;
inline bool g_held = false;
inline int g_hold_level = 0;  // 0: SDA shorted low, 1: SDA disconnected from the controller (open line, pulled high)
inline bool g_bus_reset_on_probe = false;
inline uint32_t g_out_sel = 0;
inline bool g_saved = false;
inline bool g_log_pad_once = true;
inline uint32_t out_sel_reg() { return REG_READ(GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * g_sda); }
inline uint32_t pin_reg() { return REG_READ(GPIO_PIN0_REG + 4 * g_sda); }
inline uint32_t mux_reg() { return REG_READ(GPIO_PIN_MUX_REG[g_sda]); }
inline uint32_t en_bit() { return (REG_READ(GPIO_ENABLE_REG) >> g_sda) & 1; }
inline uint32_t g_pin = 0, g_mux = 0, g_en = 0;
inline void log_pad(const char *when) {
  ESP_LOGI(TAG, "PAD %s out_sel=0x%08X pin=0x%08X mux=0x%08X en=%u", when, (unsigned) out_sel_reg(),
           (unsigned) pin_reg(), (unsigned) mux_reg(), (unsigned) en_bit());
}
inline void sda_hold() {
  if (!g_saved) {
    g_out_sel = out_sel_reg();  // routing the i2c driver set up; restored verbatim on release
    g_pin = pin_reg();
    g_mux = mux_reg();
    g_en = en_bit();
    log_pad("before-hold");
    g_saved = true;
    ESP_LOGI(TAG, "SDA out_sel saved 0x%08X (I2CEXT0_SDA_OUT_IDX=%d port=%d)", (unsigned) g_out_sel,
             I2CEXT0_SDA_OUT_IDX, g_port);
  }
  gpio_set_level((gpio_num_t) g_sda, g_hold_level);
  esp_rom_gpio_connect_out_signal(g_sda, SIG_GPIO_OUT_IDX, false, false);
  g_held = true;
  if (g_log_pad_once) {
    log_pad("held");
  }
}
inline void sda_release() {
  gpio_set_level((gpio_num_t) g_sda, 1);
  REG_WRITE(GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * g_sda, g_out_sel);
  REG_WRITE(GPIO_PIN0_REG + 4 * g_sda, g_pin);
  REG_WRITE(GPIO_PIN_MUX_REG[g_sda], g_mux);
  if (g_en)
    REG_WRITE(GPIO_ENABLE_W1TS_REG, 1u << g_sda);
  else
    REG_WRITE(GPIO_ENABLE_W1TC_REG, 1u << g_sda);
  g_held = false;
  if (g_log_pad_once) {
    log_pad("released");
    g_log_pad_once = false;
  }
}

// Wraps the sensor's bus: injects the start write faults and logs every failed transfer
class FaultBus : public esphome::i2c::I2CBus {
 public:
  explicit FaultBus(esphome::i2c::I2CBus *real) : real_(real) {}
  uint32_t hold_until{0};  // millis() deadline of a timed hold
  bool arm_start{false};   // hold SDA only during the next forced mode start write (PWR_CTRL mode bits 01)
  // until this deadline every transfer fails on its own: free-bus dummy read, hold, transfer, release
  uint32_t window_until{0};
  bool soft_arm{false};    // next start write fails with NACK in software, the wire is not touched
  uint32_t soft_until{0};  // every start write fails with NACK in software until this deadline
  uint32_t xfers{0}, fails{0}, start_writes{0}, start_fails{0}, normal_writes{0};
  esphome::i2c::ErrorCode write_readv(uint8_t address, const uint8_t *w, size_t wn, uint8_t *r, size_t rn) override {
    // mode bits of a PWR_CTRL write: 1 forced (a start), 3 normal
    int mode = wn == 2 && w[0] == 0x1B ? (w[1] >> 4) & 3 : -1;
    bool is_start = mode == 1;
    bool targeted = is_start && this->arm_start;
    if (is_start && (this->soft_arm || (int32_t) (this->soft_until - esphome::millis()) > 0)) {
      this->soft_arm = false;
      this->start_writes++;
      this->start_fails++;
      this->xfers++;
      this->fails++;
      ESP_LOGI(TAG, "SOFTFAIL start write reg=0x1B val=0x%02X -> NACK", w[1]);
      return esphome::i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    bool timed = (int32_t) (this->hold_until - esphome::millis()) > 0;
    bool window = (int32_t) (this->window_until - esphome::millis()) > 0;
    if (window) {
      // let the driver run its post-error FSM reset with free lines, so each injected failure starts from a clean bus
      uint8_t reg = 0x00, v;
      this->real_->write_readv(address, &reg, 1, &v, 1);
      targeted = true;
    }
    if (timed || targeted)
      sda_hold();  // reasserted per transfer in case the driver re-routes the pin after an error
    if (is_start)
      this->start_writes++;
    if (mode == 3)
      this->normal_writes++;
    auto err = this->real_->write_readv(address, w, wn, r, rn);
    if (targeted) {
      this->arm_start = false;
      sda_release();
    } else if (!timed && g_held) {
      sda_release();
    }
    this->xfers++;
    if (err != esphome::i2c::ERROR_OK) {
      this->fails++;
      if (is_start)
        this->start_fails++;
      ESP_LOGI(TAG, "XFERFAIL addr=0x%02X reg=0x%02X wn=%u rn=%u val=0x%02X err=%d start=%d", address,
               wn > 0 ? w[0] : 0xFF, (unsigned) wn, (unsigned) rn, wn > 1 ? w[1] : 0, (int) err, is_start);
    }
    return err;
  }

 protected:
  esphome::i2c::I2CBus *real_;
};
inline FaultBus *g_fb = nullptr;
inline esphome::i2c::I2CBus *g_real = nullptr;

// Release a timed hold once its deadline passes even when no transfer happens.
inline void fault_tick() {
  if (g_fb != nullptr && g_held && !g_fb->arm_start && (int32_t) (g_fb->hold_until - esphome::millis()) <= 0)
    sda_release();
}

inline bool rd(uint8_t addr, uint8_t reg, uint8_t *v, size_t n = 1) {
  return g_real->write_readv(addr, &reg, 1, v, n) == esphome::i2c::ERROR_OK;
}
inline bool wr(uint8_t addr, uint8_t reg, uint8_t v) {
  uint8_t b[2] = {reg, v};
  return g_real->write_readv(addr, b, 2, nullptr, 0) == esphome::i2c::ERROR_OK;
}

inline void dump_regs(uint8_t addr) {
  uint8_t chip = 0, err = 0, ictl = 0, pwr = 0, osr = 0, odr = 0, cfg = 0;
  bool ok = rd(addr, 0x00, &chip) && rd(addr, 0x02, &err) && rd(addr, 0x19, &ictl) && rd(addr, 0x1B, &pwr) &&
            rd(addr, 0x1C, &osr) && rd(addr, 0x1D, &odr) && rd(addr, 0x1F, &cfg);
  ESP_LOGI(TAG, "REGS ok=%d chip=0x%02X err=0x%02X int_ctrl=0x%02X pwr=0x%02X mode=%u osr=0x%02X odr=0x%02X cfg=0x%02X",
           ok, chip, err, ictl, pwr, (pwr >> 4) & 3, osr, odr, cfg);
}

inline void log_heap() {
  ESP_LOGI(TAG, "HEAP internal_free=%u internal_total=%u min_free=%u",
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
}

// Probe the chip; on failure log the line levels, reset the IDF bus once and probe again.
inline void probe(uint8_t addr, int scl) {
  uint8_t v = 0;
  bool ok = rd(addr, 0x00, &v);
  ESP_LOGI(TAG, "PROBE %s chip=0x%02X held=%d sel=0x%08X sda=%d scl=%d", ok ? "ok" : "err", v, g_held,
           (unsigned) out_sel_reg(), gpio_get_level((gpio_num_t) g_sda), gpio_get_level((gpio_num_t) scl));
  if (ok || !g_bus_reset_on_probe)
    return;
  i2c_master_bus_handle_t h;
  esp_err_t e = i2c_master_get_bus_handle((i2c_port_num_t) g_port, &h);
  if (e == ESP_OK)
    e = i2c_master_bus_reset(h);
  ok = rd(addr, 0x00, &v);
  ESP_LOGI(TAG, "PROBE after bus reset (%d): %s chip=0x%02X", e, ok ? "ok" : "err", v);
}
}  // namespace bmpt
