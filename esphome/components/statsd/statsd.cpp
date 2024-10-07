#include "esphome/core/log.h"

#include "statsd.h"

namespace esphome {
namespace statsd {

// send UDP packet if we reach 1Kb packed size
// this is needed since statsD does not support fragmented UDP packets
static const uint16_t SEND_THRESHOLD = 1024;

static const char *const TAG = "statsD";

void StatsdComponent::setup() {
#ifndef USE_ESP8266
  this->sock_ = esphome::socket::socket(AF_INET, SOCK_DGRAM, 0);

  struct sockaddr_in source;
  source.sin_family = AF_INET;
  source.sin_addr.s_addr = htonl(INADDR_ANY);
  source.sin_port = htons(this->port_);
  this->sock_->bind((struct sockaddr *) &source, sizeof(source));

  this->destination_.sin_family = AF_INET;
  this->destination_.sin_port = htons(this->port_);
  this->destination_.sin_addr.s_addr = inet_addr(this->host_);
#endif
}

StatsdComponent::~StatsdComponent() {
#ifndef USE_ESP8266
  if (!this->sock_) {
    return;
  }
  this->sock_->close();
#endif
}

void StatsdComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "statsD:");
  ESP_LOGCONFIG(TAG, "  host: %s", this->host_);
  ESP_LOGCONFIG(TAG, "  port: %d", this->port_);
  if (this->prefix_) {
    ESP_LOGCONFIG(TAG, "  prefix: %s", this->prefix_);
  }

  ESP_LOGCONFIG(TAG, "  metrics:");
  for (sensors_t s : this->sensors_) {
    ESP_LOGCONFIG(TAG, "    - name: %s", s.name);
    ESP_LOGCONFIG(TAG, "      type: %d", s.type);
  }
}

float StatsdComponent::get_setup_priority() const { return esphome::setup_priority::AFTER_WIFI; }

#ifdef USE_SENSOR
void StatsdComponent::register_sensor(const char *name, esphome::sensor::Sensor *sensor) {
  sensors_t s;
  s.name = name;
  s.sensor = sensor;
  s.type = TYPE_SENSOR;
  this->sensors_.push_back(s);
}
#endif

#ifdef USE_BINARY_SENSOR
void StatsdComponent::register_binary_sensor(const char *name, esphome::binary_sensor::BinarySensor *binary_sensor) {
  sensors_t s;
  s.name = name;
  s.binary_sensor = binary_sensor;
  s.type = TYPE_BINARY_SENSOR;
  this->sensors_.push_back(s);
}
#endif

void StatsdComponent::update() {
  std::string out;
  out.reserve(SEND_THRESHOLD);

  for (sensors_t s : this->sensors_) {
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
      if (this->prefix_) {
        out.append(str_sprintf("%s.", this->prefix_));
      }
      out.append(str_sprintf("%s:0|g\n", s.name));
    }
    if (this->prefix_) {
      out.append(str_sprintf("%s.", this->prefix_));
    }
    out.append(str_sprintf("%s:%f|g\n", s.name, val));

    if (out.length() > SEND_THRESHOLD) {
      this->send_(&out);
      out.clear();
    }
  }

  this->send_(&out);
}

void StatsdComponent::send_(std::string *out) {
  if (out->empty()) {
    return;
  }
#ifdef USE_ESP8266
  IPAddress ip;
  ip.fromString(this->host_);

  this->sock_.beginPacket(ip, this->port_);
  this->sock_.write((const uint8_t *) out->c_str(), out->length());
  this->sock_.endPacket();

#else
  if (!this->sock_) {
    return;
  }

  int n_bytes = this->sock_->sendto(out->c_str(), out->length(), 0, reinterpret_cast<sockaddr *>(&this->destination_),
                                    sizeof(this->destination_));
  if (n_bytes != out->length()) {
    ESP_LOGE(TAG, "Failed to send UDP packed (%d of %d)", n_bytes, out->length());
  }
#endif
}

}  // namespace statsd
}  // namespace esphome
