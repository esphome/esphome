#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_DATETIME_DATETIME

#include "esphome/components/datetime/datetime_entity.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTDateTimeComponent : public mqtt::MQTTComponent {
 public:
  /** Construct this MQTTDateTimeComponent instance with the provided friendly_name and time
   *
   * @param time The time entity.
   */
  explicit MQTTDateTimeComponent(datetime::DateTimeEntity *datetime);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Override setup.
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool publish_state(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);

 protected:
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  datetime::DateTimeEntity *datetime_;
};

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_DATETIME_DATETIME
#endif  // USE_MQTT
