#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include <algorithm>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "zigbee_attribute_esp32.h"
#include "zigbee_esp32.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "zigbee_helpers_esp32.h"
#ifdef USE_WIFI
#include "esp_coexist.h"
#endif

namespace esphome::zigbee {

ESPHOME_LOG_TAG(TAG, "zigbee");

static constexpr uint32_t COMMISSIONING_RETRY_TIMEOUT_ID = 0;
static constexpr uint32_t PARENT_LINK_TIMEOUT_ID = 1;
static constexpr uint32_t PARENT_LINK_RETRY_TIMEOUT_ID = 2;

// Since esp-zigbee-lib 2.0.5 every lost poll or transmission raises PARENT_LINK_FAILURE, even during a short radio
// outage such as a BLE scan. After a failure the device polls at least every 3 s. It rejoins only after failures in
// 4 polls that span at least 6 s; failures less than two thirds of a polling interval apart (poll retries, fast
// polls) count once. Two intervals without a counted failure mean that a poll got through.
static constexpr uint8_t PARENT_LINK_MISSED_POLLS = 4;
static constexpr uint32_t PARENT_LINK_POLL_INTERVAL_MS = 3000;
static constexpr uint32_t PARENT_LINK_MIN_SPAN_MS = 6000;

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

void ZigbeeComponent::factory_reset() {
  esp_zigbee_lock_acquire(portMAX_DELAY);
  if (this->joined_) {
    // send leave request and trigger EZB_ZDO_SIGNAL_LEAVE
    ezb_bdb_reset_via_local_action();
  } else {
    esp_zigbee_factory_reset();  // triggers a reboot
  }
  esp_zigbee_lock_release();
}

void ZigbeeComponent::report() {
  for (const auto &[_, attribute] : this->attributes_) {
    attribute->report();
  }
}

void ZigbeeComponent::esp_zigbee_alarm_bdb_commissioning(ezb_bdb_comm_mode_mask_t mode) {
  if (!esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    global_zigbee->set_timeout(COMMISSIONING_RETRY_TIMEOUT_ID, 100,
                               [mode]() { ZigbeeComponent::esp_zigbee_alarm_bdb_commissioning(mode); });
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
      global_zigbee->started_ = true;
      global_zigbee->enable_loop_soon_any_context();
      ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
      break;
    case EZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case EZB_BDB_SIGNAL_DEVICE_REBOOT: {
      ezb_bdb_comm_status_t status = *((ezb_bdb_comm_status_t *) ezb_app_signal_get_params(app_signal));
      if (status == EZB_BDB_STATUS_SUCCESS) {
        ESP_LOGD(TAG, "Device started up in %sfactory-reset mode", ezb_bdb_is_factory_new() ? "" : "non ");
        if (ezb_bdb_is_factory_new()) {
          global_zigbee->factory_new_ = true;
          ESP_LOGD(TAG, "Start network steering");
          ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
        } else {
          ESP_LOGD(TAG, "Device rebooted");
          global_zigbee->joined_ = true;
          global_zigbee->join_pending_ = true;
          global_zigbee->enable_loop_soon_any_context();
        }
      } else {
        ESP_LOGW(TAG, "The %s failed with status(0x%02x), please retry", ezb_app_signal_to_string(signal_type), status);
        global_zigbee->set_timeout(COMMISSIONING_RETRY_TIMEOUT_ID, 1000, []() {
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
        global_zigbee->joined_ = true;
        global_zigbee->join_pending_ = true;
        global_zigbee->enable_loop_soon_any_context();
      } else {
        ESP_LOGD(TAG, "Failed to join network with status(0x%02x)", status);
        if (steering_retry_count < 10) {
          steering_retry_count++;
          global_zigbee->set_timeout(COMMISSIONING_RETRY_TIMEOUT_ID, 1000, []() {
            ZigbeeComponent::esp_zigbee_alarm_bdb_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
          });
        } else {
          global_zigbee->set_timeout(COMMISSIONING_RETRY_TIMEOUT_ID, 600 * 1000, []() {
            ZigbeeComponent::esp_zigbee_alarm_bdb_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
          });
        }
      }
    } break;
    case EZB_ZDO_SIGNAL_LEAVE: {
      const ezb_zdo_signal_leave_params_t *leave_params =
          (const ezb_zdo_signal_leave_params_t *) ezb_app_signal_get_params(app_signal);
      if (leave_params->leave_type == EZB_ZDO_LEAVE_TYPE_RESET) {
        esp_zigbee_factory_reset();  // triggers a reboot
      }
      global_zigbee->joined_ = false;
      // For a leave asked for by the network: failures before it must not count after the rejoin
      global_zigbee->reset_parent_link_failures_();
    } break;
    case EZB_NWK_SIGNAL_NETWORK_STATUS: {
      const ezb_nwk_signal_network_status_params_t *network_status_params =
          (const ezb_nwk_signal_network_status_params_t *) ezb_app_signal_get_params(app_signal);
      if (network_status_params->status == EZB_NWK_NETWORK_STATUS_PARENT_LINK_FAILURE) {
        if (global_zigbee->joined_) {  // else not joined yet, or a rejoin is running
          global_zigbee->on_parent_link_failure_();
        }
      } else {
        ESP_LOGD(TAG, "Zigbee APP Signal NETWORK_STATUS: 0x%02x", network_status_params->status);
      }
    } break;
    default:
      ESP_LOGD(TAG, "Zigbee APP Signal: %s(type: 0x%02x)", ezb_app_signal_to_string(signal_type), signal_type);
      break;
  }
  return true;
}

// Runs on the Zigbee task with the stack lock held
void ZigbeeComponent::on_parent_link_failure_() {
  const uint32_t now = millis();
  if (this->link_failures_ == 0) {
    // Poll faster for now, so that a lost parent is noticed soon also with a long polling interval
    this->saved_keep_alive_ = ezb_nwk_get_keepalive_interval();
    if (this->saved_keep_alive_ > PARENT_LINK_POLL_INTERVAL_MS) {
      ezb_nwk_set_keepalive_interval(PARENT_LINK_POLL_INTERVAL_MS);
    }
  }
  // The polling interval during the series
  const uint32_t interval = std::min(this->saved_keep_alive_, PARENT_LINK_POLL_INTERVAL_MS);
  if (this->link_failures_ > 0) {
    if (now - this->last_link_failure_ms_ >= 2 * interval) {
      // The series has ended, but its timeout has not run yet: start a new one, keep the saved interval
      this->link_failures_ = 0;
    } else if (now - this->last_link_failure_ms_ < 2 * interval / 3) {
      ESP_LOGV(TAG, "Parent link failure (same poll)");
      return;
    }
  }
  if (this->link_failures_ == 0) {
    this->first_link_failure_ms_ = now;
  }
  this->last_link_failure_ms_ = now;
  this->link_failure_seq_++;
  if (this->link_failures_ < UINT8_MAX) {
    this->link_failures_++;
  }
  if (this->link_failures_ < PARENT_LINK_MISSED_POLLS || now - this->first_link_failure_ms_ < PARENT_LINK_MIN_SPAN_MS) {
    ESP_LOGD(TAG, "Parent link failure %u", this->link_failures_);
    // The stack does not report a poll that got through, so the series ends when no failure follows
    const uint8_t seq = this->link_failure_seq_;
    this->set_timeout(PARENT_LINK_TIMEOUT_ID, 2 * interval, [this, seq]() { this->end_parent_link_failures_(seq); });
    return;
  }
  ESP_LOGW(TAG, "Parent link failed in %u polls, attempting rejoin", this->link_failures_);
  this->cancel_timeout(PARENT_LINK_TIMEOUT_ID);
  this->reset_parent_link_failures_();
  this->joined_ = false;
  ezb_zdo_nwk_mgmt_leave_req_t leave_req = {
      .dst_nwk_addr = ezb_nwk_get_short_address(),
      .field =
          {
              .remove_children = false,
              .rejoin = true,
          },
  };
  // Send leave request to the network to rejoin
  // triggers EZB_ZDO_SIGNAL_LEAVE signal first, then EZB_BDB_SIGNAL_DEVICE_REBOOT
  if (ezb_zdo_nwk_mgmt_leave_req(&leave_req) != EZB_ERR_NONE) {
    ESP_LOGW(TAG, "Leave request for rejoin failed");
    this->joined_ = true;  // still joined, so later failures can start a rejoin again
  }
}

// Runs on the main loop when no failure was counted for two poll intervals
void ZigbeeComponent::end_parent_link_failures_(uint8_t seq) {
  if (!esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    // Retry with its own id, so that it cannot replace the timeout of a newer failure
    this->set_timeout(PARENT_LINK_RETRY_TIMEOUT_ID, 100, [this, seq]() { this->end_parent_link_failures_(seq); });
    return;
  }
  // A newer failure has set its own timeout, which ends the series instead
  const uint8_t failures = seq == this->link_failure_seq_ ? this->link_failures_ : 0;
  if (failures > 0) {
    this->reset_parent_link_failures_();
  }
  esp_zigbee_lock_release();
  if (failures > 0) {  // after the release: the main task writes log lines out at once
    ESP_LOGD(TAG, "Parent link failures ended (%u)", failures);
  }
}

// Ends the current series, if any. Needs the stack lock
void ZigbeeComponent::reset_parent_link_failures_() {
  if (this->link_failures_ == 0) {
    return;  // no series: the saved interval is stale
  }
  this->link_failures_ = 0;
  // Put the polling interval back, unless something else changed it in the meantime
  if (this->saved_keep_alive_ > PARENT_LINK_POLL_INTERVAL_MS &&
      ezb_nwk_get_keepalive_interval() == PARENT_LINK_POLL_INTERVAL_MS) {
    ezb_nwk_set_keepalive_interval(this->saved_keep_alive_);
  }
}

void load_zb_event(ZBEvent *event, ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute) {
  event->load_set_attr_value_event(info, attribute);
}

template<typename... Args> void enqueue_zb_event(Args... args) {
  // Allocate an event from the pool
  ZBEvent *event = global_zigbee->zb_event_pool_.allocate();
  if (event == nullptr) {
    // No events available - queue is full or we're out of memory
    global_zigbee->zb_events_.increment_dropped_count();
    return;
  }

  // Load new event data (replaces previous event)
  load_zb_event(event, args...);

  // Push the event to the queue
  global_zigbee->zb_events_.push(event);
  // Push always succeeds because we're the only producer and the pool ensures we never exceed queue size
  global_zigbee->enable_loop_soon_any_context();
}

// Explicit template instantiations for the friend function
template void enqueue_zb_event(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute);

static void zb_attribute_handler(ezb_zcl_set_attr_value_message_t *message) {
  ESP_RETURN_ON_FALSE(message, , TAG, "Empty message");
  ESP_RETURN_ON_FALSE(message->info.status == EZB_ZCL_STATUS_SUCCESS, , TAG, "Received message: error status(%d)",
                      message->info.status);
  ESP_LOGD(TAG, "ZCL SetAttributeValue message for endpoint(%d) cluster(0x%04x) %s with status(0x%02x)",
           message->info.dst_ep, message->info.cluster_id,
           message->info.cluster_role == EZB_ZCL_CLUSTER_SERVER ? "server" : "client", message->info.status);
  enqueue_zb_event(message->info, message->in.attribute);
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

void ZigbeeComponent::handle_attribute_(ezb_zcl_message_info_t info, ezb_zcl_attribute_t attribute) {
  auto it = this->attributes_.find({info.dst_ep, info.cluster_id, info.cluster_role, attribute.id});
  if (it != this->attributes_.end()) {
    it->second->on_value(attribute);
  }
}

void ZigbeeComponent::create_default_cluster(uint8_t endpoint_id, uint16_t device_id) {
  ezb_af_ep_desc_t ep_desc =
      esphome_zb_zha_default_ep_desc_create(endpoint_id, device_id, this->basic_cluster_data_.power_source);
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

void ZigbeeComponent::set_basic_cluster(const char *model, const char *manufacturer, const char *location,
                                        const char *sw_build_id, uint8_t power_source, uint8_t app_version) {
  char date_buf[16];
  time_t time_val = App.get_build_time();
  struct tm *timeinfo = localtime(&time_val);
  strftime(date_buf, sizeof(date_buf), "%Y%m%d %H%M%S", timeinfo);
  this->basic_cluster_data_ = {
      .model = get_zcl_string(model, 31),
      .manufacturer = get_zcl_string(manufacturer, 31),
      .date = get_zcl_string(date_buf, 15),
      .location = get_zcl_string(location, 15),
      .sw_build_id = get_zcl_string(sw_build_id, 15),
      .power_source = power_source,
      .app_version = app_version,
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
    ezb_af_endpoint_add_cluster_desc(ep_desc, cluster_desc);
  }
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                      this->basic_cluster_data_.manufacturer);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                      this->basic_cluster_data_.model);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_DATE_CODE_ID, this->basic_cluster_data_.date);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_APPLICATION_VERSION_ID,
                                      &(this->basic_cluster_data_.app_version));
  uint8_t stack_version = 0;
  uint8_t hw_version = 0;
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_STACK_VERSION_ID, &stack_version);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_HW_VERSION_ID, &hw_version);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_LOCATION_DESCRIPTION_ID,
                                      this->basic_cluster_data_.location);
  ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, EZB_ZCL_ATTR_BASIC_SW_BUILD_ID_ID,
                                      this->basic_cluster_data_.sw_build_id);
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

