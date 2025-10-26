#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "esp_zigbee_core.h"

esp_zb_cluster_list_t *esphome_zb_default_clusters_create(esp_zb_ha_standard_devices_t device_type);

esp_err_t esphome_zb_cluster_list_add_or_update_cluster(uint16_t cluster_id, esp_zb_cluster_list_t *cluster_list,
                                                        esp_zb_attribute_list_t *attr_list, uint8_t role_mask);

esp_zb_attribute_list_t *esphome_zb_default_attr_list_create(uint16_t cluster_id);
esp_err_t esphome_zb_cluster_add_attr(uint16_t cluster_id, esp_zb_attribute_list_t *attr_list, uint16_t attr_id,
                                      void *value_p);
esp_err_t esphome_zb_cluster_add_or_update_attr(uint16_t cluster_id, esp_zb_attribute_list_t *attr_list,
                                                uint16_t attr_id, uint8_t attr_type, uint8_t attr_access,
                                                void *value_p);

#ifdef __cplusplus
}

namespace esphome {
namespace zigbee {}
}  // namespace esphome

#endif
