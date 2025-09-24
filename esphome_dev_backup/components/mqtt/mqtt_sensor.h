#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_SENSOR

#include "esphome/components/sensor/sensor.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTSensorComponent : public mqtt::MQTTComponent {
 public:
  /** Construct this MQTTSensorComponent instance with the provided friendly_name and sensor
   *
   * Note the sensor is never stored and is only used for initializing some values of this class.
   * If sensor is nullptr, then automatic initialization of these fields is disabled.
   *
   * @param sensor The sensor, this can be null to disable automatic setup.
   */
  explicit MQTTSensorComponent(sensor::Sensor *sensor);

  /// Setup an expiry, 0 disables it
  void set_expire_after(uint32_t expire_after);
  /// Disable Home Assistant value expiry.
  void disable_expire_after();

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Override setup.
  void setup() override;

  void dump_config() override;

  /// Get the expire_after in milliseconds used for Home Assistant discovery, first checks override.
  uint32_t get_expire_after() const;

  bool publish_state(float value);
  bool send_initial_state() override;

 protected:
  /// Override for MQTTComponent, returns "sensor".
  std::string component_type() const override;
  const EntityBase *get_entity() const override;
  std::string unique_id() override;

  sensor::Sensor *sensor_;
  optional<uint32_t> expire_after_;  // Override the expire after advertised to Home Assistant
};

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
