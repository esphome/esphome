#include "dfu.h"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/cdc_acm.h>

namespace esphome {
namespace dfu {

volatile bool goto_dfu = false;

#define DFU_DBL_RESET_MEM 0x20007F7C
#define DFU_DBL_RESET_MAGIC 0x5A1AD5  // SALADS
uint32_t *dbl_reset_mem = ((uint32_t *) DFU_DBL_RESET_MEM);

#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

const struct device *cdc_dev[] = {DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)};

static void cdc_dte_rate_callback(const struct device *, uint32_t rate) {
  if (rate == 1200) {
    goto_dfu = true;
  }
}

void DeviceFirmwareUpdate::setup() {
  for (int idx = 0; idx < ARRAY_SIZE(cdc_dev); idx++) {
    cdc_acm_dte_rate_callback_set(cdc_dev[idx], cdc_dte_rate_callback);
  }
}

void DeviceFirmwareUpdate::loop() {
  if (goto_dfu) {
    goto_dfu = false;
    (*dbl_reset_mem) = DFU_DBL_RESET_MAGIC;
    reset_output_->set_state(true);
  }
}

}  // namespace dfu
}  // namespace esphome

