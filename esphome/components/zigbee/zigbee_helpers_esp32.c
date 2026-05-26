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

ezb_af_ep_desc_t esphome_zb_zha_default_ep_desc_create(uint8_t ep_id, uint16_t device_id, uint8_t power_source) {
  ezb_af_ep_desc_t ep_desc;
  switch (device_id) {
    case EZB_ZHA_TEMPERATURE_SENSOR_DEVICE_ID: {
      ezb_zha_temperature_sensor_config_t config = EZB_ZHA_TEMPERATURE_SENSOR_CONFIG();
      config.basic_cfg.power_source = power_source;
      ep_desc = ezb_zha_create_temperature_sensor(ep_id, &config);
      break;
    }
    default:
      ezb_af_ep_config_t config = {
          .ep_id = ep_id,
          .app_profile_id = EZB_AF_HA_PROFILE_ID,
          .app_device_id = device_id,
          .app_device_version = 0,
      };
      ep_desc = ezb_af_create_endpoint_desc(&config);
  }
  return ep_desc;
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
    case EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT:
      return ezb_zcl_illuminance_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT:
      return ezb_zcl_temperature_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT:
      return ezb_zcl_pressure_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT:
      return ezb_zcl_flow_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT:
      return ezb_zcl_rel_humidity_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT:
      return ezb_zcl_carbon_dioxide_measurement_create_cluster_desc(NULL, role_mask);
    case EZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT:
      return ezb_zcl_pm2_5_measurement_create_cluster_desc(NULL, role_mask);
    default:
      ezb_zcl_custom_cluster_config_t config = {0};
      config.cluster_id = cluster_id;
      return ezb_zcl_custom_create_cluster_desc(&config, role_mask);
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
    case EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT:
      return ezb_zcl_illuminance_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT:
      return ezb_zcl_temperature_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT:
      return ezb_zcl_pressure_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_FLOW_MEASUREMENT:
      return ezb_zcl_flow_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT:
      return ezb_zcl_rel_humidity_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT:
      return ezb_zcl_carbon_dioxide_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    case EZB_ZCL_CLUSTER_ID_PM2_5_MEASUREMENT:
      return ezb_zcl_pm2_5_measurement_cluster_desc_add_attr(cluster_desc, attr_id, value_p);
    default:
      return EZB_ERR_NOT_FOUND;
  }
}

#endif
#endif
