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
inline bool ok(esp_err_t err, const char *what) {
  if (err != ESP_OK)
    ESP_LOGE("hcsim", "%s failed: %s", what, esp_err_to_name(err));
  return err == ESP_OK;
}
inline void start() {
  esp_err_t isr = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  bool good = ok(gpio_set_direction(DATA, GPIO_MODE_INPUT_OUTPUT), "DATA direction") &&
              ok(gpio_input_enable(CLK), "CLK input") && ok(gpio_input_enable(LOAD), "LOAD input") &&
              ok(isr == ESP_ERR_INVALID_STATE ? ESP_OK : isr, "ISR service") &&
              ok(gpio_set_intr_type(LOAD, GPIO_INTR_NEGEDGE), "LOAD edge") &&
              ok(gpio_set_intr_type(CLK, GPIO_INTR_POSEDGE), "CLK edge") &&
              ok(gpio_isr_handler_add(LOAD, on_load, nullptr), "LOAD handler") &&
              ok(gpio_isr_handler_add(CLK, on_clk, nullptr), "CLK handler") &&
              ok(gpio_intr_enable(LOAD), "LOAD interrupt") && ok(gpio_intr_enable(CLK), "CLK interrupt");
  if (good)
    ESP_LOGI("hcsim", "HC sim started");
}
}  // namespace hcsim
