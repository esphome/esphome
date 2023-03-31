// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef USE_ESP32

#include <string.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_eth.h"
#include "eth_phy_regs_struct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"

static const char *TAG = "jl1101";
#define PHY_CHECK(a, str, goto_tag, ...) \
  do { \
    if (!(a)) { \
      ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
      goto goto_tag; \
    } \
  } while (0)

/***************Vendor Specific Register***************/

/**
 * @brief PSR(Page Select Register)
 *
 */
typedef union {
  struct {
    uint16_t page_select : 8; /* Select register page, default is 0 */
    uint16_t reserved : 8;    /* Reserved */
  };
  uint16_t val;
} psr_reg_t;
#define ETH_PHY_PSR_REG_ADDR (0x1F)

typedef struct {
  esp_eth_phy_t parent;
  esp_eth_mediator_t *eth;
  int addr;
  uint32_t reset_timeout_ms;
  uint32_t autonego_timeout_ms;
  eth_link_t link_status;
  int reset_gpio_num;
} phy_jl1101_t;

static esp_err_t jl1101_page_select(phy_jl1101_t *jl1101, uint32_t page) {
  esp_eth_mediator_t *eth = jl1101->eth;
  psr_reg_t psr = {.page_select = page};
  PHY_CHECK(eth->phy_reg_write(eth, jl1101->addr, ETH_PHY_PSR_REG_ADDR, psr.val) == ESP_OK, "write PSR failed", err);
  return ESP_OK;
err:
  return ESP_FAIL;
}

static esp_err_t jl1101_update_link_duplex_speed(phy_jl1101_t *jl1101) {
  esp_eth_mediator_t *eth = jl1101->eth;
  eth_speed_t speed = ETH_SPEED_10M;
  eth_duplex_t duplex = ETH_DUPLEX_HALF;
  bmcr_reg_t bmcr;
  bmsr_reg_t bmsr;
  uint32_t peer_pause_ability = false;
  anlpar_reg_t anlpar;
  PHY_CHECK(jl1101_page_select(jl1101, 0) == ESP_OK, "select page 0 failed", err);
  PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)) == ESP_OK, "read BMSR failed",
            err);
  PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_ANLPAR_REG_ADDR, &(anlpar.val)) == ESP_OK,
            "read ANLPAR failed", err);
  eth_link_t link = bmsr.link_status ? ETH_LINK_UP : ETH_LINK_DOWN;
  /* check if link status changed */
  if (jl1101->link_status != link) {
    /* when link up, read negotiation result */
    if (link == ETH_LINK_UP) {
      PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK, "read BMCR failed",
                err);
      if (bmcr.speed_select) {
        speed = ETH_SPEED_100M;
      } else {
        speed = ETH_SPEED_10M;
      }
      if (bmcr.duplex_mode) {
        duplex = ETH_DUPLEX_FULL;
      } else {
        duplex = ETH_DUPLEX_HALF;
      }
      PHY_CHECK(eth->on_state_changed(eth, ETH_STATE_SPEED, (void *) speed) == ESP_OK, "change speed failed", err);
      PHY_CHECK(eth->on_state_changed(eth, ETH_STATE_DUPLEX, (void *) duplex) == ESP_OK, "change duplex failed", err);
      /* if we're in duplex mode, and peer has the flow control ability */
      if (duplex == ETH_DUPLEX_FULL && anlpar.symmetric_pause) {
        peer_pause_ability = 1;
      } else {
        peer_pause_ability = 0;
      }
      PHY_CHECK(eth->on_state_changed(eth, ETH_STATE_PAUSE, (void *) peer_pause_ability) == ESP_OK,
                "change pause ability failed", err);
    }
    PHY_CHECK(eth->on_state_changed(eth, ETH_STATE_LINK, (void *) link) == ESP_OK, "change link failed", err);
    jl1101->link_status = link;
  }
  return ESP_OK;
err:
  return ESP_FAIL;
}

static esp_err_t jl1101_set_mediator(esp_eth_phy_t *phy, esp_eth_mediator_t *eth) {
  PHY_CHECK(eth, "can't set mediator to null", err);
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  jl1101->eth = eth;
  return ESP_OK;
err:
  return ESP_ERR_INVALID_ARG;
}

static esp_err_t jl1101_get_link(esp_eth_phy_t *phy) {
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  /* Updata information about link, speed, duplex */
  PHY_CHECK(jl1101_update_link_duplex_speed(jl1101) == ESP_OK, "update link duplex speed failed", err);
  return ESP_OK;
err:
  return ESP_FAIL;
}

