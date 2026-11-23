#pragma once
#include "esp_eth_driver.h"
#include "esphome/components/ethernet/ethernet_component.h"
// Power the PHY down (BMCR bit 11) or back up with autonegotiation restarted: the link partner and the
// driver see the same carrier loss as a pulled cable. A failure logs "HWT phy failed": the link did not change.
static bool hwt_phy_stuck = false;
inline bool hwt_phy_up_retry();
inline bool hwt_phy_power(bool on) {
  auto *eth = esphome::ethernet::global_eth_component;
  esp_eth_handle_t h = eth != nullptr ? eth->get_eth_handle() : nullptr;
  if (h == nullptr) {
    ESP_LOGE("hwt", "HWT phy failed: no Ethernet handle");
    return false;
  }
  uint32_t v = 0;
  esp_eth_phy_reg_rw_data_t rd{0, &v};
  esp_err_t e1 = esp_eth_ioctl(h, ETH_CMD_READ_PHY_REG, &rd);
  if (e1 != ESP_OK) {
    ESP_LOGE("hwt", "HWT phy failed: %s bmcr read error %d, not written", on ? "up" : "down", e1);
    return false;
  }
  uint32_t before = v;
  if (!on && (v & (1u << 11))) {  // someone else's power-down: this one would not change the link
    ESP_LOGW("hwt", "HWT phy failed: already powered down");
    return false;
  }
  if (on)
    v = (v & ~(1u << 11)) | (1u << 12) | (1u << 9);
  else
    v |= (1u << 11);
  esp_eth_phy_reg_rw_data_t wr{0, &v};
  esp_err_t e2 = esp_eth_ioctl(h, ETH_CMD_WRITE_PHY_REG, &wr);
  if (e2 != ESP_OK) {
    ESP_LOGE("hwt", "HWT phy failed: %s bmcr write error %d", on ? "up" : "down", e2);
    return false;
  }
  // read it back: a PHY that ignored the write must not get the changed-link markers
  uint32_t check = 0;
  esp_eth_phy_reg_rw_data_t rb{0, &check};
  if (esp_eth_ioctl(h, ETH_CMD_READ_PHY_REG, &rb) != ESP_OK || ((check >> 11) & 1) != (on ? 0u : 1u)) {
    ESP_LOGE("hwt", "HWT phy failed: %s bmcr reads 0x%04x after the write", on ? "up" : "down", (unsigned) check);
    if (!on)  // the power-down may have taken effect anyway: bring the PHY back now, as the callers skip the power-up
      hwt_phy_up_retry();
    return false;
  }
  ESP_LOGI("hwt", "HWT phy %s bmcr 0x%04x -> 0x%04x (%d/%d)", on ? "up" : "down", (unsigned) before, (unsigned) v, e1,
           e2);
  return true;
}

// Power the PHY back up, retrying: a PHY left down would show as a lost backbone in every later cycle. After three
// failures it logs "HWT phy stuck down" and sets hwt_phy_stuck, which blocks further drop and race actions.
inline bool hwt_phy_up_retry() {
  for (int i = 0; i < 3; i++) {
    if (hwt_phy_power(true))
      return true;
    esphome::delay(200);
  }
  hwt_phy_stuck = true;
  ESP_LOGE("hwt", "HWT phy stuck down: drop and race actions are blocked, later results are not valid");
  return false;
}
