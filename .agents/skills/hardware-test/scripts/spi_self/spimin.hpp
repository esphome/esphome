#pragma once
// Classic ESP32 without the capture slave: full duplex reads through a MOSI to MISO matrix loopback on SPI2.
#include "esphome/core/log.h"
#include "esphome/components/spi_device/spi_device.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "soc/spi_periph.h"
namespace spimin {
inline void run(esphome::spi_device::SPIDeviceComponent *d, int mosi, int n) {
  gpio_input_enable((gpio_num_t) mosi);
  esp_rom_gpio_connect_in_signal(mosi, spi_periph_signal[SPI2_HOST].spiq_in, false);
  for (int k = 0; k < n; k++) {
    uint8_t b[8] = {0x01, 0x80, 0xA5, 0x3C, 0xF0, 0x0F, 0x55, (uint8_t) k};
    d->enable();
    d->transfer_array(b, 8);
    d->disable();
    ESP_LOGI("spimin", "read %d: %02x%02x%02x%02x%02x%02x%02x%02x", k, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
  }
}
}  // namespace spimin