static esp_err_t jl1101_reset(esp_eth_phy_t *phy) {
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  jl1101->link_status = ETH_LINK_DOWN;
  esp_eth_mediator_t *eth = jl1101->eth;
  bmcr_reg_t bmcr = {.reset = 1};
  PHY_CHECK(eth->phy_reg_write(eth, jl1101->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val) == ESP_OK, "write BMCR failed", err);
  /* Wait for reset complete */
  uint32_t to = 0;
  for (to = 0; to < jl1101->reset_timeout_ms / 50; to++) {
    vTaskDelay(pdMS_TO_TICKS(50));
    PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK, "read BMCR failed",
              err);
    if (!bmcr.reset) {
      break;
    }
  }
  PHY_CHECK(to < jl1101->reset_timeout_ms / 50, "reset timeout", err);
  return ESP_OK;
err:
  return ESP_FAIL;
}

static esp_err_t jl1101_reset_hw(esp_eth_phy_t *phy) {
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  if (jl1101->reset_gpio_num >= 0) {
    esp_rom_gpio_pad_select_gpio(jl1101->reset_gpio_num);
    gpio_set_direction(jl1101->reset_gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(jl1101->reset_gpio_num, 0);
    esp_rom_delay_us(100);  // insert min input assert time
    gpio_set_level(jl1101->reset_gpio_num, 1);
  }
  return ESP_OK;
}

static esp_err_t jl1101_negotiate(esp_eth_phy_t *phy) {
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  esp_eth_mediator_t *eth = jl1101->eth;
  /* in case any link status has changed, let's assume we're in link down status */
  jl1101->link_status = ETH_LINK_DOWN;
  /* Restart auto negotiation */
  bmcr_reg_t bmcr = {
      .speed_select = 1,     /* 100Mbps */
      .duplex_mode = 1,      /* Full Duplex */
      .en_auto_nego = 1,     /* Auto Negotiation */
      .restart_auto_nego = 1 /* Restart Auto Negotiation */
  };
  PHY_CHECK(eth->phy_reg_write(eth, jl1101->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val) == ESP_OK, "write BMCR failed", err);
  /* Wait for auto negotiation complete */
  bmsr_reg_t bmsr;
  uint32_t to = 0;
  for (to = 0; to < jl1101->autonego_timeout_ms / 100; to++) {
    vTaskDelay(pdMS_TO_TICKS(100));
    PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_BMSR_REG_ADDR, &(bmsr.val)) == ESP_OK, "read BMSR failed",
              err);
    if (bmsr.auto_nego_complete) {
      break;
    }
  }
  /* Auto negotiation failed, maybe no network cable plugged in, so output a warning */
  if (to >= jl1101->autonego_timeout_ms / 100) {
    ESP_LOGW(TAG, "auto negotiation timeout");
  }
  return ESP_OK;
err:
  return ESP_FAIL;
}

static esp_err_t jl1101_pwrctl(esp_eth_phy_t *phy, bool enable) {
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  esp_eth_mediator_t *eth = jl1101->eth;
  bmcr_reg_t bmcr;
  PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK, "read BMCR failed",
            err);
  if (!enable) {
    /* Enable IEEE Power Down Mode */
    bmcr.power_down = 1;
  } else {
    /* Disable IEEE Power Down Mode */
    bmcr.power_down = 0;
  }
  PHY_CHECK(eth->phy_reg_write(eth, jl1101->addr, ETH_PHY_BMCR_REG_ADDR, bmcr.val) == ESP_OK, "write BMCR failed", err);
  if (!enable) {
    PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK, "read BMCR failed",
              err);
    PHY_CHECK(bmcr.power_down == 1, "power down failed", err);
  } else {
    /* wait for power up complete */
    uint32_t to = 0;
    for (to = 0; to < jl1101->reset_timeout_ms / 10; to++) {
      vTaskDelay(pdMS_TO_TICKS(10));
      PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_BMCR_REG_ADDR, &(bmcr.val)) == ESP_OK, "read BMCR failed",
                err);
      if (bmcr.power_down == 0) {
        break;
      }
    }
    PHY_CHECK(to < jl1101->reset_timeout_ms / 10, "power up timeout", err);
  }
  return ESP_OK;
err:
  return ESP_FAIL;
}

static esp_err_t jl1101_set_addr(esp_eth_phy_t *phy, uint32_t addr) {
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  jl1101->addr = addr;
  return ESP_OK;
}

