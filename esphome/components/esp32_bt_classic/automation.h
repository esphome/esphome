#pragma once

#ifdef USE_ESP32

#include <utility>
#include <vector>
#include "bt_classic.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace esp32_bt_classic {

class BtClassicScannerNode : public BtClassicChildBase {
 public:
  BtClassicScannerNode(ESP32BtClassic *bt_client) { set_parent(bt_client); }

  void set_scan_delay(uint32_t delay) { this->scan_delay_ = delay; }

  void scan(const std::vector<uint64_t> &u64_addrs, uint16_t num_scans) {
    for (const auto &addr : u64_addrs) {
      parent()->addScan(bt_scan_item(addr, num_scans));
    }
  }

  void scan(const std::vector<std::string> &str_addrs, uint16_t num_scans) {
    for (const auto &str : str_addrs) {
      esp_bd_addr_t bd_addr;
      if (str_to_bd_addr(str.c_str(), bd_addr)) {
        ESP_LOGV(TAG, "Adding '%02X:%02X:%02X:%02X:%02X:%02X' to scan list with %d scans", EXPAND_MAC_F(bd_addr),
                 num_scans);
        parent()->addScan(bt_scan_item(bd_addr_to_uint64(bd_addr), num_scans));
      }
    }
  }

 private:
  uint32_t scan_delay_{};
};

template<typename... Ts> class BtClassicScanAction : public Action<Ts...>, public BtClassicScannerNode {
 public:
  BtClassicScanAction(ESP32BtClassic *bt_client) : BtClassicScannerNode(bt_client) {}

  void play(Ts... x) override {
    ESP_LOGI(TAG, "BtClassicScanAction::play()");
    uint8_t scanCount = this->num_scans_simple_;
    if (num_scans_template_ != nullptr) {
      scanCount = this->num_scans_template_(x...);
    }

    if (addr_template_ == nullptr) {
      scan(addr_simple_, scanCount);
    } else {
      scan(addr_template_(x...), scanCount);
    }
  }

  void set_addr_template(std::function<std::vector<std::string>(Ts...)> func) {
    this->addr_template_ = std::move(func);
  }
  void set_addr_simple(const std::vector<uint64_t> &addr) { this->addr_simple_ = addr; }

  void set_num_scans_simple(uint8_t num_scans) { this->num_scans_simple_ = num_scans; }
  void set_num_scans_template(std::function<uint8_t(Ts...)> func) { this->num_scans_template_ = std::move(func); }

 private:
  uint8_t num_scans_simple_{1};
  std::vector<uint64_t> addr_simple_;
  std::function<std::vector<std::string>(Ts...)> addr_template_{};
  std::function<uint8_t(Ts...)> num_scans_template_{};
};

class BtClassicScanStartTrigger : public Trigger<>, public BtClassicScanStartListner {
 public:
  explicit BtClassicScanStartTrigger(ESP32BtClassic *parent) { parent->register_scan_start_listener(this); }
  void on_scan_start() override { this->trigger(); }
};

class BtClassicScanResultTrigger : public Trigger<const BtAddress &, const BtStatus &, const char *>,
                                   public BtClassicScanResultListner {
 public:
  explicit BtClassicScanResultTrigger(ESP32BtClassic *parent, std::initializer_list<uint64_t> addresses = {})
      : addresses_(addresses) {
    parent->register_scan_result_listener(this);
  }

  void on_scan_result(const rmt_name_result &result) override {
    // struct read_rmt_name_param {
    //   esp_bt_status_t stat;                            /*!< read Remote Name status */
    //   uint8_t rmt_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1]; /*!< Remote device name */
    //   esp_bd_addr_t bda;                               /*!< remote bluetooth device address*/
    // } read_rmt_name;

    uint64_t result_addr = bd_addr_to_uint64(result.bda);
    if (!addresses_.empty()) {
      if (std::find(addresses_.begin(), addresses_.end(), result_addr) == addresses_.end()) {
        return;  // Result address not in list to watch. Skip!
      }
    }

    this->trigger(result_addr, result.stat, (const char *) result.rmt_name);
  }

 protected:
  std::vector<uint64_t> addresses_;
};

}  // namespace esp32_bt_classic
}  // namespace esphome

#endif
