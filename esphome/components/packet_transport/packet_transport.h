#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#
#include <vector>
#include <map>

/**
 * Providing packet encoding functions for exchanging data with a remote host.
 *
 * A transport is required to send the data; this is provided by a child class.
 * The child class should implement the virtual functions send_packet_ and get_max_packet_size_.
 * On receipt of a data packet, it should call `this->process_()` with the data.
 */

namespace esphome {
namespace packet_transport {

struct Provider {
  std::vector<uint8_t> encryption_key;
  const char *name;
  uint32_t last_code[2];
};

#ifdef USE_SENSOR
struct Sensor {
  sensor::Sensor *sensor;
  const char *id;
  bool updated;
};
#endif
#ifdef USE_BINARY_SENSOR
struct BinarySensor {
  binary_sensor::BinarySensor *sensor;
  const char *id;
  bool updated;
};
#endif

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
  void add_remote_sensor(const char *hostname, const char *remote_id, sensor::Sensor *sensor) {
    this->add_provider(hostname);
    this->remote_sensors_[hostname][remote_id] = sensor;
  }
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(const char *id, binary_sensor::BinarySensor *sensor) {
    BinarySensor st{sensor, id, true};
    this->binary_sensors_.push_back(st);
  }

  void add_remote_binary_sensor(const char *hostname, const char *remote_id, binary_sensor::BinarySensor *sensor) {
    this->add_provider(hostname);
    this->remote_binary_sensors_[hostname][remote_id] = sensor;
  }
#endif

  void add_provider(const char *hostname) {
    if (this->providers_.count(hostname) == 0) {
      Provider provider;
      provider.encryption_key = std::vector<uint8_t>{};
      provider.last_code[0] = 0;
      provider.last_code[1] = 0;
      provider.name = hostname;
      this->providers_[hostname] = provider;
#ifdef USE_SENSOR
      this->remote_sensors_[hostname] = std::map<std::string, sensor::Sensor *>();
#endif
#ifdef USE_BINARY_SENSOR
      this->remote_binary_sensors_[hostname] = std::map<std::string, binary_sensor::BinarySensor *>();
#endif
    }
  }

  void set_encryption_key(std::vector<uint8_t> key) { this->encryption_key_ = std::move(key); }
  void set_rolling_code_enable(bool enable) { this->rolling_code_enable_ = enable; }
  void set_ping_pong_enable(bool enable) { this->ping_pong_enable_ = enable; }
  void set_ping_pong_recycle_time(uint32_t recycle_time) { this->ping_pong_recyle_time_ = recycle_time; }
  void set_provider_encryption(const char *name, std::vector<uint8_t> key) {
    this->providers_[name].encryption_key = std::move(key);
  }
  void set_platform_name(const char *name) { this->platform_name_ = name; }

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

  std::vector<uint8_t> encryption_key_{};

#ifdef USE_SENSOR
  std::vector<Sensor> sensors_{};
  std::map<std::string, std::map<std::string, sensor::Sensor *>> remote_sensors_{};
#endif
#ifdef USE_BINARY_SENSOR
  std::vector<BinarySensor> binary_sensors_{};
  std::map<std::string, std::map<std::string, binary_sensor::BinarySensor *>> remote_binary_sensors_{};
#endif

  std::map<std::string, Provider> providers_{};
  std::vector<uint8_t> ping_header_{};
  std::vector<uint8_t> header_{};
  std::vector<uint8_t> data_{};
  std::map<const char *, uint32_t> ping_keys_{};
  const char *platform_name_{""};
  void add_key_(const char *name, uint32_t key);
  void send_ping_pong_request_();

  inline bool is_encrypted_() { return !this->encryption_key_.empty(); }
};

}  // namespace packet_transport
}  // namespace esphome
