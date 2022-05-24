#pragma once

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "bedjet_base.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome {
namespace bedjet {

namespace espbt = esphome::esp32_ble_tracker;

static const espbt::ESPBTUUID BEDJET_SERVICE_UUID = espbt::ESPBTUUID::from_raw("00001000-bed0-0080-aa55-4265644a6574");
static const espbt::ESPBTUUID BEDJET_STATUS_UUID = espbt::ESPBTUUID::from_raw("00002000-bed0-0080-aa55-4265644a6574");
static const espbt::ESPBTUUID BEDJET_COMMAND_UUID = espbt::ESPBTUUID::from_raw("00002004-bed0-0080-aa55-4265644a6574");
static const espbt::ESPBTUUID BEDJET_NAME_UUID = espbt::ESPBTUUID::from_raw("00002001-bed0-0080-aa55-4265644a6574");

class Bedjet : public climate::Climate, public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
#endif
  void set_status_timeout(uint32_t timeout) { this->timeout_ = timeout; }

  /** Attempts to check for and apply firmware updates. */
  void upgrade_firmware();

  climate::ClimateTraits traits() override {
    auto traits = climate::ClimateTraits();
    traits.set_supports_action(true);
    traits.set_supports_current_temperature(true);
    traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_HEAT,
        // climate::CLIMATE_MODE_TURBO // Not supported by Climate: see presets instead
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_DRY,
    });

    // It would be better if we had a slider for the fan modes.
    traits.set_supported_custom_fan_modes(BEDJET_FAN_STEP_NAMES_SET);
    traits.set_supported_presets({
        // If we support NONE, then have to decide what happens if the user switches to it (turn off?)
        // climate::CLIMATE_PRESET_NONE,
        // Climate doesn't have a "TURBO" mode, but we can use the BOOST preset instead.
        climate::CLIMATE_PRESET_BOOST,
    });
    traits.set_supported_custom_presets({
        // We could fetch biodata from bedjet and set these names that way.
        // But then we have to invert the lookup in order to send the right preset.
        // For now, we can leave them as M1-3 to match the remote buttons.
        // EXT HT added to match remote button.
        "EXT HT",
        "M1",
        "M2",
        "M3",
    });
    traits.set_visual_min_temperature(19.0);
    traits.set_visual_max_temperature(43.0);
    traits.set_visual_temperature_step(1.0);
    return traits;
  }

 protected:
  void control(const climate::ClimateCall &call) override;

#ifdef USE_TIME
  void setup_time_();
  void send_local_time_();
  optional<time::RealTimeClock *> time_id_{};
#endif

  uint32_t timeout_{DEFAULT_STATUS_TIMEOUT};

  static const uint32_t MIN_NOTIFY_THROTTLE = 5000;
  static const uint32_t NOTIFY_WARN_THRESHOLD = 300000;
  static const uint32_t DEFAULT_STATUS_TIMEOUT = 900000;

  uint8_t set_notify_(bool enable);
  uint8_t write_bedjet_packet_(BedjetPacket *pkt);
  void reset_state_();
  bool update_status_();

  bool is_valid_() {
    // FIXME: find a better way to check this?
    return !std::isnan(this->current_temperature) && !std::isnan(this->target_temperature) &&
           this->current_temperature > 1 && this->target_temperature > 1;
  }

  uint32_t last_notify_ = 0;
  bool force_refresh_ = false;

  std::unique_ptr<BedjetCodec> codec_;
  uint16_t char_handle_cmd_;
  uint16_t char_handle_name_;
  uint16_t char_handle_status_;
  uint16_t config_descr_status_;

  uint8_t write_notify_config_descriptor_(bool enable);
};

}  // namespace bedjet
}  // namespace esphome

#endif
