#include "mqtt_alarm_control_panel.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_ALARM_CONTROL_PANEL

namespace esphome::mqtt {

static const char *const TAG = "mqtt.alarm_control_panel";

using namespace esphome::alarm_control_panel;

static ProgmemStr alarm_state_to_mqtt_str(AlarmControlPanelState state) {
  switch (state) {
    case ACP_STATE_DISARMED:
      return ESPHOME_F("disarmed");
    case ACP_STATE_ARMED_HOME:
      return ESPHOME_F("armed_home");
    case ACP_STATE_ARMED_AWAY:
      return ESPHOME_F("armed_away");
    case ACP_STATE_ARMED_NIGHT:
      return ESPHOME_F("armed_night");
    case ACP_STATE_ARMED_VACATION:
      return ESPHOME_F("armed_vacation");
    case ACP_STATE_ARMED_CUSTOM_BYPASS:
      return ESPHOME_F("armed_custom_bypass");
    case ACP_STATE_PENDING:
      return ESPHOME_F("pending");
    case ACP_STATE_ARMING:
      return ESPHOME_F("arming");
    case ACP_STATE_DISARMING:
      return ESPHOME_F("disarming");
    case ACP_STATE_TRIGGERED:
      return ESPHOME_F("triggered");
    default:
      return ESPHOME_F("unknown");
  }
}

MQTTAlarmControlPanelComponent::MQTTAlarmControlPanelComponent(AlarmControlPanel *alarm_control_panel)
    : alarm_control_panel_(alarm_control_panel) {}
void MQTTAlarmControlPanelComponent::setup() {
  this->alarm_control_panel_->add_on_state_callback([this]() { this->publish_state(); });
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &payload) {
    auto call = this->alarm_control_panel_->make_call();
    if (ESPHOME_strcasecmp_P(payload.c_str(), ESPHOME_PSTR("ARM_AWAY")) == 0) {
      call.arm_away();
    } else if (ESPHOME_strcasecmp_P(payload.c_str(), ESPHOME_PSTR("ARM_HOME")) == 0) {
      call.arm_home();
    } else if (ESPHOME_strcasecmp_P(payload.c_str(), ESPHOME_PSTR("ARM_NIGHT")) == 0) {
      call.arm_night();
    } else if (ESPHOME_strcasecmp_P(payload.c_str(), ESPHOME_PSTR("ARM_VACATION")) == 0) {
      call.arm_vacation();
    } else if (ESPHOME_strcasecmp_P(payload.c_str(), ESPHOME_PSTR("ARM_CUSTOM_BYPASS")) == 0) {
      call.arm_custom_bypass();
    } else if (ESPHOME_strcasecmp_P(payload.c_str(), ESPHOME_PSTR("DISARM")) == 0) {
      call.disarm();
    } else if (ESPHOME_strcasecmp_P(payload.c_str(), ESPHOME_PSTR("PENDING")) == 0) {
      call.pending();
    } else if (ESPHOME_strcasecmp_P(payload.c_str(), ESPHOME_PSTR("TRIGGERED")) == 0) {
      call.triggered();
    } else {
      ESP_LOGW(TAG, "'%s': Received unknown command payload %s", this->friendly_name_().c_str(), payload.c_str());
    }
    call.perform();
  });
}

void MQTTAlarmControlPanelComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT alarm_control_panel '%s':", this->alarm_control_panel_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true);
  ESP_LOGCONFIG(TAG,
                "  Supported Features: %" PRIu32 "\n"
                "  Requires Code to Disarm: %s\n"
                "  Requires Code To Arm: %s",
                this->alarm_control_panel_->get_supported_features(),
                YESNO(this->alarm_control_panel_->get_requires_code()),
                YESNO(this->alarm_control_panel_->get_requires_code_to_arm()));
}

void MQTTAlarmControlPanelComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  JsonArray supported_features = root[MQTT_SUPPORTED_FEATURES].to<JsonArray>();
  const uint32_t acp_supported_features = this->alarm_control_panel_->get_supported_features();
  if (acp_supported_features & ACP_FEAT_ARM_AWAY) {
    supported_features.add(ESPHOME_F("arm_away"));
  }
  if (acp_supported_features & ACP_FEAT_ARM_HOME) {
    supported_features.add(ESPHOME_F("arm_home"));
  }
  if (acp_supported_features & ACP_FEAT_ARM_NIGHT) {
    supported_features.add(ESPHOME_F("arm_night"));
  }
  if (acp_supported_features & ACP_FEAT_ARM_VACATION) {
    supported_features.add(ESPHOME_F("arm_vacation"));
  }
  if (acp_supported_features & ACP_FEAT_ARM_CUSTOM_BYPASS) {
    supported_features.add(ESPHOME_F("arm_custom_bypass"));
  }
  if (acp_supported_features & ACP_FEAT_TRIGGER) {
    supported_features.add(ESPHOME_F("trigger"));
  }
  root[MQTT_CODE_DISARM_REQUIRED] = this->alarm_control_panel_->get_requires_code();
  root[MQTT_CODE_ARM_REQUIRED] = this->alarm_control_panel_->get_requires_code_to_arm();
}

MQTT_COMPONENT_TYPE(MQTTAlarmControlPanelComponent, "alarm_control_panel")
const EntityBase *MQTTAlarmControlPanelComponent::get_entity() const { return this->alarm_control_panel_; }

bool MQTTAlarmControlPanelComponent::send_initial_state() { return this->publish_state(); }
bool MQTTAlarmControlPanelComponent::publish_state() {
  char topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
  return this->publish(this->get_state_topic_to_(topic_buf),
                       alarm_state_to_mqtt_str(this->alarm_control_panel_->get_state()));
}

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT
