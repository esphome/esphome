#pragma once

#include <vector>
#include <array>
#include <span>

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/climate/climate.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome::daikin_madoka {

struct Status {
  bool status;
  uint8_t mode;
};

struct Query {
  uint16_t cmd;
  std::vector<uint8_t> args;
};

namespace espbt = esphome::esp32_ble_tracker;

static const uint8_t MAX_CHUNK_SIZE = 20;
static const uint8_t BLE_SEND_MAX_RETRIES = 5;

static const espbt::ESPBTUUID MADOKA_SERVICE_UUID = espbt::ESPBTUUID::from_raw("2141e110-213a-11e6-b67b-9e71128cae77");
static const espbt::ESPBTUUID NOTIFY_CHARACTERISTIC_UUID =
    espbt::ESPBTUUID::from_raw("2141e111-213a-11e6-b67b-9e71128cae77");
static const espbt::ESPBTUUID WWR_CHARACTERISTIC_UUID =
    espbt::ESPBTUUID::from_raw("2141e112-213a-11e6-b67b-9e71128cae77");

static const float MIN_TEMP = 16.0f;
static const float MAX_TEMP = 32.0f;

template<typename T> class VectorFIFO : std::vector<T> {
 protected:
  size_t pop_index_ = 0;

  void compact_() {
    this->erase(this->begin(), this->begin() + this->pop_index_);
    this->pop_index_ = 0;
  }

 public:
  bool empty() const { return this->pop_index_ >= this->size(); }

  T &front() { return this->at(this->pop_index_); }
  const T &front() const { return this->at(this->pop_index_); }

  void clear() {
    this->std::vector<T>::clear();
    this->pop_index_ = 0;
  }

  void pop() {
    if (this->empty()) {
      return;
    }
    this->pop_index_++;
    if (this->pop_index_ >= this->size() / 2) {
      this->compact_();
    }
  }

  void push(const T &value) { this->std::vector<T>::push_back(value); }
  void push(T &&value) { this->std::vector<T>::push_back(std::move(value)); }
  template<typename... Args> void emplace(Args &&...args) {
    this->std::vector<T>::emplace_back(std::forward<Args>(args)...);
  }
};

struct Chunk {
  std::array<uint8_t, MAX_CHUNK_SIZE> data = {};
  size_t length = 0;
};

class DaikinMadoka : public climate::Climate, public esphome::ble_client::BLEClientNode, public PollingComponent {
 protected:
  bool should_update_ = false;
  VectorFIFO<Chunk> received_chunks_ = {};
  struct {
    std::vector<uint8_t> data = {};
    size_t expected_chunk_id = 0;
  } partial_incoming_message_ = {};
  VectorFIFO<Query> query_queue_ = {};
  bool pending_message_ = false;
  uint16_t notify_handle_{0};
  uint16_t wwr_handle_{0};
  Status cur_status_{
      .status = false,
      .mode = 0,
  };

  esp_err_t send_message_(std::span<uint8_t> chk);
  void query_(uint16_t cmd, std::vector<uint8_t> &args);
  void parse_cb_(std::span<const uint8_t> msg);
  void process_incoming_chunk_(const Chunk &chk);

  void control(const climate::ClimateCall &call) override;

 public:
  void setup() override;
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
