#include "esphome/core/defines.h"
#if defined(USE_ZIGBEE) && defined(USE_NRF52) && defined(USE_SENSOR)
extern "C" {
#include "zboss_api.h"
#include "zcl/zb_zcl_common.h"
}
#include "zigbee_sensor_zephyr.h"

namespace esphome::zigbee {

const zb_uint8_t ZB_ZCL_ANALOG_INPUT_STATUS_FLAG_MAX_VALUE = 0x0F;

static zb_ret_t check_value_analog_server(zb_uint16_t attr_id, zb_uint8_t endpoint,
                                          zb_uint8_t *value) {  // NOLINT(readability-non-const-parameter)
  zb_ret_t ret = RET_OK;
  ZVUNUSED(endpoint);

  switch (attr_id) {
    case ZB_ZCL_ATTR_ANALOG_INPUT_OUT_OF_SERVICE_ID:
      ret = ZB_ZCL_CHECK_BOOL_VALUE(*value) ? RET_OK : RET_ERROR;
      break;
    case ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID:
      break;

    case ZB_ZCL_ATTR_ANALOG_INPUT_STATUS_FLAG_ID:
      if (*value > ZB_ZCL_ANALOG_INPUT_STATUS_FLAG_MAX_VALUE) {
        ret = RET_ERROR;
      }
      break;

    default:
      break;
  }

  return ret;
}

}  // namespace esphome::zigbee

void zb_zcl_analog_input_init_server() {
  zb_zcl_add_cluster_handlers(ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ZB_ZCL_CLUSTER_SERVER_ROLE,
                              esphome::zigbee::check_value_analog_server, (zb_zcl_cluster_write_attr_hook_t) NULL,
                              (zb_zcl_cluster_handler_t) NULL);
}

void zb_zcl_analog_input_init_client() {
  zb_zcl_add_cluster_handlers(ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ZB_ZCL_CLUSTER_CLIENT_ROLE,
                              (zb_zcl_cluster_check_value_t) NULL, (zb_zcl_cluster_write_attr_hook_t) NULL,
                              (zb_zcl_cluster_handler_t) NULL);
}

#endif
