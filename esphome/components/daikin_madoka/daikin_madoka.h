#pragma once

#ifdef USE_ESP32

#include <array>
#include <span>

#include <esp_gattc_api.h>

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "daikin_madoka_protocol.h"

namespace esphome::daikin_madoka {

namespace espbt = esphome::esp32_ble_tracker;

// Deepest backlog is one control() burst (4) plus one update() poll (5); update() never queues a second poll
static constexpr uint8_t QUERY_QUEUE_SIZE = 10;

// BLE service and characteristic UUIDs
static const espbt::ESPBTUUID MADOKA_SERVICE_UUID = espbt::ESPBTUUID::from_raw("2141e110-213a-11e6-b67b-9e71128cae77");
static const espbt::ESPBTUUID NOTIFY_CHARACTERISTIC_UUID =
    espbt::ESPBTUUID::from_raw("2141e111-213a-11e6-b67b-9e71128cae77");
static const espbt::ESPBTUUID WWR_CHARACTERISTIC_UUID =
    espbt::ESPBTUUID::from_raw("2141e112-213a-11e6-b67b-9e71128cae77");

struct Query {
  uint16_t cmd;
  uint8_t len;
  std::array<uint8_t, MAX_QUERY_ARGS> args;
};

class DaikinMadoka : public climate::Climate, public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void dump_config() override;
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
    traits.set_visual_min_temperature(MIN_TEMP);
    traits.set_visual_max_temperature(MAX_TEMP);
    traits.set_visual_temperature_step(1);
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                             climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
    return traits;
  }

 protected:
  void control(const climate::ClimateCall &call) override;

  void enqueue_query_(uint16_t cmd, std::span<const uint8_t> args);
  void send_next_query_();
  bool write_query_(const Query &query);
  void process_incoming_chunk_(std::span<const uint8_t> chunk);
  void parse_cb_(std::span<const uint8_t> msg);
  void update_mode_from_status_();
  void fail_connection_(const LogString *reason);

  StaticRingBuffer<Query, QUERY_QUEUE_SIZE> query_queue_;
  MessageAssembler assembler_;
  uint16_t notify_handle_{0};
  uint16_t wwr_handle_{0};
  uint8_t device_mode_{0};
  // Responses still expected from the queries sent; the state is published when it reaches zero
  uint8_t pending_responses_{0};
  bool power_on_{false};
  // True while the query timeout is armed to pace the next send
  bool query_cooldown_{false};

  static constexpr uint32_t QUERY_TIMEOUT_ID = 0;
};

}  // namespace esphome::daikin_madoka

#endif
