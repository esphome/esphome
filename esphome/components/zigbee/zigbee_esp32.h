#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include <map>
#include <tuple>
#include <atomic>

#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "zigbee_helpers_esp32.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::zigbee {

/* Zigbee configuration */
static const uint16_t ED_KEEP_ALIVE = 3000; /* 3000 millisecond */
static const uint8_t MAX_CHILDREN = 10;
static const uint32_t EZB_PRIMARY_CHANNEL_MASK = 0x07FFF800U; /* channels 11-26 */

#define EZB_DEFAULT_RADIO_CONFIG() \
  { .radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE, }

uint8_t *get_zcl_string(const char *str, uint8_t max_size, bool use_max_size = false);

class ZigbeeAttribute;

class ZigbeeComponent final : public Component {
 public:
  ZigbeeComponent();
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_basic_cluster(const char *model, const char *manufacturer, uint8_t power_source);
  void add_cluster(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role);
  void create_default_cluster(uint8_t endpoint_id, uint16_t device_id);
  void setup_reporting();

  template<typename T>
  void add_attr(ZigbeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                uint8_t max_size, T value);

  template<typename T>
  void add_attr(uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id, uint8_t max_size, T value);

  static bool app_signal_handler(const ezb_app_signal_t *app_signal);
  static void esp_zigbee_alarm_bdb_commissioning(ezb_bdb_comm_mode_mask_t mode);

  void factory_reset() {
    esp_zigbee_lock_acquire(portMAX_DELAY);
    esp_zigbee_factory_reset();  // triggers a reboot
    esp_zigbee_lock_release();
  }

  template<typename F> void add_on_join_callback(F &&cb) { this->join_cb_.add(std::forward<F>(cb)); }

  bool is_battery_powered() { return this->basic_cluster_data_.power_source == EZB_ZCL_BASIC_POWER_SOURCE_BATTERY; }
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
    uint8_t power_source;
  } basic_cluster_data_;
  bool connected_ = false;
#ifdef CONFIG_ZB_ZED
  ezb_nwk_device_type_t device_role_ = EZB_NWK_DEVICE_TYPE_END_DEVICE;
#else
  ezb_nwk_device_type_t device_role_ = EZB_NWK_DEVICE_TYPE_ROUTER;
#endif
  void update_basic_cluster_(ezb_af_ep_desc_t ep_desc);
  template<typename T>
  void add_attr_(ZigbeeAttribute *attr, uint8_t endpoint_id, uint16_t cluster_id, uint8_t role, uint16_t attr_id,
                 T *value_p);
  // attributes_ will be used during operation in zigbee callbacks to update the attribute values and trigger
  // automations
  // key tuple could be replaced by single 64 (48) bit int with bit fields for endpoint, cluster, role and attr_id
  std::map<std::tuple<uint8_t, uint16_t, uint8_t, uint16_t>, ZigbeeAttribute *> attributes_;
  ezb_af_device_desc_t dev_desc_;
  CallbackManager<void(bool)> join_cb_{};
};

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
  ezb_af_ep_desc_t ep_desc = ezb_af_device_get_endpoint_desc(this->dev_desc_, endpoint_id);
  if (ep_desc == NULL) {
    return;
  }
  ezb_zcl_cluster_desc_t cluster_desc = ezb_af_endpoint_get_cluster_desc(ep_desc, cluster_id, role);
  if (cluster_desc == NULL) {
    return;
  }
  esphome_zb_cluster_add_or_update_attr(cluster_id, cluster_desc, attr_id, value_p);

  if (attr != nullptr) {
    this->attributes_[{endpoint_id, cluster_id, role, attr_id}] = attr;
  }
}

}  // namespace esphome::zigbee

#endif
#endif
