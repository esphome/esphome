#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include <map>
#include <tuple>
#include <deque>

#include "esp_zigbee_core.h"
#include "zboss_api.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "zigbee_helpers.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::zigbee {

static const char *const TAG = "zigbee";

using device_params_t = struct DeviceParamsS {
  esp_zb_ieee_addr_t ieee_addr;
  uint8_t endpoint;
  uint16_t short_addr;
};

using zdo_info_user_ctx_t = struct ZdoInfoCtxS {
  uint8_t endpoint;
  uint16_t short_addr;
};

using zb_device_params_t = struct ZbDeviceParamsS {
  esp_zb_ieee_addr_t ieee_addr;
  uint8_t endpoint;
  uint16_t short_addr;
};

enum ZigbeeReportT {
  ZIGBEE_REPORT_NO,
  ZIGBEE_REPORT_YES,
  ZIGBEE_REPORT_FORCE,
};

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE false /* enable the install code policy for security */
#define ED_AGING_TIMEOUT ESP_ZB_ED_AGING_TIMEOUT_64MIN
static const uint16_t ED_KEEP_ALIVE = 3000; /* 3000 millisecond */
static const uint8_t MAX_CHILDREN = 10;
#define ESP_ZB_PRIMARY_CHANNEL_MASK \
  ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */

#define ESP_ZB_DEFAULT_RADIO_CONFIG() \
  { .radio_mode = ZB_RADIO_MODE_NATIVE, }

#define ESP_ZB_DEFAULT_HOST_CONFIG() \
  { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, }

uint8_t *get_zcl_string(const char *str, uint8_t max_size, bool use_max_size = false);

class ZigBeeAttribute;
class ZigbeeTime;

class ZigBeeComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  esp_err_t create_endpoint(uint8_t endpoint_id, esp_zb_ha_standard_devices_t device_id);
  void set_basic_cluster(std::string model, std::string manufacturer, std::string date);
  void add_cluster(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role);
  void create_default_cluster(uint8_t endpoint_id, esp_zb_ha_standard_devices_t device_id);

  template<typename T>
  void add_attr(ZigBeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                uint8_t attr_type, uint8_t attr_access, uint8_t max_size, T value);

  void set_report(ZigBeeAttribute *attribute, esp_zb_zcl_reporting_info_t reporting_info);
  void handle_attribute(esp_zb_device_cb_common_info_t info, esp_zb_zcl_attribute_t attribute);
  void search_bindings();
  static void binding_table_cb(const esp_zb_zdo_binding_table_info_t *table_info, void *user_ctx);

  void reset() {
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_factory_reset();
    esp_zb_lock_release();
  }
  void report();

  bool is_started() { return this->started_; }
  bool is_connected() { return this->connected_; }
  bool connected_ = false;
  bool started_ = false;

  std::deque<std::tuple<ZigBeeAttribute *, esp_zb_zcl_reporting_info_t>> reporting_list;
  struct {
    std::string model;
    std::string manufacturer;
    std::string date;
  } basic_cluster_data_;
#ifdef ZB_ED_ROLE
  esp_zb_nwk_device_type_t device_role_ = ESP_ZB_DEVICE_TYPE_ED;
#else
  esp_zb_nwk_device_type_t device_role_ = ESP_ZB_DEVICE_TYPE_ROUTER;
#endif

 protected:
  esp_zb_attribute_list_t *create_basic_cluster_();
  template<typename T>
  void add_attr_(ZigBeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                 uint8_t attr_type, uint8_t attr_access, T *value_p);
  std::map<uint8_t, esp_zb_ha_standard_devices_t> endpoint_list_;
  std::map<uint8_t, esp_zb_cluster_list_t *> cluster_list_;
  std::map<std::tuple<uint8_t, uint16_t, uint8_t>, esp_zb_attribute_list_t *> attribute_list_;
  std::map<std::tuple<uint8_t, uint16_t, uint8_t, uint16_t>, ZigBeeAttribute *> attributes_;
  esp_zb_ep_list_t *esp_zb_ep_list_ = esp_zb_ep_list_create();
};

extern "C" void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);
extern "C" void zb_set_ed_node_descriptor(bool power_src, bool rx_on_when_idle, bool alloc_addr);

template<typename T>
void ZigBeeComponent::add_attr(ZigBeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role,
                               uint16_t attr_id, uint8_t attr_type, uint8_t attr_access, uint8_t max_size, T value) {
  // The size byte of the zcl_str must be set to the maximum value,
  // even though the initial string may be shorter.
  if constexpr (std::is_same<T, std::string>::value) {
    auto zcl_str = get_zcl_string(value.c_str(), max_size, true);
    add_attr_(attr, endpoint_id, cluster_id, role, attr_id, attr_type, attr_access, zcl_str);
    delete[] zcl_str;
  } else if constexpr (std::is_convertible<T, const char *>::value) {
    auto zcl_str = get_zcl_string(value, max_size, true);
    add_attr_(attr, endpoint_id, cluster_id, role, attr_id, attr_type, attr_access, zcl_str);
    delete[] zcl_str;
  } else {
    add_attr_(attr, endpoint_id, cluster_id, role, attr_id, attr_type, attr_access, &value);
  }
}

template<typename T>
void ZigBeeComponent::add_attr_(ZigBeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role,
                                uint16_t attr_id, uint8_t attr_type, uint8_t attr_access, T *value_p) {
  esp_zb_attribute_list_t *attr_list = this->attribute_list_[{endpoint_id, cluster_id, role}];
  esp_err_t ret =
      esphome_zb_cluster_add_or_update_attr(cluster_id, attr_list, attr_id, attr_type, attr_access, value_p);
  this->attributes_[{endpoint_id, cluster_id, role, attr_id}] = attr;
}

}  // namespace esphome::zigbee

#endif
#endif
