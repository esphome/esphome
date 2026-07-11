#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "zigbee_attribute_esp32.h"
#include "zigbee_esp32.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "zigbee_helpers_esp32.h"
#ifdef USE_WIFI
#include "esp_coexist.h"
#endif

namespace esphome::zigbee {

static const char *const TAG = "zigbee";

static ZigbeeComponent *global_zigbee = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

uint8_t *get_zcl_string(const char *str, uint8_t max_size, bool use_max_size) {
  uint8_t str_len = static_cast<uint8_t>(strlen(str));
  uint8_t zcl_str_size = use_max_size ? max_size : std::min(max_size, str_len);
  uint8_t *zcl_str = new uint8_t[zcl_str_size + 1];  // string + length octet
  zcl_str[0] = zcl_str_size;

  // Initialize payload to avoid leaking uninitialized heap contents and clamp copy length
  memset(zcl_str + 1, 0, zcl_str_size);
  uint8_t copy_len = std::min(zcl_str_size, str_len);
  if (copy_len > 0) {
    memcpy(zcl_str + 1, str, copy_len);
  }
  return zcl_str;
}

void ZigbeeComponent::esp_zigbee_alarm_bdb_commissioning(ezb_bdb_comm_mode_mask_t mode) {
  if (!esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    global_zigbee->set_timeout("zb_init", 10, [mode]() { ZigbeeComponent::esp_zigbee_alarm_bdb_commissioning(mode); });
    return;
  }
  if (ezb_bdb_start_top_level_commissioning(mode) != EZB_ERR_NONE) {
    ESP_LOGE(TAG, "Start top level commissioning failed!");
  }
  esp_zigbee_lock_release();
}

bool ZigbeeComponent::app_signal_handler(const ezb_app_signal_t *app_signal) {
  static uint8_t steering_retry_count = 0;
  ezb_app_signal_type_t signal_type = ezb_app_signal_get_type(app_signal);
  switch (signal_type) {
    case EZB_ZDO_SIGNAL_SKIP_STARTUP:
      ESP_LOGD(TAG, "Zigbee stack initialized");
      ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
      break;
    case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case EZB_BDB_SIGNAL_DEVICE_REBOOT: {
      ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *) ezb_app_signal_get_params(app_signal));
      if (status == EZB_BDB_STATUS_SUCCESS) {
        ESP_LOGD(TAG, "Device started up in %sfactory-reset mode", ezb_bdb_is_factory_new() ? "" : "non ");
        global_zigbee->started = true;
        if (ezb_bdb_is_factory_new()) {
          global_zigbee->factory_new = true;
          ESP_LOGD(TAG, "Start network steering");
          ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
        } else {
          ESP_LOGD(TAG, "Device rebooted");
          global_zigbee->joined = true;
          global_zigbee->enable_loop_soon_any_context();
        }
      } else {
        ESP_LOGW(TAG, "The %s failed with status(0x%02x), please retry", ezb_app_signal_to_string(signal_type), status);
        global_zigbee->set_timeout("zb_init", 1000, []() {
          ZigbeeComponent::esp_zigbee_alarm_bdb_commissioning(EZB_BDB_MODE_INITIALIZATION);
        });
      }
    } break;
    case EZB_BDB_SIGNAL_STEERING: {
      ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *) ezb_app_signal_get_params(app_signal));
      if (status == EZB_BDB_STATUS_SUCCESS) {
        steering_retry_count = 0;
        ezb_extpanid_t extended_pan_id;
        ezb_nwk_get_extended_panid(&extended_pan_id);
        ESP_LOGD(TAG, "Joined network successfully: PAN ID(0x%04hx, EXT: 0x%llx), Channel(%d), Short Address(0x%04hx)",
                 ezb_nwk_get_panid(), extended_pan_id.u64, ezb_nwk_get_current_channel(), ezb_nwk_get_short_address());
        global_zigbee->joined = true;
        global_zigbee->enable_loop_soon_any_context();
      } else {
        ESP_LOGD(TAG, "Failed to join network with status(0x%02x)", status);
        if (steering_retry_count < 10) {
          steering_retry_count++;
          global_zigbee->set_timeout("zb_init", 1000, []() {
            ZigbeeComponent::esp_zigbee_alarm_bdb_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
          });
        } else {
          global_zigbee->set_timeout("zb_init", 600 * 1000, []() {
            ZigbeeComponent::esp_zigbee_alarm_bdb_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
          });
        }
      }
    } break;
    case EZB_ZDO_SIGNAL_LEAVE: {
      const ezb_zdo_signal_leave_params_t *leave_params =
          (const ezb_zdo_signal_leave_params_t *) ezb_app_signal_get_params(app_signal);
      if (leave_params->leave_type == EZB_ZDO_LEAVE_TYPE_RESET) {
        esp_zigbee_factory_reset();
      }
    } break;
    default:
      ESP_LOGD(TAG, "Zigbee APP Signal: %s(type: 0x%02x)", ezb_app_signal_to_string(signal_type), signal_type);
      break;
  }
  return true;
}

