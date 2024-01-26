/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef USE_ESP32
#include "esp_idf_version.h"
#if ESP_IDF_VERSION_MAJOR >= 5

#include "esp_eth_driver.h"
#include "esp_eth_phy.h"

#ifdef __cplusplus
namespace esphome {
namespace ethernet {
extern "C" {
#endif

typedef enum {
    LAN867X_ETH_CMD_S_PLCA = ETH_CMD_CUSTOM_PHY_CMDS,   /*!< Enable or disable PLCA */
    LAN867X_ETH_CMD_S_PLCA_NCNT,                        /*!< Set PLCA node count */
    LAN867X_ETH_CMD_G_PLCA_NCNT,                        /*!< Get PLCA node count */
    LAN867X_ETH_CMD_S_PLCA_ID,                          /*!< Set PLCA ID */
    LAN867X_ETH_CMD_G_PLCA_ID,                          /*!< Get PLCA ID */
    LAN768X_ETH_CMD_PLCA_RST                            /*!< Reset PLCA*/
} phy_lan867x_custom_io_cmd_t;

/**
* @brief Create a PHY instance of LAN867x
*
* @param[in] config: configuration of PHY
*
* @return
*      - instance: create PHY instance successfully
*      - NULL: create PHY instance failed because some error occurred
*/
esp_eth_phy_t *esp_eth_phy_new_lan867x(const eth_phy_config_t *config);

#ifdef __cplusplus
}
}
}
#endif

#endif  // ESP_IDF_VERSION_MAJOR >= 5
#endif  // USE_ESP32
