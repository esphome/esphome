#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/lock_free_queue.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

// IDF headers
#include <esp_bt_defs.h>
#include <esp_gap_bt_api.h>

#include "utils.h"

namespace esphome {
namespace esp32_bt_classic {

class ESP32BtClassic;

typedef esp_bt_gap_cb_param_t::read_rmt_name_param rmt_name_result;

struct BtGapEvent {
  explicit BtGapEvent(esp_bt_gap_cb_event_t Event, esp_bt_gap_cb_param_t *Param) : event(Event), param(*Param) {}
  esp_bt_gap_cb_event_t event;
  esp_bt_gap_cb_param_t param;
};

struct bt_scan_item {
  bt_scan_item() : address{}, scans_remaining{}, next_scan_time{}, scan_duration{} {}
  bt_scan_item(uint64_t u64_addr, uint8_t num_scans)
      : address(u64_addr), scans_remaining(num_scans), next_scan_time{}, scan_duration{} {}
  uint64_t address;
  uint8_t scans_remaining;
  uint32_t next_scan_time;
  uint32_t scan_duration;
};

struct BtAddress {
  BtAddress(uint64_t address) : address_(address) {}
  BtAddress(const esp_bd_addr_t &address) : address_(bd_addr_to_uint64(address)) {}

  // Implicit conversion operators
  operator uint64_t() const { return address_; }
  operator std::string() const { return str(); }
  operator const char *() const { return c_str(); }

  // Explicit type accessors
  uint64_t u64() const { return address_; }
  std::string str() const { return u64_addr_to_str(address_); }
  const char *c_str() const { return str().c_str(); }

 protected:
  uint64_t address_;
};

struct BtStatus {
  BtStatus(esp_bt_status_t status) : status_(status) {}

  // Implicit conversion operators
  operator esp_bt_status_t() const { return status_; }
  operator const char *() const { return c_str(); }
  operator std::string() const { return c_str(); }

  // Explicit type accessors
  esp_bt_status_t bt_status() const { return status_; }
  const char *c_str() const { return esp_bt_status_to_str(status_); }
  std::string str() const { return c_str(); }

 protected:
  esp_bt_status_t status_;
};

struct ScanStatus {
  ScanStatus(scan_status_t status) : status_(status) {}

  // Implicit conversion operators
  operator scan_status_t() const { return status_; }
  operator const char *() const { return c_str(); }
  operator std::string() const { return c_str(); }

  // Explicit type accessors
  scan_status_t scan_status() const { return status_; }
  const char *c_str() const { return scan_status_to_str(status_); }
  std::string str() const { return c_str(); }

 protected:
  scan_status_t status_;
};

class BtClassicItf {
 public:
  virtual void addScan(const bt_scan_item &scan) = 0;
  virtual void addScan(const std::vector<bt_scan_item> &scan_list) = 0;
};

class BtClassicChildBase {
 public:
  BtClassicItf *parent() { return this->parent_; }
  void set_parent(BtClassicItf *parent) { parent_ = parent; }

 protected:
  BtClassicItf *parent_{nullptr};
};

class BtClassicScanStartListner : public BtClassicChildBase {
 public:
  virtual void on_scan_start() = 0;
};

class BtClassicScanResultListner : public BtClassicChildBase {
 public:
  virtual void on_scan_result(const rmt_name_result &result, const optional<bt_scan_item> &scan_item) = 0;
};

#ifdef USE_BUTTON
class ResetBtStackButton : public button::Button, public Parented<ESP32BtClassic> {
 public:
  ResetBtStackButton() = default;

 protected:
  void press_action() override;
};
#endif

// -----------------------------------------------
// Main BT Classic class:
//
class ESP32BtClassic : public Component, public BtClassicItf {
 public:
  virtual ~ESP32BtClassic(){};
  void setup() override;
  void reset_bt_stack();
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }

  void enable();
  void disable();

#ifdef USE_BUTTON
  void set_reset_bt_stack_button(button::Button *button) { reset_bt_stack_button_ = button; }
#endif

#ifdef USE_TEXT_SENSOR
  void set_last_error_sensor(text_sensor::TextSensor *last_error) { last_error_sensor_ = last_error; }
#endif

  void register_scan_start_listener(BtClassicScanStartListner *listner) {
    listner->set_parent(this);
    scan_start_listners_.push_back(listner);
  }
  void register_scan_result_listener(BtClassicScanResultListner *listner) {
    listner->set_parent(this);
    scan_result_listners_.push_back(listner);
  }

  // Interface functions:
  void addScan(const bt_scan_item &scan) override;
  void addScan(const std::vector<bt_scan_item> &scan_list) override;

 protected:
  static void gap_event_handler(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
  void real_gap_event_handler_(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

  void startScan(const uint64_t u64_addr);

  optional<bt_scan_item> handle_scan_result(const rmt_name_result &result);

  bool bt_pre_setup_();
  bool bt_setup_();
  bool bt_dismantle_();
  bool gap_startup();

  bool enable_on_boot_{};
  bool scanPending_{false};
  uint32_t last_scan_ms{};
  std::vector<bt_scan_item> active_scan_list_{};

  // Listners a.o.
  std::vector<BtClassicScanStartListner *> scan_start_listners_;
  std::vector<BtClassicScanResultListner *> scan_result_listners_;

  // Queue which thread safety precautions:
  esphome::LockFreeQueue<BtGapEvent, 16> bt_events_;

#ifdef USE_BUTTON
  button::Button *reset_bt_stack_button_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *last_error_sensor_{nullptr};
#endif
  const uint32_t scan_delay_{100};  // (ms) minimal time between consecutive scans
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern ESP32BtClassic *global_bt_classic;

template<typename... Ts> class BTClassicEnableAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override {
    if (global_bt_classic != nullptr)
      global_bt_classic->enable();
  }
};

template<typename... Ts> class BTClassicDisableAction : public Action<Ts...> {
 public:
  void play(const Ts &...x) override {
    if (global_bt_classic != nullptr)
      global_bt_classic->disable();
  }
};

}  // namespace esp32_bt_classic
}  // namespace esphome

#endif
