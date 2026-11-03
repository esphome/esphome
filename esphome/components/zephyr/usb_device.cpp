#if defined(CONFIG_USB_DEVICE_STACK) || defined(CONFIG_USB_DEVICE_STACK_NEXT)
#include "usb_device.h"

#ifdef CONFIG_USB_DEVICE_STACK_NEXT
#include "esphome/core/defines.h"
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <initializer_list>
#ifdef USE_ZEPHYR_CDC_ACM_RATE_CALLBACK
#include "cdc_acm.h"
#endif
#else
#include <zephyr/usb/usb_device.h>
#endif

namespace esphome::zephyr {

#ifdef CONFIG_USB_DEVICE_STACK_NEXT

// Same identity and power settings as the defaults of the legacy stack, so a host sees the
// same device on either stack.
static constexpr uint16_t USB_VID = 0x2FE3;
static constexpr uint16_t USB_PID = 0x0100;
static constexpr uint8_t USB_MAX_POWER = 50;  // in 2 mA units

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
USBD_DEVICE_DEFINE(esphome_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), USB_VID, USB_PID);
USBD_DESC_LANG_DEFINE(esphome_usbd_lang);
USBD_DESC_MANUFACTURER_DEFINE(esphome_usbd_mfr, "ZEPHYR");
USBD_DESC_PRODUCT_DEFINE(esphome_usbd_product, "USB-DEV");
USBD_DESC_SERIAL_NUMBER_DEFINE(esphome_usbd_sn);
USBD_CONFIGURATION_DEFINE(esphome_usbd_fs_config, USB_SCD_SELF_POWERED, USB_MAX_POWER, nullptr);
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_ZEPHYR_CDC_ACM_RATE_CALLBACK
// Runs on the system work queue, which also runs all USB transfers: keep it short
static void usbd_msg_callback(usbd_context *const /*ctx*/, const usbd_msg *const msg) {
  if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING)
    CdcAcm::rate_changed();
}
#endif

static void usbd_setup() {
  for (usbd_desc_node *desc : {&esphome_usbd_lang, &esphome_usbd_mfr, &esphome_usbd_product, &esphome_usbd_sn}) {
    if (usbd_add_descriptor(&esphome_usbd, desc) != 0)
      return;
  }
  // nRF52 has a full-speed controller only, so a single full-speed configuration is enough
  if (usbd_add_configuration(&esphome_usbd, USBD_SPEED_FS, &esphome_usbd_fs_config) != 0 ||
      usbd_register_all_classes(&esphome_usbd, USBD_SPEED_FS, 1, nullptr) != 0 ||
      // Each CDC ACM function has an Interface Association Descriptor
      usbd_device_set_code_triple(&esphome_usbd, USBD_SPEED_FS, USB_BCC_MISCELLANEOUS, 0x02, 0x01) != 0)
    return;
  usbd_self_powered(&esphome_usbd, true);
#ifdef USE_ZEPHYR_CDC_ACM_RATE_CALLBACK
  if (usbd_msg_register_cb(&esphome_usbd, usbd_msg_callback) != 0)
    return;
#endif
  if (usbd_init(&esphome_usbd) == 0)
    usbd_enable(&esphome_usbd);
}

void usb_device_enable() {
  static bool enabled = false;
  if (enabled)
    return;
  enabled = true;
  usbd_setup();
}

#else  // legacy USB device stack

// usb_enable() itself returns -EALREADY on later calls
void usb_device_enable() { usb_enable(nullptr); }

#endif

}  // namespace esphome::zephyr

#endif
