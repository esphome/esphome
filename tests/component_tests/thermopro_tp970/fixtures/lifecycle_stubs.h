#pragma once

#include <cstdint>
#include <functional>

using esp_gatt_if_t = int;
enum esp_gattc_cb_event_t {
  ESP_GATTC_DISCONNECT_EVT,
  ESP_GATTC_CLOSE_EVT,
  ESP_GATTC_SEARCH_CMPL_EVT,
  ESP_GATTC_REG_FOR_NOTIFY_EVT,
  ESP_GATTC_NOTIFY_EVT,
};
struct esp_ble_gattc_cb_param_t {
  struct {
    uint16_t handle;
    int status;
  } reg_for_notify{};
  struct {
    uint16_t handle;
    uint16_t conn_id;
    uint8_t *value;
    uint16_t value_len;
  } notify{};
};
static constexpr int ESP_OK = 0;
static constexpr int ESP_GATT_OK = 0;

namespace esphome {
inline uint32_t now_ms = 0;
inline uint32_t millis() { return now_ms; }
class Component {
 public:
  virtual void setup() {}
  virtual void dump_config() {}
  void set_interval(uint32_t, std::function<void()> callback) { interval = callback; }
  void status_set_warning() { warning = true; }
  void status_clear_warning() { warning = false; }
  std::function<void()> interval;
  bool warning{false};
};
namespace sensor {
class Sensor {
 public:
  float state{0};
  void publish_state(float value) { state = value; }
};
}  // namespace sensor
namespace esp32_ble {
struct ESPBTUUID {
  static ESPBTUUID from_raw(const char *) { return {}; }
  static ESPBTUUID from_uint16(uint16_t) { return {}; }
};
}  // namespace esp32_ble
namespace esp32_ble_tracker {
enum class ClientState { ESTABLISHED };
}  // namespace esp32_ble_tracker
namespace ble_client {
class BLEClient {
 public:
  struct Characteristic {
    uint16_t handle{7};
  } characteristic;
  Characteristic *get_characteristic(esp32_ble::ESPBTUUID, esp32_ble::ESPBTUUID) { return &characteristic; }
  int register_for_notify(uint16_t) { return ESP_OK; }
  uint16_t get_conn_id() { return 1; }
};
class BLEClientNode {
 public:
  virtual void gattc_event_handler(esp_gattc_cb_event_t, esp_gatt_if_t, esp_ble_gattc_cb_param_t *) {}
  BLEClient *parent() { return &client; }
  BLEClient client;
  esp32_ble_tracker::ClientState node_state{};
};
}  // namespace ble_client
}  // namespace esphome

#define SUB_SENSOR(name) \
  sensor::Sensor *name##_sensor_{nullptr}; \
  void set_##name##_sensor(sensor::Sensor *value) { name##_sensor_ = value; }
#define ESPHOME_LOG_TAG(...)
#define LOG_SENSOR(...)
