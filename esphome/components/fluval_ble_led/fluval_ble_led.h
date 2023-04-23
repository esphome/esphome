#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/defines.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

#ifdef USE_ESP32

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

namespace esphome {
namespace fluval_ble_led {

namespace espbt = esphome::esp32_ble_tracker;

static const espbt::ESPBTUUID FLUVAL_SERVICE_UUID = espbt::ESPBTUUID::from_raw("00001000-0000-1000-8000-00805F9B34FB");
static const espbt::ESPBTUUID FLUVAL_CHARACTERISTIC_WRITE =
    espbt::ESPBTUUID::from_raw("00001001-0000-1000-8000-00805F9B34FB");  // notify, read, write, write no response
static const espbt::ESPBTUUID FLUVAL_CHARACTERISTIC_READ =
    espbt::ESPBTUUID::from_raw("00001002-0000-1000-8000-00805F9B34FB");  // notify, read
static const espbt::ESPBTUUID FLUVAL_CHARACTERISTIC_READ_REG =
    espbt::ESPBTUUID::from_raw("00001004-0000-1000-8000-00805F9B34FB");  //  read
static const espbt::ESPBTUUID FLUVAL_CHARACTERISTIC_WRITE_REG_ID =
    espbt::ESPBTUUID::from_raw("00001005-0000-1000-8000-00805F9B34FB");  // write

static const uint8_t MANUAL_MODE = 0x00;
static const uint8_t AUTO_MODE = 0x01;
static const uint8_t PRO_MODE = 0x02;

class FluvalBleLed;

class FluvalLedClient {
 public:
  FluvalBleLed *parent() { return this->parent_; }
  void set_parent(FluvalBleLed *parent) { this->parent_ = parent; }
  virtual void notify() {}

 protected:
  FluvalBleLed *parent_;
};

struct FluvalStatus {
  uint8_t led_on_off;
  uint8_t mode;
  float channel1;
  float channel2;
  float channel3;
  float channel4;
  float channel5;
};

class FluvalBleLed : public esphome::ble_client::BLEClientNode, public Component {
 public:
  void dump_config() override;
  void setup() override;

#ifdef USE_TIME
  void loop() override;
#endif

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void set_number_of_channels(int number_of_channels) { this->number_of_channels_ = number_of_channels; }
  FluvalStatus get_status() { return this->status_; }
  void update_channel(uint8_t channel, float value);

#ifdef USE_TIME
  void set_time(time::RealTimeClock *time) { this->time_ = time; }
#endif

#ifdef USE_TIME
  void sync_time();
#endif

  void set_led_state(bool state);
  void set_mode(uint8_t mode);

  void register_fluval_led_client(FluvalLedClient *fluval_led_client) {
    fluval_led_client->set_parent(this);
    this->clients_.push_back(fluval_led_client);
  }

 protected:
  std::string pkt_to_hex_(const uint8_t *data, uint16_t len);
  void decrypt_(const uint8_t *data, uint16_t len, uint8_t *decrypted);
  void encrypt_(std::vector<uint8_t> &data);
  void decode_(const uint8_t *data, uint16_t len);
  uint8_t get_crc_(const uint8_t *data, uint16_t len);
  void add_crc_to_vector_(std::vector<uint8_t> &data);

  void send_packet_(std::vector<uint8_t> &data);
  void notify_clients_();

  std::vector<uint8_t> received_packet_buffer_;

  // Status of the LED
  FluvalStatus status_;

  // Handlers to the different characterisitcs. Saved for further use
  int number_of_channels_{};
  uint8_t loop_counter_{255};
  uint8_t handshake_step_{0};
  uint8_t read_handle_;
  uint8_t write_handle_;
  uint8_t write_reg_id_handle_;
  uint8_t read_reg_handle_;

  // A vector of clients (switches, sensors...) to notify on a received
  // status update package
  std::vector<FluvalLedClient *> clients_;

#ifdef USE_TIME
  optional<time::RealTimeClock *> time_{};
  bool synchronize_device_time_{false};
#endif
};

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
