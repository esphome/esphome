#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_DATETIME_DATE

#include "esphome/components/datetime/date_entity.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTDateComponent : public mqtt::MQTTComponent {
 public:
  /** Construct this MQTTDateComponent instance with the provided friendly_name and date
   *
   * @param date The date component.
   */
  explicit MQTTDateComponent(datetime::DateEntity *date);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Override setup.
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;

  bool publish_state(uint16_t year, uint8_t month, uint8_t day);

 protected:
  std::string component_type() const override;
  const EntityBase *get_entity() const override;

  datetime::DateEntity *date_;
};

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_DATETIME_DATE
#endif  // USE_MQTT
