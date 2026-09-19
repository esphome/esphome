#include "dfu.h"

#ifdef USE_NRF52_DFU

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/components/zephyr/cdc_acm.h"

#include <hal/nrf_power.h>

namespace esphome::nrf52 {

static const char *const TAG = "dfu";

static const uint32_t DFU_DBL_RESET_MAGIC = 0x5A1AD5;  // SALADS
static const uint8_t DFU_MAGIC_UF2_RESET = 0x57;       // Adafruit nRF52 bootloader UF2 magic

void DeviceFirmwareUpdate::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
  }
#if defined(CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT)
  zephyr::global_cdc_acm->add_on_rate_callback([this](const device *, uint32_t rate) {
    if (rate == 1200) {
      volatile uint32_t *dbl_reset_mem = (volatile uint32_t *) 0x20007F7C;
      (*dbl_reset_mem) = DFU_DBL_RESET_MAGIC;
      if (this->reset_pin_ != nullptr) {
        this->reset_pin_->digital_write(true);
      } else {
        NRF_POWER->GPREGRET = DFU_MAGIC_UF2_RESET;
        App.reboot();
      }
    }
  });
#endif
}

void DeviceFirmwareUpdate::dump_config() {
  ESP_LOGCONFIG(TAG, "DFU:");
  if (this->reset_pin_ != nullptr) {
    LOG_PIN("  RESET Pin: ", this->reset_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Method: GPREGRET");
  }
}

}  // namespace esphome::nrf52

#endif
