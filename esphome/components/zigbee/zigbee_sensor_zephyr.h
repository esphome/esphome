#pragma once

#include "esphome/components/zigbee/zigbee_zephyr.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_SENSOR)
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

enum {
  ZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID = 0x001C,
  ZB_ZCL_ATTR_ANALOG_INPUT_OUT_OF_SERVICE_ID = 0x0051,
  ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID = 0x0055,
  ZB_ZCL_ATTR_ANALOG_INPUT_STATUS_FLAG_ID = 0x006F,
  ZB_ZCL_ATTR_ANALOG_INPUT_ENGINEERING_UNITS_ID = 0x0075,
};

#define ZB_ZCL_ANALOG_INPUT_CLUSTER_REVISION_DEFAULT ((zb_uint16_t) 0x0001u)

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID, ZB_ZCL_ATTR_TYPE_CHAR_STRING, ZB_ZCL_ATTR_ACCESS_READ_ONLY, \
        (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), (void *) (data_ptr) \
  }

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_INPUT_OUT_OF_SERVICE_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_INPUT_OUT_OF_SERVICE_ID, ZB_ZCL_ATTR_TYPE_BOOL, \
        ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_WRITE_OPTIONAL, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, ZB_ZCL_ATTR_TYPE_SINGLE, \
        ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_WRITE_OPTIONAL | ZB_ZCL_ATTR_ACCESS_REPORTING, \
        (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), (void *) (data_ptr) \
  }

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_INPUT_STATUS_FLAG_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_INPUT_STATUS_FLAG_ID, ZB_ZCL_ATTR_TYPE_8BITMAP, \
        ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_REPORTING, (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), \
        (void *) (data_ptr) \
  }

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_ANALOG_INPUT_ENGINEERING_UNITS_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_ANALOG_INPUT_ENGINEERING_UNITS_ID, ZB_ZCL_ATTR_TYPE_16BIT_ENUM, ZB_ZCL_ATTR_ACCESS_READ_ONLY, \
        (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), (void *) (data_ptr) \
  }

#define ESPHOME_ZB_ZCL_DECLARE_ANALOG_INPUT_ATTRIB_LIST(attr_list, out_of_service, present_value, status_flag, \
                                                        engineering_units, description) \
  ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(attr_list, ZB_ZCL_ANALOG_INPUT) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_INPUT_OUT_OF_SERVICE_ID, (out_of_service)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, (present_value)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_INPUT_STATUS_FLAG_ID, (status_flag)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_INPUT_ENGINEERING_UNITS_ID, (engineering_units)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID, (description)) \
  ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

void zb_zcl_analog_input_init_server();
void zb_zcl_analog_input_init_client();
#define ZB_ZCL_CLUSTER_ID_ANALOG_INPUT_SERVER_ROLE_INIT zb_zcl_analog_input_init_server
#define ZB_ZCL_CLUSTER_ID_ANALOG_INPUT_CLIENT_ROLE_INIT zb_zcl_analog_input_init_client

namespace esphome::zigbee {

class ZigbeeSensor : public ZigbeeEntity, public Component {
 public:
  explicit ZigbeeSensor(sensor::Sensor *sensor);
  void set_cluster_attributes(AnalogAttrs &cluster_attributes) { this->cluster_attributes_ = &cluster_attributes; }

  void setup() override;
  void dump_config() override;

 protected:
  AnalogAttrs *cluster_attributes_{nullptr};
  sensor::Sensor *sensor_{nullptr};
};

}  // namespace esphome::zigbee
#endif
