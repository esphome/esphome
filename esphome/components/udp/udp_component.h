#pragma once

#include "esphome/core/component.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <vector>
#include <map>

namespace esphome {
namespace udp {

struct provider_t {
  std::vector<uint8_t> encryption_key;
  const char *name;
  uint32_t last_code[2];
};

struct sensor_t {
  sensor::Sensor *sensor;
  const char *id;
  bool updated;
};

struct binary_sensor_t {
  binary_sensor::BinarySensor *sensor;
  const char *id;
  bool updated;
};

class UDPComponent : public PollingComponent {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void add_sensor(const char *id, sensor::Sensor *sensor) {
    sensor_t st{sensor, id, true};
    this->sensors_.push_back(st);
  }
  void add_binary_sensor(const char *id, binary_sensor::BinarySensor *sensor) {
    binary_sensor_t st{sensor, id, true};
    this->binary_sensors_.push_back(st);
  }

  void add_remote_sensor(const char *hostname, const char *remote_id, sensor::Sensor *sensor) {
    this->add_provider(hostname);
    this->remote_sensors_[hostname][remote_id] = sensor;
  }
  void add_remote_binary_sensor(const char *hostname, const char *remote_id, binary_sensor::BinarySensor *sensor) {
    this->add_provider(hostname);
    this->remote_binary_sensors_[hostname][remote_id] = sensor;
  }
  void add_address(const char *addr) { this->addresses_.emplace_back(addr); }
  void set_port(uint16_t port) { this->port_ = port; }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void add_provider(const char *hostname) {
    if (this->providers_.count(hostname) == 0) {
      provider_t provider;
      provider.encryption_key = std::vector<uint8_t>{};
      provider.last_code[0] = 0;
      provider.last_code[1] = 0;
      provider.name = hostname;
      this->providers_[hostname] = provider;
      this->remote_sensors_[hostname] = std::map<std::string, sensor::Sensor *>();
      this->remote_binary_sensors_[hostname] = std::map<std::string, binary_sensor::BinarySensor *>();
    }
  }

  void set_encryption_key(std::vector<uint8_t> key) { this->encryption_key_ = std::move(key); }
  void set_rolling_code_enable(bool enable) { this->rolling_code_enable_ = enable; }
  void set_ping_pong_enable(bool enable) { this->ping_pong_enable_ = enable; }
  void set_provider_encryption(const char *name, std::vector<uint8_t> key) {
    this->providers_[name].encryption_key = std::move(key);
  }

 protected:
  void send_data_(bool all);
  void process_(uint8_t *buf, size_t len);
  void flush_();
  void add_data_(uint8_t key, const char *id, float data);
  void add_data_(uint8_t key, const char *id, uint32_t data);
  void increment_code_();
  void add_binary_data_(uint8_t key, const char *id, bool data);
  void init_data_();

  bool updated_{};
  uint16_t port_{18511};
  uint32_t ping_key_{};
  uint32_t rolling_code_[2]{};
  bool rolling_code_enable_{};
  bool ping_pong_enable_{};
  bool resend_ping_key_{};
  bool resend_data_{};
  const char *name_{};
  ESPPreferenceObject pref_;

  std::unique_ptr<socket::Socket> broadcast_socket_ = nullptr;
  std::unique_ptr<socket::Socket> listen_socket_ = nullptr;
  std::vector<uint8_t> encryption_key_{};
  std::vector<std::string> addresses_{};

  std::vector<sensor_t> sensors_{};
  std::vector<binary_sensor_t> binary_sensors_{};

  std::map<std::string, provider_t> providers_{};
  std::map<std::string, std::map<std::string, sensor::Sensor *>> remote_sensors_{};
  std::map<std::string, std::map<std::string, binary_sensor::BinarySensor *>> remote_binary_sensors_{};
  std::vector<uint8_t> ping_header_{};
  std::vector<uint8_t> header_{};
  std::vector<uint8_t> data_{};
  std::vector<struct sockaddr> sockaddrs_{};
  std::map<const char *, uint32_t> ping_keys_{};
  void add_key_(const char *name, uint32_t key);
  void send_ping_pong_request_();
  void send_packet_(void *data, size_t len);
  void process_ping_request_(const char *name, uint8_t *ptr, size_t len);

  inline bool is_encrypted_() { return !this->encryption_key_.empty(); }
};

}  // namespace udp
}  // namespace esphome
