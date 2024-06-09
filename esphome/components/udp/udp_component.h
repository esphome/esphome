#pragma once

#include "esphome/core/component.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#include "esphome/components/socket/socket.h"
#else
#include <WiFiUdp.h>
#endif
#include <vector>
#include <map>

namespace esphome {
namespace udp {

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

class UDPComponent : public PollingComponent {
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
  void add_address(const char *addr) { this->addresses_.emplace_back(addr); }
  void set_port(uint16_t port) { this->port_ = port; }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

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
  uint32_t ping_pong_recyle_time_{};
  uint32_t last_key_time_{};
  bool resend_ping_key_{};
  bool resend_data_{};
  bool should_send_{};
  const char *name_{};
  bool should_listen_{};
  ESPPreferenceObject pref_;

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  std::unique_ptr<socket::Socket> broadcast_socket_ = nullptr;
  std::unique_ptr<socket::Socket> listen_socket_ = nullptr;
  std::vector<struct sockaddr> sockaddrs_{};
#else
  std::vector<IPAddress> ipaddrs_{};
  WiFiUDP udp_client_{};
#endif
  std::vector<uint8_t> encryption_key_{};
  std::vector<std::string> addresses_{};

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
  void add_key_(const char *name, uint32_t key);
  void send_ping_pong_request_();
  void send_packet_(void *data, size_t len);
  void process_ping_request_(const char *name, uint8_t *ptr, size_t len);

  inline bool is_encrypted_() { return !this->encryption_key_.empty(); }
};

}  // namespace udp
}  // namespace esphome
