#include "dfu.h"
#ifdef USE_ZEPHYR
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#endif
#ifdef USE_ARRUINO
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#endif

namespace esphome {
namespace dfu {

volatile bool goto_dfu = false;

#define DFU_DBL_RESET_MEM 0x20007F7C
#define DFU_DBL_RESET_MAGIC 0x5A1AD5  // SALADS
uint32_t *dbl_reset_mem = ((uint32_t *) DFU_DBL_RESET_MEM);

#ifdef USE_ZEPHYR
#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

const struct device *cdc_dev[] = {DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)};

static void cdc_dte_rate_callback(const struct device *, uint32_t rate){
  if (rate == 1200) {
    goto_dfu = true;
  }
}
#endif

void DeviceFirmwareUpdate::setup() {
#ifdef USE_ZEPHYR
  for (int idx = 0; idx < ARRAY_SIZE(cdc_dev); idx++) {
    cdc_acm_dte_rate_callback_set(cdc_dev[idx], cdc_dte_rate_callback);
  }
#endif
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

#ifdef USE_ARRUINO
extern "C" {

void __wrap_tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void) rts;

  // DTR = false is counted as disconnected
  if (!dtr) {
    // touch1200 only with first CDC instance (Serial)
    if (itf == 0) {
      cdc_line_coding_t coding;
      tud_cdc_get_line_coding(&coding);

      if (coding.bit_rate == 1200) {
        goto_dfu = true;
      }
    }
  }
}
}
#endif
