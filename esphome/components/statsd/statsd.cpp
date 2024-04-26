#include "esphome/core/log.h"

#include "statsd.h"

namespace esphome {
namespace statsd {

// send UDP packet if we reach 1Kb packed size
// this is needed since statsD does not support fragmented UDP packets
static const uint16_t SEND_THRESHOLD = 1024;

static const char *TAG = "statsD";

void statsdComponent::setup() {
  this->sock = esphome::socket::socket(AF_INET, SOCK_DGRAM, 0);

  struct sockaddr_in source;
  source.sin_family = AF_INET;
  source.sin_addr.s_addr = htonl(INADDR_ANY);
  source.sin_port = htons(this->port);
  this->sock->bind((struct sockaddr *) &source, sizeof(source));

  this->destination.sin_family = AF_INET;
  this->destination.sin_port = htons(this->port);
  this->destination.sin_addr.s_addr = inet_addr(this->host);
}

statsdComponent::~statsdComponent(void) {
  if (!this->sock) {
    return;
  }
  this->sock->close();
}

void statsdComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "statsD:");
  ESP_LOGCONFIG(TAG, "  host: %s", this->host);
  ESP_LOGCONFIG(TAG, "  port: %d", this->port);
  if (this->prefix) {
    ESP_LOGCONFIG(TAG, "  prefix: %s", this->prefix);
  }

  ESP_LOGCONFIG(TAG, "  metrics:");
  for (sensors_t s : this->sensors) {
    ESP_LOGCONFIG(TAG, "    - name: %s", s.name);
    ESP_LOGCONFIG(TAG, "      type: %d", s.type);
  }
}

float statsdComponent::get_setup_priority() const { return esphome::setup_priority::BEFORE_CONNECTION; }

#ifdef USE_SENSOR
void statsdComponent::register_sensor(const char *name, esphome::sensor::Sensor *sensor) {
  sensors_t s;
  s.name = name;
  s.sensor = sensor;
  s.type = TYPE_SENSOR;
  this->sensors.push_back(s);
}
#endif

#ifdef USE_BINARY_SENSOR
void statsdComponent::register_binary_sensor(const char *name, esphome::binary_sensor::BinarySensor *binary_sensor) {
  sensors_t s;
  s.name = name;
  s.binary_sensor = binary_sensor;
  s.type = TYPE_BINARY_SENSOR;
  this->sensors.push_back(s);
}
#endif

void statsdComponent::update() {
  std::string out;
  out.reserve(SEND_THRESHOLD);

  for (sensors_t s : this->sensors) {
    double val = 0;
    switch (s.type) {
#ifdef USE_SENSOR
      case TYPE_SENSOR:
        if (!s.sensor->has_state()) {
          continue;
        }
        val = s.sensor->state;
        break;
#endif
#ifdef USE_BINARY_SENSOR
      case TYPE_BINARY_SENSOR:
        if (!s.binary_sensor->has_state()) {
          continue;
        }
        // map bool to double
        if (s.binary_sensor->state) {
          val = 1;
        }
        break;
#endif
      default:
        ESP_LOGE(TAG, "type not known, name: %s type: %d", s.name, s.type);
        continue;
    }

    // statsD gauge:
    // https://github.com/statsd/statsd/blob/master/docs/metric_types.md
    // This implies you can't explicitly set a gauge to a negative number without first setting it to zero.
    if (val < 0) {
      if (this->prefix) {
        out.append(str_sprintf("%s.", this->prefix));
      }
      out.append(str_sprintf("%s:0|g\n", s.name));
    }
    if (this->prefix) {
      out.append(str_sprintf("%s.", this->prefix));
    }
    out.append(str_sprintf("%s:%f|g\n", s.name, val));

    if (out.length() > SEND_THRESHOLD) {
      this->send(&out);
      out.clear();
    }
  }

  this->send(&out);
}

void statsdComponent::send(std::string *out) {
  if (out->empty() || !this->sock) {
    return;
  }
  int n_bytes = this->sock->sendto(out->c_str(), out->length(), 0, reinterpret_cast<sockaddr *>(&this->destination),
                                   sizeof(this->destination));
  if (n_bytes != out->length()) {
    ESP_LOGE(TAG, "Failed to send UDP packed (%d of %d)", n_bytes, out->length());
  }
}

}  // namespace statsd
}  // namespace esphome
