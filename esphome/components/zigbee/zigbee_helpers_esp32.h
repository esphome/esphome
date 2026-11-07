#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ESP32
#ifdef USE_ZIGBEE

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_zigbee.h"

ezb_af_ep_desc_t esphome_zb_zha_default_ep_desc_create(uint8_t ep_id, uint16_t device_id, uint8_t power_source);
ezb_err_t esphome_zb_cluster_add_or_update_attr(uint16_t cluster_id, ezb_zcl_cluster_desc_t cluster_desc,
                                                uint16_t attr_id, void *value_p);
ezb_err_t esphome_zb_add_or_update_cluster(uint16_t cluster_id, ezb_af_ep_desc_t ep_desc, uint8_t role_mask);
ezb_zcl_cluster_desc_t esphome_zb_default_cluster_dscr_create(uint16_t cluster_id, uint8_t role_mask);
ezb_err_t esphome_zb_cluster_add_attr(uint16_t cluster_id, ezb_zcl_cluster_desc_t cluster_desc, uint16_t attr_id,
                                      void *value_p);

#ifdef __cplusplus
}
namespace esphome::zigbee {}  // namespace esphome::zigbee
#endif

#endif
#endif
