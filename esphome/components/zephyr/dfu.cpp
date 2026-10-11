#include "dfu.h"

#ifdef USE_ZEPHYR_NRF52_DFU

#include "usb_baud_rate.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <hal/nrf_power.h>

namespace esphome::zephyr {

static const char *const TAG = "dfu";

static const uint32_t DFU_DBL_RESET_MAGIC = 0x5A1AD5;  // SALADS
static const uint8_t DFU_MAGIC_UF2_RESET = 0x57;       // Adafruit nRF52 bootloader UF2 magic
static const uint8_t DFU_MAGIC_SKIP = 0x6d;            // Adafruit nRF52 bootloader: start the app, skip DFU
// Baud rates the host sets on the USB serial port to request an action; see on_baud_rate().
static const uint32_t DFU_TOUCH_BAUD_RATE = 1200;    // reboot into the bootloader (DFU)
static const uint32_t RESET_TOUCH_BAUD_RATE = 2001;  // plain reboot back into the application

void DeviceFirmwareUpdate::setup() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
  }
  this->usb_baud_rate_->add_on_baud_rate_callback([this](uint32_t rate) { this->on_baud_rate(rate); });
}

void DeviceFirmwareUpdate::on_baud_rate(uint32_t rate) {
  if (rate == RESET_TOUCH_BAUD_RATE) {
    // A plain reboot for host tools (a logs view's Reset device): the USB serial port has no
    // reset line, and the bootloader, once entered, has no host command back to the app.
    NRF_POWER->GPREGRET = DFU_MAGIC_SKIP;
    arch_feed_wdt();
    App.reboot();
  } else if (rate == DFU_TOUCH_BAUD_RATE) {
    volatile uint32_t *dbl_reset_mem = (volatile uint32_t *) 0x20007F7C;
    (*dbl_reset_mem) = DFU_DBL_RESET_MAGIC;
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->digital_write(true);
    } else {
      NRF_POWER->GPREGRET = DFU_MAGIC_UF2_RESET;
      arch_feed_wdt();
      App.reboot();
    }
  }
}

void DeviceFirmwareUpdate::dump_config() {
  ESP_LOGCONFIG(TAG, "DFU:");
  if (this->reset_pin_ != nullptr) {
    LOG_PIN("  RESET Pin: ", this->reset_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Method: GPREGRET");
  }
}

}  // namespace esphome::zephyr

#endif
