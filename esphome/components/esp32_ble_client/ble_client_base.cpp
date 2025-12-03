#include "ble_client_base.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <esp_gattc_api.h>

namespace esphome::esp32_ble_client {

static const char *const TAG = "esp32_ble_client";

// Intermediate connection parameters for standard operation
// ESP-IDF defaults (12.5-15ms) are too slow for stable connections through WiFi-based BLE proxies,
// causing disconnections. These medium parameters balance responsiveness with bandwidth usage.
static const uint16_t MEDIUM_MIN_CONN_INTERVAL = 0x07;  // 7 * 1.25ms = 8.75ms
static const uint16_t MEDIUM_MAX_CONN_INTERVAL = 0x09;  // 9 * 1.25ms = 11.25ms
// The timeout value was increased from 6s to 8s to address stability issues observed
// in certain BLE devices when operating through WiFi-based BLE proxies. The longer
// timeout reduces the likelihood of disconnections during periods of high latency.
static const uint16_t MEDIUM_CONN_TIMEOUT = 800;  // 800 * 10ms = 8s

// Fastest connection parameters for devices with short discovery timeouts
static const uint16_t FAST_MIN_CONN_INTERVAL = 0x06;  // 6 * 1.25ms = 7.5ms (BLE minimum)
static const uint16_t FAST_MAX_CONN_INTERVAL = 0x06;  // 6 * 1.25ms = 7.5ms
static const uint16_t FAST_CONN_TIMEOUT = 1000;       // 1000 * 10ms = 10s
static const esp_bt_uuid_t NOTIFY_DESC_UUID = {
    .len = ESP_UUID_LEN_16,
    .uuid =
        {
            .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
        },
};

void BLEClientBase::setup() {
  static uint8_t connection_index = 0;
  this->connection_index_ = connection_index++;
}

void BLEClientBase::set_state(espbt::ClientState st) {
  ESP_LOGV(TAG, "[%d] [%s] Set state %d", this->connection_index_, this->address_str_, (int) st);
  ESPBTClient::set_state(st);
}

void BLEClientBase::loop() {
  if (!esp32_ble::global_ble->is_active()) {
    this->set_state(espbt::ClientState::INIT);
    return;
  }
  if (this->state_ == espbt::ClientState::INIT) {
    auto ret = esp_ble_gattc_app_register(this->app_id);
    if (ret) {
      ESP_LOGE(TAG, "gattc app register failed. app_id=%d code=%d", this->app_id, ret);
      this->mark_failed();
    }
    this->set_state(espbt::ClientState::IDLE);
  }
  // If idle, we can disable the loop as connect()
  // will enable it again when a connection is needed.
  else if (this->state_ == espbt::ClientState::IDLE) {
    this->disable_loop();
  }
}

float BLEClientBase::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void BLEClientBase::dump_config() {
  ESP_LOGCONFIG(TAG,
                "  Address: %s\n"
                "  Auto-Connect: %s",
                this->address_str(), TRUEFALSE(this->auto_connect_));
  ESP_LOGCONFIG(TAG, "  State: %s", espbt::client_state_to_string(this->state()));
  if (this->status_ == ESP_GATT_NO_RESOURCES) {
    ESP_LOGE(TAG, "  Failed due to no resources. Try to reduce number of BLE clients in config.");
  } else if (this->status_ != ESP_GATT_OK) {
    ESP_LOGW(TAG, "  Failed due to error code %d", this->status_);
  }
}

#ifdef USE_ESP32_BLE_DEVICE
bool BLEClientBase::parse_device(const espbt::ESPBTDevice &device) {
  if (!this->auto_connect_)
    return false;
  if (this->address_ == 0 || device.address_uint64() != this->address_)
    return false;
  if (this->state_ != espbt::ClientState::IDLE)
    return false;

  this->log_event_("Found device");
  if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG)
    esp32_ble_tracker::global_esp32_ble_tracker->print_bt_device_info(device);

  this->set_state(espbt::ClientState::DISCOVERED);
  this->set_address(device.address_uint64());
  this->remote_addr_type_ = device.get_address_type();
  return true;
}
#endif

