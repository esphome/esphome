#include "esphome/core/defines.h"

#ifdef USE_ZEPHYR_USB_DEVICE

#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usbd.h>

// Zephyr's CDC_ACM_SERIAL_INITIALIZE_AT_BOOT (subsys/usb/device_next/app/cdc_acm_serial.c) registers only
// the first CDC ACM port, so with more than one port the USB device is set up here instead. Same IDs,
// strings and boot priority as that helper, so the host sees the same device, and UsbBaudRate (registered
// at priority 0) still finds it. The USBD macros declare linker symbols, so they stay global.

LOG_MODULE_REGISTER(esphome_usb_device, CONFIG_USBD_LOG_LEVEL);

USBD_DEVICE_DEFINE(esphome_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), 0x2fe3, 0x0004);

USBD_DESC_LANG_DEFINE(esphome_usbd_lang);
USBD_DESC_MANUFACTURER_DEFINE(esphome_usbd_mfr, "Zephyr Project");
USBD_DESC_PRODUCT_DEFINE(esphome_usbd_product, "CDC ACM serial backend");
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(esphome_usbd_sn)));

USBD_DESC_CONFIG_DEFINE(esphome_usbd_fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(esphome_usbd_hs_cfg_desc, "HS Configuration");

// 125 x 2 mA, bus powered
USBD_CONFIGURATION_DEFINE(esphome_usbd_fs_config, 0, 125, &esphome_usbd_fs_cfg_desc);
USBD_CONFIGURATION_DEFINE(esphome_usbd_hs_config, 0, 125, &esphome_usbd_hs_cfg_desc);

namespace esphome::zephyr {

static int esphome_usbd_add_config(const enum usbd_speed speed) {
  struct usbd_config_node *cfg_nd = speed == USBD_SPEED_HS ? &esphome_usbd_hs_config : &esphome_usbd_fs_config;
  int err = usbd_add_configuration(&esphome_usbd, speed, cfg_nd);
  if (err != 0) {
    LOG_ERR("Failed to add configuration (%d)", err);
    return err;
  }
  err = usbd_register_all_classes(&esphome_usbd, speed, 1, nullptr);
  if (err != 0) {
    LOG_ERR("Failed to register classes (%d)", err);
    return err;
  }
  return usbd_device_set_code_triple(&esphome_usbd, speed, USB_BCC_MISCELLANEOUS, 0x02, 0x01);
}

int esphome_usbd_init() {
  int err = usbd_add_descriptor(&esphome_usbd, &esphome_usbd_lang);
  if (err == 0)
    err = usbd_add_descriptor(&esphome_usbd, &esphome_usbd_mfr);
  if (err == 0)
    err = usbd_add_descriptor(&esphome_usbd, &esphome_usbd_product);
  IF_ENABLED(CONFIG_HWINFO, (if (err == 0) err = usbd_add_descriptor(&esphome_usbd, &esphome_usbd_sn);))
  if (err != 0) {
    LOG_ERR("Failed to add descriptor (%d)", err);
    return err;
  }
  if (USBD_SUPPORTS_HIGH_SPEED && usbd_caps_speed(&esphome_usbd) == USBD_SPEED_HS) {
    err = esphome_usbd_add_config(USBD_SPEED_HS);
    if (err != 0)
      return err;
  }
  err = esphome_usbd_add_config(USBD_SPEED_FS);
  if (err != 0)
    return err;
  err = usbd_init(&esphome_usbd);
  if (err != 0) {
    LOG_ERR("Failed to initialize device (%d)", err);
    return err;
  }
  err = usbd_enable(&esphome_usbd);
  if (err != 0)
    LOG_ERR("Failed to enable device (%d)", err);
  return err;
}

}  // namespace esphome::zephyr

// SYS_INIT pastes its function name into a symbol, so name it separately from the qualified function
SYS_INIT_NAMED(esphome_usbd_init, esphome::zephyr::esphome_usbd_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#endif
