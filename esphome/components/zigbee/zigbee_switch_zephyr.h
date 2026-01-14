#pragma once

#include "esphome/components/zigbee/zigbee_zephyr.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_SWITCH)
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

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

#define ESPHOME_ZB_ZCL_DECLARE_BINARY_OUTPUT_ATTRIB_LIST(attr_list, out_of_service, present_value, status_flag, \
                                                         description) \
  ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(attr_list, ZB_ZCL_BINARY_OUTPUT) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_OUTPUT_OUT_OF_SERVICE_ID, (out_of_service)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID, (present_value)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_OUTPUT_STATUS_FLAG_ID, (status_flag)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_OUTPUT_DESCRIPTION_ID, (description)) \
  ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

void zb_zcl_binary_output_init_server();
void zb_zcl_binary_output_init_client();

#define ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT_SERVER_ROLE_INIT zb_zcl_binary_output_init_server
#define ZB_ZCL_CLUSTER_ID_BINARY_OUTPUT_CLIENT_ROLE_INIT zb_zcl_binary_output_init_client

namespace esphome::zigbee {

class ZigbeeSwitch : public ZigbeeEntity, public Component {
 public:
  ZigbeeSwitch(switch_::Switch *s) : switch_(s) {}
  void set_cluster_attributes(BinaryAttrs &cluster_attributes) { this->cluster_attributes_ = &cluster_attributes; }

  void setup() override;

  void dump_config() override;

 protected:
  void zcl_device_cb_(zb_bufid_t bufid);

  BinaryAttrs *cluster_attributes_{nullptr};
  switch_::Switch *switch_;
};

}  // namespace esphome::zigbee
#endif