void BLEClientBase::connect() {
  // Prevent duplicate connection attempts
  if (this->state_ == espbt::ClientState::CONNECTING || this->state_ == espbt::ClientState::CONNECTED ||
      this->state_ == espbt::ClientState::ESTABLISHED) {
    ESP_LOGW(TAG, "[%d] [%s] Connection already in progress, state=%s", this->connection_index_, this->address_str_,
             espbt::client_state_to_string(this->state_));
    return;
  }
  ESP_LOGI(TAG, "[%d] [%s] 0x%02x Connecting", this->connection_index_, this->address_str_, this->remote_addr_type_);
  this->paired_ = false;
  // Enable loop for state processing
  this->enable_loop();
  // Immediately transition to CONNECTING to prevent duplicate connection attempts
  this->set_state(espbt::ClientState::CONNECTING);

  // Determine connection parameters based on connection type
  if (this->connection_type_ == espbt::ConnectionType::V3_WITHOUT_CACHE) {
    // V3 without cache needs fast params for service discovery
    this->set_conn_params_(FAST_MIN_CONN_INTERVAL, FAST_MAX_CONN_INTERVAL, 0, FAST_CONN_TIMEOUT, "fast");
  } else if (this->connection_type_ == espbt::ConnectionType::V3_WITH_CACHE) {
    // V3 with cache can use medium params
    this->set_conn_params_(MEDIUM_MIN_CONN_INTERVAL, MEDIUM_MAX_CONN_INTERVAL, 0, MEDIUM_CONN_TIMEOUT, "medium");
  }
  // For V1/Legacy, don't set params - use ESP-IDF defaults

  // Open the connection
  auto ret = esp_ble_gattc_open(this->gattc_if_, this->remote_bda_, this->remote_addr_type_, true);
  this->handle_connection_result_(ret);
}

esp_err_t BLEClientBase::pair() { return esp_ble_set_encryption(this->remote_bda_, ESP_BLE_SEC_ENCRYPT); }

void BLEClientBase::disconnect() {
  if (this->state_ == espbt::ClientState::IDLE || this->state_ == espbt::ClientState::DISCONNECTING) {
    ESP_LOGI(TAG, "[%d] [%s] Disconnect requested, but already %s", this->connection_index_, this->address_str_,
             espbt::client_state_to_string(this->state_));
    return;
  }
  if (this->state_ == espbt::ClientState::CONNECTING || this->conn_id_ == UNSET_CONN_ID) {
    ESP_LOGD(TAG, "[%d] [%s] Disconnect before connected, disconnect scheduled", this->connection_index_,
             this->address_str_);
    this->want_disconnect_ = true;
    return;
  }
  this->unconditional_disconnect();
}

void BLEClientBase::unconditional_disconnect() {
  // Disconnect without checking the state.
  ESP_LOGI(TAG, "[%d] [%s] Disconnecting (conn_id: %d).", this->connection_index_, this->address_str_, this->conn_id_);
  if (this->state_ == espbt::ClientState::DISCONNECTING) {
    this->log_error_("Already disconnecting");
    return;
  }
  if (this->conn_id_ == UNSET_CONN_ID) {
    this->log_error_("conn id unset, cannot disconnect");
    return;
  }
  auto err = esp_ble_gattc_close(this->gattc_if_, this->conn_id_);
  if (err != ESP_OK) {
    //
    // This is a fatal error, but we can't do anything about it
    // and it likely means the BLE stack is in a bad state.
    //
    // In the future we might consider App.reboot() here since
    // the BLE stack is in an indeterminate state.
    //
    this->log_gattc_warning_("esp_ble_gattc_close", err);
  }

  if (this->state_ == espbt::ClientState::DISCOVERED) {
    this->set_address(0);
    this->set_state(espbt::ClientState::IDLE);
  } else {
    this->set_state(espbt::ClientState::DISCONNECTING);
  }
}

void BLEClientBase::release_services() {
#ifdef USE_ESP32_BLE_DEVICE
  for (auto &svc : this->services_)
    delete svc;  // NOLINT(cppcoreguidelines-owning-memory)
  this->services_.clear();
#endif
#ifndef CONFIG_BT_GATTC_CACHE_NVS_FLASH
  esp_ble_gattc_cache_clean(this->remote_bda_);
#endif
}

