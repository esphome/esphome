#include "mqtt_alarm_control_panel.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_ALARM_CONTROL_PANEL

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.alarm_control_panel";

using namespace esphome::alarm_control_panel;

MQTTAlarmControlPanelComponent::MQTTAlarmControlPanelComponent(AlarmControlPanel *alarm_control_panel)
    : alarm_control_panel_(alarm_control_panel) {}
void MQTTAlarmControlPanelComponent::setup() {
  this->alarm_control_panel_->add_on_state_callback([this]() { this->publish_state(); });
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &payload) {
    auto call = this->alarm_control_panel_->make_call();
    if (strcasecmp(payload.c_str(), "ARM_AWAY") == 0) {
      call.arm_away();
    } else if (strcasecmp(payload.c_str(), "ARM_HOME") == 0) {
      call.arm_home();
    } else if (strcasecmp(payload.c_str(), "ARM_NIGHT") == 0) {
      call.arm_night();
    } else if (strcasecmp(payload.c_str(), "ARM_VACATION") == 0) {
      call.arm_vacation();
    } else if (strcasecmp(payload.c_str(), "ARM_CUSTOM_BYPASS") == 0) {
      call.arm_custom_bypass();
    } else if (strcasecmp(payload.c_str(), "DISARM") == 0) {
      call.disarm();
    } else if (strcasecmp(payload.c_str(), "PENDING") == 0) {
      call.pending();
    } else if (strcasecmp(payload.c_str(), "TRIGGERED") == 0) {
      call.triggered();
    } else {
      ESP_LOGW(TAG, "'%s': Received unknown command payload %s", this->friendly_name().c_str(), payload.c_str());
    }
    call.perform();
  });
}

void MQTTAlarmControlPanelComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT alarm_control_panel '%s':", this->alarm_control_panel_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true)
  ESP_LOGCONFIG(TAG, "  Supported Features: %" PRIu32, this->alarm_control_panel_->get_supported_features());
  ESP_LOGCONFIG(TAG, "  Requires Code to Disarm: %s", YESNO(this->alarm_control_panel_->get_requires_code()));
  ESP_LOGCONFIG(TAG, "  Requires Code To Arm: %s", YESNO(this->alarm_control_panel_->get_requires_code_to_arm()));
}

void MQTTAlarmControlPanelComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  JsonArray supported_features = root.createNestedArray(MQTT_SUPPORTED_FEATURES);
  const uint32_t acp_supported_features = this->alarm_control_panel_->get_supported_features();
  if (acp_supported_features & ACP_FEAT_ARM_AWAY) {
    supported_features.add("arm_away");
  }
  if (acp_supported_features & ACP_FEAT_ARM_HOME) {
    supported_features.add("arm_home");
  }
  if (acp_supported_features & ACP_FEAT_ARM_NIGHT) {
    supported_features.add("arm_night");
  }
  if (acp_supported_features & ACP_FEAT_ARM_VACATION) {
    supported_features.add("arm_vacation");
  }
  if (acp_supported_features & ACP_FEAT_ARM_CUSTOM_BYPASS) {
    supported_features.add("arm_custom_bypass");
  }
  if (acp_supported_features & ACP_FEAT_TRIGGER) {
    supported_features.add("trigger");
  }
  root[MQTT_CODE_DISARM_REQUIRED] = this->alarm_control_panel_->get_requires_code();
  root[MQTT_CODE_ARM_REQUIRED] = this->alarm_control_panel_->get_requires_code_to_arm();
}

std::string MQTTAlarmControlPanelComponent::component_type() const { return "alarm_control_panel"; }
const EntityBase *MQTTAlarmControlPanelComponent::get_entity() const { return this->alarm_control_panel_; }

bool MQTTAlarmControlPanelComponent::send_initial_state() { return this->publish_state(); }
bool MQTTAlarmControlPanelComponent::publish_state() {
  bool success = true;
  const char *state_s = "";
  switch (this->alarm_control_panel_->get_state()) {
    case ACP_STATE_DISARMED:
      state_s = "disarmed";
      break;
    case ACP_STATE_ARMED_HOME:
      state_s = "armed_home";
      break;
    case ACP_STATE_ARMED_AWAY:
      state_s = "armed_away";
      break;
    case ACP_STATE_ARMED_NIGHT:
      state_s = "armed_night";
      break;
    case ACP_STATE_ARMED_VACATION:
      state_s = "armed_vacation";
      break;
    case ACP_STATE_ARMED_CUSTOM_BYPASS:
      state_s = "armed_custom_bypass";
      break;
    case ACP_STATE_PENDING:
      state_s = "pending";
      break;
    case ACP_STATE_ARMING:
      state_s = "arming";
      break;
    case ACP_STATE_DISARMING:
      state_s = "disarming";
      break;
    case ACP_STATE_TRIGGERED:
      state_s = "triggered";
      break;
    default:
      state_s = "unknown";
  }
  if (!this->publish(this->get_state_topic_(), state_s))
    success = false;
  return success;
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
