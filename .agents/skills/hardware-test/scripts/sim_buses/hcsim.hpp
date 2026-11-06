#pragma once
#include <driver/gpio.h>
#include "esphome/core/log.h"

// Software SN74HC165 chain: load low latches the pattern, each rising clock shifts the next bit onto the data pad.
namespace hcsim {
static constexpr gpio_num_t DATA = GPIO_NUM_6, CLK = GPIO_NUM_7, LOAD = GPIO_NUM_10;
static constexpr uint8_t PATTERNS[3][2] = {{0xA5, 0x3C}, {0x01, 0x80}, {0xFF, 0x00}};
inline volatile uint8_t pat = 0;
inline volatile uint32_t loads = 0, clocks = 0;
inline volatile uint8_t idx = 0;
inline void IRAM_ATTR put() {
  const uint8_t *p = PATTERNS[pat];
  uint8_t bit = idx < 16 ? (p[idx / 8] >> (7 - idx % 8)) & 1 : 0;
  gpio_set_level(DATA, bit);
}
inline void IRAM_ATTR on_load(void *) {
  if (gpio_get_level(LOAD) == 0) {
    idx = 0;
    loads++;
    put();
  }
}
inline void IRAM_ATTR on_clk(void *) {
  idx++;
  clocks++;
  put();
}
inline void start() {
  gpio_set_direction(DATA, GPIO_MODE_INPUT_OUTPUT);
  gpio_input_enable(CLK);
  gpio_input_enable(LOAD);
  gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  gpio_set_intr_type(LOAD, GPIO_INTR_NEGEDGE);
  gpio_set_intr_type(CLK, GPIO_INTR_POSEDGE);
  gpio_isr_handler_add(LOAD, on_load, nullptr);
  gpio_isr_handler_add(CLK, on_clk, nullptr);
  gpio_intr_enable(LOAD);
  gpio_intr_enable(CLK);
  ESP_LOGI("hcsim", "HC sim started");
}
}  // namespace hcsim
