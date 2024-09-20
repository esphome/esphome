#pragma once

#include <vector>

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/network/ip_address.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#ifdef USE_ESP8266
#include "WiFiUdp.h"
#include "IPAddress.h"
#endif

namespace esphome {
namespace statsd {

using sensor_type_t = enum { TYPE_SENSOR, TYPE_BINARY_SENSOR };

using sensors_t = struct {
  const char *name;
  sensor_type_t type;
  union {
#ifdef USE_SENSOR
    esphome::sensor::Sensor *sensor;
#endif
#ifdef USE_BINARY_SENSOR
    esphome::binary_sensor::BinarySensor *binary_sensor;
#endif
  };
};

class StatsdComponent : public PollingComponent {
 public:
  ~StatsdComponent();

  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void configure(const char *host, uint16_t port, const char *prefix) {
    this->host_ = host;
    this->port_ = port;
    this->prefix_ = prefix;
  }

#ifdef USE_SENSOR
  void register_sensor(const char *name, esphome::sensor::Sensor *sensor);
#endif

#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(const char *name, esphome::binary_sensor::BinarySensor *binary_sensor);
#endif

 private:
  const char *host_;
  const char *prefix_;
  uint16_t port_;

  std::vector<sensors_t> sensors_;

#ifdef USE_ESP8266
  WiFiUDP sock_;
#else
  std::unique_ptr<esphome::socket::Socket> sock_;
  struct sockaddr_in destination_;
#endif

  void send_(std::string *out);
};

}  // namespace statsd
}  // namespace esphome
