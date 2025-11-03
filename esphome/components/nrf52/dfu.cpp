#include "dfu.h"

#ifdef USE_NRF52_DFU

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#include "esphome/core/log.h"

namespace esphome {
namespace nrf52 {

static const char *const TAG = "dfu";

volatile bool goto_dfu = false;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static const uint32_t DFU_DBL_RESET_MAGIC = 0x5A1AD5;  // SALADS

#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

static void cdc_dte_rate_callback(const struct device * /*unused*/, uint32_t rate) {
  if (rate == 1200) {
    goto_dfu = true;
  }
}
void DeviceFirmwareUpdate::setup() {
  this->reset_pin_->setup();
  const struct device *cdc_dev[] = {DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)};
  for (auto &idx : cdc_dev) {
    cdc_acm_dte_rate_callback_set(idx, cdc_dte_rate_callback);
  }
}

void DeviceFirmwareUpdate::loop() {
  if (goto_dfu) {
    goto_dfu = false;
    volatile uint32_t *dbl_reset_mem = (volatile uint32_t *) 0x20007F7C;
    (*dbl_reset_mem) = DFU_DBL_RESET_MAGIC;
    this->reset_pin_->digital_write(true);
  }
}

void DeviceFirmwareUpdate::dump_config() {
  ESP_LOGCONFIG(TAG, "DFU:");
  LOG_PIN("  RESET Pin: ", this->reset_pin_);
}

}  // namespace nrf52
}  // namespace esphome

#endif
