// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2_client.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api_client {

static const char *TAG = "api_client.client";

bool APIClientConnectionBase::send_hello_request(const HelloRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_hello_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<HelloRequest>(msg, 1);
}
bool APIClientConnectionBase::send_connect_request(const ConnectRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_connect_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<ConnectRequest>(msg, 3);
}
bool APIClientConnectionBase::send_disconnect_request(const DisconnectRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_disconnect_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<DisconnectRequest>(msg, 5);
}
bool APIClientConnectionBase::send_disconnect_response(const DisconnectResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_disconnect_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<DisconnectResponse>(msg, 6);
}
bool APIClientConnectionBase::send_ping_request(const PingRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_ping_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<PingRequest>(msg, 7);
}
bool APIClientConnectionBase::send_ping_response(const PingResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_ping_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<PingResponse>(msg, 8);
}
bool APIClientConnectionBase::send_device_info_request(const DeviceInfoRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_device_info_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<DeviceInfoRequest>(msg, 9);
}
bool APIClientConnectionBase::send_list_entities_request(const ListEntitiesRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesRequest>(msg, 11);
}
bool APIClientConnectionBase::send_subscribe_states_request(const SubscribeStatesRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_subscribe_states_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<SubscribeStatesRequest>(msg, 20);
}
#ifdef USE_COVER
bool APIClientConnectionBase::send_cover_command_request(const CoverCommandRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_cover_command_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<CoverCommandRequest>(msg, 30);
}
#endif
#ifdef USE_FAN
bool APIClientConnectionBase::send_fan_command_request(const FanCommandRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_fan_command_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<FanCommandRequest>(msg, 31);
}
#endif
#ifdef USE_LIGHT
bool APIClientConnectionBase::send_light_command_request(const LightCommandRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_light_command_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<LightCommandRequest>(msg, 32);
}
#endif
#ifdef USE_SWITCH
bool APIClientConnectionBase::send_switch_command_request(const SwitchCommandRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_switch_command_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<SwitchCommandRequest>(msg, 33);
}
#endif
bool APIClientConnectionBase::send_subscribe_logs_request(const SubscribeLogsRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_subscribe_logs_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<SubscribeLogsRequest>(msg, 28);
}
bool APIClientConnectionBase::send_subscribe_homeassistant_services_request(const SubscribeHomeassistantServicesRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_subscribe_homeassistant_services_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<SubscribeHomeassistantServicesRequest>(msg, 34);
}
bool APIClientConnectionBase::send_subscribe_home_assistant_states_request(const SubscribeHomeAssistantStatesRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_subscribe_home_assistant_states_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<SubscribeHomeAssistantStatesRequest>(msg, 38);
}
bool APIClientConnectionBase::send_home_assistant_state_response(const HomeAssistantStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_home_assistant_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<HomeAssistantStateResponse>(msg, 40);
}
bool APIClientConnectionBase::send_get_time_request(const GetTimeRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_get_time_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<GetTimeRequest>(msg, 36);
}
bool APIClientConnectionBase::send_get_time_response(const GetTimeResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_get_time_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<GetTimeResponse>(msg, 37);
}
bool APIClientConnectionBase::send_execute_service_request(const ExecuteServiceRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_execute_service_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<ExecuteServiceRequest>(msg, 42);
}
#ifdef USE_ESP32_CAMERA
bool APIClientConnectionBase::send_camera_image_request(const CameraImageRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_camera_image_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<CameraImageRequest>(msg, 45);
}
#endif
#ifdef USE_CLIMATE
bool APIClientConnectionBase::send_climate_command_request(const ClimateCommandRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_climate_command_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<ClimateCommandRequest>(msg, 48);
}
#endif
#ifdef USE_NUMBER
bool APIClientConnectionBase::send_number_command_request(const NumberCommandRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_number_command_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<NumberCommandRequest>(msg, 51);
}
#endif
#ifdef USE_SELECT
bool APIClientConnectionBase::send_select_command_request(const SelectCommandRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_select_command_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<SelectCommandRequest>(msg, 54);
}
#endif
bool APIClientConnectionBase::read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) {
  switch (msg_type) {
    case 2: {
      HelloResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_hello_response: %s", msg.dump().c_str());
#endif
      this->on_hello_response(msg);
      break;
    }
    case 4: {
      ConnectResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_connect_response: %s", msg.dump().c_str());
#endif
      this->on_connect_response(msg);
      break;
    }
    case 5: {
      DisconnectRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_disconnect_request: %s", msg.dump().c_str());
#endif
      this->on_disconnect_request(msg);
      break;
    }
    case 6: {
      DisconnectResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_disconnect_response: %s", msg.dump().c_str());
#endif
      this->on_disconnect_response(msg);
      break;
    }
    case 7: {
      PingRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_ping_request: %s", msg.dump().c_str());
#endif
      this->on_ping_request(msg);
      break;
    }
    case 8: {
      PingResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_ping_response: %s", msg.dump().c_str());
#endif
      this->on_ping_response(msg);
      break;
    }
    case 10: {
      DeviceInfoResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_device_info_response: %s", msg.dump().c_str());
#endif
      this->on_device_info_response(msg);
      break;
    }
    case 12: {
#ifdef USE_BINARY_SENSOR
      ListEntitiesBinarySensorResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_binary_sensor_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_binary_sensor_response(msg);
#endif
      break;
    }
    case 13: {
#ifdef USE_COVER
      ListEntitiesCoverResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_cover_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_cover_response(msg);
#endif
      break;
    }
    case 14: {
#ifdef USE_FAN
      ListEntitiesFanResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_fan_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_fan_response(msg);
#endif
      break;
    }
    case 15: {
#ifdef USE_LIGHT
      ListEntitiesLightResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_light_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_light_response(msg);
#endif
      break;
    }
    case 16: {
#ifdef USE_SENSOR
      ListEntitiesSensorResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_sensor_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_sensor_response(msg);
#endif
      break;
    }
    case 17: {
#ifdef USE_SWITCH
      ListEntitiesSwitchResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_switch_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_switch_response(msg);
#endif
      break;
    }
    case 18: {
#ifdef USE_TEXT_SENSOR
      ListEntitiesTextSensorResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_text_sensor_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_text_sensor_response(msg);
#endif
      break;
    }
    case 19: {
      ListEntitiesDoneResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_done_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_done_response(msg);
      break;
    }
    case 21: {
#ifdef USE_BINARY_SENSOR
      BinarySensorStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_binary_sensor_state_response: %s", msg.dump().c_str());
#endif
      this->on_binary_sensor_state_response(msg);
#endif
      break;
    }
    case 22: {
#ifdef USE_COVER
      CoverStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_cover_state_response: %s", msg.dump().c_str());
#endif
      this->on_cover_state_response(msg);
#endif
      break;
    }
    case 23: {
#ifdef USE_FAN
      FanStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_fan_state_response: %s", msg.dump().c_str());
#endif
      this->on_fan_state_response(msg);
#endif
      break;
    }
    case 24: {
#ifdef USE_LIGHT
      LightStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_light_state_response: %s", msg.dump().c_str());
#endif
      this->on_light_state_response(msg);
#endif
      break;
    }
    case 25: {
#ifdef USE_SENSOR
      SensorStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_sensor_state_response: %s", msg.dump().c_str());
#endif
      this->on_sensor_state_response(msg);
#endif
      break;
    }
    case 26: {
#ifdef USE_SWITCH
      SwitchStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_switch_state_response: %s", msg.dump().c_str());
#endif
      this->on_switch_state_response(msg);
#endif
      break;
    }
    case 27: {
#ifdef USE_TEXT_SENSOR
      TextSensorStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_text_sensor_state_response: %s", msg.dump().c_str());
#endif
      this->on_text_sensor_state_response(msg);
#endif
      break;
    }
    case 29: {
      SubscribeLogsResponse msg;
      msg.decode(msg_data, msg_size);
      this->on_subscribe_logs_response(msg);
      break;
    }
    case 35: {
      HomeassistantServiceResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_homeassistant_service_response: %s", msg.dump().c_str());
#endif
      this->on_homeassistant_service_response(msg);
      break;
    }
    case 36: {
      GetTimeRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_get_time_request: %s", msg.dump().c_str());
#endif
      this->on_get_time_request(msg);
      break;
    }
    case 37: {
      GetTimeResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_get_time_response: %s", msg.dump().c_str());
#endif
      this->on_get_time_response(msg);
      break;
    }
    case 39: {
      SubscribeHomeAssistantStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_home_assistant_state_response: %s", msg.dump().c_str());
#endif
      this->on_subscribe_home_assistant_state_response(msg);
      break;
    }
    case 41: {
      ListEntitiesServicesResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_services_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_services_response(msg);
      break;
    }
    case 43: {
#ifdef USE_ESP32_CAMERA
      ListEntitiesCameraResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_camera_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_camera_response(msg);
#endif
      break;
    }
    case 44: {
#ifdef USE_ESP32_CAMERA
      CameraImageResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_camera_image_response: %s", msg.dump().c_str());
#endif
      this->on_camera_image_response(msg);
#endif
      break;
    }
    case 46: {
#ifdef USE_CLIMATE
      ListEntitiesClimateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_climate_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_climate_response(msg);
#endif
      break;
    }
    case 47: {
#ifdef USE_CLIMATE
      ClimateStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_climate_state_response: %s", msg.dump().c_str());
#endif
      this->on_climate_state_response(msg);
#endif
      break;
    }
    case 49: {
#ifdef USE_NUMBER
      ListEntitiesNumberResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_number_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_number_response(msg);
#endif
      break;
    }
    case 50: {
#ifdef USE_NUMBER
      NumberStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_number_state_response: %s", msg.dump().c_str());
#endif
      this->on_number_state_response(msg);
#endif
      break;
    }
    case 52: {
#ifdef USE_SELECT
      ListEntitiesSelectResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_select_response: %s", msg.dump().c_str());
#endif
      this->on_list_entities_select_response(msg);
#endif
      break;
    }
    case 53: {
#ifdef USE_SELECT
      SelectStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_select_state_response: %s", msg.dump().c_str());
#endif
      this->on_select_state_response(msg);
#endif
      break;
    }
    default:
      return false;
  }
  return true;
}

}  // namespace api_client
}  // namespace esphome
