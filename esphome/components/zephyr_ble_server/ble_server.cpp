#include "ble_server.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

namespace esphome {
namespace zephyr_ble_server {

static const char *const TAG = "zephyr_ble_server";

static struct k_work advertise_work;
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
#ifdef USE_OTA

    BT_DATA_BYTES(BT_DATA_UUID128_ALL, 0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86, 0xd3, 0x4c, 0xb7, 0x1d, 0x1d,
                  0xdc, 0x53, 0x8d),
#endif
};

const struct bt_le_adv_param *adv_param = BT_LE_ADV_CONN_NAME;

static void advertise(struct k_work *work) {
  bt_le_adv_stop();

  int rc = bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
  if (rc) {
    ESP_LOGE(TAG, "Advertising failed to start (rc %d)", rc);
    return;
  }
  ESP_LOGI(TAG, "Advertising successfully started");
}

static void connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    ESP_LOGE(TAG, "Connection failed (err 0x%02x)", err);
  } else {
    ESP_LOGI(TAG, "Connected");
  }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  ESP_LOGI(TAG, "Disconnected (reason 0x%02x)", reason);
  k_work_submit(&advertise_work);
}

static void bt_ready(int err) {
  if (err != 0) {
    ESP_LOGE(TAG, "Bluetooth failed to initialise: %d", err);
  } else {
    k_work_submit(&advertise_work);
  }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

void BLEServer::setup() {
  k_work_init(&advertise_work, advertise);

  int rc = bt_enable(bt_ready);
  if (rc != 0) {
    ESP_LOGE(TAG, "Bluetooth enable failed: %d", rc);
    return;
  }
  rc = bt_set_name(App.get_name().c_str());
  if (rc != 0) {
    ESP_LOGE(TAG, "Bluetooth set name failed: %d", rc);
    return;
  }

}

}  // namespace zephyr_ble_server
}  // namespace esphome
