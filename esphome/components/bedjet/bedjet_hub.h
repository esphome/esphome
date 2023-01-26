#pragma once

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "bedjet_child.h"
#include "bedjet_codec.h"

#include <vector>

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome {
namespace bedjet {

namespace espbt = esphome::esp32_ble_tracker;

// Forward declare BedJetClient
class BedJetClient;

static const espbt::ESPBTUUID BEDJET_SERVICE_UUID = espbt::ESPBTUUID::from_raw("00001000-bed0-0080-aa55-4265644a6574");
static const espbt::ESPBTUUID BEDJET_STATUS_UUID = espbt::ESPBTUUID::from_raw("00002000-bed0-0080-aa55-4265644a6574");
static const espbt::ESPBTUUID BEDJET_COMMAND_UUID = espbt::ESPBTUUID::from_raw("00002004-bed0-0080-aa55-4265644a6574");
static const espbt::ESPBTUUID BEDJET_NAME_UUID = espbt::ESPBTUUID::from_raw("00002001-bed0-0080-aa55-4265644a6574");

/**
 * Hub component connecting to the BedJet device over Bluetooth.
 */
class BedJetHub : public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  /* BedJet functionality exposed to `BedJetClient` children and/or accessible from action lambdas. */

  /** Attempts to check for and apply firmware updates. */
  void upgrade_firmware();

  /** Press the OFF button. */
  bool button_off();
  /** Press the HEAT button. */
  bool button_heat();
  /** Press the EXT HT button. */
  bool button_ext_heat();
  /** Press the TURBO button. */
  bool button_turbo();
  /** Press the COOL button. */
  bool button_cool();
  /** Press the DRY button. */
  bool button_dry();
  /** Press the M1 (memory recall) button. */
  bool button_memory1();
  /** Press the M2 (memory recall) button. */
  bool button_memory2();
  /** Press the M3 (memory recall) button. */
  bool button_memory3();

  /** Send the `button`. */
  bool send_button(BedjetButton button);

  /** Set the target temperature to `temp_c` in °C. */
  bool set_target_temp(float temp_c);

  /** Set the fan speed to a stepped index in the range 0-19. */
  bool set_fan_index(uint8_t fan_speed_index);

  /** Set the fan speed to a percent in the range 5% - 100%, at 5% increments. */
  bool set_fan_speed(uint8_t fan_speed_pct) { return this->set_fan_index(bedjet_fan_speed_to_index(fan_speed_pct)); }

  /** Return the fan speed index, in the range 0-19. */
  uint8_t get_fan_index();

  /** Return the fan speed as a percent in the range 5%-100%. */
  uint8_t get_fan_speed() { return bedjet_fan_step_to_speed(this->get_fan_index()); }

  /** Set the operational runtime remaining.
   *
   * The unit establishes and enforces runtime limits for some modes, so this call is not guaranteed to succeed.
   */
  bool set_time_remaining(uint8_t hours, uint8_t mins);

  /** Return the remaining runtime, in seconds. */
  uint16_t get_time_remaining();

  /** @return `true` if the `BLEClient::node_state` is `ClientState::ESTABLISHED`. */
  bool is_connected() { return this->node_state == espbt::ClientState::ESTABLISHED; }

  bool has_status() { return this->codec_->has_status(); }
  const BedjetStatusPacket *get_status_packet() const { return this->codec_->get_status_packet(); }

  /** Register a `BedJetClient` child component. */
  void register_child(BedJetClient *obj);

  /** Set the status timeout.
   *
   * This is the max time to wait for a status update before the connection is presumed unusable.
   */
  void set_status_timeout(uint32_t timeout) { this->timeout_ = timeout; }

#ifdef USE_TIME
  /** Set the `time::RealTimeClock` implementation. */
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
  /** Attempts to sync the local time (via `time_id`) to the BedJet device. */
  void send_local_time();
#endif
  /** Attempt to set the BedJet device's clock to the specified time. */
  void set_clock(uint8_t hour, uint8_t minute);

  /* Component overrides */

  void loop() override;
  void update() override;
  void dump_config() override;
  void setup() override { this->codec_ = make_unique<BedjetCodec>(); }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  /** @return The BedJet's configured name, or the MAC address if not discovered yet. */
  std::string get_name() {
    if (this->name_.empty()) {
      return this->parent_->address_str();
    } else {
      return this->name_;
    }
  }

  /* BLEClient overrides */

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

 protected:
  std::vector<BedJetClient *> children_;
  void dispatch_status_();
  void dispatch_state_(bool is_ready);

#ifdef USE_TIME
  /** Initializes time sync callbacks to support syncing current time to the BedJet. */
  void setup_time_();
  optional<time::RealTimeClock *> time_id_{};
#endif

  uint32_t timeout_{DEFAULT_STATUS_TIMEOUT};
  static const uint32_t MIN_NOTIFY_THROTTLE = 15000;
  static const uint32_t NOTIFY_WARN_THRESHOLD = 300000;
  static const uint32_t DEFAULT_STATUS_TIMEOUT = 900000;

  uint8_t set_notify_(bool enable);
  /** Send the `BedjetPacket` to the device. */
  uint8_t write_bedjet_packet_(BedjetPacket *pkt);
  void set_name_(const std::string &name) { this->name_ = name; }

  std::string name_;

  uint32_t last_notify_ = 0;
  inline void status_packet_ready_();
  bool force_refresh_ = false;
  bool processing_ = false;

  std::unique_ptr<BedjetCodec> codec_;

  bool discover_characteristics_();
  uint16_t char_handle_cmd_;
  uint16_t char_handle_name_;
  uint16_t char_handle_status_;
  uint16_t config_descr_status_;

  uint8_t write_notify_config_descriptor_(bool enable);
};

}  // namespace bedjet
}  // namespace esphome

#endif
