#pragma once
// Device check of register writes through a TCA9548A channel (#22827). ESP32: the bus is recorded by the
// pdmux.hpp target (fake mux + devices, SCL rate per transaction). ESP8266: MuxShim answers the mux on the real Wire
// bus and records each upstream call as a byte stream. Mallocs counted per call; a 24 byte register write load
// (SSD1306 shaped) reports mallocs and free heap per round.
#include <cstring>
#include "pdmux.hpp"
#include <string>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"
#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif

static volatile bool mx_window = false;
#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
static TaskHandle_t mx_task = nullptr;  // count only the calling task's mallocs (other tasks run on the other core)
#define MX_THIS_TASK (mx_task == nullptr || xTaskGetCurrentTaskHandle() == mx_task)
#else
#define MX_THIS_TASK true
#endif
static volatile uint32_t mx_allocs = 0;
static void *volatile mx_caller = nullptr;
static volatile size_t mx_size = 0;
extern "C" void *__real_malloc(size_t n);
#ifdef USE_ESP8266
extern "C" void *IRAM_ATTR __wrap_malloc(size_t n) {
#else
extern "C" void *__wrap_malloc(size_t n) {
#endif
  if (mx_window && MX_THIS_TASK) {
    mx_allocs = mx_allocs + 1;
    mx_caller = __builtin_return_address(0);
    mx_size = n;
  }
  return __real_malloc(n);
}

#ifndef MX_GAP_US
#define MX_GAP_US 0
#endif
namespace mx {
static const char *const TAG = "mx";

static uint32_t fnv_add(uint32_t h, const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}
static uint32_t fnv(const std::string &s) { return fnv_add(2166136261u, (const uint8_t *) s.data(), s.size()); }

#ifndef USE_ESP32
// Answers the TCA9548A at 0x70 itself, forwards everything else to the real bus; records one token per upstream call
// with the bytes joined, so a joined write_readv() and a two-part write_prefixed() record the same
class MuxShim final : public i2c::I2CBus {
 public:
  i2c::I2CBus *real{nullptr};
  uint8_t mask{0};
  char log[2048];
  size_t pos{0};
  void add(const char *fmt, unsigned a, unsigned b = 0, unsigned c = 0, unsigned d = 0, unsigned e = 0) {
    if (pos < sizeof log - 48)
      pos += snprintf(log + pos, sizeof log - pos, fmt, a, b, c, d, e);
  }
  i2c::ErrorCode write_readv(uint8_t a, const uint8_t *w, size_t wc, uint8_t *r, size_t rc) override {
    if (a == 0x70) {
      if (wc == 1)
        mask = w[0];
      for (size_t i = 0; i < rc; i++)
        r[i] = mask;
      add("M%02X/%u ", wc ? w[0] : 0xFF, (unsigned) rc);
      return i2c::ERROR_OK;
    }
    auto e = real->write_readv(a, w, wc, r, rc);
    add("D%02X:%u:%08X:%u>%u ", a, (unsigned) wc, fnv_add(2166136261u, w, wc), (unsigned) rc, (unsigned) e);
    return e;
  }
#ifdef MX_WRITE_PREFIXED  // trees with the two-part I2CBus::write_prefixed (#22780)
  i2c::ErrorCode write_prefixed(uint8_t a, const uint8_t *p, size_t pl, const uint8_t *d, size_t len) override {
    auto e = real->write_prefixed(a, p, pl, d, len);
    add("D%02X:%u:%08X:0>%u ", a, (unsigned) (pl + len), fnv_add(fnv_add(2166136261u, p, pl), d, len), (unsigned) e);
    return e;
  }
#endif
};
static MuxShim shim;
#endif

static void clear_log() {
#ifdef USE_ESP32
  pds::ln = 0;
  pds::lovf = 0;
#else
  shim.pos = 0;
  shim.log[0] = 0;
#endif
}
static std::string get_log() {
#ifdef USE_ESP32
  delayMicroseconds(500);
  return pds::tokens();
#else
  return std::string(shim.log, shim.pos);
#endif
}
static uint32_t free_heap() {
#ifdef USE_ESP32
  return heap_caps_get_free_size(MALLOC_CAP_8BIT);
#else
  return ESP.getFreeHeap();
#endif
}

static i2c::I2CBus *root = nullptr, *ch2 = nullptr;
static uint8_t data[320];
static uint8_t rdata[16];
#ifdef MX_LOAD_ONLY
static uint32_t idx = 605 + 113, nalloc = 0;
#else
static uint32_t idx = 0, nalloc = 0;
#endif
static int round_no = -1;
static bool done = false;

template<typename F> static void run(const std::string &label, F fn) {
  clear_log();
  mx_allocs = 0;
  mx_window = true;
  i2c::ErrorCode rc = fn();
  mx_window = false;
  nalloc += mx_allocs != 0;
  std::string got = get_log();
  ESP_LOGI(TAG, "MX %s|rc=%d|alloc=%u|fnv=%08X|%s", label.c_str(), (int) rc, (unsigned) mx_allocs, (unsigned) fnv(got),
           got.substr(0, 110).c_str());
  if (mx_allocs)
    ESP_LOGI(TAG, "MXALLOC %s|caller=%p size=%u", label.c_str(), mx_caller, (unsigned) mx_size);
  if (got.size() > 110)
    ESP_LOGI(TAG, "MXTAIL %s|...%s", label.c_str(), got.substr(got.size() - 110).c_str());
  App.feed_wdt();
}

static void step(i2c::I2CDevice *oled, i2c::I2CDevice *ads) {
  if (done)
    return;
#ifdef USE_ESP32
  mx_task = xTaskGetCurrentTaskHandle();
#endif
  uint32_t i = idx++;
  for (size_t k = 0; k < sizeof data; k++)
    data[k] = static_cast<uint8_t>(0x11 + i + k * 7);
  if (i <= 300) {
    run("wreg8 len=" + std::to_string(i), [&] { return oled->write_register(0x40, data, i); });
  } else if (i <= 601) {
    size_t len = i - 301;
    run("wreg16 len=" + std::to_string(len), [&] { return oled->write_register16(0x4041, data, len); });
  } else if (i < 602 + 3) {
    size_t n = 1u << (i - 602);
    run("rreg8 n=" + std::to_string(n), [&] { return ads->read_register(0x01, rdata, n); });
    run("rreg16 n=" + std::to_string(n), [&] { return ads->read_register16(0x0100, rdata, n); });
    run("read n=" + std::to_string(n), [&] { return ads->read(rdata, n); });
    run("write len=" + std::to_string(n * 8), [&] { return oled->write(data, n * 8); });
  } else if (i < 605 + 112) {
    uint8_t a = 8 + (i - 605);
    run("probe ch2 " + std::to_string(a), [&] { return ch2->write_readv(a, nullptr, 0, nullptr, 0); });
  } else if (i == 605 + 112) {
    // a channel write followed by a transfer on the root bus, which shows the restored frequency
    run("wreg8 len=24 then root probe 0x50", [&] {
      auto e = oled->write_register(0x40, data, 24);
      root->write_readv(0x50, nullptr, 0, nullptr, 0);
      return e;
    });
  } else if (round_no < 6 || (MX_GAP_US != 0 && round_no < 9)) {
    // load: 500 register writes of 24 bytes per round; round 0 is a warm-up
    round_no++;
    uint32_t allocs = 0, errs = 0;
    for (int k = 0; k < 500; k++) {
      clear_log();
      mx_allocs = 0;
      mx_window = true;
      if (oled->write_register(0x40, data, 24) != i2c::ERROR_OK)
        errs++;
      if (round_no > 6)
        delayMicroseconds(MX_GAP_US);
      mx_window = false;
      allocs += mx_allocs;
      if ((k & 31) == 0)
        App.feed_wdt();
    }
    ESP_LOGI(TAG, "%s round=%d writes=500 allocs=%u errors=%u free_heap=%u", round_no > 6 ? "LOADGAP" : "LOAD",
             round_no, (unsigned) allocs, (unsigned) errs, (unsigned) free_heap());
  } else {
    done = true;
    ESP_LOGI(TAG, "MXDONE cases=%u alloc_calls=%u", (unsigned) idx - 1 - 7, (unsigned) nalloc);
  }
}
}  // namespace mx
