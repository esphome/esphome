#pragma once

#include "esphome/core/defines.h"

#ifdef USE_NUMBER

#include "esphome/components/number/number.h"
#include "mqtt_component.h"

namespace esphome {
namespace mqtt {

class MQTTNumberComponent : public mqtt::MQTTComponent {
 public:
  /** Construct this MQTTNumberComponent instance with the provided friendly_name and number
   *
   * @param number The number.
   */
  explicit MQTTNumberComponent(number::Number *number);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Override setup.
  void setup() override;
  void dump_config() override;

  void send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) override;

  bool send_initial_state() override;
  bool is_internal() override;

  bool publish_state(float value);

 protected:
  /// Override for MQTTComponent, returns "number".
  std::string component_type() const override;

  std::string friendly_name() const override;

  number::Number *number_;
};

}  // namespace mqtt
}  // namespace esphome

#endif