void BLEClientBase::log_event_(const char *name) {
  ESP_LOGD(TAG, "[%d] [%s] %s", this->connection_index_, this->address_str_, name);
}

void BLEClientBase::log_gattc_event_(const char *name) {
  ESP_LOGD(TAG, "[%d] [%s] ESP_GATTC_%s_EVT", this->connection_index_, this->address_str_, name);
}

void BLEClientBase::log_gattc_warning_(const char *operation, esp_gatt_status_t status) {
  ESP_LOGW(TAG, "[%d] [%s] %s error, status=%d", this->connection_index_, this->address_str_, operation, status);
}

void BLEClientBase::log_gattc_warning_(const char *operation, esp_err_t err) {
  ESP_LOGW(TAG, "[%d] [%s] %s error, status=%d", this->connection_index_, this->address_str_, operation, err);
}

void BLEClientBase::log_connection_params_(const char *param_type) {
  ESP_LOGD(TAG, "[%d] [%s] %s conn params", this->connection_index_, this->address_str_, param_type);
}

void BLEClientBase::handle_connection_result_(esp_err_t ret) {
  if (ret) {
    this->log_gattc_warning_("esp_ble_gattc_open", ret);
    this->set_state(espbt::ClientState::IDLE);
  }
}

void BLEClientBase::log_error_(const char *message) {
  ESP_LOGE(TAG, "[%d] [%s] %s", this->connection_index_, this->address_str_, message);
}

void BLEClientBase::log_error_(const char *message, int code) {
  ESP_LOGE(TAG, "[%d] [%s] %s=%d", this->connection_index_, this->address_str_, message, code);
}

void BLEClientBase::log_warning_(const char *message) {
  ESP_LOGW(TAG, "[%d] [%s] %s", this->connection_index_, this->address_str_, message);
}

void BLEClientBase::update_conn_params_(uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                        uint16_t timeout, const char *param_type) {
  esp_ble_conn_update_params_t conn_params = {{0}};
  memcpy(conn_params.bda, this->remote_bda_, sizeof(esp_bd_addr_t));
  conn_params.min_int = min_interval;
  conn_params.max_int = max_interval;
  conn_params.latency = latency;
  conn_params.timeout = timeout;
  this->log_connection_params_(param_type);
  esp_err_t err = esp_ble_gap_update_conn_params(&conn_params);
  if (err != ESP_OK) {
    this->log_gattc_warning_("esp_ble_gap_update_conn_params", err);
  }
}

void BLEClientBase::set_conn_params_(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout,
                                     const char *param_type) {
  // Set preferred connection parameters before connecting
  // These will be used when establishing the connection
  this->log_connection_params_(param_type);
  esp_err_t err = esp_ble_gap_set_prefer_conn_params(this->remote_bda_, min_interval, max_interval, latency, timeout);
  if (err != ESP_OK) {
    this->log_gattc_warning_("esp_ble_gap_set_prefer_conn_params", err);
  }
}

