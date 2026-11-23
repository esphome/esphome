#pragma once
#include <Wire.h>
#include "esphome/core/log.h"

// Fake HDC1080 at 0x40: 0x00 temperature (26.0 C), 0x01 humidity (50 %), other registers are a
// 256 byte file; reading 0x7E holds SCL for 500 us, reading 0x7F for 5 ms.
static volatile uint8_t SL_REGS[256];
static volatile uint8_t sl_ptr = 0;
static volatile uint32_t sl_rx = 0, sl_tx = 0, sl_stretch = 0;

static void sl_on_receive(int n) {
  if (n < 1)
    return;
  sl_ptr = Wire.read();
  uint8_t p = sl_ptr;
  while (--n > 0 && Wire.available())
    SL_REGS[p++] = Wire.read();
  sl_rx++;
}

static void sl_on_request() {
  uint8_t b[8];
  uint8_t p = sl_ptr;
  sl_tx++;
  if (p == 0x7E || p == 0x7F) {
    sl_stretch++;
    delayMicroseconds(p == 0x7E ? 500 : 5000);
  }
  for (int i = 0; i < 8; i++)
    b[i] = SL_REGS[(uint8_t) (p + i)];
  if (p == 0x00) {
    b[0] = 0x66;
    b[1] = 0x66;
  } else if (p == 0x01) {
    b[0] = 0x80;
    b[1] = 0x00;
  }
  Wire.write(b, 8);
}

inline void i2cslave_begin() {
  Wire.begin(4, 5, 0x40);
  Wire.onReceive(sl_on_receive);
  Wire.onRequest(sl_on_request);
  ESP_LOGI("slave", "I2C slave at 0x40 on SDA GPIO4 SCL GPIO5");
}

inline void i2cslave_report() {
  ESP_LOGI("slave", "rx %u tx %u stretch %u", (unsigned) sl_rx, (unsigned) sl_tx, (unsigned) sl_stretch);
}
