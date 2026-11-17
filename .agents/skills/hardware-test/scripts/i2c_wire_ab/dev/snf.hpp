#pragma once
// esphome#22780 device check: GPIO interrupt sniffer on the I2C pads, a sweep of I2CDevice call shapes, and a
// malloc counter (-Wl,--wrap=malloc). One "SN" line per case: rc, mallocs, decoded wire stream hash, match with the
// stream the call should produce.
#include <cstring>
#include <string>
#include <vector>
#include <driver/gpio.h>
#include <soc/gpio_reg.h>
#include <esp_attr.h>
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"
#ifdef SNF_PDSLAVE
// the board is its own target (pdslave.hpp); its token stream replaces the pad sniffer's decode
#include "pdslave.hpp"
#endif

extern "C" void *__real_malloc(size_t);
static volatile bool snf_alloc_window = false;
static volatile uint32_t snf_allocs = 0;
extern "C" void *__wrap_malloc(size_t n) {
  if (snf_alloc_window)
    snf_allocs = snf_allocs + 1;
  return __real_malloc(n);
}

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

// Decode level states into tokens; counts transitions where both lines changed between samples
static std::string decode(uint8_t start, uint32_t &amb) {
  std::string out;
  uint8_t prev = start;
  bool open = false;
  int bits = -1;  // -1: no frame; 0..8 bit index in the current 9-bit unit
  uint16_t sh = 0;
  bool first = false, reading = false;
  char t[8];
  for (uint32_t i = 0; i < n; i++) {
    uint8_t cur = buf[i];
    bool psda = prev & 1, pscl = prev & 2, sda = cur & 1, scl = cur & 2;
    if ((psda != sda) && (pscl != scl))
      amb++;
    if (pscl && scl && psda && !sda) {
      out += open ? "Sr " : "S ";
      open = true;
      bits = 0;
      sh = 0;
      first = true;
    } else if (pscl && scl && !psda && sda) {
      out += "P ";
      open = false;
      bits = -1;
    } else if (!pscl && scl && bits >= 0) {
      if (bits < 8) {
        sh = (sh << 1) | (sda ? 1 : 0);
        bits++;
      } else {
        bool ack = !sda;
        uint8_t b = sh & 0xFF;
        if (first) {
          reading = b & 1;
          snprintf(t, sizeof t, "%02X%c ", b, ack ? '+' : '-');
        } else {
          snprintf(t, sizeof t, reading ? "r%02X%c " : "%02X%c ", b, ack ? '+' : '-');
        }
        out += t;
        first = false;
        bits = 0;
        sh = 0;
      }
    }
    prev = cur;
  }
  return out;
}

static uint32_t fnv(const std::string &s) {
  uint32_t h = 2166136261u;
  for (unsigned char c : s)
    h = (h ^ c) * 16777619u;
  return h;
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
  last = 3;
  n = 0;
  ovf = 0;
  uint8_t start = 3;
  auto arm = [&] {
    uint32_t v = REG_READ(GPIO_IN_REG);
    start = ((v >> SNF_SDA) & 1) | (((v >> SNF_SCL) & 1) << 1);
    last = start;
    snf_allocs = 0;
    snf_alloc_window = true;
#ifdef SNF_PDSLAVE
    pds::ln = 0;
    pds::lovf = 0;
#endif
    armed = true;
  };
  auto disarm = [&] {
    armed = false;
    snf_alloc_window = false;
  };
  if (i <= 300) {
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
  } else if (i <= 601) {
    size_t len = i - 301;
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
  } else if (i <= 611) {
    static const size_t ns[] = {1, 2, 3, 4, 8};
    size_t k = ns[(i - 602) % 5];
    bool sixteen = (i - 602) >= 5;
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
  } else if (i <= 614) {
    size_t k = i - 611;  // 1..3
    label = "read n=" + std::to_string(k);
    arm();
    rc = dev->read(rdata, k);
    disarm();
    exp = "S ";
    wb(exp, (A << 1) | 1);
    rb(exp, rdata, k);
    exp += "P ";
  } else if (i <= 617) {
    static const uint8_t p1[] = {0x02};
    size_t k = i - 614;  // 1..3
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
  } else if (i < 618 + 112) {
    uint8_t a = 8 + (i - 618);
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
  std::string got = decode(start, amb);
#endif
  bool ok = got == exp && ovf == 0;
  nok += ok;
  ndiff += !ok;
  namb += amb;
  nalloc += snf_allocs != 0;
  ESP_LOGI(TAG, "SN %s|rc=%d|alloc=%u|ev=%u|amb=%u|ovf=%u|fnv=%08X|%s", label.c_str(), (int) rc, (unsigned) snf_allocs,
           (unsigned) n, (unsigned) amb, (unsigned) ovf, (unsigned) fnv(got), ok ? "OK" : "DIFF");
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