bool BLEClientBase::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t esp_gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
  if (event == ESP_GATTC_REG_EVT && this->app_id != param->reg.app_id)
    return false;
  if (event != ESP_GATTC_REG_EVT && esp_gattc_if != ESP_GATT_IF_NONE && esp_gattc_if != this->gattc_if_)
    return false;

  ESP_LOGV(TAG, "[%d] [%s] gattc_event_handler: event=%d gattc_if=%d", this->connection_index_, this->address_str_,
           event, esp_gattc_if);

  switch (event) {
    case ESP_GATTC_REG_EVT: {
      if (param->reg.status == ESP_GATT_OK) {
        ESP_LOGV(TAG, "[%d] [%s] gattc registered app id %d", this->connection_index_, this->address_str_,
                 this->app_id);
        this->gattc_if_ = esp_gattc_if;
      } else {
        this->log_error_("gattc app registration failed status", param->reg.status);
        this->status_ = param->reg.status;
        this->mark_failed();
      }
      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      if (!this->check_addr(param->open.remote_bda))
        return false;
      this->log_gattc_event_("OPEN");
      // conn_id was already set in ESP_GATTC_CONNECT_EVT
      this->service_count_ = 0;

      // ESP-IDF's BLE stack may send ESP_GATTC_OPEN_EVT after esp_ble_gattc_open() returns an
      // error, if the error occurred at the BTA/GATT layer. This can result in the event
      // arriving after we've already transitioned to IDLE state.
      if (this->state_ == espbt::ClientState::IDLE) {
        ESP_LOGD(TAG, "[%d] [%s] ESP_GATTC_OPEN_EVT in IDLE state (status=%d), ignoring", this->connection_index_,
                 this->address_str_, param->open.status);
        break;
      }

      if (this->state_ != espbt::ClientState::CONNECTING) {
        // This should not happen but lets log it in case it does
        // because it means we have a bad assumption about how the
        // ESP BT stack works.
        ESP_LOGE(TAG, "[%d] [%s] ESP_GATTC_OPEN_EVT in %s state (status=%d)", this->connection_index_,
                 this->address_str_, espbt::client_state_to_string(this->state_), param->open.status);
      }
      if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
        this->log_gattc_warning_("Connection open", param->open.status);
        this->set_state(espbt::ClientState::IDLE);
        break;
      }
      if (this->want_disconnect_) {
        // Disconnect was requested after connecting started,
        // but before the connection was established. Now that we have
        // this->conn_id_ set, we can disconnect it.
        this->unconditional_disconnect();
        this->conn_id_ = UNSET_CONN_ID;
        break;
      }
      // MTU negotiation already started in ESP_GATTC_CONNECT_EVT
      this->set_state(espbt::ClientState::CONNECTED);
      ESP_LOGI(TAG, "[%d] [%s] Connection open", this->connection_index_, this->address_str_);
      if (this->connection_type_ == espbt::ConnectionType::V3_WITH_CACHE) {
        // Cached connections already connected with medium parameters, no update needed
        // only set our state, subclients might have more stuff to do yet.
        this->state_ = espbt::ClientState::ESTABLISHED;
        break;
      }
      // For V3_WITHOUT_CACHE, we already set fast params before connecting
      // No need to update them again here
      this->log_event_("Searching for services");
      esp_ble_gattc_search_service(esp_gattc_if, param->cfg_mtu.conn_id, nullptr);
      break;
    }
    case ESP_GATTC_CONNECT_EVT: {
      if (!this->check_addr(param->connect.remote_bda))
        return false;
      this->log_gattc_event_("CONNECT");
      this->conn_id_ = param->connect.conn_id;
      // Start MTU negotiation immediately as recommended by ESP-IDF examples
      // (gatt_client, ble_throughput) which call esp_ble_gattc_send_mtu_req in
      // ESP_GATTC_CONNECT_EVT instead of waiting for ESP_GATTC_OPEN_EVT.
      // This saves ~3ms in the connection process.
      auto ret = esp_ble_gattc_send_mtu_req(this->gattc_if_, param->connect.conn_id);
      if (ret) {
        this->log_gattc_warning_("esp_ble_gattc_send_mtu_req", ret);
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      if (!this->check_addr(param->disconnect.remote_bda))
        return false;
      // Check if we were disconnected while waiting for service discovery
      if (param->disconnect.reason == ESP_GATT_CONN_TERMINATE_PEER_USER &&
          this->state_ == espbt::ClientState::CONNECTED) {
        this->log_warning_("Remote closed during discovery");
      } else {
        ESP_LOGD(TAG, "[%d] [%s] ESP_GATTC_DISCONNECT_EVT, reason 0x%02x", this->connection_index_, this->address_str_,
                 param->disconnect.reason);
      }
      this->release_services();
      this->set_state(espbt::ClientState::IDLE);
      break;
    }

    case ESP_GATTC_CFG_MTU_EVT: {
      if (this->conn_id_ != param->cfg_mtu.conn_id)
        return false;
      if (param->cfg_mtu.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%d] [%s] cfg_mtu failed, mtu %d, status %d", this->connection_index_, this->address_str_,
                 param->cfg_mtu.mtu, param->cfg_mtu.status);
        // No state change required here - disconnect event will follow if needed.
        break;
      }
      ESP_LOGD(TAG, "[%d] [%s] cfg_mtu status %d, mtu %d", this->connection_index_, this->address_str_,
               param->cfg_mtu.status, param->cfg_mtu.mtu);
      this->mtu_ = param->cfg_mtu.mtu;
      break;
    }
    case ESP_GATTC_CLOSE_EVT: {
      if (this->conn_id_ != param->close.conn_id)
        return false;
      this->log_gattc_event_("CLOSE");
      this->release_services();
      this->set_state(espbt::ClientState::IDLE);
      this->conn_id_ = UNSET_CONN_ID;
      break;
    }
    case ESP_GATTC_SEARCH_RES_EVT: {
      if (this->conn_id_ != param->search_res.conn_id)
        return false;
      this->service_count_++;
      if (this->connection_type_ == espbt::ConnectionType::V3_WITHOUT_CACHE) {
        // V3 clients don't need services initialized since
        // as they use the ESP APIs to get services.
        break;
      }
#ifdef USE_ESP32_BLE_DEVICE
      BLEService *ble_service = new BLEService();  // NOLINT(cppcoreguidelines-owning-memory)
      ble_service->uuid = espbt::ESPBTUUID::from_uuid(param->search_res.srvc_id.uuid);
      ble_service->start_handle = param->search_res.start_handle;
      ble_service->end_handle = param->search_res.end_handle;
      ble_service->client = this;
      this->services_.push_back(ble_service);
#endif
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (this->conn_id_ != param->search_cmpl.conn_id)
        return false;
      this->log_gattc_event_("SEARCH_CMPL");
      // For V3_WITHOUT_CACHE, switch back to medium connection parameters after service discovery
      // This balances performance with bandwidth usage after the critical discovery phase
      if (this->connection_type_ == espbt::ConnectionType::V3_WITHOUT_CACHE) {
        this->update_conn_params_(MEDIUM_MIN_CONN_INTERVAL, MEDIUM_MAX_CONN_INTERVAL, 0, MEDIUM_CONN_TIMEOUT, "medium");
      } else if (this->connection_type_ != espbt::ConnectionType::V3_WITH_CACHE) {
#ifdef USE_ESP32_BLE_DEVICE
        for (auto &svc : this->services_) {
          ESP_LOGV(TAG, "[%d] [%s] Service UUID: %s", this->connection_index_, this->address_str_,
                   svc->uuid.to_string().c_str());
          ESP_LOGV(TAG, "[%d] [%s]  start_handle: 0x%x  end_handle: 0x%x", this->connection_index_, this->address_str_,
                   svc->start_handle, svc->end_handle);
        }
#endif
      }
      ESP_LOGI(TAG, "[%d] [%s] Service discovery complete", this->connection_index_, this->address_str_);
      this->state_ = espbt::ClientState::ESTABLISHED;
      break;
    }
    case ESP_GATTC_READ_DESCR_EVT: {
      if (this->conn_id_ != param->write.conn_id)
        return false;
      this->log_gattc_event_("READ_DESCR");
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (this->conn_id_ != param->write.conn_id)
        return false;
      this->log_gattc_event_("WRITE_DESCR");
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (this->conn_id_ != param->write.conn_id)
        return false;
      this->log_gattc_event_("WRITE_CHAR");
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (this->conn_id_ != param->read.conn_id)
        return false;
      this->log_gattc_event_("READ_CHAR");
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (this->conn_id_ != param->notify.conn_id)
        return false;
      this->log_gattc_event_("NOTIFY");
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->log_gattc_event_("REG_FOR_NOTIFY");
      if (this->connection_type_ == espbt::ConnectionType::V3_WITH_CACHE ||
          this->connection_type_ == espbt::ConnectionType::V3_WITHOUT_CACHE) {
        // Client is responsible for flipping the descriptor value
        // when using the cache
        break;
      }
      esp_gattc_descr_elem_t desc_result;
      uint16_t count = 1;
      esp_gatt_status_t descr_status = esp_ble_gattc_get_descr_by_char_handle(
          this->gattc_if_, this->conn_id_, param->reg_for_notify.handle, NOTIFY_DESC_UUID, &desc_result, &count);
      if (descr_status != ESP_GATT_OK) {
        this->log_gattc_warning_("esp_ble_gattc_get_descr_by_char_handle", descr_status);
        break;
      }
      esp_gattc_char_elem_t char_result;
      esp_gatt_status_t char_status =
          esp_ble_gattc_get_all_char(this->gattc_if_, this->conn_id_, param->reg_for_notify.handle,
                                     param->reg_for_notify.handle, &char_result, &count, 0);
      if (char_status != ESP_GATT_OK) {
        this->log_gattc_warning_("esp_ble_gattc_get_all_char", char_status);
        break;
      }

      /*
        1 = notify
        2 = indicate
      */
      uint16_t notify_en = char_result.properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY ? 1 : 2;
      esp_err_t status =
          esp_ble_gattc_write_char_descr(this->gattc_if_, this->conn_id_, desc_result.handle, sizeof(notify_en),
                                         (uint8_t *) &notify_en, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
      ESP_LOGD(TAG, "Wrote notify descriptor %d, properties=%d", notify_en, char_result.properties);
      if (status) {
        this->log_gattc_warning_("esp_ble_gattc_write_char_descr", status);
      }
      break;
    }

    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: {
      this->log_gattc_event_("UNREG_FOR_NOTIFY");
      break;
    }

    default:
      // ideally would check all other events for matching conn_id
      ESP_LOGD(TAG, "[%d] [%s] Event %d", this->connection_index_, this->address_str_, event);
      break;
  }
  return true;
}

