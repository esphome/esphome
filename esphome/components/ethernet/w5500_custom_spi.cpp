#include "w5500_custom_spi.h"

#if defined(USE_ESP32) && defined(USE_ETHERNET_W5500)

#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>
#include <new>

namespace esphome::ethernet {

namespace {

// Per-device context returned by init() and handed back to read/write/deinit.
struct W5500CustomSpiContext {
  spi_device_handle_t handle;
  SemaphoreHandle_t lock;
};

// Transfers up to the ESP32 SPI hardware FIFO size (64 bytes) stay on the polling path; larger
// transfers (the frame payloads) use the blocking, DMA-backed transmit.
constexpr uint32_t W5500_SPI_BULK_THRESHOLD = 64;
constexpr uint32_t W5500_SPI_LOCK_TIMEOUT_MS = 50;

void *w5500_custom_spi_init(const void *spi_config) {
  const auto *config = static_cast<const eth_w5500_config_t *>(spi_config);
  auto *ctx = new (std::nothrow) W5500CustomSpiContext{};
  if (ctx == nullptr) {
    return nullptr;
  }
  // The W5500 SPI frame carries the 16-bit address in the command phase and the 8-bit control
  // byte in the address phase; mirror what the stock driver configures.
  spi_device_interface_config_t devcfg = *config->spi_devcfg;
  devcfg.command_bits = 16;
  devcfg.address_bits = 8;
  if (spi_bus_add_device(config->spi_host_id, &devcfg, &ctx->handle) != ESP_OK) {
    delete ctx;
    return nullptr;
  }
  ctx->lock = xSemaphoreCreateMutex();
  if (ctx->lock == nullptr) {
    spi_bus_remove_device(ctx->handle);
    delete ctx;
    return nullptr;
  }
  return ctx;
}

esp_err_t w5500_custom_spi_deinit(void *spi_ctx) {
  auto *ctx = static_cast<W5500CustomSpiContext *>(spi_ctx);
  spi_bus_remove_device(ctx->handle);
  vSemaphoreDelete(ctx->lock);
  delete ctx;
  return ESP_OK;
}

// Runs one transaction under the device lock, choosing the polling vs blocking transmit by size.
// Bulk payloads (> FIFO size) block so the calling task sleeps while DMA runs; small register
// accesses stay on the cheaper polling path. Used by both read and write.
esp_err_t w5500_custom_spi_transfer(W5500CustomSpiContext *ctx, spi_transaction_t *trans, uint32_t len) {
  if (xSemaphoreTake(ctx->lock, pdMS_TO_TICKS(W5500_SPI_LOCK_TIMEOUT_MS)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }
  esp_err_t ret;
  if (len > W5500_SPI_BULK_THRESHOLD) {
    ret = spi_device_transmit(ctx->handle, trans);
  } else {
    ret = spi_device_polling_transmit(ctx->handle, trans);
  }
  xSemaphoreGive(ctx->lock);
  return ret;
}

esp_err_t w5500_custom_spi_write(void *spi_ctx, uint32_t cmd, uint32_t addr, const void *data, uint32_t len) {
  auto *ctx = static_cast<W5500CustomSpiContext *>(spi_ctx);
  spi_transaction_t trans = {};
  trans.cmd = static_cast<uint16_t>(cmd);
  trans.addr = addr;
  trans.length = 8 * len;
  trans.tx_buffer = data;
  return w5500_custom_spi_transfer(ctx, &trans, len);
}

esp_err_t w5500_custom_spi_read(void *spi_ctx, uint32_t cmd, uint32_t addr, void *data, uint32_t len) {
  auto *ctx = static_cast<W5500CustomSpiContext *>(spi_ctx);
  spi_transaction_t trans = {};
  // Reads of <= 4 bytes use the transaction's inline RX buffer to avoid 4-byte boundary
  // overwrites of adjacent registers (same guard the stock driver uses).
  const bool use_rxdata = len <= 4;
  trans.flags = use_rxdata ? SPI_TRANS_USE_RXDATA : 0;
  trans.cmd = static_cast<uint16_t>(cmd);
  trans.addr = addr;
  trans.length = 8 * len;
  trans.rx_buffer = data;
  esp_err_t ret = w5500_custom_spi_transfer(ctx, &trans, len);
  if (use_rxdata && (ret == ESP_OK)) {
    memcpy(data, trans.rx_data, len);
  }
  return ret;
}

}  // namespace

void install_w5500_async_spi(eth_w5500_config_t &config) {
  // Point the custom driver's config at the W5500 config itself; init() reads spi_host_id and
  // spi_devcfg back out of it. The self-reference is valid because both the config and the
  // spi_devcfg it points at outlive the esp_eth_mac_new_w5500() call that runs init().
  config.custom_spi_driver.config = &config;
  config.custom_spi_driver.init = w5500_custom_spi_init;
  config.custom_spi_driver.deinit = w5500_custom_spi_deinit;
  config.custom_spi_driver.read = w5500_custom_spi_read;
  config.custom_spi_driver.write = w5500_custom_spi_write;
}

}  // namespace esphome::ethernet

#endif  // USE_ESP32 && USE_ETHERNET_W5500
