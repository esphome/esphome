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

typedef enum { TYPE_SENSOR, TYPE_BINARY_SENSOR } sensor_type_t;

typedef struct {
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
} sensors_t;

class statsdComponent : public PollingComponent {
 public:
  statsdComponent() : PollingComponent(10000){};
  ~statsdComponent();

  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void configure(const char *host, uint16_t port, const char *prefix) {
    this->host = host;
    this->port = port;
    this->prefix = prefix;
  }

#ifdef USE_SENSOR
  void register_sensor(const char *name, esphome::sensor::Sensor *sensor);
#endif

#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(const char *name, esphome::binary_sensor::BinarySensor *binarySensor);
#endif

 private:
  std::unique_ptr<esphome::socket::Socket> sock;
  const char *host;
  const char *prefix;
  uint16_t port;
  struct sockaddr_in destination;

  std::vector<sensors_t> sensors;

  void send(std::string *out);
};

}  // namespace statsd
}  // namespace esphome
