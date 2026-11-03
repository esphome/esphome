#if defined(CONFIG_USB_DEVICE_STACK) || defined(CONFIG_USB_DEVICE_STACK_NEXT)
#include "usb_device.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#ifdef CONFIG_USB_DEVICE_STACK_NEXT
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usbd.h>
#include <initializer_list>
#ifdef USE_ZEPHYR_CDC_ACM_RATE_CALLBACK
#include "cdc_acm.h"
#endif
#else
#include <zephyr/usb/usb_device.h>
#endif

namespace esphome::zephyr {

ESPHOME_LOG_TAG(TAG, "zephyr.usb");

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

// Runs on the USB device stack's thread or its message work queue, never on the main loop
static void usbd_msg_callback(usbd_context *const ctx, const usbd_msg *const msg) {
#ifdef USE_ZEPHYR_CDC_ACM_RATE_CALLBACK
  if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING) {
    uint32_t rate = 0;
    if (uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_BAUD_RATE, &rate) == 0) {
      CdcAcm::rate_changed(msg->dev, rate);
    }
  }
#endif
}

static int usbd_setup() {
  for (usbd_desc_node *desc : {&esphome_usbd_lang, &esphome_usbd_mfr, &esphome_usbd_product, &esphome_usbd_sn}) {
    if (int err = usbd_add_descriptor(&esphome_usbd, desc); err != 0)
      return err;
  }
  // nRF52 has a full-speed controller only, so a single full-speed configuration is enough
  if (int err = usbd_add_configuration(&esphome_usbd, USBD_SPEED_FS, &esphome_usbd_fs_config); err != 0)
    return err;
  if (int err = usbd_register_all_classes(&esphome_usbd, USBD_SPEED_FS, 1, nullptr); err != 0)
    return err;
  // Each CDC ACM function has an Interface Association Descriptor
  if (int err = usbd_device_set_code_triple(&esphome_usbd, USBD_SPEED_FS, USB_BCC_MISCELLANEOUS, 0x02, 0x01); err != 0)
    return err;
  usbd_self_powered(&esphome_usbd, true);
  if (int err = usbd_msg_register_cb(&esphome_usbd, usbd_msg_callback); err != 0)
    return err;
  if (int err = usbd_init(&esphome_usbd); err != 0)
    return err;
  return usbd_enable(&esphome_usbd);
}

void usb_device_enable() {
  static bool enabled = false;
  if (enabled)
    return;
  enabled = true;
  if (int err = usbd_setup(); err != 0) {
    ESP_LOGE(TAG, "USB device setup failed, err %d", err);
  }
}

#else  // legacy USB device stack

void usb_device_enable() {
  // Returns -EALREADY when the device is already enabled
  if (int err = usb_enable(nullptr); err != 0 && err != -EALREADY) {
    ESP_LOGE(TAG, "USB enable failed, err %d", err);
  }
}

#endif

}  // namespace esphome::zephyr

#endif
