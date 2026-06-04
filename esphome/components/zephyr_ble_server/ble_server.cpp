#ifdef USE_ZEPHYR
#include "ble_server.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/settings/settings.h>

namespace esphome::zephyr_ble_server {

static const char *const TAG = "zephyr_ble_server";

static k_work advertise_work;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

BLEServer *global_ble_server;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const bt_data AD[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const bt_data SD[] = {
#ifdef USE_OTA
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, 0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86, 0xd3, 0x4c, 0xb7, 0x1d, 0x1d,
                  0xdc, 0x53, 0x8d),
#endif
};

const bt_le_adv_param *const ADV_PARAM = BT_LE_ADV_CONN;

static void advertise(k_work *work) {
  int rc = bt_le_adv_stop();
  if (rc) {
    ESP_LOGE(TAG, "Advertising failed to stop (rc %d)", rc);
  }

  rc = bt_le_adv_start(ADV_PARAM, AD, ARRAY_SIZE(AD), SD, ARRAY_SIZE(SD));
  if (rc) {
    ESP_LOGE(TAG, "Advertising failed to start (rc %d)", rc);
    return;
  }
  ESP_LOGI(TAG, "Advertising successfully started");
}

void BLEServer::connected(bt_conn *conn, uint8_t err) {
  char addr[BT_ADDR_LE_STR_LEN];
  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  if (err) {
    ESP_LOGE(TAG, "Failed to connect to %s (%u)", addr, err);
    return;
  }
  ESP_LOGI(TAG, "Connected %s", addr);
#ifdef CONFIG_BT_SMP
  if (bt_conn_set_security(conn, BT_SECURITY_L4)) {
    ESP_LOGE(TAG, "Failed to set security");
  }
#endif
  conn = bt_conn_ref(conn);
  global_ble_server->defer([conn]() { global_ble_server->conn_ = conn; });
}

void BLEServer::disconnected(bt_conn *conn, uint8_t reason) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  ESP_LOGI(TAG, "Disconnected from %s (reason 0x%02x)", addr, reason);
  global_ble_server->defer([]() {
    if (global_ble_server->conn_) {
      bt_conn_unref(global_ble_server->conn_);
      global_ble_server->conn_ = nullptr;
    }
  });
  k_work_submit(&advertise_work);
}

#ifdef CONFIG_BT_SMP
static void identity_resolved(bt_conn *conn, const bt_addr_le_t *rpa, const bt_addr_le_t *identity) {
  char addr_identity[BT_ADDR_LE_STR_LEN];
  char addr_rpa[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(identity, addr_identity, sizeof(addr_identity));
  bt_addr_le_to_str(rpa, addr_rpa, sizeof(addr_rpa));

  ESP_LOGD(TAG, "Identity resolved %s -> %s", addr_rpa, addr_identity);
}

static void security_changed(bt_conn *conn, bt_security_t level, bt_security_err err) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  if (!err) {
    ESP_LOGD(TAG, "Security changed: %s level %u", addr, level);
  } else {
    ESP_LOGE(TAG, "Security failed: %s level %u err %d", addr, level, err);
  }
}

static void pairing_complete(bt_conn *conn, bool bonded) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  ESP_LOGD(TAG, "Pairing completed: %s, bonded: %d", addr, bonded);
}

static void pairing_failed(bt_conn *conn, bt_security_err reason) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  ESP_LOGE(TAG, "Pairing failed conn: %s, reason %d", addr, reason);

  bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static void bond_deleted(uint8_t id, const bt_addr_le_t *peer) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(peer, addr, sizeof(addr));
  ESP_LOGD(TAG, "Bond deleted for %s, id %u", addr, id);
}

static void auth_passkey_display(bt_conn *conn, unsigned int passkey) {
  char addr[BT_ADDR_LE_STR_LEN];
  char passkey_str[7];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  snprintk(passkey_str, 7, "%06u", passkey);

  ESP_LOGI(TAG, "Passkey for %s: %s", addr, passkey_str);
}

static void conn_addr_str(bt_conn *conn, char *addr, size_t len) {
  struct bt_conn_info info;

  if (bt_conn_get_info(conn, &info) < 0) {
    addr[0] = '\0';
    return;
  }

  switch (info.type) {
    case BT_CONN_TYPE_LE:
      bt_addr_le_to_str(info.le.dst, addr, len);
      break;
    default:
      ESP_LOGE(TAG, "Not implemented");
      addr[0] = '\0';
      break;
  }
}

static void auth_cancel(bt_conn *conn) {
  char addr[BT_ADDR_LE_STR_LEN];

  conn_addr_str(conn, addr, sizeof(addr));

  ESP_LOGI(TAG, "Pairing cancelled: %s", addr);
}

