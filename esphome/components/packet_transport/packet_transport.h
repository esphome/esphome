#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <vector>

/**
 * Providing packet encoding functions for exchanging data with a remote host.
 *
 * A transport is required to send the data; this is provided by a child class.
 * The child class should implement the virtual functions send_packet_ and get_max_packet_size_.
 * On receipt of a data packet, it should call `this->process_()` with the data.
 */

namespace esphome {
namespace packet_transport {

static const uint8_t MAX_PROVIDERS = 8;
static const uint8_t MAX_REMOTE_SENSORS = 16;
static const uint8_t MAX_REMOTE_BINARY_SENSORS = 16;
static const uint8_t MAX_ENCRYPTION_KEY_SIZE = 32;
static const uint8_t MAX_PING_KEYS = 4;
static const uint8_t MAX_PACKET_BUFFER_SIZE = 255;

struct Provider {
  uint8_t encryption_key[MAX_ENCRYPTION_KEY_SIZE];
  uint8_t key_length;
  const char *name;
  uint32_t last_code[2];
  uint32_t last_key_response_time;
  bool active;
#ifdef USE_STATUS_SENSOR
  binary_sensor::BinarySensor *status_sensor{nullptr};
#endif
};

#ifdef USE_SENSOR
struct Sensor {
  sensor::Sensor *sensor;
  const char *id;
  bool updated;
};

struct RemoteSensor {
  sensor::Sensor *sensor;
  const char *sensor_id;
  uint8_t provider_index;
  bool active;
};
#endif

#ifdef USE_BINARY_SENSOR
struct BinarySensor {
  binary_sensor::BinarySensor *sensor;
  const char *id;
  bool updated;
};

struct RemoteBinarySensor {
  binary_sensor::BinarySensor *sensor;
  const char *sensor_id;
  uint8_t provider_index;
  bool active;
};
#endif

struct PingKey {
  const char *name;
  uint32_t key;
  bool active;
};

class PacketTransport : public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

#ifdef USE_SENSOR
  void add_sensor(const char *id, sensor::Sensor *sensor) {
    Sensor st{sensor, id, true};
    this->sensors_.push_back(st);
  }
  void add_remote_sensor(const char *hostname, const char *remote_id, sensor::Sensor *sensor);
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(const char *id, binary_sensor::BinarySensor *sensor) {
    BinarySensor st{sensor, id, true};
    this->binary_sensors_.push_back(st);
  }

  void add_remote_binary_sensor(const char *hostname, const char *remote_id, binary_sensor::BinarySensor *sensor);
#endif

  void add_provider(const char *hostname);

  void set_encryption_key(const uint8_t *key, uint8_t key_length);
  void set_rolling_code_enable(bool enable) { this->rolling_code_enable_ = enable; }
  void set_ping_pong_enable(bool enable) { this->ping_pong_enable_ = enable; }
  void set_ping_pong_recycle_time(uint32_t recycle_time) { this->ping_pong_recyle_time_ = recycle_time; }
  void set_provider_encryption(const char *name, const uint8_t *key, uint8_t key_length);
#ifdef USE_STATUS_SENSOR
  void set_provider_status_sensor(const char *name, binary_sensor::BinarySensor *sensor);
#endif
  void set_platform_name(const char *name) { this->platform_name_ = name; }

  // Helper method for compatibility with code checking if providers exist
  bool has_providers() const { return this->provider_count_ > 0; }

  // Public access to providers for child classes (backward compatibility)
  Provider providers_[MAX_PROVIDERS];
  uint8_t provider_count_{};

 protected:
  // child classes must implement this
  virtual void send_packet(const std::vector<uint8_t> &buf) const = 0;
  virtual size_t get_max_packet_size() = 0;
  virtual bool should_send() { return true; }

  // to be called by child classes when a data packet is received.
  void process_(const std::vector<uint8_t> &data);
  void send_data_(bool all);
  void flush_();
  void add_data_(uint8_t key, const char *id, float data);
  void add_data_(uint8_t key, const char *id, uint32_t data);
  void increment_code_();
  void add_binary_data_(uint8_t key, const char *id, bool data);
  void init_data_();
  int8_t find_provider_(const char *name);
  int8_t find_or_create_provider_(const char *name);
#ifdef USE_SENSOR
  int8_t find_remote_sensor_(uint8_t provider_index, const char *sensor_id);
#endif
#ifdef USE_BINARY_SENSOR
  int8_t find_remote_binary_sensor_(uint8_t provider_index, const char *sensor_id);
#endif

  bool updated_{};
  uint32_t ping_key_{};
  uint32_t rolling_code_[2]{};
  bool rolling_code_enable_{};
  bool ping_pong_enable_{};
  uint32_t ping_pong_recyle_time_{};
  uint32_t last_key_time_{};
  bool resend_ping_key_{};
  bool resend_data_{};
  const char *name_{};
  ESPPreferenceObject pref_{};

  uint8_t encryption_key_[MAX_ENCRYPTION_KEY_SIZE];
  uint8_t encryption_key_length_{};

#ifdef USE_SENSOR
  std::vector<Sensor> sensors_{};
  RemoteSensor remote_sensors_[MAX_REMOTE_SENSORS];
  uint8_t remote_sensor_count_{};
#endif
#ifdef USE_BINARY_SENSOR
  std::vector<BinarySensor> binary_sensors_{};
  RemoteBinarySensor remote_binary_sensors_[MAX_REMOTE_BINARY_SENSORS];
  uint8_t remote_binary_sensor_count_{};
#endif

  uint8_t ping_header_[MAX_PACKET_BUFFER_SIZE];
  size_t ping_header_len_{};
  uint8_t header_[MAX_PACKET_BUFFER_SIZE];
  size_t header_len_{};
  uint8_t data_[MAX_PACKET_BUFFER_SIZE];
  size_t data_len_{};
  PingKey ping_keys_[MAX_PING_KEYS];
  uint8_t ping_key_count_{};
  const char *platform_name_{""};
  void add_key_(const char *name, uint32_t key);
  void send_ping_pong_request_();

  inline bool is_encrypted_() { return this->encryption_key_length_ > 0; }
};

}  // namespace packet_transport
}  // namespace esphome
