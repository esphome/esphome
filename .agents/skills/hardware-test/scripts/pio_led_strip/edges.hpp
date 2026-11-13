#pragma once
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include "esphome/core/log.h"

static volatile uint32_t g_edges[2];
static void edges_cb(uint gpio, uint32_t events) {
  if (gpio == 2 || gpio == 3)
    g_edges[gpio - 2]++;
}
static void edges_start() {
  gpio_set_irq_enabled_with_callback(2, GPIO_IRQ_EDGE_RISE, true, &edges_cb);
  gpio_set_irq_enabled(3, GPIO_IRQ_EDGE_RISE, true);
}
static void edges_report() {
  static uint32_t last[2];
  uint32_t e0 = g_edges[0], e1 = g_edges[1];
  ESP_LOGI("pio", "EDGES gp2(pio0)=%u gp3(pio1)=%u total %u/%u pio1 sm_en=0x%x pc=%u,%u,%u,%u",
           (unsigned) (e0 - last[0]), (unsigned) (e1 - last[1]), (unsigned) e0, (unsigned) e1,
           (unsigned) (pio1->ctrl & 0xF), pio_sm_get_pc(pio1, 0), pio_sm_get_pc(pio1, 1), pio_sm_get_pc(pio1, 2),
           pio_sm_get_pc(pio1, 3));
  last[0] = e0;
  last[1] = e1;
}
