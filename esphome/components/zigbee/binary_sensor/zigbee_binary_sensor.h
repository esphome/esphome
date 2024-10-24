#pragma once

#include "esphome/components/zigbee/zigbee_component.h"
#ifdef USE_ZIGBEE
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

#define ESPHOME_ZB_HA_DECLARE_BINARY_INPUT_CLUSTER_LIST(cluster_list_name, binary_attr_list, basic_attr_list, \
                                                        identify_attr_list, groups_attr_list, scenes_attr_list) \
  zb_zcl_cluster_desc_t cluster_list_name[] = { \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t), \
                          (identify_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t), \
                          (basic_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BINARY_INPUT, ZB_ZCL_ARRAY_SIZE(binary_attr_list, zb_zcl_attr_t), \
                          (binary_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_GROUPS, ZB_ZCL_ARRAY_SIZE(groups_attr_list, zb_zcl_attr_t), \
                          (groups_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_SCENES, ZB_ZCL_ARRAY_SIZE(scenes_attr_list, zb_zcl_attr_t), \
                          (scenes_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID)}

#define ESPHOME_ZB_HA_DEVICE_VER_SIMPLE_SENSOR 0

#define ESPHOME_ZB_ZCL_DECLARE_BINARY_INPUT_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num) \
  ESPHOME_ZB_DECLARE_SIMPLE_DESC(ep_name, in_clust_num, out_clust_num); \
  ESPHOME_ZB_AF_SIMPLE_DESC_TYPE(ep_name, in_clust_num, out_clust_num) \
  simple_desc_##ep_name = {ep_id, \
                           ZB_AF_HA_PROFILE_ID, \
                           ZB_HA_CUSTOM_ATTR_DEVICE_ID, \
                           ESPHOME_ZB_HA_DEVICE_VER_SIMPLE_SENSOR, \
                           0, \
                           in_clust_num, \
                           out_clust_num, \
                           {ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_CLUSTER_ID_BINARY_INPUT, \
                            ZB_ZCL_CLUSTER_ID_SCENES, ZB_ZCL_CLUSTER_ID_GROUPS}}

#define ESPHOME_ZB_HA_BINARY_INPUT_REPORT_ATTR_COUNT ZB_ZCL_BINARY_INPUT_REPORT_ATTR_COUNT
#define ESPHOME_ZB_HA_BINARY_INPUT_IN_CLUSTER_NUM 5   // server roles in ESPHOME_ZB_HA_DECLARE_BINARY_INPUT_CLUSTER_LIST
#define ESPHOME_ZB_HA_BINARY_INPUT_OUT_CLUSTER_NUM 0  // client roles in ESPHOME_ZB_HA_DECLARE_BINARY_INPUT_CLUSTER_LIST

#define ESPHOME_ZB_HA_DECLARE_BINARY_INPUT_EP(ep_name, ep_id, cluster_list) \
  ESPHOME_ZB_ZCL_DECLARE_BINARY_INPUT_SIMPLE_DESC(ep_name, ep_id, ESPHOME_ZB_HA_BINARY_INPUT_IN_CLUSTER_NUM, \
                                                  ESPHOME_ZB_HA_BINARY_INPUT_OUT_CLUSTER_NUM); \
  ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info##ep_name, ESPHOME_ZB_HA_BINARY_INPUT_REPORT_ATTR_COUNT); \
  ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID, 0, NULL, \
                              ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t), cluster_list, \
                              (zb_af_simple_desc_1_1_t *) &simple_desc_##ep_name, \
                              ESPHOME_ZB_HA_BINARY_INPUT_REPORT_ATTR_COUNT, reporting_info##ep_name, 0, NULL)

// it cannot have ESPHOME prefix since it is used outside
#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID, ZB_ZCL_ATTR_TYPE_CHAR_STRING, ZB_ZCL_ATTR_ACCESS_READ_ONLY, \
        (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), (void *) (data_ptr) \
  }

#define ESPHOME_ZB_ZCL_DECLARE_BINARY_INPUT_ATTRIB_LIST(attr_list, out_of_service, present_value, status_flag, \
                                                        description) \
  ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(attr_list, ZB_ZCL_BINARY_INPUT) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_INPUT_OUT_OF_SERVICE_ID, (out_of_service)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, (present_value)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_INPUT_STATUS_FLAG_ID, (status_flag)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID, (description)) \
  ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

namespace esphome {
namespace zigbee {

class ZigbeeBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  void set_template(std::function<optional<bool>()> &&f) { this->f_ = f; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_parent(Zigbee *parent);
  void set_ep(zb_uint8_t ep) { this->ep_ = ep; }
  void set_cluster_attributes(BinaryAttrs &cluster_attributes) { this->cluster_attributes_ = &cluster_attributes; }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  std::function<optional<bool>()> f_{nullptr};
  zb_uint8_t ep_{0};
  Zigbee *parent_{nullptr};
  BinaryAttrs *cluster_attributes_{nullptr};
};

}  // namespace zigbee
}  // namespace esphome
#endif
