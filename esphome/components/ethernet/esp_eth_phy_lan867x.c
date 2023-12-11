/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_eth_phy_802_3.h"
#include "esp_eth_driver.h"
#include "esp_eth_phy_lan867x.h"

static const char *TAG = "lan867x";

/***************List of Supported Models***************/
#define LAN867X_MODEL_NUM  0x16
#define LAN867X_OUI 0xC0001C

static const uint8_t supported_models[] = {
    LAN867X_MODEL_NUM,
};

/***************Vendor Specific Register***************/
typedef union {
    struct {
        uint32_t oui_bits_2_9 : 8; /*!< Organizationally Unique Identifier(OUI) bits 3 to 10 */
        uint32_t oui_bits_10_17 : 8; /*!< Organizationally Unique Identifier(OUI) bits 11 to 18 */
    };
    uint32_t val;
} lan867x_phyidr1_reg_t;
#define ETH_PHY_IDR1_REG_ADDR (0x02)

typedef union {
    struct {
        uint32_t model_revision : 4; /*!< Model revision number */
        uint32_t vendor_model : 6;   /*!< Vendor model number */
        uint32_t oui_bits_18_23 : 6; /*!< Organizationally Unique Identifier(OUI) bits 19 to 24 */
    };
    uint32_t val;
} lan867x_phyidr2_reg_t;
#define ETH_PHY_IDR2_REG_ADDR (0x03)

typedef union {
    struct {
        uint32_t reserved1 : 14;    // Reserved
        uint32_t rst : 1;           // PLCA Reset
        uint32_t en : 1;            // PLCA Enable
    };
    uint32_t val;
} lan867x_plca_ctrl0_reg_t;
#define ETH_PHY_PLCA_CTRL0_REG_ADDR (0xCA01)

typedef union {
    struct {
        uint8_t id;     // PLCA ID
        uint8_t ncnt;   // Node count
    };
    uint32_t val;
} lan867x_plca_ctrl1_reg_t;
#define ETH_PHY_PLCA_CTRL1_REG_ADDR (0xCA02)


typedef struct {
    phy_802_3_t phy_802_3;
} phy_lan867x_t;

/***********Custom functions implementations***********/
esp_err_t esp_eth_phy_lan867x_read_oui(phy_802_3_t *phy_802_3, uint32_t *oui)
{
    esp_err_t ret = ESP_OK;
    lan867x_phyidr1_reg_t id1;
    lan867x_phyidr2_reg_t id2;
    esp_eth_mediator_t *eth = phy_802_3->eth;

    ESP_GOTO_ON_FALSE(oui != NULL, ESP_ERR_INVALID_ARG, err, TAG, "oui can't be null");

    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_IDR1_REG_ADDR, &(id1.val)), err, TAG, "read ID1 failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_IDR2_REG_ADDR, &(id2.val)), err, TAG, "read ID2 failed");

    *oui = (id2.oui_bits_18_23 << 18) + ((id1.oui_bits_10_17 << 10) + (id1.oui_bits_2_9 << 2));
    return ESP_OK;
err:
    return ret;
}

static esp_err_t lan867x_update_link_duplex_speed(phy_lan867x_t *lan867x)
{
    esp_err_t ret = ESP_OK;
    esp_eth_mediator_t *eth = lan867x->phy_802_3.eth;
    uint32_t addr = lan867x->phy_802_3.addr;
    bmcr_reg_t bmcr;
    bmsr_reg_t bmsr;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)), err, TAG, "read BMCR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)), err, TAG, "read BMSR failed");
    eth_speed_t speed = bmcr.speed_select ? ETH_SPEED_100M : ETH_SPEED_10M;
    eth_duplex_t duplex = bmcr.duplex_mode  ? ETH_DUPLEX_FULL : ETH_DUPLEX_HALF;
    eth_link_t link = bmsr.link_status ? ETH_LINK_UP : ETH_LINK_DOWN;
    // This will be ran exactly once, when everything is setting up
    if (lan867x->phy_802_3.link_status != link) {
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *)speed), err, TAG, "change speed failed");
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *)duplex), err, TAG, "change duplex failed");
        ESP_GOTO_ON_ERROR(eth->on_state_changed(eth, ETH_STATE_LINK, (void *)link), err, TAG, "change link failed");
        lan867x->phy_802_3.link_status = link;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t lan867x_get_link(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_lan867x_t *lan867x = __containerof(esp_eth_phy_into_phy_802_3(phy), phy_lan867x_t, phy_802_3);

    /* Update information about link, speed, duplex */
    ESP_GOTO_ON_ERROR(lan867x_update_link_duplex_speed(lan867x), err, TAG, "update link duplex speed failed");
    return ESP_OK;
err:
    return ret;
}