static esp_err_t jl1101_get_addr(esp_eth_phy_t *phy, uint32_t *addr) {
  PHY_CHECK(addr, "addr can't be null", err);
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  *addr = jl1101->addr;
  return ESP_OK;
err:
  return ESP_ERR_INVALID_ARG;
}

static esp_err_t jl1101_del(esp_eth_phy_t *phy) {
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  free(jl1101);
  return ESP_OK;
}

static esp_err_t jl1101_advertise_pause_ability(esp_eth_phy_t *phy, uint32_t ability) {
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  esp_eth_mediator_t *eth = jl1101->eth;
  /* Set PAUSE function ability */
  anar_reg_t anar;
  PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_ANAR_REG_ADDR, &(anar.val)) == ESP_OK, "read ANAR failed",
            err);
  if (ability) {
    anar.asymmetric_pause = 1;
    anar.symmetric_pause = 1;
  } else {
    anar.asymmetric_pause = 0;
    anar.symmetric_pause = 0;
  }
  PHY_CHECK(eth->phy_reg_write(eth, jl1101->addr, ETH_PHY_ANAR_REG_ADDR, anar.val) == ESP_OK, "write ANAR failed", err);
  return ESP_OK;
err:
  return ESP_FAIL;
}

static esp_err_t jl1101_init(esp_eth_phy_t *phy) {
  phy_jl1101_t *jl1101 = __containerof(phy, phy_jl1101_t, parent);
  esp_eth_mediator_t *eth = jl1101->eth;
  // Detect PHY address
  if (jl1101->addr == ESP_ETH_PHY_ADDR_AUTO) {
    PHY_CHECK(esp_eth_detect_phy_addr(eth, &jl1101->addr) == ESP_OK, "Detect PHY address failed", err);
  }
  /* Power on Ethernet PHY */
  PHY_CHECK(jl1101_pwrctl(phy, true) == ESP_OK, "power control failed", err);
  /* Reset Ethernet PHY */
  PHY_CHECK(jl1101_reset(phy) == ESP_OK, "reset failed", err);
  /* Check PHY ID */
  phyidr1_reg_t id1;
  phyidr2_reg_t id2;
  PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_IDR1_REG_ADDR, &(id1.val)) == ESP_OK, "read ID1 failed", err);
  PHY_CHECK(eth->phy_reg_read(eth, jl1101->addr, ETH_PHY_IDR2_REG_ADDR, &(id2.val)) == ESP_OK, "read ID2 failed", err);
  PHY_CHECK(id1.oui_msb == 0x937C && id2.oui_lsb == 0x10 && id2.vendor_model == 0x2, "wrong chip ID", err);
  return ESP_OK;
err:
  return ESP_FAIL;
}

static esp_err_t jl1101_deinit(esp_eth_phy_t *phy) {
  /* Power off Ethernet PHY */
  PHY_CHECK(jl1101_pwrctl(phy, false) == ESP_OK, "power control failed", err);
  return ESP_OK;
err:
  return ESP_FAIL;
}

esp_eth_phy_t *esp_eth_phy_new_jl1101(const eth_phy_config_t *config) {
  PHY_CHECK(config, "can't set phy config to null", err);
  phy_jl1101_t *jl1101 = calloc(1, sizeof(phy_jl1101_t));
  PHY_CHECK(jl1101, "calloc jl1101 failed", err);
  jl1101->addr = config->phy_addr;
  jl1101->reset_gpio_num = config->reset_gpio_num;
  jl1101->reset_timeout_ms = config->reset_timeout_ms;
  jl1101->link_status = ETH_LINK_DOWN;
  jl1101->autonego_timeout_ms = config->autonego_timeout_ms;
  jl1101->parent.reset = jl1101_reset;
  jl1101->parent.reset_hw = jl1101_reset_hw;
  jl1101->parent.init = jl1101_init;
  jl1101->parent.deinit = jl1101_deinit;
  jl1101->parent.set_mediator = jl1101_set_mediator;
  jl1101->parent.negotiate = jl1101_negotiate;
  jl1101->parent.get_link = jl1101_get_link;
  jl1101->parent.pwrctl = jl1101_pwrctl;
  jl1101->parent.get_addr = jl1101_get_addr;
  jl1101->parent.set_addr = jl1101_set_addr;
  jl1101->parent.advertise_pause_ability = jl1101_advertise_pause_ability;
  jl1101->parent.del = jl1101_del;

  return &(jl1101->parent);
err:
  return NULL;
}
#endif /* USE_ESP32 */