// clients can't call defer() directly since it's protected.
void BLEClientBase::run_later(std::function<void()> &&f) {  // NOLINT
  this->defer(std::move(f));
}

void BLEClientBase::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    // This event is sent by the server when it requests security
    case ESP_GAP_BLE_SEC_REQ_EVT:
      if (!this->check_addr(param->ble_security.auth_cmpl.bd_addr))
        return;
      ESP_LOGV(TAG, "[%d] [%s] ESP_GAP_BLE_SEC_REQ_EVT %x", this->connection_index_, this->address_str_, event);
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;
    // This event is sent once authentication has completed
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      if (!this->check_addr(param->ble_security.auth_cmpl.bd_addr))
        return;
      esp_bd_addr_t bd_addr;
      memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
      ESP_LOGI(TAG, "[%d] [%s] auth complete addr: %s", this->connection_index_, this->address_str_,
               format_hex(bd_addr, 6).c_str());
      if (!param->ble_security.auth_cmpl.success) {
        this->log_error_("auth fail reason", param->ble_security.auth_cmpl.fail_reason);
      } else {
        this->paired_ = true;
        ESP_LOGD(TAG, "[%d] [%s] auth success type = %d mode = %d", this->connection_index_, this->address_str_,
                 param->ble_security.auth_cmpl.addr_type, param->ble_security.auth_cmpl.auth_mode);
      }
      break;

    // There are other events we'll want to implement at some point to support things like pass key
    // https://github.com/espressif/esp-idf/blob/cba69dd088344ed9d26739f04736ae7a37541b3a/examples/bluetooth/bluedroid/ble/gatt_security_client/tutorial/Gatt_Security_Client_Example_Walkthrough.md
    default:
      break;
  }
}

