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

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
  if (esp_zb_bdb_start_top_level_commissioning(mode_mask) != ESP_OK) {
    ESP_LOGE(TAG, "Start network steering failed!");
  }
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
  static uint8_t steering_retry_count = 0;
  uint32_t *p_sg_p = signal_struct->p_app_signal;
  esp_err_t err_status = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t) *p_sg_p;
  esp_zb_zdo_signal_leave_params_t *leave_params = NULL;
  switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      ESP_LOGD(TAG, "Zigbee stack initialized");
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      if (err_status == ESP_OK) {
        ESP_LOGD(TAG, "Device started up in %sfactory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non ");
        global_zigbee->started = true;
        if (esp_zb_bdb_is_factory_new()) {
          global_zigbee->factory_new = true;
          ESP_LOGD(TAG, "Start network steering");
          esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
          ESP_LOGD(TAG, "Device rebooted");
          global_zigbee->joined = true;
          global_zigbee->enable_loop_soon_any_context();
        }
      } else {
        ESP_LOGE(TAG, "FIRST_START.  Device started up in %sfactory-reset mode with an error %d (%s)",
                 esp_zb_bdb_is_factory_new() ? "" : "non ", err_status, esp_err_to_name(err_status));
        ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        esp_zb_scheduler_alarm((esp_zb_callback_t) bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_INITIALIZATION,
                               1000);
      }
      break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
      if (err_status == ESP_OK) {
        steering_retry_count = 0;
        ESP_LOGI(TAG, "Joined network successfully (PAN ID: 0x%04hx, Channel:%d)", esp_zb_get_pan_id(),
                 esp_zb_get_current_channel());
        global_zigbee->joined = true;
        global_zigbee->enable_loop_soon_any_context();
      } else {
        ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
        if (steering_retry_count < 10) {
          steering_retry_count++;
          esp_zb_scheduler_alarm((esp_zb_callback_t) bdb_start_top_level_commissioning_cb,
                                 ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        } else {
          esp_zb_scheduler_alarm((esp_zb_callback_t) bdb_start_top_level_commissioning_cb,
                                 ESP_ZB_BDB_MODE_NETWORK_STEERING, 600 * 1000);
        }
      }
      break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE:
      leave_params = (esp_zb_zdo_signal_leave_params_t *) esp_zb_app_signal_get_params(p_sg_p);
      if (leave_params->leave_type == ESP_ZB_NWK_LEAVE_TYPE_RESET) {
        esp_zb_factory_reset();
      }
      break;
    default:
      ESP_LOGD(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
               esp_err_to_name(err_status));
      break;
  }
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
  esp_err_t ret = ESP_OK;
  ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
  ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG,
                      "Received message: error status(%d)", message->info.status);
  ESP_LOGD(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)",
           message->info.dst_endpoint, message->info.cluster, message->attribute.id, message->attribute.data.size);
  return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
  esp_err_t ret = ESP_OK;
  switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
      ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *) message);
      break;
    default:
      ESP_LOGD(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
      break;
  }
  return ret;
}

void ZigbeeComponent::create_default_cluster(uint8_t endpoint_id, zb_ha_standard_devs_e device_id) {
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  this->endpoint_list_[endpoint_id] =
      std::tuple<zb_ha_standard_devs_e, esp_zb_cluster_list_t *>(device_id, cluster_list);
  // Add basic cluster
  this->add_cluster(endpoint_id, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  // Add identify cluster if not already present
  if (esp_zb_cluster_list_get_cluster(cluster_list, ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE) ==
      nullptr) {
    this->add_cluster(endpoint_id, ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  }
}

void ZigbeeComponent::add_cluster(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role) {
  esp_zb_attribute_list_t *attr_list;
  if (cluster_id == 0) {
    attr_list = create_basic_cluster_();
  } else {
    attr_list = esphome_zb_default_attr_list_create(cluster_id);
  }
  this->attribute_list_[{endpoint_id, cluster_id, role}] = attr_list;
}

void ZigbeeComponent::set_basic_cluster(const char *model, const char *manufacturer) {
  char date_buf[16];
  time_t time_val = App.get_build_time();
  struct tm *timeinfo = localtime(&time_val);
  strftime(date_buf, sizeof(date_buf), "%Y%m%d %H%M%S", timeinfo);
  this->basic_cluster_data_ = {
      .model = get_zcl_string(model, 31),
      .manufacturer = get_zcl_string(manufacturer, 31),
      .date = get_zcl_string(date_buf, 15),
  };
}

esp_zb_attribute_list_t *ZigbeeComponent::create_basic_cluster_() {
  esp_zb_basic_cluster_cfg_t basic_cluster_cfg = {
      .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
      .power_source = 0,
  };
  esp_zb_attribute_list_t *attr_list = esp_zb_basic_cluster_create(&basic_cluster_cfg);
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                this->basic_cluster_data_.manufacturer);
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, this->basic_cluster_data_.model);
  esp_zb_basic_cluster_add_attr(attr_list, ESP_ZB_ZCL_ATTR_BASIC_DATE_CODE_ID, this->basic_cluster_data_.date);
  return attr_list;
}

esp_err_t ZigbeeComponent::create_endpoint(uint8_t endpoint_id, zb_ha_standard_devs_e device_id,
                                           esp_zb_cluster_list_t *esp_zb_cluster_list) {
  esp_zb_endpoint_config_t endpoint_config = {.endpoint = endpoint_id,
                                              .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
                                              .app_device_id = device_id,
                                              .app_device_version = 0};
  return esp_zb_ep_list_add_ep(this->esp_zb_ep_list_, esp_zb_cluster_list, endpoint_config);
}

