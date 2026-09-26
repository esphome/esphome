#pragma once

#include <array>
#include <span>

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome::daikin_madoka {

namespace espbt = esphome::esp32_ble_tracker;

// Wire protocol and buffer sizing
static const uint8_t MAX_CHUNK_SIZE = 20;
static const uint8_t BLE_SEND_MAX_RETRIES = 5;
// A message's leading length byte is a uint8_t, so a full message is at most 255 bytes
static const uint16_t MAX_MESSAGE_SIZE = 255;
// Longest args payload is a two-point setpoint: {0x20,0x02,hi,lo,0x21,0x02,hi,lo}
static const uint8_t MAX_QUERY_ARGS = 8;
// Chunks are fully drained every loop; a 255-byte message spans at most 14 chunks
static const uint8_t RECEIVED_CHUNKS_QUEUE_SIZE = 16;
// Deepest backlog is one control() burst (4) plus a full update() poll (5)
static const uint8_t QUERY_QUEUE_SIZE = 16;

// Climate temperature limits
static const float MIN_TEMP = 16.0f;
static const float MAX_TEMP = 32.0f;

// BLE service and characteristic UUIDs
static const espbt::ESPBTUUID MADOKA_SERVICE_UUID = espbt::ESPBTUUID::from_raw("2141e110-213a-11e6-b67b-9e71128cae77");
static const espbt::ESPBTUUID NOTIFY_CHARACTERISTIC_UUID =
    espbt::ESPBTUUID::from_raw("2141e111-213a-11e6-b67b-9e71128cae77");
static const espbt::ESPBTUUID WWR_CHARACTERISTIC_UUID =
    espbt::ESPBTUUID::from_raw("2141e112-213a-11e6-b67b-9e71128cae77");

struct Status {
  bool status;
  uint8_t mode;
};

struct Query {
  uint16_t cmd;
  StaticVector<uint8_t, MAX_QUERY_ARGS> args;
};

struct Chunk {
  std::array<uint8_t, MAX_CHUNK_SIZE> data = {};
  size_t length = 0;
};

class DaikinMadoka : public climate::Climate, public esphome::ble_client::BLEClientNode, public PollingComponent {
 protected:
  // Members are ordered by descending alignment to minimise padding.
  StaticRingBuffer<Chunk, RECEIVED_CHUNKS_QUEUE_SIZE> received_chunks_;
  StaticRingBuffer<Query, QUERY_QUEUE_SIZE> query_queue_;
  struct {
    StaticVector<uint8_t, MAX_MESSAGE_SIZE> data;
    size_t expected_chunk_id = 0;
  } partial_incoming_message_;
  uint16_t notify_handle_{0};
  uint16_t wwr_handle_{0};
  Status cur_status_{
      .status = false,
      .mode = 0,
  };
  bool should_update_ = false;
  bool pending_message_ = false;

  esp_err_t send_message_(std::span<uint8_t> chk);
  void enqueue_query_(uint16_t cmd, StaticVector<uint8_t, MAX_QUERY_ARGS> args);
  void query_(uint16_t cmd, std::span<const uint8_t> args);
  void parse_cb_(std::span<const uint8_t> msg);
  void process_incoming_chunk_(const Chunk &chk);

  void control(const climate::ClimateCall &call) override;

 public:
  void loop() override;
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
};

}  // namespace esphome::daikin_madoka

#endif
