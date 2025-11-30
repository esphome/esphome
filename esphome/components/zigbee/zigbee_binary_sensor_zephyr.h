#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_BINARY_SENSOR)
#include "esphome/components/zigbee/zigbee_zephyr.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
extern "C" {
#include <zboss_api.h>
#include <zboss_api_addons.h>
}

// it should have been defined inside of sdk. It is missing though
#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID(data_ptr) \
  { \
    ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID, ZB_ZCL_ATTR_TYPE_CHAR_STRING, ZB_ZCL_ATTR_ACCESS_READ_ONLY, \
        (ZB_ZCL_NON_MANUFACTURER_SPECIFIC), (void *) (data_ptr) \
  }

// copy of ZB_ZCL_DECLARE_BINARY_INPUT_ATTRIB_LIST + description
#define ESPHOME_ZB_ZCL_DECLARE_BINARY_INPUT_ATTRIB_LIST(attr_list, out_of_service, present_value, status_flag, \
                                                        description) \
  ZB_ZCL_START_DECLARE_ATTRIB_LIST_CLUSTER_REVISION(attr_list, ZB_ZCL_BINARY_INPUT) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_INPUT_OUT_OF_SERVICE_ID, (out_of_service)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, (present_value)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_INPUT_STATUS_FLAG_ID, (status_flag)) \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID, (description)) \
  ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

namespace esphome::zigbee {

class ZigbeeBinarySensor : public ZigbeeEntity, public Component {
 public:
  explicit ZigbeeBinarySensor(binary_sensor::BinarySensor *binary_sensor);
  void set_cluster_attributes(BinaryAttrs &cluster_attributes) { this->cluster_attributes_ = &cluster_attributes; }

  void setup() override;
  void dump_config() override;

 protected:
  BinaryAttrs *cluster_attributes_{nullptr};
  binary_sensor::BinarySensor *binary_sensor_;
};

}  // namespace esphome::zigbee
#endif
