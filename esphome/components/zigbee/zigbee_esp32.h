#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include <map>
#include <tuple>
#include <atomic>

#include "esp_zigbee_core.h"
#include "zboss_api.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "zigbee_helpers_esp32.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::zigbee {

/* Zigbee configuration */
static const uint16_t ED_KEEP_ALIVE = 3000; /* 3000 millisecond */
static const uint8_t MAX_CHILDREN = 10;

#define ESP_ZB_DEFAULT_RADIO_CONFIG() \
  { .radio_mode = ZB_RADIO_MODE_NATIVE, }

#define ESP_ZB_DEFAULT_HOST_CONFIG() \
  { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, }

uint8_t *get_zcl_string(const char *str, uint8_t max_size, bool use_max_size = false);

class ZigbeeAttribute;

class ZigbeeComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  esp_err_t create_endpoint(uint8_t endpoint_id, zb_ha_standard_devs_e device_id,
                            esp_zb_cluster_list_t *esp_zb_cluster_list);
  void set_basic_cluster(const char *model, const char *manufacturer);
  void add_cluster(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role);
  void create_default_cluster(uint8_t endpoint_id, zb_ha_standard_devs_e device_id);

  template<typename T>
  void add_attr(ZigbeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                uint8_t max_size, T value);

  template<typename T>
  void add_attr(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id, uint8_t max_size, T value);

  void factory_reset() {
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_factory_reset();  // triggers a reboot
    esp_zb_lock_release();
  }

  template<typename F> void add_on_join_callback(F &&cb) { this->join_cb_.add(std::forward<F>(cb)); }

  bool is_started() { return this->started; }
  bool is_connected() { return this->connected_; }
  std::atomic<bool> started = false;
  std::atomic<bool> joined = false;
  std::atomic<bool> factory_new = false;

 protected:
  struct {
    uint8_t *model;
    uint8_t *manufacturer;
    uint8_t *date;
  } basic_cluster_data_;
  bool connected_ = false;
#ifdef ZB_ED_ROLE
  esp_zb_nwk_device_type_t device_role_ = ESP_ZB_DEVICE_TYPE_ED;
#else
  esp_zb_nwk_device_type_t device_role_ = ESP_ZB_DEVICE_TYPE_ROUTER;
#endif
  esp_zb_attribute_list_t *create_basic_cluster_();
  template<typename T>
  void add_attr_(ZigbeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                 T *value_p);
  // endpoint_list_ and attribute_list_ are only used during setup and are cleared afterwards
  // value tuple could be replaced by struct
  std::map<uint8_t, std::tuple<zb_ha_standard_devs_e, esp_zb_cluster_list_t *>> endpoint_list_;
  // key tuple could be replaced by single 32 bit int with bit fields for endpoint, cluster and role
  std::map<std::tuple<uint8_t, uint16_t, uint8_t>, esp_zb_attribute_list_t *> attribute_list_;
  // attributes_ will be used during operation in zigbee callbacks to update the attribute values and trigger
  // automations
  // key tuple could be replaced by single 64 (48) bit int with bit fields for endpoint, cluster, role and attr_id
  std::map<std::tuple<uint8_t, uint16_t, uint8_t, uint16_t>, ZigbeeAttribute *> attributes_;
  esp_zb_ep_list_t *esp_zb_ep_list_ = esp_zb_ep_list_create();
  CallbackManager<void(bool)> join_cb_{};
};

extern "C" void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);

template<typename T>
void ZigbeeComponent::add_attr(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                               uint8_t max_size, T value) {
  this->add_attr<T>(nullptr, endpoint_id, cluster_id, role, attr_id, max_size, value);
}

template<typename T>
void ZigbeeComponent::add_attr(ZigbeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role,
                               uint16_t attr_id, uint8_t max_size, T value) {
  // The size byte of the zcl_str must be set to the maximum value,
  // even though the initial string may be shorter.
  if constexpr (std::is_same<T, std::string>::value) {
    auto zcl_str = get_zcl_string(value.c_str(), max_size, true);
    add_attr_(attr, endpoint_id, cluster_id, role, attr_id, zcl_str);
    delete[] zcl_str;
  } else if constexpr (std::is_convertible<T, const char *>::value) {
    auto zcl_str = get_zcl_string(value, max_size, true);
    add_attr_(attr, endpoint_id, cluster_id, role, attr_id, zcl_str);
    delete[] zcl_str;
  } else {
    add_attr_(attr, endpoint_id, cluster_id, role, attr_id, &value);
  }
}

template<typename T>
void ZigbeeComponent::add_attr_(ZigbeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role,
                                uint16_t attr_id, T *value_p) {
  esp_zb_attribute_list_t *attr_list = this->attribute_list_[{endpoint_id, cluster_id, role}];
  esp_err_t ret = esphome_zb_cluster_add_or_update_attr(cluster_id, attr_list, attr_id, value_p);

  if (attr != nullptr) {
    this->attributes_[{endpoint_id, cluster_id, role, attr_id}] = attr;
  }
}

}  // namespace esphome::zigbee

#endif
#endif
