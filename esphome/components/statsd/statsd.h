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
  StatsdComponent() : PollingComponent(10000){};
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
  std::unique_ptr<esphome::socket::Socket> sock_;
  const char *host_;
  const char *prefix_;
  uint16_t port_;
  struct sockaddr_in destination_;

  std::vector<sensors_t> sensors_;

  void send_(std::string *out);
};

}  // namespace statsd
}  // namespace esphome
