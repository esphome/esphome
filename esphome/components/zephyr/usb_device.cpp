#if defined(CONFIG_USB_DEVICE_STACK) || defined(CONFIG_USB_DEVICE_STACK_NEXT)
#include "usb_device.h"

#ifdef CONFIG_USB_DEVICE_STACK_NEXT
#include "esphome/core/defines.h"
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <cerrno>
#include <initializer_list>
#ifdef CONFIG_UDC_NRF
#include <nrfx_power.h>
#endif
#ifdef USE_ZEPHYR_CDC_ACM_RATE_CALLBACK
#include "cdc_acm.h"
#endif
#else
#include <cerrno>
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
#if USBD_SUPPORTS_HIGH_SPEED
USBD_CONFIGURATION_DEFINE(esphome_usbd_hs_config, USB_SCD_SELF_POWERED, USB_MAX_POWER, nullptr);
#endif
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// Runs on the system work queue, which also runs all USB transfers: keep it short
static void usbd_msg_callback(usbd_context *const ctx, const usbd_msg *const msg) {
  // Run the controller only while VBUS is present, so it and its clock stop when the cable is removed
  if (usbd_can_detect_vbus(ctx)) {
    if (msg->type == USBD_MSG_VBUS_READY) {
      usbd_enable(ctx);
    } else if (msg->type == USBD_MSG_VBUS_REMOVED) {
      usbd_disable(ctx);
    }
  }
#ifdef USE_ZEPHYR_CDC_ACM_RATE_CALLBACK
  if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING)
    CdcAcm::rate_changed(msg->dev);
#endif
}

static int usbd_add_speed(usbd_speed speed, usbd_config_node *config) {
  int err = usbd_add_configuration(&esphome_usbd, speed, config);
  if (err == 0)
    err = usbd_register_all_classes(&esphome_usbd, speed, 1, nullptr);
#ifdef CONFIG_USBD_CDC_ACM_CLASS
  // Each CDC ACM function has an Interface Association Descriptor
  if (err == 0)
    err = usbd_device_set_code_triple(&esphome_usbd, speed, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
#endif
  return err;
}

// The nRF52 controller reports a cable connected at reset only if no USB bootloader (such as UF2)
// handled the event first, so the cable is checked here as well
static bool usbd_vbus_present() {
#ifdef CONFIG_UDC_NRF
  return nrfx_power_usbstatus_get() != NRFX_POWER_USB_STATE_DISCONNECTED;
#else
  return false;
#endif
}

static int usbd_setup() {
  for (usbd_desc_node *desc : {&esphome_usbd_lang, &esphome_usbd_mfr, &esphome_usbd_product, &esphome_usbd_sn}) {
    if (int err = usbd_add_descriptor(&esphome_usbd, desc); err != 0)
      return err;
  }
#if USBD_SUPPORTS_HIGH_SPEED
  // A high-speed controller (nRF54LM20) needs a high-speed configuration too
  if (usbd_caps_speed(&esphome_usbd) == USBD_SPEED_HS) {
    if (int err = usbd_add_speed(USBD_SPEED_HS, &esphome_usbd_hs_config); err != 0)
      return err;
  }
#endif
  if (int err = usbd_add_speed(USBD_SPEED_FS, &esphome_usbd_fs_config); err != 0)
    return err;
  usbd_self_powered(&esphome_usbd, true);
  if (int err = usbd_msg_register_cb(&esphome_usbd, usbd_msg_callback); err != 0)
    return err;
  if (int err = usbd_init(&esphome_usbd); err != 0)
    return err;
  // Otherwise the message callback enables the device once VBUS is present
  if (!usbd_can_detect_vbus(&esphome_usbd) || usbd_vbus_present()) {
    int err = usbd_enable(&esphome_usbd);
    // The message callback may have enabled it already
    return err == -EALREADY ? 0 : err;
  }
  return 0;
}

int usb_device_enable() {
  // Setup cannot be repeated after a failure: the parts added before it stay registered
  static const int SETUP_RESULT = usbd_setup();
  return SETUP_RESULT;
}

#else  // legacy USB device stack

int usb_device_enable() {
  int err = usb_enable(nullptr);
  // Later calls, or a board that starts USB at boot
  return err == -EALREADY ? 0 : err;
}

#endif

}  // namespace esphome::zephyr

#endif
