#pragma once
// esphome#22780 device check: GPIO interrupt sniffer on the I2C pads, a sweep of I2CDevice call shapes, and a
// malloc counter (alloc_count.hpp, -Wl,--wrap=malloc). One "SN" line per case: rc, mallocs, decoded wire stream
// hash, match with the stream the call should produce.
#include <cstring>
#include <string>
#include <driver/gpio.h>
#include <soc/gpio_reg.h>
#include <esp_attr.h>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"
#include "alloc_count.hpp"
#include "wiredecode.hpp"
#ifdef SNF_PDSLAVE
// the board is its own target (pdslave.hpp); its token stream replaces the pad sniffer's decode
#include "pdslave.hpp"
#endif

namespace snf {
static const char *const TAG = "snf";
#ifndef SNF_SDA
#define SNF_SDA 4
#endif
#ifndef SNF_SCL
#define SNF_SCL 5
#endif
static constexpr size_t N = 24000;
static DRAM_ATTR uint8_t buf[N];
static volatile DRAM_ATTR uint32_t n = 0;
static volatile DRAM_ATTR uint32_t ovf = 0;
static volatile DRAM_ATTR uint8_t last = 3;
static volatile DRAM_ATTR bool armed = false;
static uint8_t start = 3;  // levels when the sniffer was armed

static void IRAM_ATTR isr(void *) {
  uint32_t v = REG_READ(GPIO_IN_REG);
  uint8_t s = ((v >> SNF_SDA) & 1) | (((v >> SNF_SCL) & 1) << 1);
  if (!armed || s == last)
    return;
  last = s;
  if (n < N)
    buf[n++] = s;
  else
    ovf = ovf + 1;
}

static void install() {
  gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  for (int p : {SNF_SDA, SNF_SCL}) {
    gpio_set_intr_type((gpio_num_t) p, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add((gpio_num_t) p, isr, nullptr);
    gpio_intr_enable((gpio_num_t) p);
  }
  ESP_LOGI(TAG, "sniffer on SDA %d SCL %d", SNF_SDA, SNF_SCL);
}

static i2c::I2CBus *bus = nullptr;  // the bus the device uses (hardware or the software copy)

static void wb(std::string &e, uint8_t b) {
  char t[8];
  snprintf(t, sizeof t, "%02X+ ", b);
  e += t;
}
static void rb(std::string &e, const uint8_t *r, size_t k) {
  char t[8];
  for (size_t i = 0; i < k; i++) {
    snprintf(t, sizeof t, "r%02X%c ", r[i], i + 1 < k ? '+' : '-');
    e += t;
  }
}

static uint8_t data[320];
static uint8_t rdata[40];
static uint32_t idx = 0, nok = 0, ndiff = 0, namb = 0, nalloc = 0;
static bool done = false;

static void arm() {
  n = 0;
  ovf = 0;
  uint32_t v = REG_READ(GPIO_IN_REG);
  start = ((v >> SNF_SDA) & 1) | (((v >> SNF_SCL) & 1) << 1);
  last = start;
  alloc_count::count = 0;
  alloc_count::window = true;
#ifdef SNF_PDSLAVE
  pds::ln = 0;
  pds::lovf = 0;
#endif
  armed = true;
}
static void disarm() {
  armed = false;
  alloc_count::window = false;
}

// Runs one case and returns false when the sweep is over
static bool step(i2c::I2CDevice *dev) {
  if (done)
    return false;
  const uint8_t A = 0x48;
  std::string label, exp;
#ifdef SNF_PDSLAVE
  if (idx == 0) {
    // warm the target ISR (it runs from flash) so the first case is not lost to a cache miss
    bus->write_readv(0x48, nullptr, 0, nullptr, 0);
    delay(5);
  }
#endif
  uint32_t i = idx++;
  for (size_t k = 0; k < sizeof data; k++)
    data[k] = static_cast<uint8_t>(0x11 + i + k * 7);
  i2c::ErrorCode rc;
  static constexpr uint32_t W8 = 301, W16 = 301;  // write_register lengths 0..300
  if (i < W8) {
    size_t len = i;
    label = "wreg8 len=" + std::to_string(len);
    arm();
    rc = dev->write_register(0x02, data, len);
    disarm();
    exp = "S ";
    wb(exp, A << 1);
    wb(exp, 0x02);
    for (size_t k = 0; k < len; k++)
      wb(exp, data[k]);
    exp += "P ";
  } else if (i < W8 + W16) {
    size_t len = i - W8;
    label = "wreg16 len=" + std::to_string(len);
    arm();
    rc = dev->write_register16(0x0203, data, len);
    disarm();
    exp = "S ";
    wb(exp, A << 1);
    wb(exp, 0x02);
    wb(exp, 0x03);
    for (size_t k = 0; k < len; k++)
      wb(exp, data[k]);
    exp += "P ";
  } else if (i < W8 + W16 + 10) {
    static const size_t ns[] = {1, 2, 3, 4, 8};
    uint32_t j = i - W8 - W16;
    size_t k = ns[j % 5];
    bool sixteen = j >= 5;
    label = std::string(sixteen ? "rreg16" : "rreg8") + " n=" + std::to_string(k);
    memset(rdata, 0, sizeof rdata);
    arm();
    rc = sixteen ? dev->read_register16(0x0100, rdata, k) : dev->read_register(0x01, rdata, k);
    disarm();
    exp = "S ";
    wb(exp, A << 1);
    wb(exp, 0x01);
    if (sixteen)
      wb(exp, 0x00);
    exp += "Sr ";
    wb(exp, (A << 1) | 1);
    rb(exp, rdata, k);
    exp += "P ";
  } else if (i < W8 + W16 + 13) {
    size_t k = i - (W8 + W16 + 10) + 1;
    label = "read n=" + std::to_string(k);
    arm();
    rc = dev->read(rdata, k);
    disarm();
    exp = "S ";
    wb(exp, (A << 1) | 1);
    rb(exp, rdata, k);
    exp += "P ";
  } else if (i < W8 + W16 + 16) {
    static const uint8_t p1[] = {0x02};
    size_t k = i - (W8 + W16 + 13) + 1;
    label = "write1+read n=" + std::to_string(k);
    arm();
    rc = dev->write_read(p1, 1, rdata, k);
    disarm();
    exp = "S ";
    wb(exp, A << 1);
    wb(exp, 0x02);
    exp += "Sr ";
    wb(exp, (A << 1) | 1);
    rb(exp, rdata, k);
    exp += "P ";
  } else if (i < W8 + W16 + 16 + 112) {
    uint8_t a = 8 + (i - (W8 + W16 + 16));
    label = "probe " + std::to_string(a);
    arm();
    rc = bus->write_readv(a, nullptr, 0, nullptr, 0);
    disarm();
    exp = "S ";
    char t[8];
    snprintf(t, sizeof t, "%02X%c ", a << 1, a == A ? '+' : '-');
    exp += t;
    exp += "P ";
  } else {
    done = true;
    ESP_LOGI(TAG, "SNDONE cases=%u ok=%u diff=%u amb=%u alloc_calls=%u", (unsigned) idx - 1, (unsigned) nok,
             (unsigned) ndiff, (unsigned) namb, (unsigned) nalloc);
    return false;
  }
  uint32_t amb = 0;
#ifdef SNF_PDSLAVE
  delayMicroseconds(500);
  std::string got = pds::tokens();
  ovf = pds::lovf;
  n = pds::ln;
#else
  std::string got = wiredecode::decode(buf, n, start, amb);
#endif
  bool ok = got == exp && ovf == 0;
  nok += ok;
  ndiff += !ok;
  namb += amb;
  nalloc += alloc_count::count != 0;
  ESP_LOGI(TAG, "SN %s|rc=%d|alloc=%u|ev=%u|amb=%u|ovf=%u|fnv=%08X|%s", label.c_str(), (int) rc,
           (unsigned) alloc_count::count, (unsigned) n, (unsigned) amb, (unsigned) ovf, (unsigned) fnv1a_hash(got),
           ok ? "OK" : "DIFF");
  if (!ok) {
    ESP_LOGI(TAG, "SNGOT %s", got.substr(0, 200).c_str());
    ESP_LOGI(TAG, "SNEXP %s", exp.substr(0, 200).c_str());
  }
  App.feed_wdt();
  return true;
}

#ifdef SNF_SW
static i2c::I2CBus *make_sw() {
  auto *sda = new esp32::ESP32InternalGPIOPin();
  sda->set_pin((gpio_num_t) SNF_SDA);
  sda->set_flags(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  auto *scl = new esp32::ESP32InternalGPIOPin();
  scl->set_pin((gpio_num_t) SNF_SCL);
  scl->set_flags(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  auto *b = new i2c::SoftwareI2CBus();
  b->set_sda_pin(sda);
  b->set_scl_pin(scl);
  b->set_frequency(SNF_SW_FREQ);
  b->set_scan(true);
  b->set_timeout(1000);
  b->setup();
  b->dump_config();
  return b;
}
#endif
}  // namespace snf
