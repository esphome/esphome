#pragma once

#ifdef USE_ESP32

#include <esp_gattc_api.h>
#include <algorithm>
#include <iterator>
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace radon_eye_rd200 {

static const char *const SERVICE_UUID = "00001523-1212-efde-1523-785feabcd123";
static const char *const WRITE_CHARACTERISTIC_UUID = "00001524-1212-efde-1523-785feabcd123";
static const char *const READ_CHARACTERISTIC_UUID = "00001525-1212-efde-1523-785feabcd123";

static const char *const SERVICE_UUID_V2 = "00001523-0000-1000-8000-00805f9b34fb";
static const char *const COMMAND_CHARACTERISTIC_UUID_V2 = "00001524-0000-1000-8000-00805f9b34fb";
static const char *const STATUS_CHARACTERISTIC_UUID_V2 = "00001525-0000-1000-8000-00805f9b34fb";
static const char *const HISTROY_CHARACTERISTIC_UUID_V2 = "00001526-0000-1000-8000-00805f9b34fb";

/* Thanks to sormy https://github.com/esphome/issues/issues/3371#issuecomment-1851004514 */
typedef struct {
  /* 00 */ uint8_t command;  // supposed to be 0x40
  /* 01 */ uint8_t size;     // supposed to be 0x42
  /* 02 */ char serial_part2[6];
  /* 08 */ char serial_part1[3];
  /* 11 */ char serial_part3[4];
  /* 15 */ uint8_t __unk1[1];
  /* 16 */ char model[6];
  /* 22 */ char version[6];
  /* 28 */ uint8_t __unk2[5];
  /* 33 */ uint16_t latest_bq_m3;
  /* 35 */ uint16_t day_avg_bq_m3;
  /* 37 */ uint16_t month_avg_bq_m3;
  /* 39 */ uint8_t __unk3[12];
  /* 51 */ uint16_t peak_bq_m3;
  /* 53 */ uint8_t __unk4[16]; // Length 15 or 16? Maybe because of version 3
} __attribute__((packed)) radoneye_value_response_t;

typedef struct {
  /* 00 */ uint8_t command;         // supposed to be 0x41
  /* 01 */ uint8_t response_count;  // total number of responses (each response is separate event)
  /* 02 */ uint8_t response_no;     // number of response
  /* 03 */ uint8_t value_count;     // within response
  /* 04 */ uint16_t values_bq_m3[250];
} __attribute__((packed)) radoneye_history_response_t;
/* Thanks to sormy https://github.com/esphome/issues/issues/3371#issuecomment-1851004514 */


class RadonEyeRD200 : public PollingComponent, public ble_client::BLEClientNode {
 public:
  RadonEyeRD200();

  void dump_config() override;
  void update() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

 void set_version(int version) { radon_version_ = version; }
  void set_radon(sensor::Sensor *radon) { radon_sensor_ = radon; }
  void set_radon_day_avg(sensor::Sensor *radon_day_avg) { radon_day_avg_ = radon_day_avg; }
  void set_radon_long_term(sensor::Sensor *radon_long_term) { radon_long_term_sensor_ = radon_long_term; }
  void set_radon_peak(sensor::Sensor *radon_peak) { radon_peak_ = radon_peak; }

 protected:
  void handle_status_response_(const uint8_t *response, uint16_t length);

  void handle_history_response_(const uint8_t *response, uint16_t length);

  bool is_valid_radon_value_(float radon);

  void read_sensors_(uint8_t *value, uint16_t value_len);
  void write_query_message_();
  void write_status_query_message_();
  void write_history_query_message_();
  void request_read_values_();

  int radon_version_{1};
  sensor::Sensor *radon_sensor_{nullptr};
  sensor::Sensor *radon_day_avg_{nullptr};
  sensor::Sensor *radon_long_term_sensor_{nullptr};
  sensor::Sensor *radon_peak_{nullptr};

  uint16_t read_handle_;
  uint16_t write_handle_;
  uint16_t command_handle_;
  uint16_t status_handle_;
  esp32_ble_tracker::ClientState status_handle_state_;
  uint16_t history_handle_;
  esp32_ble_tracker::ClientState history_handle_state_;
  esp32_ble_tracker::ESPBTUUID service_uuid_;
  esp32_ble_tracker::ESPBTUUID service_uuid_v2_;
  esp32_ble_tracker::ESPBTUUID sensors_write_characteristic_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_read_characteristic_uuid_;
  esp32_ble_tracker::ESPBTUUID sensors_command_characteristic_uuid_v2_;
  esp32_ble_tracker::ESPBTUUID sensors_status_characteristic_uuid_v2_;
  esp32_ble_tracker::ESPBTUUID sensors_history_characteristic_uuid_v2_;

  union RadonValue {
    char chars[4];
    float number;
  };
};

}  // namespace radon_eye_rd200
}  // namespace esphome

#endif  // USE_ESP32
