#pragma once
// esphome#22780 sweep for the Arduino (ESP8266, RP2040) and software (BK72xx) buses: one "SW" line per I2CDevice call
// with rc, mallocs during the call (alloc_count.hpp) and, where a sniffer exists (ESP8266, RP2), the decoded wire
// stream and its hash.
#include <cstring>
#include <functional>
#include <string>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"
#include "alloc_count.hpp"
#include "wiredecode.hpp"
#ifdef USE_RP2
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/structs/sio.h>
#endif

#ifndef SWP_ADDR
#define SWP_ADDR 0x48
#endif
#ifndef SWP_WREG
#define SWP_WREG 0x02
#endif
#ifndef SWP_RREG
#define SWP_RREG 0x01
#endif
#ifndef SWP_MAXLEN8
#define SWP_MAXLEN8 300
#endif
#ifndef SWP_MAXLEN16
#define SWP_MAXLEN16 300
#endif
#ifndef SWP_SDA
#define SWP_SDA 4
#endif
#ifndef SWP_SCL
#define SWP_SCL 5
#endif

namespace swp {
static const char *const TAG = "swp";

#if defined(USE_ESP8266) || defined(USE_RP2)
#define SWP_SNIFF 1
static constexpr uint32_t N = 12000;
static uint8_t buf[N];
static volatile uint32_t n = 0, ovf = 0;
static volatile uint8_t last = 3;
static volatile bool armed = false;
static inline __attribute__((always_inline)) uint8_t levels() {
#ifdef USE_ESP8266
  uint32_t v = GPI;
#else
  uint32_t v = sio_hw->gpio_in;
#endif
  return ((v >> SWP_SDA) & 1) | (((v >> SWP_SCL) & 1) << 1);
}
#ifdef USE_ESP8266
static void IRAM_ATTR isr() {
#else
static void isr() {
  for (int p : {SWP_SDA, SWP_SCL}) {
    uint32_t ev = gpio_get_irq_event_mask(p);
    if (ev)
      gpio_acknowledge_irq(p, ev);
  }
#endif
  uint8_t s = levels();
  if (!armed || s == last)
    return;
  last = s;
  if (n < N)
    buf[n++] = s;
  else
    ovf = ovf + 1;
}
static void install() {
#ifdef USE_ESP8266
  attachInterrupt(SWP_SDA, isr, CHANGE);
  attachInterrupt(SWP_SCL, isr, CHANGE);
#else
  gpio_add_raw_irq_handler_masked((1u << SWP_SDA) | (1u << SWP_SCL), isr);
  gpio_set_irq_enabled(SWP_SDA, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  gpio_set_irq_enabled(SWP_SCL, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
  irq_set_enabled(IO_IRQ_BANK0, true);
#endif
  ESP_LOGI(TAG, "sniffer on SDA %d SCL %d", SWP_SDA, SWP_SCL);
}
#else
#define SWP_SNIFF 0
static void install() {}
#endif

static i2c::I2CBus *bus = nullptr;
static uint8_t data[320];
static uint8_t rdata[40];
static uint32_t idx = 0, nalloc = 0;
static bool done = false, probed = false;
#if SWP_SNIFF
static uint8_t start = 3;  // levels when the sniffer was armed
#endif

static void arm() {
#if SWP_SNIFF
  n = 0;
  ovf = 0;
  start = levels();
  last = start;
  armed = true;
#endif
  alloc_count::count = 0;
  alloc_count::window = true;
}
static void disarm() {
  alloc_count::window = false;
#if SWP_SNIFF
  armed = false;
#endif
}
static std::string sniffed(uint32_t &amb) {
#if SWP_SNIFF
  return wiredecode::decode(buf, n, start, amb);
#else
  return {};
#endif
}

static bool step(i2c::I2CDevice *dev) {
  if (done)
    return false;
  if (!probed) {
    // the counter must see a malloc inside the window
    alloc_count::count = 0;
    alloc_count::window = true;
    void *volatile p = malloc(100);
    alloc_count::window = false;
    free(p);
    ESP_LOGI(TAG, "ALLOC_PROBE %u", (unsigned) alloc_count::count);
    probed = true;
  }
  const uint32_t W8 = SWP_MAXLEN8 + 1, W16 = SWP_MAXLEN16 + 1;
  std::string label;
  uint32_t i = idx++;
  for (size_t k = 0; k < sizeof data; k++)
    data[k] = static_cast<uint8_t>(0x11 + i + k * 7);
  memset(rdata, 0, sizeof rdata);
  i2c::ErrorCode rc;
  if (i < W8) {
    label = "wreg8 len=" + std::to_string(i);
    arm();
    rc = dev->write_register(SWP_WREG, data, i);
    disarm();
  } else if (i < W8 + W16) {
    size_t len = i - W8;
    label = "wreg16 len=" + std::to_string(len);
    arm();
    rc = dev->write_register16((SWP_WREG << 8) | 0x03, data, len);
    disarm();
  } else if (i < W8 + W16 + 10) {
    static const size_t ns[] = {1, 2, 3, 4, 8};
    uint32_t j = i - W8 - W16;
    size_t k = ns[j % 5];
    bool sixteen = j >= 5;
    label = std::string(sixteen ? "rreg16" : "rreg8") + " n=" + std::to_string(k);
    arm();
    rc = sixteen ? dev->read_register16(SWP_RREG << 8, rdata, k) : dev->read_register(SWP_RREG, rdata, k);
    disarm();
  } else if (i < W8 + W16 + 13) {
    size_t k = i - (W8 + W16 + 10) + 1;
    label = "read n=" + std::to_string(k);
    arm();
    rc = dev->read(rdata, k);
    disarm();
  } else if (i < W8 + W16 + 16) {
    static const uint8_t p1[] = {SWP_RREG};
    size_t k = i - (W8 + W16 + 13) + 1;
    label = "write1+read n=" + std::to_string(k);
    arm();
    rc = dev->write_read(p1, 1, rdata, k);
    disarm();
  } else if (i < W8 + W16 + 16 + 112) {
    uint8_t a = 8 + (i - (W8 + W16 + 16));
    label = "probe " + std::to_string(a);
    arm();
    rc = bus->write_readv(a, nullptr, 0, nullptr, 0);
    disarm();
  } else {
    done = true;
    ESP_LOGI(TAG, "SWDONE cases=%u alloc_calls=%u", (unsigned) idx - 1, (unsigned) nalloc);
    return false;
  }
  nalloc += alloc_count::count != 0;
  uint32_t amb = 0, events = 0, overflow = 0;
  std::string got = sniffed(amb);
#if SWP_SNIFF
  events = n;
  overflow = ovf;
#endif
  char rd[3 * 8 + 1] = "";
  for (int k = 0; k < 8 && label.rfind("wreg", 0) != 0 && label.rfind("probe", 0) != 0; k++)
    snprintf(rd + 3 * k, 4, "%02X ", rdata[k]);
  ESP_LOGI(TAG, "SW %s|rc=%d|alloc=%u|ev=%u|amb=%u|ovf=%u|fnv=%08X|%s|rd=%s", label.c_str(), (int) rc,
           (unsigned) alloc_count::count, (unsigned) events, (unsigned) amb, (unsigned) overflow,
           (unsigned) fnv1a_hash(got), got.substr(0, 90).c_str(), rd);
  App.feed_wdt();
  return true;
}

// Over-long register write and what the bus does afterwards (on arduino-pico this leaves Wire stuck, dev too).
// Each step is sniffed; the probe step shows whether a stale buffer goes out.
static void wedge_step(const char *what, const std::function<i2c::ErrorCode()> &fn) {
  uint32_t amb = 0;
  arm();
  auto rc = fn();
  disarm();
  std::string got = sniffed(amb);
  ESP_LOGI(TAG, "WEDGE %s rc=%d ev=%u wire=%s", what, (int) rc, (unsigned) got.size(), got.substr(0, 120).c_str());
  App.feed_wdt();
}
static void wedge(i2c::I2CDevice *dev) {
  static uint8_t r[2];
  wedge_step("read_register before", [dev] { return dev->read_register(SWP_RREG, r, 2); });
  wedge_step("plain read before", [dev] { return dev->read(r, 1); });
  wedge_step("write_register len=300", [dev] { return dev->write_register(SWP_WREG, data, 300); });
  wedge_step("read_register after", [dev] { return dev->read_register(SWP_RREG, r, 2); });
  wedge_step("plain read after", [dev] { return dev->read(r, 1); });
  wedge_step("write_register len=2 after", [dev] { return dev->write_register(SWP_WREG, data, 2); });
  wedge_step("probe 0x50 after", [] { return bus->write_readv(0x50, nullptr, 0, nullptr, 0); });
  wedge_step("plain read after probe", [dev] { return dev->read(r, 1); });
  wedge_step("write_register len=2 after probe", [dev] { return dev->write_register(SWP_WREG, data, 2); });
}
}  // namespace swp