static esp_err_t lan867x_init(esp_eth_phy_t *phy)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);

    /* Basic PHY init */
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_basic_phy_init(phy_802_3), err, TAG, "failed to init PHY");
    /* Check PHY ID */
    uint32_t oui;
    uint8_t model;
    ESP_GOTO_ON_ERROR(esp_eth_phy_lan867x_read_oui(phy_802_3, &oui), err, TAG, "read OUI failed");
    ESP_GOTO_ON_ERROR(esp_eth_phy_802_3_read_manufac_info(phy_802_3, &model, NULL), err, TAG, "read manufacturer's info failed");
    ESP_GOTO_ON_FALSE(oui == LAN867X_OUI, ESP_FAIL, err, TAG, "wrong chip OUI %lx (expected %x)", oui, LAN867X_OUI);

    bool supported_model = false;
    for (unsigned int i = 0; i < sizeof(supported_models); i++) {
        if (model == supported_models[i]) {
            supported_model = true;
            break;
        }
    }
    ESP_GOTO_ON_FALSE(supported_model, ESP_FAIL, err, TAG, "unsupported chip model %x", model);
    return ESP_OK;
err:
    return ret;
}

static esp_err_t lan867x_autonego_ctrl(esp_eth_phy_t *phy, eth_phy_autoneg_cmd_t cmd, bool *autonego_en_stat)
{
    switch (cmd) {
    case ESP_ETH_PHY_AUTONEGO_RESTART:
    // Fallthrough
    case ESP_ETH_PHY_AUTONEGO_EN:
    // Fallthrough
    case ESP_ETH_PHY_AUTONEGO_DIS:
        return ESP_ERR_NOT_SUPPORTED; // no autonegotiation operations are supported
    case ESP_ETH_PHY_AUTONEGO_G_STAT:
        // since autonegotiation is not supported it is always indicated disabled
        *autonego_en_stat = false;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t lan867x_advertise_pause_ability(esp_eth_phy_t *phy, uint32_t ability)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t lan867x_set_speed(esp_eth_phy_t *phy, eth_speed_t speed)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t lan867x_set_duplex(esp_eth_phy_t *phy, eth_duplex_t duplex)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t lan867x_custom_ioctl(esp_eth_phy_t *phy, uint32_t cmd, void *data)
{
    esp_err_t ret;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
    esp_eth_mediator_t *eth = phy_802_3->eth;
    lan867x_plca_ctrl0_reg_t plca_ctrl0;
    lan867x_plca_ctrl1_reg_t plca_ctrl1;
    switch (cmd) {
    case LAN867X_ETH_CMD_S_PLCA:
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL0_REG_ADDR, &(plca_ctrl0.val)), err, TAG, "read PLCA_CTRL0 failed");
        plca_ctrl0.en = (*(bool *)data == true);
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL0_REG_ADDR, plca_ctrl0.val), err, TAG, "write PLCA_CTRL0 failed");
        break;
    case LAN867X_ETH_CMD_S_PLCA_NCNT:
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL1_REG_ADDR, &(plca_ctrl1.val)), err, TAG, "read PLCA_CTRL1 failed");
        plca_ctrl1.ncnt = *(uint8_t *)data;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL1_REG_ADDR, plca_ctrl1.val), err, TAG, "write PLCA_CTRL1 failed");
        break;
    case LAN867X_ETH_CMD_G_PLCA_NCNT:
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL1_REG_ADDR, &(plca_ctrl1.val)), err, TAG, "read PLCA_CTRL1 failed");
        *(uint8_t *)data = plca_ctrl1.ncnt;
        break;
    case LAN867X_ETH_CMD_S_PLCA_ID:
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL1_REG_ADDR, &(plca_ctrl1.val)), err, TAG, "read PLCA_CTRL1 failed");
        plca_ctrl1.id = *(uint8_t *)data;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL1_REG_ADDR, plca_ctrl1.val), err, TAG, "write PLCA_CTRL1 failed");
        break;
    case LAN867X_ETH_CMD_G_PLCA_ID:
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL1_REG_ADDR, &(plca_ctrl1.val)), err, TAG, "read PLCA_CTRL1 failed");
        *(uint8_t *)data = plca_ctrl1.id;
        break;
    case LAN768X_ETH_CMD_PLCA_RST:
        ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL0_REG_ADDR, &(plca_ctrl0.val)), err, TAG, "read PLCA_CTRL0 failed");
        plca_ctrl0.rst = 1;
        ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL0_REG_ADDR, plca_ctrl0.val), err, TAG, "write PLCA_CTRL0 failed");
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    return ESP_OK;
err:
    return ret;
}

