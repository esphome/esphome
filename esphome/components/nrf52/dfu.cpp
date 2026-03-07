#include "dfu.h"

#ifdef USE_NRF52_DFU

#include "esphome/core/log.h"
#include "esphome/components/zephyr/cdc_acm.h"

namespace esphome {
namespace nrf52 {

static const char *const TAG = "dfu";

static const uint32_t DFU_DBL_RESET_MAGIC = 0x5A1AD5;  // SALADS

void DeviceFirmwareUpdate::setup() {
  this->reset_pin_->setup();
#if defined(CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT)
  zephyr::global_cdc_acm->add_on_rate_callback([this](const device *, uint32_t rate) {
    if (rate == 1200) {
      volatile uint32_t *dbl_reset_mem = (volatile uint32_t *) 0x20007F7C;
      (*dbl_reset_mem) = DFU_DBL_RESET_MAGIC;
      this->reset_pin_->digital_write(true);
    }
  });
#endif
}

void DeviceFirmwareUpdate::dump_config() {
  ESP_LOGCONFIG(TAG, "DFU:");
  LOG_PIN("  RESET Pin: ", this->reset_pin_);
}

}  // namespace nrf52
}  // namespace esphome

#endif