static void esp_zb_task_(void *pvParameters) {
  if (esp_zb_start(false) != ESP_OK) {
    ESP_LOGE(TAG, "Could not setup Zigbee");
    vTaskDelete(NULL);
  }
  esp_zb_set_node_descriptor_power_source(1);
  esp_zb_stack_main_loop();
}

void ZigbeeComponent::setup() {
  global_zigbee = this;
  esp_zb_platform_config_t config = {
      .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
      .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
  };
#ifdef USE_WIFI
  if (esp_coex_wifi_i154_enable() != ESP_OK) {
    this->mark_failed();
    return;
  }
#endif
  if (esp_zb_platform_config(&config) != ESP_OK) {
    this->mark_failed();
    return;
  }

  esp_zb_zed_cfg_t zb_zed_cfg = {
      .ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN,
      .keep_alive = ED_KEEP_ALIVE,
  };
  esp_zb_zczr_cfg_t zb_zczr_cfg = {
      .max_children = MAX_CHILDREN,
  };
  esp_zb_cfg_t zb_nwk_cfg = {
      .esp_zb_role = this->device_role_,
      .install_code_policy = false,
  };
#ifdef ZB_ROUTER_ROLE
  zb_nwk_cfg.nwk_cfg.zczr_cfg = zb_zczr_cfg;
#else
  zb_nwk_cfg.nwk_cfg.zed_cfg = zb_zed_cfg;
#endif
  esp_zb_init(&zb_nwk_cfg);

  esp_err_t ret;
  for (auto const &[key, val] : this->attribute_list_) {
    esp_zb_cluster_list_t *esp_zb_cluster_list = std::get<1>(this->endpoint_list_[std::get<0>(key)]);
    ret = esphome_zb_cluster_list_add_or_update_cluster(std::get<1>(key), esp_zb_cluster_list, val, std::get<2>(key));
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Could not create cluster 0x%04X with role %u: %s", std::get<1>(key), std::get<2>(key),
               esp_err_to_name(ret));
    } else {
      ESP_LOGD(TAG, "Endpoint %u: Added cluster 0x%04X with role %u", std::get<0>(key), std::get<1>(key),
               std::get<2>(key));
#ifdef ESPHOME_LOG_HAS_VERBOSE
      // Dump cluster attributes in verbose log
      ESP_LOGV(TAG, "Cluster 0x%04X attributes:", std::get<1>(key));
      esp_zb_attribute_list_t *attr_list = val;
      while (attr_list) {
        esp_zb_zcl_attr_t *attr = &attr_list->attribute;
        ESP_LOGV(TAG, "  Attr ID: 0x%04X, Type: 0x%02X, Access: 0x%02X", attr->id, attr->type, attr->access);
        attr_list = attr_list->next;
      }
#endif
    }
  }
  this->attribute_list_.clear();

  for (auto const &[ep_id, dev_id] : this->endpoint_list_) {
    if (create_endpoint(ep_id, std::get<0>(dev_id), std::get<1>(dev_id)) != ESP_OK) {
      ESP_LOGE(TAG, "Could not create endpoint %u", ep_id);
    }
  }
  this->endpoint_list_.clear();

  if (esp_zb_device_register(this->esp_zb_ep_list_) != ESP_OK) {
    ESP_LOGE(TAG, "Could not register the endpoint list");
    this->mark_failed();
    return;
  }

  esp_zb_core_action_handler_register(zb_action_handler);

  if (esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK) != ESP_OK) {
    ESP_LOGE(TAG, "Could not setup Zigbee");
    this->mark_failed();
    return;
  }
  for (auto &[_, attribute] : this->attributes_) {
    if (attribute->report_enabled) {
      esp_zb_zcl_reporting_info_t reporting_info = attribute->get_reporting_info();
      ESP_LOGD(TAG, "set reporting for cluster: %u", reporting_info.cluster_id);
      if (esp_zb_zcl_update_reporting_info(&reporting_info) != ESP_OK) {
        ESP_LOGE(TAG, "Could not configure reporting for attribute 0x%04X in cluster 0x%04X in endpoint %u",
                 reporting_info.attr_id, reporting_info.cluster_id, reporting_info.ep);
      }
    }
  }
  xTaskCreate(esp_zb_task_, "Zigbee_main", 4096, NULL, 24, NULL);
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
  if (esp_zb_lock_acquire(10 / portTICK_PERIOD_MS)) {
    ESP_LOGCONFIG(TAG,
                  "Zigbee\n"
                  "  Model: %s\n"
                  "  Router: %s\n"
                  "  Device is joined to the network: %s\n"
                  "  Current channel: %d\n"
                  "  Short addr: 0x%04X\n"
                  "  Short pan id: 0x%04X",
                  this->basic_cluster_data_.model, YESNO(this->device_role_ == ESP_ZB_DEVICE_TYPE_ROUTER),
                  YESNO(esp_zb_bdb_dev_joined()), esp_zb_get_current_channel(), esp_zb_get_short_address(),
                  esp_zb_get_pan_id());
    esp_zb_lock_release();
  } else {
    ESP_LOGCONFIG(TAG,
                  "Zigbee\n"
                  "  Model: %s\n"
                  "  Router: %s\n",
                  this->basic_cluster_data_.model, YESNO(this->device_role_ == ESP_ZB_DEVICE_TYPE_ROUTER));
  }
}
}  // namespace esphome::zigbee

#endif
#endif
