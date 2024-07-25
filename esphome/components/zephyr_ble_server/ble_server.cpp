#ifdef USE_ZEPHYR
#include "ble_server.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

namespace esphome {
namespace zephyr_ble_server {

static const char *const TAG = "zephyr_ble_server";

static struct k_work advertise_work;

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data AD[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data SD[] = {
#ifdef USE_OTA
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, 0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86, 0xd3, 0x4c, 0xb7, 0x1d, 0x1d,
                  0xdc, 0x53, 0x8d),
#endif
};

const struct bt_le_adv_param *const ADV_PARAM = BT_LE_ADV_CONN;

static void advertise(struct k_work *work) {
  bt_le_adv_stop();

  int rc = bt_le_adv_start(ADV_PARAM, AD, ARRAY_SIZE(AD), SD, ARRAY_SIZE(SD));
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
  resume_();
}

void BLEServer::loop() {
  if (suspended_) {
    resume_();
    suspended_ = false;
  }
}

void BLEServer::resume_() {
  int rc = bt_enable(bt_ready);
  if (rc != 0) {
    ESP_LOGE(TAG, "Bluetooth enable failed: %d", rc);
    return;
  }
}

void BLEServer::on_shutdown() {
  bt_disable();
  suspended_ = true;
}

}  // namespace zephyr_ble_server
}  // namespace esphome

#endif