// Parse GATT values into a float for a sensor.
// Ref: https://www.bluetooth.com/specifications/assigned-numbers/format-types/
float BLEClientBase::parse_char_value(uint8_t *value, uint16_t length) {
  // A length of one means a single octet value.
  if (length == 0)
    return 0;
  if (length == 1)
    return (float) ((uint8_t) value[0]);

  switch (value[0]) {
    case 0x1:  // boolean.
    case 0x2:  // 2bit.
    case 0x3:  // nibble.
    case 0x4:  // uint8.
      return (float) ((uint8_t) value[1]);
    case 0x5:  // uint12.
    case 0x6:  // uint16.
      if (length > 2) {
        return (float) encode_uint16(value[1], value[2]);
      }
      [[fallthrough]];
    case 0x7:  // uint24.
      if (length > 3) {
        return (float) encode_uint24(value[1], value[2], value[3]);
      }
      [[fallthrough]];
    case 0x8:  // uint32.
      if (length > 4) {
        return (float) encode_uint32(value[1], value[2], value[3], value[4]);
      }
      [[fallthrough]];
    case 0xC:  // int8.
      return (float) ((int8_t) value[1]);
    case 0xD:  // int12.
    case 0xE:  // int16.
      if (length > 2) {
        return (float) ((int16_t) (value[1] << 8) + (int16_t) value[2]);
      }
      [[fallthrough]];
    case 0xF:  // int24.
      if (length > 3) {
        return (float) ((int32_t) (value[1] << 16) + (int32_t) (value[2] << 8) + (int32_t) (value[3]));
      }
      [[fallthrough]];
    case 0x10:  // int32.
      if (length > 4) {
        return (float) ((int32_t) (value[1] << 24) + (int32_t) (value[2] << 16) + (int32_t) (value[3] << 8) +
                        (int32_t) (value[4]));
      }
  }
  ESP_LOGW(TAG, "[%d] [%s] Cannot parse characteristic value of type 0x%x length %d", this->connection_index_,
           this->address_str_, value[0], length);
  return NAN;
}

