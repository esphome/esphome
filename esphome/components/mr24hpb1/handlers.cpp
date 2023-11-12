#include "constants.h"
#include "mr24hpb1.h"

namespace esphome {
namespace mr24hpb1 {
void MR24HPB1Component::handle_active_reporting_(Packet &packet) {
  AddressCode1 current_addr_code_1 = packet.address_code_1();
  switch (current_addr_code_1) {
    case PROREP_REPORT_RADAR_INFO:
      this->handle_radar_report_(packet);
      break;
    case PROREP_REPORT_OTHER_INFORMATION:
      this->handle_other_information_(packet);
      break;
    case PROREP_REPORTING_MODULE_ID:
#ifdef USE_TEXT_SENSOR
      this->handle_module_id_report_(packet);
#endif
      break;

    default:
      ESP_LOGW(TAG, "Active reporting packet had unknown address code 1: 0x%x", current_addr_code_1);
      packet.log();
      break;
  }
}

void MR24HPB1Component::handle_passive_reporting_(Packet &packet) {
  AddressCode1 current_addr_code_1 = packet.address_code_1();
  switch (current_addr_code_1) {
    case PR_REPORTING_MODULE_ID:
      this->handle_module_id_report_(packet);
      break;
    case PR_REPORT_RADAR_INFO:
      this->handle_radar_report_(packet);
      break;
    case PR_REPORTING_SYSTEM_INFO:
      this->handle_system_report_(packet);
      break;
    case PR_OTHER_FUNCTIONS:
      this->handle_other_function_report_(packet);
      break;

    default:
      ESP_LOGW(TAG, "Passive reporting packet had unknown address code 1: 0x%x", current_addr_code_1);
      packet.log();
      break;
  }
}

void MR24HPB1Component::handle_sleep_data_report_(Packet &packet) {
  AddressCode1 current_addr_code_1 = packet.address_code_1();
  switch (current_addr_code_1) {
    case SLP_BREATHING_REPORTING_PARAMETERS:
      ESP_LOGD(TAG, "Breathing reporting is not implemented!");
      break;
    case SLP_SCENARIO_ASSESMENT:
      ESP_LOGD(TAG, "Sleep scenario reporting is not implemented!");
      break;
    case SLP_SLEEP_DURATIONS:
      ESP_LOGD(TAG, "Sleep duration reporting is not implemented!");
      break;
    case SLP_SLEEP_QUALITY_PARAMETER:
      ESP_LOGD(TAG, "Sleep quality reporting is not implemented!");
      break;
    case SLP_HEART_RATE_PARAMETER:
      ESP_LOGD(TAG, "Heart rate reporting is not implemented!");
      break;

    default:
      ESP_LOGW(TAG, "Sleep data reporting packet had unknown address code 1: 0x%x", current_addr_code_1);
      packet.log();
      break;
  }
}

void MR24HPB1Component::handle_fall_data_report_(Packet &packet) {
  AddressCode1 current_addr_code_1 = packet.address_code_1();
  switch (current_addr_code_1) {
    case FAL_ALARM:
      ESP_LOGD(TAG, "Fall detection reporting is not implemented!");
      break;
    default:
      ESP_LOGW(TAG, "Fall detection reporting packet had unknown address code 1: 0x%x", current_addr_code_1);
      packet.log();
      break;
  }
}

void MR24HPB1Component::handle_radar_report_(Packet &packet) {
  AddressCode2 current_addr_code_2 = packet.address_code_2();
  switch (current_addr_code_2) {
    case PROREP_RRI_ENVIRONMENT_STATUS: {
      uint32_t data = packet.data_as_int();
      bool occupied = false;
      bool movement = false;
      switch (data) {
        case UNOCCUPIED:
          break;
        case STATIONARY:
          occupied = true;
          break;
        case MOVING:
          occupied = true;
          movement = true;
          break;
        default:
          ESP_LOGW(TAG, "Environment state data was invalid: 0x%x", data);
          return;
      }
#ifdef USE_BINARY_SENSOR
      if (this->occupancy_sensor_ != nullptr) {
        this->occupancy_sensor_->publish_state(occupied);
      }
      if (this->movement_sensor_ != nullptr) {
        this->movement_sensor_->publish_state(movement);
      }
#endif

#ifdef USE_TEXT_SENSOR
      if (this->environment_status_sensor_ != nullptr) {
        const char *status = environment_status_to_string(EnvironmentStatus(data));
        ESP_LOGD(TAG, "Got environment status=%s", status);
        this->environment_status_sensor_->publish_state(status);
      }
#endif
      break;
    }
    case PROREP_RRI_MOVEMENT_SIGNS_PARAMETERS: {
#ifdef USE_SENSOR
      float float_data = packet.data_as_float();
      if (this->movement_rate_sensor_ != nullptr) {
        this->movement_rate_sensor_->publish_state(float_data);
      }
#endif
      break;
    }
    case PROREP_RRI_APPROACHING_AWAY_STATE: {
#ifdef USE_TEXT_SENSOR
      if (this->movement_type_sensor_ != nullptr) {
        std::vector<uint8_t> packet_data = packet.data();
        uint8_t value = packet_data.at(2);
        MovementType type = static_cast<MovementType>(value);
        const char *type_str = movement_type_to_string(type);

        ESP_LOGD(TAG, "Got movement type=0x%x (%s)", value, type_str);
        this->movement_type_sensor_->publish_state(type_str);
      }
#endif
      break;
    }
    default:
      ESP_LOGW(TAG, "Radar report -> Packet had unknown address code 2: 0x%x", current_addr_code_2);
      packet.log();
      break;
  }
}

void MR24HPB1Component::handle_other_information_(Packet &packet) {
  AddressCode2 current_addr_code_2 = packet.address_code_2();
  uint32_t data = packet.data_as_int();
  switch (current_addr_code_2) {
    case PROREP_ROI_HEARTBEAT_PACK: {
      bool occupied = false;
      bool movement = false;
      switch (data) {
        case UNOCCUPIED:
          break;
        case STATIONARY:
          occupied = true;
          break;
        case MOVING:
          occupied = true;
          movement = true;
          break;
        default:
          ESP_LOGW(TAG, "Heartbeat state data was invalid: %d", data);
          return;
      }

#ifdef USE_BINARY_SENSOR
      if (this->occupancy_sensor_ != nullptr) {
        this->occupancy_sensor_->publish_state(occupied);
      }
      if (this->movement_sensor_ != nullptr) {
        this->movement_sensor_->publish_state(movement);
      }
#endif

#ifdef USE_TEXT_SENSOR
      if (this->environment_status_sensor_ != nullptr) {
        const char *status = environment_status_to_string(EnvironmentStatus(data));
        ESP_LOGD(TAG, "Got environment status=%s", status);
        this->environment_status_sensor_->publish_state(status);
      }
#endif
      break;
    }
    case PROREP_ROI_ABNORMAL_RESET: {
      if (data == 0x0F) {
        ESP_LOGW(TAG, "Sensor abnormal Reset");
      } else {
        ESP_LOGW(TAG, "Heartbeat state data was invalid: %d", data);
      }
      break;
    }
    default: {
      ESP_LOGW(TAG, "Other Information -> Packet had unknown address code 2: 0x%x", current_addr_code_2);
      packet.log();
      break;
    }
  }
}

#ifdef USE_TEXT_SENSOR
void MR24HPB1Component::handle_module_id_report_(Packet &packet) {
  AddressCode2 current_addr_code_2 = packet.address_code_2();
  switch (current_addr_code_2) {
    case RC_MS_DEVICE_ID:
      if (this->device_id_sensor_ != nullptr) {
        std::string device_id = packet.data_as_string();
        this->device_id_sensor_->publish_state(device_id);
        this->response_requested_ = 0;
      }
      break;
    case RC_MS_SOFTWARE_VERSION:
      if (this->software_version_sensor_ != nullptr) {
        std::string software_version = packet.data_as_string();
        this->software_version_sensor_->publish_state(software_version);
        this->response_requested_ = 0;
      }
      break;
    case RC_MS_HARDWARE_VERSION:
      if (this->hardware_version_sensor_ != nullptr) {
        std::string hardware_version = packet.data_as_string();
        this->hardware_version_sensor_->publish_state(hardware_version);
        this->response_requested_ = 0;
      }
      break;
    case RC_MS_PROTOCOL_VERSION:
      if (this->protocol_version_sensor_ != nullptr) {
        std::string protocol_version = esphome::to_string(packet.data_as_int());
        this->protocol_version_sensor_->publish_state(protocol_version);
        this->response_requested_ = 0;
      }
      break;
    default:
      ESP_LOGW(TAG, "Module ID report -> Packet had unknown address code 2: 0x%x", current_addr_code_2);
      packet.log();
      break;
  }
}
#endif

void MR24HPB1Component::handle_system_report_(Packet &packet) {
  AddressCode2 current_addr_code_2 = packet.address_code_2();
  switch (current_addr_code_2) {
    case PR_RSI_THRESHOLD_GEAR:
      ESP_LOGD(TAG, "Threshold gear changed to: %d", packet.data_as_int());
      break;
    case PR_RSI_SCENE_SETTINGS:
      ESP_LOGD(TAG, "Scene setting changed to: %s",
               scene_setting_to_string(static_cast<SceneSetting>(packet.data_as_int())));
      break;
    case PR_RSI_FORCED_UNOCCUPIED_SETTINGS:
      ESP_LOGD(TAG, "Force unoccupied setting changed to: %s",
               forced_unoccupied_to_string(static_cast<ForcedUnoccupied>(packet.data_as_int())));
      break;
    default:
      ESP_LOGW(TAG, "System report -> Packet had unknown address code 2: 0x%x", current_addr_code_2);
      packet.log();
      break;
  }
}

void MR24HPB1Component::handle_other_function_report_(Packet &packet) {
  AddressCode2 current_addr_code_2 = packet.address_code_2();
  switch (current_addr_code_2) {
    case PR_OTH_SLEEP_FUNCTION_SWITCH:
      ESP_LOGD(TAG, "Sleep function is switchted to: %d", packet.data_as_int());
      break;
    case PR_OTH_START_OTA_UPGRADE:
      ESP_LOGD(TAG, "Start OTA upgrade status is: %d", packet.data_as_int());
      break;
    case PR_OTH_UPGRADE_PACKAGE:
      ESP_LOGD(TAG, "OTA upgrade package transfer: %d", packet.data_as_int());
      break;
    case PR_OTH_FALL_ALARM_TIME:
      ESP_LOGD(TAG, "Fall alarm time package transfer: %d", packet.data_as_int());
      break;
    case PR_OTH_FALL_FUNCTION_SWITCH:
      ESP_LOGD(TAG, "Fall function switch package transfer: %d", packet.data_as_int());
      break;
    case PR_OTH_FALL_SENSITIVITY_SETTING:
      ESP_LOGD(TAG, "Fall sensitivity setting package transfer: %d", packet.data_as_int());
      break;
    default:
      ESP_LOGW(TAG, "Other function report -> Packet had unknown address code 2: 0x%x", current_addr_code_2);
      packet.log();
      break;
  }
}

}  // namespace mr24hpb1
}  // namespace esphome
