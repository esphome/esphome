#include "tinyuf2_touch.h"

#ifdef USE_ZEPHYR_TINYUF2_TOUCH

#include "usb_baud_rate.h"

#include <zephyr/sys/reboot.h>

namespace esphome::zephyr {

// TinyUF2's double-tap word values (src/board_api.h).
static constexpr uint32_t DBL_TAP_MAGIC = 0xf01669ef;             // stay in the bootloader
static constexpr uint32_t DBL_TAP_MAGIC_QUICK_BOOT = 0xf02669ef;  // start the app, skip the double-tap wait
static constexpr uint32_t TOUCH_BAUD_RATE = 1200;
static constexpr uint32_t RESET_TOUCH_BAUD_RATE = 2001;

void TinyUf2Touch::setup() {
  this->usb_baud_rate_->add_on_baud_rate_callback([this](uint32_t rate) {
    uint32_t magic;
    if (rate == TOUCH_BAUD_RATE) {
      magic = DBL_TAP_MAGIC;
    } else if (rate == RESET_TOUCH_BAUD_RATE) {
      magic = DBL_TAP_MAGIC_QUICK_BOOT;
    } else {
      return;
    }
    // The word is likely inside the heap, so reboot at once, without shutdown hooks.
    *reinterpret_cast<volatile uint32_t *>(this->dbl_tap_addr_) = magic;
    sys_reboot(SYS_REBOOT_COLD);
  });
}

}  // namespace esphome::zephyr

#endif
