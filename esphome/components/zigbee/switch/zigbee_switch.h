#pragma once

#include "esphome/components/zigbee/zigbee_component.h"
#ifdef USE_ZIGBEE
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/output/binary_output.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

#define ESPHOME_ZB_HA_DEVICE_VER_SIMPLE_OUTPUT 0  // TODO what to set here?
#define ZB_ZCL_BINARY_OUTPUT_CLUSTER_REVISION_DEFAULT ((zb_uint16_t) 0x0001u)

// NOLINTNEXTLINE(readability-identifier-naming)
enum zb_zcl_binary_output_attr_e {
  ZB_ZCL_ATTR_BINARY_OUTPUT_DESCRIPTION_ID = 0x001C,
  ZB_ZCL_ATTR_BINARY_OUTPUT_OUT_OF_SERVICE_ID = 0x0051,
  ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID = 0x0055,
  ZB_ZCL_ATTR_BINARY_OUTPUT_STATUS_FLAG_ID = 0x006F,
};

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_BINARY_OUTPUT_OUT_OF_SERVICE_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_BINARY_OUTPUT_OUT_OF_SERVICE_ID, ZB_ZCL_ATTR_TYPE_BOOL, \
        ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_WRITE_OPTIONAL, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID, ZB_ZCL_ATTR_TYPE_BOOL, \
        ZB_ZCL_ATTR_ACCESS_READ_WRITE | ZB_ZCL_ATTR_ACCESS_REPORTING, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_BINARY_OUTPUT_STATUS_FLAG_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_BINARY_OUTPUT_STATUS_FLAG_ID, ZB_ZCL_ATTR_TYPE_8BITMAP, \
        ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_REPORTING, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_BINARY_OUTPUT_DESCRIPTION_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_BINARY_OUTPUT_DESCRIPTION_ID, ZB_ZCL_ATTR_TYPE_CHAR_STRING, ZB_ZCL_ATTR_ACCESS_READ_ONLY, \
        (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), (void *) (data_ptr) \
  }

#define ESPHOME_ZB_ZCL_DECLARE_BINARY_OUTPUT_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num) \
  ESPHOME_ZB_DECLARE_SIMPLE_DESC(ep_name, in_clust_num, out_clust_num); \
  ESPHOME_ZB_AF_SIMPLE_DESC_TYPE(ep_name, in_clust_num, out_clust_num) \
  simple_desc_##ep_name = {ep_id, \
                           ZB_AF_HA_PROFILE_ID, \
                           ZB_HA_CUSTOM_ATTR_DEVICE_ID, \
                           ESPHOME_ZB_HA_DEVICE_VER_SIMPLE_OUTPUT, \
                           0, \
                           in_clust_num, \
                           out_clust_num, \
                           {ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, \
                            ZB_ZCL_CLUSTER_ID_SCENES, ZB_ZCL_CLUSTER_ID_GROUPS}}

#define ESPHOME_ZB_ZCL_DECLARE_BINARY_OUTPUT_ATTRIB_LIST(attr_list, out_of_service, present_value, status_flag, \
                                                         description) \
  ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(attr_list, ZB_ZCL_BINARY_OUTPUT) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_OUTPUT_OUT_OF_SERVICE_ID, (out_of_service)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID, (present_value)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_OUTPUT_STATUS_FLAG_ID, (status_flag)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_OUTPUT_DESCRIPTION_ID, (description)) \
  ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

#define ESPHOME_ZB_HA_BINARY_OUTPUT_REPORT_ATTR_COUNT 2
#define ESPHOME_ZB_HA_BINARY_OUTPUT_IN_CLUSTER_NUM \
  5  // server roles in ESPHOME_ZB_HA_DECLARE_BINARY_OUTPUT_CLUSTER_LIST
#define ESPHOME_ZB_HA_BINARY_OUTPUT_OUT_CLUSTER_NUM \
  0  // client roles in ESPHOME_ZB_HA_DECLARE_BINARY_OUTPUT_CLUSTER_LIST

#define ESPHOME_ZB_HA_DECLARE_BINARY_OUTPUT_EP(ep_name, ep_id, cluster_list) \
  ESPHOME_ZB_ZCL_DECLARE_BINARY_OUTPUT_SIMPLE_DESC(ep_name, ep_id, ESPHOME_ZB_HA_BINARY_OUTPUT_IN_CLUSTER_NUM, \
                                                   ESPHOME_ZB_HA_BINARY_OUTPUT_OUT_CLUSTER_NUM); \
  ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info##ep_name, ESPHOME_ZB_HA_BINARY_OUTPUT_REPORT_ATTR_COUNT); \
  ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID, 0, NULL, \
                              ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t), cluster_list, \
                              (zb_af_simple_desc_1_1_t *) &simple_desc_##ep_name, \
                              ESPHOME_ZB_HA_BINARY_OUTPUT_REPORT_ATTR_COUNT, reporting_info##ep_name, 0, NULL)

#define ESPHOME_ZB_HA_DECLARE_BINARY_OUTPUT_CLUSTER_LIST(cluster_list_name, binary_attr_list, basic_attr_list, \
                                                         identify_attr_list, groups_attr_list, scenes_attr_list) \
  zb_zcl_cluster_desc_t cluster_list_name[] = { \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t), \
                          (identify_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t), \
                          (basic_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, ZB_ZCL_ARRAY_SIZE(binary_attr_list, zb_zcl_attr_t), \
                          (binary_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_GROUPS, ZB_ZCL_ARRAY_SIZE(groups_attr_list, zb_zcl_attr_t), \
                          (groups_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
      ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_SCENES, ZB_ZCL_ARRAY_SIZE(scenes_attr_list, zb_zcl_attr_t), \
                          (scenes_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID)}

void zb_zcl_binary_output_init_server();
void zb_zcl_binary_output_init_client();

#define ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT_SERVER_ROLE_INIT zb_zcl_binary_output_init_server
#define ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT_CLIENT_ROLE_INIT zb_zcl_binary_output_init_client

namespace esphome {
namespace zigbee {

class ZigbeeSwitch : public switch_::Switch, public Component {
 public:
  void set_output(output::BinaryOutput *output) { this->output_ = output; }
  void set_cluster_attributes(BinaryAttrs &cluster_attributes) { this->cluster_attributes_ = &cluster_attributes; }
  void set_ep(zb_uint8_t ep) { this->ep_ = ep; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE - 1.0f; }
  void dump_config() override;

  void set_parent(Zigbee *parent);

 protected:
  void write_state(bool state) override;
  void zcl_device_cb_(zb_bufid_t bufid);

  output::BinaryOutput *output_;
  BinaryAttrs *cluster_attributes_ = nullptr;
  zb_uint8_t ep_{0};
  Zigbee *parent_{nullptr};
};

}  // namespace zigbee
}  // namespace esphome
#endif
