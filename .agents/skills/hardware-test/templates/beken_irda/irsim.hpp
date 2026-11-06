#pragma once
// Drive the receive pin from the chip itself: input stays enabled, the output driver is turned on and bit 1 sets the
// level, so the timer ISR (or the IRDA block) reads the pad we drive. The receiver idles high and pulls low on a mark.
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remote_base/nec_protocol.h"
#include "esphome/components/remote_base/samsung_protocol.h"
#include "esphome/components/remote_base/sony_protocol.h"
#include "esphome/components/remote_base/rc5_protocol.h"
#include "esphome/components/remote_base/lg_protocol.h"
#include "esphome/components/remote_base/panasonic_protocol.h"
#include "esphome/components/remote_base/jvc_protocol.h"
#include <FreeRTOS.h>
#include <task.h>

namespace irsim {
using esphome::remote_base::RemoteTransmitData;
static int pin = 26;
static uint32_t sent = 0;
static bool strip_func = false;  // drive with the second function off

inline volatile uint32_t *cfg() { return reinterpret_cast<volatile uint32_t *>(0x0802800 + pin * 4); }
inline void level(bool high) {
  uint32_t v = *cfg();
  v |= 1u << 2;     // input enable
  v &= ~(1u << 3);  // output enable (active low)
  if (strip_func)
    v &= ~(1u << 6);
  v = high ? (v | 2u) : (v & ~2u);
  *cfg() = v;
}
inline void claim(int p) {
  pin = p;
  level(true);
  ESP_LOGI("irsim", "claimed P%d cfg=0x%02X", pin, (unsigned) *cfg());
}
// glitch_us > 0 puts a short mark in the middle of every space longer than 1 ms
// The scheduler is suspended so no task stretches a duration; interrupts (and the sampling timer) stay on.
inline void send(const std::vector<int32_t> &d, uint32_t glitch_us = 0) {
  vTaskSuspendAll();
  for (int32_t v : d) {
    uint32_t us = v > 0 ? v : -v;
    if (v < 0 && glitch_us && us > 1000) {
      level(true);
      delayMicroseconds(us / 2);
      level(false);
      delayMicroseconds(glitch_us);
      level(true);
      delayMicroseconds(us - us / 2 - glitch_us);
      continue;
    }
    level(v < 0);
    delayMicroseconds(us);
  }
  level(true);
  xTaskResumeAll();
  sent++;
}
inline std::vector<int32_t> nec(uint16_t address, uint16_t command) {
  RemoteTransmitData d;
  esphome::remote_base::NECProtocol().encode(&d, {.address = address, .command = command, .command_repeats = 1});
  return d.get_data();
}
inline std::vector<int32_t> nec_repeat() { return {9000, -2250, 560}; }
inline void log_tx(const char *what, const std::vector<int32_t> &d) {
  ESP_LOGI("irsim", "TX %s n=%u", what, (unsigned) d.size());
}
}  // namespace irsim
