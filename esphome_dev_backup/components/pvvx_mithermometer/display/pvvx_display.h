#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/ble_client/ble_client.h"

#include <cinttypes>

#ifdef USE_ESP32
#include <esp_gattc_api.h>
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace pvvx_mithermometer {

class PVVXDisplay;

/// Possible units for the big number
enum UNIT {
  UNIT_NONE = 0,  ///< do not show a unit
  UNIT_DEG_GHE,   ///< show "°Г"
  UNIT_MINUS,     ///< show " -"
  UNIT_DEG_F,     ///< show "°F"
  UNIT_LOWDASH,   ///< show " _"
  UNIT_DEG_C,     ///< show "°C"
  UNIT_LINES,     ///< show " ="
  UNIT_DEG_E,     ///< show "°E"
};

using pvvx_writer_t = std::function<void(PVVXDisplay &)>;

class PVVXDisplay : public ble_client::BLEClientNode, public PollingComponent {
 public:
  void set_writer(pvvx_writer_t &&writer) { this->writer_ = writer; }
  void set_auto_clear(bool auto_clear_enabled) { this->auto_clear_enabled_ = auto_clear_enabled; }
  void set_disconnect_delay(uint32_t ms) { this->disconnect_delay_ms_ = ms; }

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  void update() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  /// Set validity period of the display information in seconds (1..65535)
  void set_validity_period(uint16_t validity_period) { this->validity_period_ = validity_period; }
  /// Clear the screen
  void clear() {
    this->bignum_ = 0;
    this->smallnum_ = 0;
    this->cfg_ = 0;
  }
  /**
   * Print the big number
   *
   * Valid values are from -99.5 to 1999.5. Smaller values are displayed as Lo, higher as Hi.
   * It will printed as it fits in the screen.
   */
  void print_bignum(float bignum) { this->bignum_ = bignum * 10; }
  /**
   * Print the small number
   *
   * Valid values are from -9 to 99. Smaller values are displayed as Lo, higher as Hi.
   */
  void print_smallnum(float smallnum) { this->smallnum_ = smallnum; }
  /**
   * Print a happy face
   *
   * Can be combined with print_sad()  print_bracket().
   * Possible ouputs are:
   *
   * @verbatim
   * bracket sad happy
   *       0   0     0     "     "
   *       0   0     1     " ^_^ "
   *       0   1     0     " -∧- "
   *       0   1     1     " Δ△Δ "
   *       1   0     0     "(   )"
   *       1   0     1     "(^_^)"
   *       1   1     0     "(-∧-)"
   *       1   1     1     "(Δ△Δ)"
   * @endverbatim
   */
  void print_happy(bool happy = true) { this->setcfgbit_(0, happy); }
  /// Print a sad face
  void print_sad(bool sad = true) { this->setcfgbit_(1, sad); }
  /// Print round brackets around the face
  void print_bracket(bool bracket = true) { this->setcfgbit_(2, bracket); }
  /// Print percent sign at small number
  void print_percent(bool percent = true) { this->setcfgbit_(3, percent); }
  /// Print battery sign
  void print_battery(bool battery = true) { this->setcfgbit_(4, battery); }
  /// Print unit of big number
  void print_unit(UNIT unit) { this->cfg_ = (this->cfg_ & 0x1F) | ((unit & 0x7) << 5); }

  void display();

#ifdef USE_TIME
  void set_time(time::RealTimeClock *time) { this->time_ = time; };
#endif

 protected:
  bool auto_clear_enabled_{true};
  uint32_t disconnect_delay_ms_ = 5000;
  uint16_t validity_period_ = 300;
  uint16_t bignum_ = 0;
  uint16_t smallnum_ = 0;
  uint8_t cfg_ = 0;

  void setcfgbit_(uint8_t bit, bool value);
  void send_to_setup_char_(uint8_t *blk, size_t size);
  void delayed_disconnect_();
#ifdef USE_TIME
  void sync_time_();
  time::RealTimeClock *time_{nullptr};
#endif
  uint16_t char_handle_ = 0;
  bool connection_established_ = false;

  esp32_ble_tracker::ESPBTUUID service_uuid_ =
      esp32_ble_tracker::ESPBTUUID::from_raw("00001f10-0000-1000-8000-00805f9b34fb");
  esp32_ble_tracker::ESPBTUUID char_uuid_ =
      esp32_ble_tracker::ESPBTUUID::from_raw("00001f1f-0000-1000-8000-00805f9b34fb");

  optional<pvvx_writer_t> writer_{};
};

}  // namespace pvvx_mithermometer
}  // namespace esphome

#endif
