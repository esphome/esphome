#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "ecocomfort2_child.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome::ecocomfort2 {

namespace espbt = esphome::esp32_ble_tracker;

// C_OPER byte 0: operating mode
static constexpr uint8_t OPER_OFF = 0x00;
static constexpr uint8_t OPER_IN = 0x01;
static constexpr uint8_t OPER_OUT = 0x02;
static constexpr uint8_t OPER_IN_OUT = 0x03;
static constexpr uint8_t OPER_SENSOR_OR_AUTO = 0x04;

// Speed byte: speed in the low nibble, flags in the high nibble.
// C_OPER carries FLAG_AUTO; the unit reports FLAG_BOOST in byte 1 of C_STATE.
static constexpr uint8_t SPEED_OFF = 0x00;
static constexpr uint8_t SPEED_SLEEP = 0x01;
static constexpr uint8_t SPEED_VEL1 = 0x02;
static constexpr uint8_t SPEED_VEL2 = 0x03;
static constexpr uint8_t SPEED_VEL3 = 0x04;
static constexpr uint8_t SPEED_MASK = 0x0F;
static constexpr uint8_t FLAG_AUTO = 0x10;
static constexpr uint8_t FLAG_BOOST = 0x40;

/** Hub component for Ecocomfort 2.0 VMC via BLE. */
class Ecocomfort2Hub : public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

#ifdef ECOCOMFORT2_CHILD_COUNT
  void register_child(Ecocomfort2Client *obj);
#endif
#ifdef USE_SENSOR
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_humidity_sensor(sensor::Sensor *s) { this->humidity_sensor_ = s; }
  void set_voc_sensor(sensor::Sensor *s) { this->voc_sensor_ = s; }
  void set_direction_sensor(sensor::Sensor *s) { this->direction_sensor_ = s; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_connected_binary_sensor(binary_sensor::BinarySensor *s) { this->connected_binary_sensor_ = s; }
  void set_boost_binary_sensor(binary_sensor::BinarySensor *s) { this->boost_binary_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_firmware_text_sensor(text_sensor::TextSensor *s) { this->firmware_text_sensor_ = s; }
#endif
#ifdef USE_TIME
  /// Keeps the unit's clock in step with this time source while connected.
  void set_time(time::RealTimeClock *time) { this->time_ = time; }
#endif

  /// True once the link is established and paired.
  bool is_ready() const { return this->node_state == espbt::ClientState::ESTABLISHED && this->parent_->is_paired(); }

  /// Write C_OPER (mode byte, speed/flags byte) and read it back shortly after.
  void write_oper(uint8_t mode, uint8_t speed);

  /// Raw C_OPER values from the last readback.
  uint8_t get_mode() const { return this->mode_; }
  uint8_t get_speed() const { return this->speed_; }

 protected:
  static constexpr uint32_t AUTH_TIMEOUT_ID = 0;
  static constexpr uint32_t READBACK_TIMEOUT_ID = 1;
  static constexpr uint32_t AUTH_TIMEOUT_MS = 10000;
  static constexpr uint32_t READBACK_DELAY_MS = 300;

  void on_paired_();
  void set_connected_(bool connected);
  void publish_unknown_();
  void read_char_(uint16_t handle);
  bool read_ok_(const esp_ble_gattc_cb_param_t *param);
  void on_oper_(const uint8_t *data, uint16_t len);
  void on_state_(const uint8_t *data, uint16_t len);
  void on_info_(const uint8_t *data, uint16_t len);
#ifdef USE_TIME
  void sync_clock_();
#endif

#ifdef ECOCOMFORT2_CHILD_COUNT
  StaticVector<Ecocomfort2Client *, ECOCOMFORT2_CHILD_COUNT> children_;
#endif
#ifdef USE_SENSOR
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *voc_sensor_{nullptr};
  sensor::Sensor *direction_sensor_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *connected_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *boost_binary_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *firmware_text_sensor_{nullptr};
#endif
#ifdef USE_TIME
  time::RealTimeClock *time_{nullptr};
#endif
  uint16_t char_handle_oper_{0};
  uint16_t char_handle_state_{0};
  uint16_t char_handle_info_{0};
  uint16_t char_handle_clock_{0};
  uint8_t mode_{OPER_OFF};
  uint8_t speed_{SPEED_OFF};
};

}  // namespace esphome::ecocomfort2

#endif  // USE_ESP32
