#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52)
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

// copy of ZB_DECLARE_SIMPLE_DESC. Due to https://github.com/nrfconnect/sdk-nrfxlib/pull/666
#define ESPHOME_ZB_DECLARE_SIMPLE_DESC(ep_name, in_clusters_count, out_clusters_count) \
  typedef ZB_PACKED_PRE struct zb_af_simple_desc_##ep_name##_##in_clusters_count##_##out_clusters_count##_s { \
    zb_uint8_t endpoint;                  /* Endpoint */ \
    zb_uint16_t app_profile_id;           /* Application profile identifier */ \
    zb_uint16_t app_device_id;            /* Application device identifier */ \
    zb_bitfield_t app_device_version : 4; /* Application device version */ \
    zb_bitfield_t reserved : 4;           /* Reserved */ \
    zb_uint8_t app_input_cluster_count;   /* Application input cluster count */ \
    zb_uint8_t app_output_cluster_count;  /* Application output cluster count */ \
    /* Application input and output cluster list */ \
    zb_uint16_t app_cluster_list[(in_clusters_count) + (out_clusters_count)]; \
  } ZB_PACKED_STRUCT zb_af_simple_desc_##ep_name##_##in_clusters_count##_##out_clusters_count##_t

#define ESPHOME_CAT7(a, b, c, d, e, f, g) a##b##c##d##e##f##g
// needed to use ESPHOME_ZB_DECLARE_SIMPLE_DESC
#define ESPHOME_ZB_AF_SIMPLE_DESC_TYPE(ep_name, in_num, out_num) \
  ESPHOME_CAT7(zb_af_simple_desc_, ep_name, _, in_num, _, out_num, _t)

// needed to use ESPHOME_ZB_DECLARE_SIMPLE_DESC
#define ESPHOME_ZB_ZCL_DECLARE_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num, app_device_id, ...) \
  ESPHOME_ZB_DECLARE_SIMPLE_DESC(ep_name, in_clust_num, out_clust_num); \
  ESPHOME_ZB_AF_SIMPLE_DESC_TYPE(ep_name, in_clust_num, out_clust_num) \
  simple_desc_##ep_name = {ep_id, ZB_AF_HA_PROFILE_ID, app_device_id, 0, 0, in_clust_num, out_clust_num, {__VA_ARGS__}}

// needed to use ESPHOME_ZB_ZCL_DECLARE_SIMPLE_DESC
#define ESPHOME_ZB_HA_DECLARE_EP(ep_name, ep_id, cluster_list, in_cluster_num, out_cluster_num, report_attr_count, \
                                 app_device_id, ...) \
  ESPHOME_ZB_ZCL_DECLARE_SIMPLE_DESC(ep_name, ep_id, in_cluster_num, out_cluster_num, app_device_id, __VA_ARGS__); \
  ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info##ep_name, report_attr_count); \
  ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID, 0, NULL, \
                              ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t), cluster_list, \
                              (zb_af_simple_desc_1_1_t *) &simple_desc_##ep_name, report_attr_count, \
                              reporting_info##ep_name, 0, NULL)

namespace esphome::zigbee {

struct BinaryAttrs {
  zb_bool_t out_of_service;
  zb_bool_t present_value;
  zb_uint8_t status_flags;
  zb_uchar_t description[ZB_ZCL_MAX_STRING_SIZE];
};

struct AnalogAttrs {
  zb_bool_t out_of_service;
  float present_value;
  zb_uint8_t status_flags;
  zb_uint16_t engineering_units;
  zb_uchar_t description[ZB_ZCL_MAX_STRING_SIZE];
};

class ZigbeeComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void add_callback(zb_uint8_t endpoint, std::function<void(zb_bufid_t bufid)> &&cb) {
    // endpoints are enumerated from 1
    this->callbacks_[endpoint - 1] = std::move(cb);
  }
  void add_join_callback(std::function<void()> &&cb) { this->join_cb_.add(std::move(cb)); }
  void zboss_signal_handler_esphome(zb_bufid_t bufid);
  void factory_reset();
  Trigger<> *get_join_trigger() { return &this->join_trigger_; };
  void flush();
  void loop() override;

 protected:
  static void zcl_device_cb(zb_bufid_t bufid);
  void on_join_();
#ifdef USE_ZIGBEE_WIPE_ON_BOOT
  void erase_flash_(int area);
#endif
  std::array<std::function<void(zb_bufid_t bufid)>, ZIGBEE_ENDPOINTS_COUNT> callbacks_{};
  CallbackManager<void()> join_cb_;
  Trigger<> join_trigger_;
  bool need_flush_{false};
};

class ZigbeeEntity {
 public:
  void set_parent(ZigbeeComponent *parent) { this->parent_ = parent; }
  void set_endpoint(zb_uint8_t endpoint) { this->endpoint_ = endpoint; }

 protected:
  zb_uint8_t endpoint_{0};
  ZigbeeComponent *parent_{nullptr};
};

}  // namespace esphome::zigbee
#endif
