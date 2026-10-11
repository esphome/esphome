#include "bootsel_touch.h"

#ifdef USE_ZEPHYR_BOOTSEL_TOUCH

#include "usb_baud_rate.h"

#include <zephyr/retention/bootmode.h>
#include <zephyr/sys/reboot.h>

namespace esphome::zephyr {

static const uint32_t TOUCH_BAUD_RATE = 1200;        // reboot into the bootloader (BOOTSEL)
static const uint32_t RESET_TOUCH_BAUD_RATE = 2001;  // plain reboot back into the application

void BootselTouch::setup() {
  this->usb_baud_rate_->add_on_baud_rate_callback([](uint32_t rate) {
    if (rate == TOUCH_BAUD_RATE) {
      bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);
      sys_reboot(SYS_REBOOT_COLD);
    } else if (rate == RESET_TOUCH_BAUD_RATE) {
      sys_reboot(SYS_REBOOT_COLD);
    }
  });
}

}  // namespace esphome::zephyr

#endif
