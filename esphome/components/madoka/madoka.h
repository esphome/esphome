#pragma once

#include <string>
#include <vector>
#include <map>

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/climate/climate.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

#define MAX_CHUNK_SIZE 20
static const uint8_t BLE_SEND_MAX_RETRIES = 5;

namespace esphome {
namespace madoka {

static const char *TAG = "madoka";

using chunk = std::vector<uint8_t>;
using message = std::vector<uint8_t>;

struct setpoint {
  uint16_t cooling;
  uint16_t heating;
};

struct fan_speed {
  uint8_t cooling;
  uint8_t heating;
};

struct sensor_reading {
  uint8_t indoor;
  uint8_t outdoor;
};

struct status {
  bool status;
  uint8_t mode;
};

namespace espbt = esphome::esp32_ble_tracker;

#define TO_ESPBTUUID(x) espbt::ESPBTUUID::from_raw(std::string(x))

#define MADOKA_SERVICE_UUID TO_ESPBTUUID("2141e110-213a-11e6-b67b-9e71128cae77")
#define NOTIFY_CHARACTERISTIC_UUID TO_ESPBTUUID("2141e111-213a-11e6-b67b-9e71128cae77")
#define WWR_CHARACTERISTIC_UUID TO_ESPBTUUID("2141e112-213a-11e6-b67b-9e71128cae77")

class Madoka : public climate::Climate, public esphome::ble_client::BLEClientNode, public PollingComponent {
 protected:
  std::map<uint8_t, chunk> chunks_ = {};
  uint16_t notify_handle_;
  uint16_t wwr_handle_;
  SemaphoreHandle_t query_semaphore_ = NULL;
  status cur_status_;

  std::vector<chunk> split_payload(message msg);
  message prepare_message(uint16_t cmd, message args);
  void query(uint16_t cmd, message args, int t_d);
  void parse_cb(message msg);
  void process_incoming_chunk(chunk chk);

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
    traits.set_supports_two_point_target_temperature(true);
    traits.set_supports_current_temperature(true);
    return traits;
  }
  void set_unit_of_measurement(const char *);
};

}  // namespace madoka
}  // namespace esphome

#endif