void ZigbeeComponent::setup() {
  global_zigbee = this;

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
      .keep_alive = this->keep_alive_,
  };
  device_config.zed_config = zb_zed_cfg;
#endif
  esp_zigbee_config_t config = {.device_config = device_config, .platform_config = platform_config};
  if (esp_zigbee_init(&config) != ESP_OK) {
    ESP_LOGE(TAG, "Could not initialize Zigbee");
    this->mark_failed();
    return;
  }

#ifdef USE_WIFI
  if (esp_coex_wifi_i154_enable() != ESP_OK) {
    this->mark_failed();
    return;
  }
#endif

#ifdef CONFIG_ZB_ZCZR
  ezb_bdb_set_router_rejoin_required(true);
#else
  ezb_nwk_set_rx_on_when_idle(!this->sleepy_);  // if sleepy, disable RX when idle
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
  // Process all pending events
  ZBEvent *event = this->zb_events_.pop();
  while (event != nullptr) {
    // Handle the event
    switch (event->callback_id_) {
      case EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID:
        this->handle_attribute_(event->event_.set_attr.info, event->event_.set_attr.attribute);
        break;
      default:
        ESP_LOGW(TAG, "Received event with unhandled callback id: 0x%x", static_cast<unsigned>(event->callback_id_));
        break;
    }

    // Free the event back to the pool
    this->zb_event_pool_.release(event);
    // Get the next event
    event = this->zb_events_.pop();
  }
  // Log dropped events periodically
  uint16_t dropped = this->zb_events_.get_and_reset_dropped_count();
  if (dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u Zigbee events due to buffer overflow", dropped);
  }

  if (!this->start_reported_ && this->started_) {
    this->start_cb_.call();
    this->start_reported_ = true;
  }
  if (this->join_pending_.exchange(false)) {
    this->join_cb_.call(this->factory_new_);
    this->factory_new_ = false;
  }
  this->disable_loop();
}

void ZigbeeComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Zigbee\n"
                "  Model: %.*s\n"
                "  Manufacturer: %.*s\n"
#ifdef CONFIG_ZB_ZCZR
                "  Router: %s\n"
#else
                "  Sleepy end device: %s\n"
                "  Poll interval: %" PRIu32 " ms\n"
#endif
                "  App Version: %u",
                this->basic_cluster_data_.model[0], reinterpret_cast<const char *>(this->basic_cluster_data_.model + 1),
                this->basic_cluster_data_.manufacturer[0],
                reinterpret_cast<const char *>(this->basic_cluster_data_.manufacturer + 1),
#ifdef CONFIG_ZB_ZCZR
                YESNO(this->device_role_ == EZB_NWK_DEVICE_TYPE_ROUTER),
#else
                YESNO(this->sleepy_), this->keep_alive_,
#endif
                this->basic_cluster_data_.app_version);
  if (esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    ESP_LOGCONFIG(TAG,
                  "  Device is joined to the network: %s\n"
                  "  Current channel: %d\n"
                  "  Short addr: 0x%04X\n"
                  "  Short pan id: 0x%04X",
                  YESNO(ezb_bdb_dev_joined()), ezb_nwk_get_current_channel(), ezb_nwk_get_short_address(),
                  ezb_nwk_get_panid());
    esp_zigbee_lock_release();
  }
}
}  // namespace esphome::zigbee

#endif
#endif
