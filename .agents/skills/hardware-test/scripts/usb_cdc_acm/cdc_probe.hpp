#pragma once
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include "esphome/core/log.h"

// Direct look at the Zephyr cdc_acm_uart1 driver state, bypassing the usb_cdc_acm component
inline void cdc_probe() {
  const device *d = DEVICE_DT_GET(DT_NODELABEL(cdc_acm_uart1));
  uint8_t tmp[32];
  int rr = uart_irq_rx_ready(d);
  int n = uart_fifo_read(d, tmp, sizeof(tmp));
  ESP_LOGI("probe", "driver rx_ready=%d fifo_read=%d first=0x%02X", rr, n, n > 0 ? tmp[0] : 0);
}