#ifdef USE_ESP32_BLE_DEVICE
BLEService *BLEClientBase::get_service(espbt::ESPBTUUID uuid) {
  for (auto *svc : this->services_) {
    if (svc->uuid == uuid)
      return svc;
  }
  return nullptr;
}

BLEService *BLEClientBase::get_service(uint16_t uuid) { return this->get_service(espbt::ESPBTUUID::from_uint16(uuid)); }

BLECharacteristic *BLEClientBase::get_characteristic(espbt::ESPBTUUID service, espbt::ESPBTUUID chr) {
  auto *svc = this->get_service(service);
  if (svc == nullptr)
    return nullptr;
  return svc->get_characteristic(chr);
}

BLECharacteristic *BLEClientBase::get_characteristic(uint16_t service, uint16_t chr) {
  return this->get_characteristic(espbt::ESPBTUUID::from_uint16(service), espbt::ESPBTUUID::from_uint16(chr));
}

BLECharacteristic *BLEClientBase::get_characteristic(uint16_t handle) {
  for (auto *svc : this->services_) {
    if (!svc->parsed)
      svc->parse_characteristics();
    for (auto *chr : svc->characteristics) {
      if (chr->handle == handle)
        return chr;
    }
  }
  return nullptr;
}

BLEDescriptor *BLEClientBase::get_config_descriptor(uint16_t handle) {
  auto *chr = this->get_characteristic(handle);
  if (chr != nullptr) {
    if (!chr->parsed)
      chr->parse_descriptors();
    for (auto &desc : chr->descriptors) {
      if (desc->uuid.get_uuid().uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG)
        return desc;
    }
  }
  return nullptr;
}

BLEDescriptor *BLEClientBase::get_descriptor(espbt::ESPBTUUID service, espbt::ESPBTUUID chr, espbt::ESPBTUUID descr) {
  auto *svc = this->get_service(service);
  if (svc == nullptr)
    return nullptr;
  auto *ch = svc->get_characteristic(chr);
  if (ch == nullptr)
    return nullptr;
  return ch->get_descriptor(descr);
}

BLEDescriptor *BLEClientBase::get_descriptor(uint16_t service, uint16_t chr, uint16_t descr) {
  return this->get_descriptor(espbt::ESPBTUUID::from_uint16(service), espbt::ESPBTUUID::from_uint16(chr),
                              espbt::ESPBTUUID::from_uint16(descr));
}

BLEDescriptor *BLEClientBase::get_descriptor(uint16_t handle) {
  for (auto *svc : this->services_) {
    if (!svc->parsed)
      svc->parse_characteristics();
    for (auto *chr : svc->characteristics) {
      if (!chr->parsed)
        chr->parse_descriptors();
      for (auto *desc : chr->descriptors) {
        if (desc->handle == handle)
          return desc;
      }
    }
  }
  return nullptr;
}
#endif  // USE_ESP32_BLE_DEVICE

}  // namespace esphome::esp32_ble_client

#endif  // USE_ESP32
