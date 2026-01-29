#pragma once

#include "esphome/core/defines.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_NUMBER)
#include "esphome/components/zigbee/zigbee_zephyr.h"
#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

enum {
  ZB_ZCL_ATTR_ANALOG_OUTPUT_DESCRIPTION_ID = 0x001C,
  ZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID = 0x0041,
  ZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID = 0x0045,
  ZB_ZCL_ATTR_ANALOG_OUTPUT_OUT_OF_SERVICE_ID = 0x0051,
  ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID = 0x0055,
  ZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID = 0x006A,
  ZB_ZCL_ATTR_ANALOG_OUTPUT_STATUS_FLAG_ID = 0x006F,
  ZB_ZCL_ATTR_ANALOG_OUTPUT_ENGINEERING_UNITS_ID = 0x0075,
};

#define ZB_ZCL_ANALOG_OUTPUT_CLUSTER_REVISION_DEFAULT ((zb_uint16_t) 0x0001u)

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_OUTPUT_DESCRIPTION_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_OUTPUT_DESCRIPTION_ID, ZB_ZCL_ATTR_TYPE_CHAR_STRING, ZB_ZCL_ATTR_ACCESS_READ_ONLY, \
        (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), (void *) (data_ptr) \
  }

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_OUTPUT_OUT_OF_SERVICE_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_OUTPUT_OUT_OF_SERVICE_ID, ZB_ZCL_ATTR_TYPE_BOOL, \
        ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_WRITE_OPTIONAL, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }
// PresentValue
#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, ZB_ZCL_ATTR_TYPE_SINGLE, \
        ZB_ZCL_ATTR_ACCESS_READ_WRITE | ZB_ZCL_ATTR_ACCESS_REPORTING, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }
// MaxPresentValue
#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID, ZB_ZCL_ATTR_TYPE_SINGLE, \
        ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_WRITE_OPTIONAL, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }
// MinPresentValue
#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID, ZB_ZCL_ATTR_TYPE_SINGLE, \
        ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_WRITE_OPTIONAL, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }
// Resolution
#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID, ZB_ZCL_ATTR_TYPE_SINGLE, \
        ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_WRITE_OPTIONAL, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_OUTPUT_STATUS_FLAG_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_OUTPUT_STATUS_FLAG_ID, ZB_ZCL_ATTR_TYPE_8BITMAP, \
        ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_REPORTING, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_OUTPUT_ENGINEERING_UNITS_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_OUTPUT_ENGINEERING_UNITS_ID, ZB_ZCL_ATTR_TYPE_16BIT_ENUM, ZB_ZCL_ATTR_ACCESS_READ_ONLY, \
        (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), (void *) (data_ptr) \
  }

#define ESPHOME_ZB_ZCL_DECLARE_ANALOG_OUTPUT_ATTRIB_LIST(attr_list, out_of_service, present_value, status_flag, \
                                                         max_present_value, min_present_value, resolution, \
                                                         engineering_units, description) \
  ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(attr_list, ZB_ZCL_ANALOG_OUTPUT) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_OUTPUT_OUT_OF_SERVICE_ID, (out_of_service)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, (present_value)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_OUTPUT_STATUS_FLAG_ID, (status_flag)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID, (max_present_value)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID, (min_present_value)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID, (resolution)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_OUTPUT_ENGINEERING_UNITS_ID, (engineering_units)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_OUTPUT_DESCRIPTION_ID, (description)) \
  ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

void zb_zcl_analog_output_init_server();
void zb_zcl_analog_output_init_client();
#define ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT_SERVER_ROLE_INIT zb_zcl_analog_output_init_server
#define ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT_CLIENT_ROLE_INIT zb_zcl_analog_output_init_client

namespace esphome::zigbee {

class ZigbeeNumber : public ZigbeeEntity, public Component {
 public:
  ZigbeeNumber(number::Number *n) : number_(n) {}
  void set_cluster_attributes(AnalogAttrsOutput &cluster_attributes) {
    this->cluster_attributes_ = &cluster_attributes;
  }

  void setup() override;
  void dump_config() override;

 protected:
  number::Number *number_;
  AnalogAttrsOutput *cluster_attributes_{nullptr};
  void zcl_device_cb_(zb_bufid_t bufid);
};

}  // namespace esphome::zigbee
#endif
