#pragma once
// Real BMI270 probe for ESP8266: malloc count and free heap around the setup (link with -Wl,--wrap=malloc) and a raw
// dump of the registers that show the config upload state (INTERNAL_STATUS 0x01 = config loaded).
#include <Arduino.h>
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c_bus.h"

extern "C" void *__real_malloc(size_t);
namespace bmiprobe {
inline volatile bool armed = false;
inline volatile uint32_t count = 0, bytes = 0;
inline uint32_t heap0 = 0, heap1 = 0, us0 = 0, us1 = 0;
inline void start() {
  heap0 = ESP.getFreeHeap();
  count = 0;
  bytes = 0;
  us0 = micros();
  armed = true;
}
inline void stop() {
  armed = false;
  us1 = micros();
  heap1 = ESP.getFreeHeap();
}
inline uint8_t rd(esphome::i2c::I2CBus *bus, uint8_t addr, uint8_t reg, bool &ok) {
  uint8_t v = 0;
  ok = bus->write_readv(addr, &reg, 1, &v, 1) == esphome::i2c::ERROR_OK && ok;
  return v;
}
inline void regs(esphome::i2c::I2CBus *bus, uint8_t addr, const char *tag) {
  bool ok = true;
  uint8_t chip = rd(bus, addr, 0x00, ok), err = rd(bus, addr, 0x02, ok), st = rd(bus, addr, 0x03, ok);
  uint8_t is = rd(bus, addr, 0x21, ok), accc = rd(bus, addr, 0x40, ok), accr = rd(bus, addr, 0x41, ok);
  uint8_t gyc = rd(bus, addr, 0x42, ok), gyr = rd(bus, addr, 0x43, ok), auxid = rd(bus, addr, 0x4B, ok);
  uint8_t ifc = rd(bus, addr, 0x6B, ok), pconf = rd(bus, addr, 0x7C, ok), pctl = rd(bus, addr, 0x7D, ok);
  ESP_LOGI("bmiprobe",
           "REGS %s @0x%02X ok=%d CHIP_ID=%02X ERR=%02X STATUS=%02X INTERNAL_STATUS=%02X (msg %u) ACC_CONF=%02X "
           "ACC_RANGE=%02X GYR_CONF=%02X GYR_RANGE=%02X AUX_DEV_ID=%02X IF_CONF=%02X PWR_CONF=%02X PWR_CTRL=%02X",
           tag, addr, ok, chip, err, st, is, is & 0x0F, accc, accr, gyc, gyr, auxid, ifc, pconf, pctl);
}
inline void report(const char *name) {
  ESP_LOGI("bmiprobe", "SETUP %s mallocs=%u bytes=%u heap_before=%u heap_after=%u setup_us=%u", name, (unsigned) count,
           (unsigned) bytes, (unsigned) heap0, (unsigned) heap1, (unsigned) (us1 - us0));
  ESP_LOGI("bmiprobe", "HEAP %s free=%u max_block=%u frag=%u", name, ESP.getFreeHeap(), ESP.getMaxFreeBlockSize(),
           ESP.getHeapFragmentation());
}
}  // namespace bmiprobe
extern "C" IRAM_ATTR void *__wrap_malloc(size_t n) {
  if (bmiprobe::armed) {
    bmiprobe::count = bmiprobe::count + 1;
    bmiprobe::bytes = bmiprobe::bytes + n;
  }
  return __real_malloc(n);
}