static esp_err_t lan867x_loopback(esp_eth_phy_t *phy, bool enable)
{
    esp_err_t ret = ESP_OK;
    phy_802_3_t *phy_802_3 = esp_eth_phy_into_phy_802_3(phy);
    esp_eth_mediator_t *eth = phy_802_3->eth;
    /* Set Loopback function */
    bmcr_reg_t bmcr;
    lan867x_plca_ctrl0_reg_t plca_ctrl0;
    lan867x_plca_ctrl1_reg_t plca_ctrl1;
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)), err, TAG, "read BMCR failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL0_REG_ADDR, &(plca_ctrl0.val)), err, TAG, "read PLCA_CTRL0 failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_read(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL1_REG_ADDR, &(plca_ctrl1.val)), err, TAG, "read PLCA_CTRL1 failed");
    if (enable) {
        // For loopback to work PLCA must be disabled
        bmcr.en_loopback = 1;
        bool plca_en = false;
        lan867x_custom_ioctl(phy, LAN867X_ETH_CMD_S_PLCA, &plca_en);
    } else {
        bmcr.en_loopback = 0;
    }
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL0_REG_ADDR, plca_ctrl0.val), err, TAG, "write PLCA_CTRL0 failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_PLCA_CTRL1_REG_ADDR, plca_ctrl1.val), err, TAG, "write PLCA_CTRL1 failed");
    ESP_GOTO_ON_ERROR(eth->phy_reg_write(eth, phy_802_3->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val), err, TAG, "write BMCR failed");
    return ESP_OK;
err:
    return ret;
}

esp_eth_phy_t *esp_eth_phy_new_lan867x(const eth_phy_config_t *config)
{
    esp_eth_phy_t *ret = NULL;
    phy_lan867x_t *lan867x = calloc(1, sizeof(phy_lan867x_t));
    ESP_GOTO_ON_FALSE(lan867x, NULL, err, TAG, "calloc lan867x failed");
    ESP_GOTO_ON_FALSE(esp_eth_phy_802_3_obj_config_init(&lan867x->phy_802_3, config) == ESP_OK,
                      NULL, err, TAG, "configuration initialization of PHY 802.3 failed");

    // redefine functions which need to be customized for sake of LAN867x
    lan867x->phy_802_3.parent.init = lan867x_init;
    lan867x->phy_802_3.parent.get_link = lan867x_get_link;
    lan867x->phy_802_3.parent.autonego_ctrl = lan867x_autonego_ctrl;
    lan867x->phy_802_3.parent.set_speed = lan867x_set_speed;
    lan867x->phy_802_3.parent.set_duplex = lan867x_set_duplex;
    lan867x->phy_802_3.parent.loopback = lan867x_loopback;
    lan867x->phy_802_3.parent.custom_ioctl = lan867x_custom_ioctl;
    lan867x->phy_802_3.parent.advertise_pause_ability = lan867x_advertise_pause_ability;

    return &lan867x->phy_802_3.parent;
err:
    if (lan867x != NULL) {
        free(lan867x);
    }
    return ret;
}