static void zb_attribute_handler(ezb_zcl_set_attr_value_message_t *message) {
  ESP_RETURN_ON_FALSE(message, , TAG, "Empty message");
  ESP_RETURN_ON_FALSE(message->info.status == EZB_ZCL_STATUS_SUCCESS, , TAG, "Received message: error status(%d)",
                      message->info.status);
  ESP_LOGD(TAG, "ZCL SetAttributeValue message for endpoint(%d) cluster(0x%04x) %s with status(0x%02x)",
           message->info.dst_ep, message->info.cluster_id,
           message->info.cluster_role == EZB_ZCL_CLUSTER_SERVER ? "server" : "client", message->info.status);
}

static void zb_action_handler(ezb_zcl_core_action_callback_id_t callback_id, void *message) {
  switch (callback_id) {
    case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:
      zb_attribute_handler((ezb_zcl_set_attr_value_message_t *) message);
      break;
    case EZB_ZCL_CORE_DEFAULT_RSP_CB_ID: {
#ifdef ESPHOME_LOG_HAS_VERBOSE
      ezb_zcl_cmd_default_rsp_message_t *default_rsp = (ezb_zcl_cmd_default_rsp_message_t *) message;
      ESP_LOGV(TAG, "Received ZCL Default Response: 0x%02x", default_rsp->in.status_code);
#endif
    } break;
    default:
      ESP_LOGD(TAG, "Receive Zigbee action(0x%04x) callback", static_cast<unsigned>(callback_id));
      break;
  }
}

void ZigbeeComponent::create_default_cluster(uint8_t endpoint_id, uint16_t device_id) {
  ezb_af_ep_config_t config = {
      .ep_id = endpoint_id,
      .app_profile_id = EZB_AF_HA_PROFILE_ID,
      .app_device_id = device_id,
      .app_device_version = 0,
  };
  ezb_af_ep_desc_t ep_desc = ezb_af_create_endpoint_desc(&config);
  if (ezb_af_device_add_endpoint_desc(this->dev_desc_, ep_desc) != EZB_ERR_NONE) {
    ESP_LOGE(TAG, "Could not create endpoint %u", endpoint_id);
  }
  // Add basic cluster
  this->update_basic_cluster_(ep_desc);
  // Add identify cluster if not already present
  this->add_cluster(endpoint_id, EZB_ZCL_CLUSTER_ID_IDENTIFY, EZB_ZCL_CLUSTER_SERVER);
}

void ZigbeeComponent::add_cluster(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role) {
  if (cluster_id == EZB_ZCL_CLUSTER_ID_BASIC) {
    return;
  }
  ezb_af_ep_desc_t ep_desc = ezb_af_device_get_endpoint_desc(this->dev_desc_, endpoint_id);
  if (ep_desc == NULL) {
    ESP_LOGE(TAG, "Endpoint %u does not exist, cannot add cluster 0x%04X", endpoint_id, cluster_id);
    return;
  }
  esphome_zb_add_or_update_cluster(cluster_id, ep_desc, role);
  ESP_LOGD(TAG, "Endpoint %u: Added cluster 0x%04X with role %u", endpoint_id, cluster_id, role);
}

void ZigbeeComponent::set_basic_cluster(const char *model, const char *manufacturer, uint8_t power_source) {
  char date_buf[16];
  time_t time_val = App.get_build_time();
  struct tm *timeinfo = localtime(&time_val);
  strftime(date_buf, sizeof(date_buf), "%Y%m%d %H%M%S", timeinfo);
  this->basic_cluster_data_ = {
      .model = get_zcl_string(model, 31),
      .manufacturer = get_zcl_string(manufacturer, 31),
      .date = get_zcl_string(date_buf, 15),
      .power_source = power_source,
  };
}

void ZigbeeComponent::update_basic_cluster_(ezb_af_ep_desc_t ep_desc) {
  ezb_zcl_cluster_desc_t cluster_desc =
      ezb_af_endpoint_get_cluster_desc(ep_desc, EZB_ZCL_CLUSTER_ID_BASIC, EZB_ZCL_CLUSTER_SERVER);
  if (cluster_desc == NULL) {
    ezb_zcl_basic_cluster_config_t basic_cluster_cfg = {
        .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = this->basic_cluster_data_.power_source,
    };
    cluster_desc = ezb_zcl_basic_create_cluster_desc(&basic_cluster_cfg, EZB_ZCL_CLUSTER_SERVER);
  }
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                      this->basic_cluster_data_.manufacturer);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                      this->basic_cluster_data_.model);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_DATE_CODE_ID, this->basic_cluster_data_.date);
  ezb_af_endpoint_add_cluster_desc(ep_desc, cluster_desc);
}

bool ZigbeeComponent::register_device() {
  if (ezb_af_device_desc_register(this->dev_desc_) != EZB_ERR_NONE) {
    ESP_LOGE(TAG, "Could not register the endpoint list");
    this->mark_failed();
    return false;
  }
  return true;
}

