#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include "zigbee_helpers_esp32.h"
#include "ezbee/zha.h"

ezb_err_t esphome_zb_cluster_add_or_update_attr(uint16_t cluster_id, ezb_zcl_cluster_desc_t cluster_desc,
                                                uint16_t attr_id, void *value_p) {
  ezb_zcl_attr_desc_t attr_desc = ezb_zcl_cluster_get_attr_desc(cluster_desc, attr_id, EZB_ZCL_STD_MANUF_CODE);
  if (attr_desc != NULL) {
    return ezb_zcl_attr_desc_set_value(attr_desc, value_p);
  }
  return esphome_zb_cluster_add_attr(cluster_id, cluster_desc, attr_id, value_p);
}

ezb_err_t esphome_zb_add_or_update_cluster(uint16_t cluster_id, ezb_af_ep_desc_t ep_desc, uint8_t role_mask) {
  if (ezb_af_endpoint_get_cluster_desc(ep_desc, cluster_id, role_mask) != NULL) {
    // Cluster already exists, nothing to do
    return EZB_ERR_NONE;
  }
  ezb_zcl_cluster_desc_t cluster_desc;
  cluster_desc = esphome_zb_default_cluster_dscr_create(cluster_id, role_mask);
  return ezb_af_endpoint_add_cluster_desc(ep_desc, cluster_desc);
}

ezb_zcl_cluster_desc_t esphome_zb_default_cluster_dscr_create(uint16_t cluster_id, uint8_t role_mask) {
  switch (cluster_id) {
    case EZB_ZCL_CLUSTER_ID_BASIC:
      return ezb_zcl_basic_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_IDENTIFY:
      return ezb_zcl_identify_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_ANALOG_INPUT:
      return ezb_zcl_analog_input_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_BINARY_INPUT:
      return ezb_zcl_binary_input_create_cluster_desc(NULL, role_mask);
    default: {
      ezb_zcl_custom_cluster_config_t config = {0};
      config.cluster_id = cluster_id;
      return ezb_zcl_custom_create_cluster_desc(&config, role_mask);
    }
  }
}

ezb_err_t esphome_zb_cluster_add_attr(uint16_t cluster_id, ezb_zcl_cluster_desc_t cluster_desc, uint16_t attr_id,
                                      void *value_p) {
  switch (cluster_id) {
    case EZB_ZCL_CLUSTER_ID_BASIC:
      return ezb_zcl_basic_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_IDENTIFY:
      return ezb_zcl_identify_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_ANALOG_INPUT:
      return ezb_zcl_analog_input_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_BINARY_INPUT:
      return ezb_zcl_binary_input_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    default:
      return EZB_ERR_NOT_FOUND;
  }
}

#endif
#endif
