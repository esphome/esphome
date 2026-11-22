#pragma once
#include "esphome.h"
#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/pio.h>

namespace lb {
using namespace esphome;
static constexpr const char *const TAG = "lb";

inline const char *owner(int pin) {
  if (pin < 0)
    return "none";
  auto f = gpio_get_function(pin);
  if (f == GPIO_FUNC_UART)
    return (((pin + 4) >> 3) & 1) ? "UART1" : "UART0";
  if (f == GPIO_FUNC_PIO0)
    return "PIO0";
  if (f == GPIO_FUNC_PIO1)
    return "PIO1";
#if NUM_PIOS > 2
  if (f == GPIO_FUNC_PIO2)
    return "PIO2";
#endif
  return "other";
}

inline bool pio_retarget(int rx, int tx) {
  PIO pios[] = {pio0, pio1
#if NUM_PIOS > 2
                ,
                pio2
#endif
  };
  for (PIO p : pios) {
    for (int sm = 0; sm < 4; sm++) {
      if (!(p->ctrl & (1u << sm)))
        continue;
      uint32_t in_base = (p->sm[sm].pinctrl & PIO_SM0_PINCTRL_IN_BASE_BITS) >> PIO_SM0_PINCTRL_IN_BASE_LSB;
      uint32_t jmp = (p->sm[sm].execctrl & PIO_SM0_EXECCTRL_JMP_PIN_BITS) >> PIO_SM0_EXECCTRL_JMP_PIN_LSB;
      if ((int) in_base == rx && (int) jmp == rx) {
        hw_write_masked(&p->sm[sm].pinctrl, (uint32_t) tx << PIO_SM0_PINCTRL_IN_BASE_LSB, PIO_SM0_PINCTRL_IN_BASE_BITS);
        hw_write_masked(&p->sm[sm].execctrl, (uint32_t) tx << PIO_SM0_EXECCTRL_JMP_PIN_LSB,
                        PIO_SM0_EXECCTRL_JMP_PIN_BITS);
        return true;
      }
    }
  }
  return false;
}

// Reports which peripheral owns the bus pins; with loop set, feeds the bus TX back into its RX and checks 3 rounds.
inline void check(const char *label, uart::UARTComponent *u, int tx, int rx, bool loop, bool inverted) {
  const char *otx = owner(tx), *orx = owner(rx);
  const char *kind = "?";
  bool hw_tx = tx >= 0 && strncmp(otx, "UART", 4) == 0, hw_rx = rx >= 0 && strncmp(orx, "UART", 4) == 0;
  bool pio_tx = tx >= 0 && strncmp(otx, "PIO", 3) == 0, pio_rx = rx >= 0 && strncmp(orx, "PIO", 3) == 0;
  if ((tx < 0 || hw_tx) && (rx < 0 || hw_rx))
    kind = hw_tx ? otx : orx;
  else if ((tx < 0 || pio_tx) && (rx < 0 || pio_rx))
    kind = "SerialPIO";
  ESP_LOGI(TAG, "CHOICE %s tx=%d(%s) rx=%d(%s) -> %s [Serial1=%d Serial2=%d]", label, tx, otx, rx, orx, kind,
           (bool) Serial1, (bool) Serial2);
  if (!loop)
    return;
  bool ok_route = false;
  if (strcmp(kind, "UART0") == 0 || strcmp(kind, "UART1") == 0) {
    uart_hw_t *h = uart_get_hw(kind[4] == '1' ? uart1 : uart0);
    hw_set_bits(&h->cr, UART_UARTCR_LBE_BITS);
    ok_route = true;
  } else if (strcmp(kind, "SerialPIO") == 0) {
    ok_route = pio_retarget(rx, tx);
    if (inverted)
      gpio_set_inover(tx, GPIO_OVERRIDE_INVERT);
  }
  if (!ok_route) {
    ESP_LOGE(TAG, "LOOP %s no route", label);
    return;
  }
  (esphome::delay)(5);
  uint8_t junk;
  while (u->available())
    u->read_byte(&junk);
  int good_rounds = 0;
  for (int r = 0; r < 3; r++) {
    uint8_t out[64], in[64];
    for (int i = 0; i < 64; i++)
      out[i] = r == 0 ? i * 4 + 3 : (r == 1 ? (i & 1 ? 0x00 : 0xFF) : (uint8_t) (0x55 ^ (i * 37)));
    u->write_array(out, sizeof(out));
    u->flush();
    (esphome::delay)(20);
    size_t got = u->available();
    size_t n = got > 64 ? 64 : got;
    if (n)
      u->read_array(in, n);
    bool match = got == 64 && memcmp(in, out, 64) == 0;
    good_rounds += match;
    ESP_LOGI(TAG, "LOOP %s round %d sent 64 got %u %s", label, r, (unsigned) got, match ? "MATCH" : "MISMATCH");
    while (u->available())
      u->read_byte(&junk);
  }
  ESP_LOGI(TAG, "LOOPRESULT %s %d/3", label, good_rounds);
}
}  // namespace lb
