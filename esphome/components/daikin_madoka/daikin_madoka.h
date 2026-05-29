#pragma once

#include <vector>
#include <queue>
#include <map>

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/climate/climate.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

static const uint8_t MAX_CHUNK_SIZE = 20;
static const uint8_t BLE_SEND_MAX_RETRIES = 5;

namespace esphome::daikin_madoka {

static const char *const TAG = "daikin_madoka";

struct Setpoint {
  uint16_t cooling;
  uint16_t heating;
};

struct FanSpeed {
  uint8_t cooling;
  uint8_t heating;
};

struct SensorReading {
  uint8_t indoor;
  uint8_t outdoor;
};

struct Status {
  bool status;
  uint8_t mode;
};

namespace espbt = esphome::esp32_ble_tracker;

static const espbt::ESPBTUUID MADOKA_SERVICE_UUID = espbt::ESPBTUUID::from_raw("2141e110-213a-11e6-b67b-9e71128cae77");
static const espbt::ESPBTUUID NOTIFY_CHARACTERISTIC_UUID =
    espbt::ESPBTUUID::from_raw("2141e111-213a-11e6-b67b-9e71128cae77");
static const espbt::ESPBTUUID WWR_CHARACTERISTIC_UUID =
    espbt::ESPBTUUID::from_raw("2141e112-213a-11e6-b67b-9e71128cae77");

class DaikinMadoka : public climate::Climate, public esphome::ble_client::BLEClientNode, public PollingComponent {
 protected:
  bool should_update_ = false;
  std::queue<std::vector<uint8_t>> received_chunks_ = {};
  std::map<uint8_t, std::vector<uint8_t>> pending_chunks_ = {};
  uint16_t notify_handle_;
  uint16_t wwr_handle_;
  SemaphoreHandle_t receive_semaphore_ = nullptr;
  Status cur_status_;

  std::vector<std::vector<uint8_t>> split_payload_(std::vector<uint8_t> msg);
  std::vector<uint8_t> prepare_message_(uint16_t cmd, std::vector<uint8_t> args);
  void query_(uint16_t cmd, std::vector<uint8_t> args, int t_d);
  void parse_cb_(std::vector<uint8_t> msg);
  void process_incoming_chunk_(std::vector<uint8_t> chk);

  void control(const climate::ClimateCall &call) override;

 public:
  void setup() override;
  void loop() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  climate::ClimateTraits traits() override {
    auto traits = climate::ClimateTraits();
    traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_HEAT_COOL,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_DRY,
    });
    traits.set_supported_fan_modes({
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_MEDIUM,
        climate::CLIMATE_FAN_HIGH,
        climate::CLIMATE_FAN_AUTO,
    });
    traits.set_visual_min_temperature(16);
    traits.set_visual_max_temperature(32);
    traits.set_visual_temperature_step(1);
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                             climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    return traits;
  }
};

}  // namespace esphome::daikin_madoka

#endif