static void ezb_task(void *pv_parameters) {
  if (!global_zigbee->register_device()) {
    vTaskDelete(NULL);
    return;
  }
  if (esp_zigbee_start(false) != ESP_OK) {
    ESP_LOGE(TAG, "Could not setup Zigbee");
    global_zigbee->mark_failed();
    vTaskDelete(NULL);
    return;  // vTaskDelete(NULL) never returns, but keep intent explicit
  }

  // Increase priority to 5 to align with openthread or BLE
  vTaskPrioritySet(NULL, 5);

  esp_zigbee_launch_mainloop();

  esp_zigbee_deinit();

  vTaskDelete(NULL);
}

ZigbeeComponent::ZigbeeComponent() {
  esp_zigbee_platform_config_t platform_config = {
      .storage_partition_name = "nvs",
      .radio_config = EZB_DEFAULT_RADIO_CONFIG(),
  };
  esp_zigbee_device_config_t device_config = {
      .device_type = this->device_role_,
      .install_code_policy = false,
  };
#ifdef CONFIG_ZB_ZCZR
  esp_zigbee_zczr_config_s zb_zczr_cfg = {
      .max_children = MAX_CHILDREN,
  };
  device_config.zczr_config = zb_zczr_cfg;
#else
  esp_zigbee_zed_config_s zb_zed_cfg = {
      .ed_timeout = EZB_NWK_ED_TIMEOUT_64MIN,
      .keep_alive = ED_KEEP_ALIVE,
  };
  device_config.zed_config = zb_zed_cfg;
#endif
  esp_zigbee_config_t config = {.device_config = device_config, .platform_config = platform_config};
  if (esp_zigbee_init(&config) != ESP_OK) {
    ESP_LOGE(TAG, "Could not initialize Zigbee");
    this->mark_failed();
    return;
  }
  this->dev_desc_ = ezb_af_create_device_desc();
}

void ZigbeeComponent::setup() {
  global_zigbee = this;
#ifdef USE_WIFI
  if (esp_coex_wifi_i154_enable() != ESP_OK) {
    this->mark_failed();
    return;
  }
#endif
  ezb_aps_secur_enable_distributed_security(false);
  ezb_nwk_set_min_join_lqi(32);
  if (ezb_app_signal_add_handler(ZigbeeComponent::app_signal_handler) != ESP_OK) {
    ESP_LOGE(TAG, "Could not set application signal handler");
    this->mark_failed();
    return;
  }

  ezb_zcl_core_action_handler_register(zb_action_handler);

  if (ezb_bdb_set_primary_channel_set(EZB_PRIMARY_CHANNEL_MASK) != ESP_OK) {
    ESP_LOGE(TAG, "Could not setup Zigbee");
    this->mark_failed();
    return;
  }

  uint8_t power_source = static_cast<uint8_t>(this->is_battery_powered() ? EZB_AF_NODE_POWER_SOURCE_RECHARGEABLE_BATTERY
                                                                         : EZB_AF_NODE_POWER_SOURCE_CONSTANT_POWER);
  ezb_af_node_power_desc_t desc = {
      .current_power_mode = EZB_AF_NODE_POWER_MODE_SYNC_ON_WHEN_IDLE,
      .available_power_sources = power_source,
      .current_power_source = power_source,
      .current_power_source_level = EZB_AF_NODE_POWER_SOURCE_LEVEL_100_PERCENT,
  };
  ezb_af_set_node_power_desc(&desc);

  // Start the Zigbee task with priority 1 to ensure main loop can still run even if Zigbee is busy
  xTaskCreate(ezb_task, "Zigbee_main", 4096, NULL, 1, NULL);
  this->disable_loop();  // loop is only needed for processing events, so disable until we join a network
}

void ZigbeeComponent::loop() {
  if (this->joined.exchange(false)) {
    this->connected_ = true;
    this->join_cb_.call(this->factory_new);
  }
  this->disable_loop();
}

void ZigbeeComponent::dump_config() {
  if (esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    ESP_LOGCONFIG(TAG,
                  "Zigbee\n"
                  "  Model: %.*s\n"
                  "  Router: %s\n"
                  "  Device is joined to the network: %s\n"
                  "  Current channel: %d\n"
                  "  Short addr: 0x%04X\n"
                  "  Short pan id: 0x%04X",
                  this->basic_cluster_data_.model[0],
                  reinterpret_cast<const char *>(this->basic_cluster_data_.model + 1),
                  YESNO(this->device_role_ == EZB_NWK_DEVICE_TYPE_ROUTER), YESNO(ezb_bdb_dev_joined()),
                  ezb_nwk_get_current_channel(), ezb_nwk_get_short_address(), ezb_nwk_get_panid());
    esp_zigbee_lock_release();
  } else {
    ESP_LOGCONFIG(TAG,
                  "Zigbee\n"
                  "  Model: %.*s\n"
                  "  Router: %s\n",
                  this->basic_cluster_data_.model[0],
                  reinterpret_cast<const char *>(this->basic_cluster_data_.model + 1),
                  YESNO(this->device_role_ == EZB_NWK_DEVICE_TYPE_ROUTER));
  }
}
}  // namespace esphome::zigbee

#endif
#endif