void BLEServer::auth_passkey_confirm(bt_conn *conn, unsigned int passkey) {
  char addr[BT_ADDR_LE_STR_LEN];
  char passkey_str[7];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  snprintk(passkey_str, 7, "%06u", passkey);

  ESP_LOGI(TAG, "Confirm passkey for %s: %s", addr, passkey_str);
  global_ble_server->defer([passkey]() { global_ble_server->passkey_cb_(passkey); });
}

static void auth_pairing_confirm(bt_conn *conn) {
  /* Automatically confirm pairing request from the device side. */
  auto err = bt_conn_auth_pairing_confirm(conn);
  if (err) {
    ESP_LOGE(TAG, "Can't confirm pairing (err: %d)", err);
    return;
  }

  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  ESP_LOGI(TAG, "Pairing confirmed: %s", addr);
}

#endif

void BLEServer::setup() {
  global_ble_server = this;
  int err = 0;
  k_work_init(&advertise_work, advertise);

  static bt_conn_cb conn_callbacks = {
      .connected = connected,
      .disconnected = disconnected,
#ifdef CONFIG_BT_SMP
      .identity_resolved = identity_resolved,
      .security_changed = security_changed,
#endif
  };

  bt_conn_cb_register(&conn_callbacks);
#ifdef CONFIG_BT_SMP
  static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
      .pairing_complete = pairing_complete, .pairing_failed = pairing_failed, .bond_deleted = bond_deleted};
  err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
  if (err) {
    ESP_LOGE(TAG, "Failed to register authorization info callbacks.");
  }
  static struct bt_conn_auth_cb auth_cb = {
      .passkey_display = auth_passkey_display,
      .passkey_confirm = auth_passkey_confirm,
      .cancel = auth_cancel,
      .pairing_confirm = auth_pairing_confirm,
  };
  err = bt_conn_auth_cb_register(&auth_cb);
  if (err) {
    ESP_LOGE(TAG, "Failed to set auth handlers (%d)", err);
  }
#endif
  // callback cannot be used to start scanning due to race conditions with BT_SETTINGS
  err = bt_enable(nullptr);
  if (err) {
    ESP_LOGE(TAG, "Bluetooth enable failed: %d", err);
    return;
  }
#ifdef CONFIG_BT_SETTINGS
  err = settings_load();
  if (err) {
    ESP_LOGE(TAG, "Cannot load settings, err: %d", err);
  }
#endif
  k_work_submit(&advertise_work);
}

#ifdef ESPHOME_LOG_HAS_DEBUG
static const char *role_str(uint8_t role) {
  switch (role) {
    case BT_CONN_ROLE_CENTRAL:
      return "Central";
    case BT_CONN_ROLE_PERIPHERAL:
      return "Peripheral";
  }

  return "Unknown";
}

static void connection_info(bt_conn *conn, void *user_data) {
  char addr[BT_ADDR_LE_STR_LEN];
  struct bt_conn_info info;

  if (bt_conn_get_info(conn, &info) < 0) {
    ESP_LOGE(TAG, "Unable to get info: conn %p", conn);
    return;
  }

  switch (info.type) {
    case BT_CONN_TYPE_LE:
      bt_addr_le_to_str(info.le.dst, addr, sizeof(addr));
      ESP_LOGD(TAG, "  %u [LE][%s] %s: Interval %u latency %u timeout %u security L%u", info.id, role_str(info.role),
               addr, info.le.interval, info.le.latency, info.le.timeout, info.security.level);
      break;
    default:
      ESP_LOGE(TAG, "Not implemented");
      break;
  }
}
#ifdef CONFIG_BT_BONDABLE
static void bond_info(const struct bt_bond_info *info, void *user_data) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(&info->addr, addr, sizeof(addr));
  ESP_LOGD(TAG, "  Bond remote identity: %s", addr);
}
#endif
#endif

void BLEServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ble server:\n"
                "  connected: %s\n"
                "  name: %s\n"
                "  appearance: %u\n"
                "  ready: %s\n"
#ifdef CONFIG_BT_SMP
                "  security manager: YES",
#else
                "  security manager: NO",
#endif
                YESNO(this->conn_), bt_get_name(), bt_get_appearance(), YESNO(bt_is_ready()));

#ifdef ESPHOME_LOG_HAS_DEBUG
  bt_conn_foreach(BT_CONN_TYPE_ALL, connection_info, nullptr);
#ifdef CONFIG_BT_BONDABLE
  bt_foreach_bond(BT_ID_DEFAULT, bond_info, nullptr);
#endif
#endif
}

void BLEServer::numeric_comparison_reply(bool accept) {
  if (this->conn_ == nullptr) {
    ESP_LOGE(TAG, "Not connected");
    return;
  }
  ESP_LOGD(TAG, "Numeric comparison %s", accept ? "accepted" : "rejected");
  if (accept) {
    bt_conn_auth_passkey_confirm(this->conn_);
  } else {
    bt_conn_auth_cancel(this->conn_);
  }
}

}  // namespace esphome::zephyr_ble_server

#endif
