#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#include "ha/esp_zigbee_ha_standard.h"
#include "zigbee_helpers_esp32.h"

esp_err_t esphome_zb_cluster_add_or_update_attr(uint16_t cluster_id, esp_zb_attribute_list_t *attr_list,
                                                uint16_t attr_id, void *value_p) {
  esp_err_t ret;
  ret = esp_zb_cluster_update_attr(attr_list, attr_id, value_p);
  if (ret != ESP_OK) {
    ESP_LOGE("zigbee_helper", "Ignore previous attribute not found error");
    ret = esphome_zb_cluster_add_attr(cluster_id, attr_list, attr_id, value_p);
  }
  if (ret != ESP_OK) {
    ESP_LOGE("zigbee_helper", "Could not add attribute 0x%04X to cluster 0x%04X: %s", attr_id, cluster_id,
             esp_err_to_name(ret));
  }
  return ret;
}

esp_err_t esphome_zb_cluster_list_add_or_update_cluster(uint16_t cluster_id, esp_zb_cluster_list_t *cluster_list,
                                                        esp_zb_attribute_list_t *attr_list, uint8_t role_mask) {
  esp_err_t ret;
  ret = esp_zb_cluster_list_update_cluster(cluster_list, attr_list, cluster_id, role_mask);
  if (ret != ESP_OK) {
    ESP_LOGE("zigbee_helper", "Ignore previous cluster not found error");
    switch (cluster_id) {
      case ESP_ZB_ZCL_CLUSTER_ID_BASIC:
        ret = esp_zb_cluster_list_add_basic_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
        ret = esp_zb_cluster_list_add_identify_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT:
        ret = esp_zb_cluster_list_add_analog_input_cluster(cluster_list, attr_list, role_mask);
        break;
      case ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT:
        ret = esp_zb_cluster_list_add_binary_input_cluster(cluster_list, attr_list, role_mask);
        break;
      default:
        ret = esp_zb_cluster_list_add_custom_cluster(cluster_list, attr_list, role_mask);
    }
  }
  return ret;
}

esp_zb_attribute_list_t *esphome_zb_default_attr_list_create(uint16_t cluster_id) {
  switch (cluster_id) {
    case ESP_ZB_ZCL_CLUSTER_ID_BASIC:
      return esp_zb_basic_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
      return esp_zb_identify_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT:
      return esp_zb_analog_input_cluster_create(NULL);
    case ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT:
      return esp_zb_binary_input_cluster_create(NULL);
    default:
      return esp_zb_zcl_attr_list_create(cluster_id);
  }
}

esp_err_t esphome_zb_cluster_add_attr(uint16_t cluster_id, esp_zb_attribute_list_t *attr_list, uint16_t attr_id,
                                      void *value_p) {
  switch (cluster_id) {
    case ESP_ZB_ZCL_CLUSTER_ID_BASIC:
      return esp_zb_basic_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
      return esp_zb_identify_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT:
      return esp_zb_analog_input_cluster_add_attr(attr_list, attr_id, value_p);
    case ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT:
      return esp_zb_binary_input_cluster_add_attr(attr_list, attr_id, value_p);
    default:
      return ESP_FAIL;
  }
}

#endif
#endif
