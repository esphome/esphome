#pragma once

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/time/real_time_clock.h"
#include "crypto.h"
#include <string>

namespace esphome {
namespace motion_blinds {

namespace espbt = esphome::esp32_ble_tracker;

static const auto MOTION_BLINDS_NOTIFY_CHARACTERISTIC_UUID =
    espbt::ESPBTUUID::from_raw("d973f2e1-b19e-11e2-9e96-0800200c9a66");
static const auto MOTION_BINDS_WRITE_CHARACTERISTIC_UUID =
    espbt::ESPBTUUID::from_raw("d973f2e2-b19e-11e2-9e96-0800200c9a66");
static const auto MOTION_BLINDS_NOTIFY_DESCRIPTOR = espbt::ESPBTUUID::from_raw("00002902-0000-1000-8000-00805f9b34fb");
static const auto MOTION_BLINDS_SERVICE_UUID = espbt::ESPBTUUID::from_raw("D973F2E0-B19E-11E2-9E96-0800200C9A66");

class MotionBlindsCommunication : public esphome::ble_client::BLEClientNode {
 public:
  MotionBlindsCommunication();
  virtual ~MotionBlindsCommunication() = default;
  void connect();
  void disconnect();
  void send_command(const std::string &command);
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  static std::string format_hex_num(size_t value, bool prefix);
  void set_time_id(time::RealTimeClock *time) { this->time_ = time; }

 protected:
  virtual void on_notify(const std::string &value) = 0;
  virtual void on_disconnected() = 0;
  virtual std::string get_logging_device_name() = 0;
  void send_set_time_();
  uint16_t write_char_handle_;
  uint16_t notify_char_handle_;
  MotionBlindsMessage message_;
  bool has_mtu_change_;
  time::RealTimeClock *time_;
};

}  // namespace motion_blinds
}  // namespace esphome

#endif  // USE_ESP32
