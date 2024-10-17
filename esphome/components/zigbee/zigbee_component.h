#pragma once
#include <map>
#include "esphome/core/defines.h"
#ifdef USE_ZIGBEE
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

#define ESPHOME_ZB_DECLARE_SIMPLE_DESC(ep_name, in_clusters_count, out_clusters_count) \
  typedef ZB_PACKED_PRE struct zb_af_simple_desc_##ep_name##_##out_clusters_count##_##out_clusters_count##_s { \
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
#define ESPHOME_ZB_AF_SIMPLE_DESC_TYPE(ep_name, in_num, out_num) \
  ESPHOME_CAT7(zb_af_simple_desc_, ep_name, _, in_num, _, out_num, _t)

namespace esphome {
namespace zigbee {

struct BinaryAttrs {
  zb_bool_t out_of_service;
  zb_bool_t present_value;
  zb_uint8_t status_flags;
  zb_uchar_t description[32];  // TODO it could be in progmem, max is ZB_ZCL_MAX_STRING_SIZE
};

class Zigbee : public Component {
 public:
  void setup() override;
  void add_callback(zb_uint8_t endpoint, std::function<void(zb_bufid_t bufid)> cb) { callbacks_[endpoint] = cb; }
  void zboss_signal_handler_esphome(zb_bufid_t bufid);
  void factory_reset();
  Trigger<> *get_join_trigger() const { return this->join_trigger_; };
  void flush();
  void loop() override;

 protected:
  static void zcl_device_cb(zb_bufid_t bufid);
  std::map<zb_uint8_t, std::function<void(zb_bufid_t bufid)>> callbacks_;
  Trigger<> *join_trigger_{new Trigger<>()};
  bool need_flush_{false};
};

extern Zigbee *global_zigbee;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

template<typename... Ts> class FactoryResetAction : public Action<Ts...> {
 public:
  void play(Ts... x) override { global_zigbee->factory_reset(); }
};

}  // namespace zigbee
}  // namespace esphome
#endif
