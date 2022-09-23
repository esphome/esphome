#include "mr24hpb1.h"
#include "constants.h"

namespace esphome {
namespace mr24hpb1 {
void MR24HPB1Component::handle_active_reporting(std::vector<uint8_t> &packet) {
  AddressCode1 current_addr_code_1 = get_packet_address_code_1(packet);
  switch (current_addr_code_1) {
    case PROREP_REPORT_RADAR_INFO:
      this->handle_radar_report(packet);
      break;
    case PROREP_REPORT_OTHER_INFORMATION:
      this->handle_other_information(packet);
      break;

    default:
      ESP_LOGW(TAG, "Active reporting packet had unkown address code 1: 0x%x", current_addr_code_1);
      break;
  }
}

void MR24HPB1Component::handle_passive_reporting(std::vector<uint8_t> &packet) {
  AddressCode1 current_addr_code_1 = get_packet_address_code_1(packet);
  switch (current_addr_code_1) {
    case PR_REPORTING_MODULE_ID:
      this->handle_module_id_report(packet);
      break;
    case PR_REPORT_RADAR_INFO:
      this->handle_radar_report(packet);
      break;
    case PR_REPORTING_SYSTEM_INFO:
      this->handle_system_report(packet);
      break;

    default:
      ESP_LOGW(TAG, "Passive reporting packet had unkown address code 1: 0x%x", current_addr_code_1);
      break;
  }
}

void MR24HPB1Component::handle_radar_report(std::vector<uint8_t> &packet) {
  AddressCode2 current_addr_code_2 = get_packet_address_code_2(packet);
  bool occupied = false;
  switch (current_addr_code_2) {
    case PROREP_RRI_ENVIRONMENT_STATUS: {
      uint32_t data = packet_data_to_int(packet);
      switch (data) {
        case UNOCCUPIED:
          break;
        case STATIONARY:
          occupied = true;
          break;
        case MOVING:
          occupied = true;
          break;

        default:
          ESP_LOGW(TAG, "Environment state data was invalid: 0x%x", data);
          break;
      }

      if (this->occupancy_sensor_ != nullptr) {
        this->occupancy_sensor_->publish_state(occupied);
      }
      if (this->environment_status_sensor_ != nullptr) {
        const char *status = EnvironmentStatus_to_string(EnvironmentStatus(data));
        ESP_LOGD(TAG, "Got environment status=%s", status);
        this->environment_status_sensor_->publish_state(status);
      }
    } break;
    case PROREP_RRI_MOVEMENT_SIGNS_PARAMETERS: {
      float float_data = packet_data_to_float(packet);
      if (this->movement_sensor_ != nullptr) {
        this->movement_sensor_->publish_state(float_data);
      }
    } break;
    case PROREP_RRI_APPROACHING_AWAY_STATE: {
      std::vector<uint8_t> packet_data = get_packet_data(packet);
      switch (packet_data.at(2)) {
        case NONE:
          ESP_LOGI(TAG, "Approaching away is: None");
          break;
        case CLOSE_TO:
          ESP_LOGI(TAG, "Approaching away is: CLOSE_TO");
          break;
        case STAY_AWAY:
          ESP_LOGI(TAG, "Approaching away is: STAY_AWAY");
          break;

        default:
          ESP_LOGW(TAG, "Approaching away state data was invalid: 0x%x", packet_data.at(2));
          break;
      }
    }

    break;
    default:
      ESP_LOGW(TAG, "Radar report -> Packet had unkown address code 2: 0x%x", current_addr_code_2);
      break;
  }
}

void MR24HPB1Component::handle_other_information(std::vector<uint8_t> &packet) {
  AddressCode2 current_addr_code_2 = get_packet_address_code_2(packet);
  uint32_t data = packet_data_to_int(packet);
  bool occupied = false;
  switch (current_addr_code_2) {
    case PROREP_ROI_HEARTBEAT_PACK:
      switch (data) {
        case UNOCCUPIED:
          break;
        case STATIONARY:
          occupied = true;
          break;
        case MOVING:
          occupied = true;
          break;

        default:
          ESP_LOGW(TAG, "Heartbeat state data was invalid: %d", data);
          break;
      }

      if (this->occupancy_sensor_ != nullptr) {
        this->occupancy_sensor_->publish_state(occupied);
      }

      if (this->environment_status_sensor_ != nullptr) {
        const char *status = EnvironmentStatus_to_string(EnvironmentStatus(data));
        ESP_LOGD(TAG, "Got environment status=%s", status);
        this->environment_status_sensor_->publish_state(status);
      }
      break;
    case PROREP_ROI_ABNORMAL_RESET:
      if (data == 0x0F) {
        ESP_LOGW(TAG, "Sensor abnormal Reset");
      } else {
        ESP_LOGW(TAG, "Heartbeat state data was invalid: %d", data);
      }
      break;
    default:
      ESP_LOGW(TAG, "Other Information -> Packet had unkown address code 2: 0x%x", current_addr_code_2);
      break;
  }
}

void MR24HPB1Component::handle_module_id_report(std::vector<uint8_t> &packet) {
  AddressCode2 current_addr_code_2 = get_packet_address_code_2(packet);
  switch (current_addr_code_2) {
    case RC_MS_DEVICE_ID:
      if (this->device_id_sensor_ != nullptr) {
        std::string device_id = packet_data_to_string(packet);
        this->device_id_sensor_->publish_state(device_id);
        this->respone_requested = 0;
      }
      break;
    case RC_MS_SOFTWARE_VERSION:
      if (this->software_version_sensor_ != nullptr) {
        std::string software_version = packet_data_to_string(packet);
        this->software_version_sensor_->publish_state(software_version);
        this->respone_requested = 0;
      }
      break;
    case RC_MS_HARDWARE_VERSION:
      if (this->hardware_version_sensor_ != nullptr) {
        std::string hardware_version = packet_data_to_string(packet);
        this->hardware_version_sensor_->publish_state(hardware_version);
        this->respone_requested = 0;
      }
      break;
    case RC_MS_PROTOCOL_VERSION:
      if (this->protocol_version_sensor_ != nullptr) {
        std::string protocol_version = esphome::to_string(packet_data_to_int(packet));
        this->protocol_version_sensor_->publish_state(protocol_version);
        this->respone_requested = 0;
      }
      break;
    default:
      ESP_LOGW(TAG, "Module ID report -> Packet had unkown address code 2: 0x%x", current_addr_code_2);
      break;
  }
}

void MR24HPB1Component::handle_system_report(std::vector<uint8_t> &packet) {
  AddressCode2 current_addr_code_2 = get_packet_address_code_2(packet);
  switch (current_addr_code_2) {
    case PR_RSI_THRESHOLD_GEAR:
      ESP_LOGI(TAG, "Threshold gear changed to: %d", packet_data_to_int(packet));
      break;
    case PR_RSI_SCENE_SETTINGS:
      ESP_LOGI(TAG, "Scene setting changed to: %s",
               SceneSetting_to_string(static_cast<SceneSetting>(packet_data_to_int(packet))));
      break;
    default:
      ESP_LOGW(TAG, "System report -> Packet had unkown address code 2: 0x%x", current_addr_code_2);
      break;
  }
}

}  // namespace mr24hpb1
}  // namespace esphome