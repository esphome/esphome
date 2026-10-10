#include "ota_adafruit_ble.h"

#if defined(USE_ZEPHYR) && defined(USE_NRF52)
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cstring>
#include <hal/nrf_power.h>
#include <zephyr/bluetooth/gatt.h>

namespace esphome::adafruit_ble {

ESPHOME_LOG_TAG(TAG, "adafruit_ble");

// Written by the upload tool (dfu.py) to ask for a restart into the bootloader,
// which then serves the DFU transfer. The bootloader only advertises its own DFU
// service while it is in DFU mode, so this service is what identifies the
// running application.
static const uint8_t DFU_TRIGGER_MAGIC[] = {'D', 'F', 'U'};
// GPREGRET value the Adafruit nRF52 bootloader reads to enter OTA (BLE DFU) mode
static const uint8_t DFU_GPREGRET_OTA = 0xA8;

static ssize_t dfu_trigger_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                                 uint16_t offset, uint8_t flags) {
  if (offset != 0) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
  }
  if (len != sizeof(DFU_TRIGGER_MAGIC) || memcmp(buf, DFU_TRIGGER_MAGIC, sizeof(DFU_TRIGGER_MAGIC)) != 0) {
    return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
  }
  ESP_LOGI(TAG, "DFU mode requested, restarting into the bootloader");
  NRF_POWER->GPREGRET = DFU_GPREGRET_OTA;
  arch_feed_wdt();
  App.reboot();
  return len;
}

// Registered by bt_enable(), which zephyr_ble_server calls during its setup.
BT_GATT_SERVICE_DEFINE(  // NOLINT
    dfu_trigger_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xe5b10001, 0x9c3a, 0x4f5d, 0x8a1b,
                                                                   0x2c3d4e5f6071))),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0xe5b10002, 0x9c3a, 0x4f5d, 0x8a1b, 0x2c3d4e5f6071)),
                           BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, nullptr, dfu_trigger_write, nullptr));

void OTAComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Over-The-Air Updates:\n"
                     "  bootloader: Adafruit nRF52 (BLE DFU)");
}

}  // namespace esphome::adafruit_ble
#endif
