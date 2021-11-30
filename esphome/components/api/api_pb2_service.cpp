// This file was automatically generated with a tool.
// See scripts/api_protobuf/api_protobuf.py
#include "api_pb2_service.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

static const char *const TAG = "api.service";

bool APIServerConnectionBase::send_hello_response(const HelloResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_hello_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<HelloResponse>(msg, 2);
}
bool APIServerConnectionBase::send_connect_response(const ConnectResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_connect_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ConnectResponse>(msg, 4);
}
bool APIServerConnectionBase::send_disconnect_request(const DisconnectRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_disconnect_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<DisconnectRequest>(msg, 5);
}
bool APIServerConnectionBase::send_disconnect_response(const DisconnectResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_disconnect_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<DisconnectResponse>(msg, 6);
}
bool APIServerConnectionBase::send_ping_request(const PingRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_ping_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<PingRequest>(msg, 7);
}
bool APIServerConnectionBase::send_ping_response(const PingResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_ping_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<PingResponse>(msg, 8);
}
bool APIServerConnectionBase::send_device_info_response(const DeviceInfoResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_device_info_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<DeviceInfoResponse>(msg, 10);
}
bool APIServerConnectionBase::send_list_entities_done_response(const ListEntitiesDoneResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_done_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesDoneResponse>(msg, 19);
}
#ifdef USE_BINARY_SENSOR
bool APIServerConnectionBase::send_list_entities_binary_sensor_response(const ListEntitiesBinarySensorResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_binary_sensor_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesBinarySensorResponse>(msg, 12);
}
#endif
#ifdef USE_BINARY_SENSOR
bool APIServerConnectionBase::send_binary_sensor_state_response(const BinarySensorStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_binary_sensor_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BinarySensorStateResponse>(msg, 21);
}
#endif
#ifdef USE_COVER
bool APIServerConnectionBase::send_list_entities_cover_response(const ListEntitiesCoverResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_cover_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesCoverResponse>(msg, 13);
}
#endif
#ifdef USE_COVER
bool APIServerConnectionBase::send_cover_state_response(const CoverStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_cover_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<CoverStateResponse>(msg, 22);
}
#endif
#ifdef USE_COVER
#endif
#ifdef USE_FAN
bool APIServerConnectionBase::send_list_entities_fan_response(const ListEntitiesFanResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_fan_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesFanResponse>(msg, 14);
}
#endif
#ifdef USE_FAN
bool APIServerConnectionBase::send_fan_state_response(const FanStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_fan_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<FanStateResponse>(msg, 23);
}
#endif
#ifdef USE_FAN
#endif
#ifdef USE_LIGHT
bool APIServerConnectionBase::send_list_entities_light_response(const ListEntitiesLightResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_light_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesLightResponse>(msg, 15);
}
#endif
#ifdef USE_LIGHT
bool APIServerConnectionBase::send_light_state_response(const LightStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_light_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<LightStateResponse>(msg, 24);
}
#endif
#ifdef USE_LIGHT
#endif
#ifdef USE_SENSOR
bool APIServerConnectionBase::send_list_entities_sensor_response(const ListEntitiesSensorResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_sensor_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesSensorResponse>(msg, 16);
}
#endif
#ifdef USE_SENSOR
bool APIServerConnectionBase::send_sensor_state_response(const SensorStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_sensor_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<SensorStateResponse>(msg, 25);
}
#endif
#ifdef USE_SWITCH
bool APIServerConnectionBase::send_list_entities_switch_response(const ListEntitiesSwitchResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_switch_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesSwitchResponse>(msg, 17);
}
#endif
#ifdef USE_SWITCH
bool APIServerConnectionBase::send_switch_state_response(const SwitchStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_switch_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<SwitchStateResponse>(msg, 26);
}
#endif
#ifdef USE_SWITCH
#endif
#ifdef USE_TEXT_SENSOR
bool APIServerConnectionBase::send_list_entities_text_sensor_response(const ListEntitiesTextSensorResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_text_sensor_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesTextSensorResponse>(msg, 18);
}
#endif
#ifdef USE_TEXT_SENSOR
bool APIServerConnectionBase::send_text_sensor_state_response(const TextSensorStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_text_sensor_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<TextSensorStateResponse>(msg, 27);
}
#endif
bool APIServerConnectionBase::send_subscribe_logs_response(const SubscribeLogsResponse &msg) {
  return this->send_message_<SubscribeLogsResponse>(msg, 29);
}
bool APIServerConnectionBase::send_homeassistant_service_response(const HomeassistantServiceResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_homeassistant_service_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<HomeassistantServiceResponse>(msg, 35);
}
bool APIServerConnectionBase::send_subscribe_home_assistant_state_response(
    const SubscribeHomeAssistantStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_subscribe_home_assistant_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<SubscribeHomeAssistantStateResponse>(msg, 39);
}
bool APIServerConnectionBase::send_get_time_request(const GetTimeRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_get_time_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<GetTimeRequest>(msg, 36);
}
bool APIServerConnectionBase::send_get_time_response(const GetTimeResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_get_time_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<GetTimeResponse>(msg, 37);
}
bool APIServerConnectionBase::send_list_entities_services_response(const ListEntitiesServicesResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_services_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesServicesResponse>(msg, 41);
}
#ifdef USE_ESP32_CAMERA
bool APIServerConnectionBase::send_list_entities_camera_response(const ListEntitiesCameraResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_camera_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesCameraResponse>(msg, 43);
}
#endif
#ifdef USE_ESP32_CAMERA
bool APIServerConnectionBase::send_camera_image_response(const CameraImageResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_camera_image_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<CameraImageResponse>(msg, 44);
}
#endif
#ifdef USE_ESP32_CAMERA
#endif
#ifdef USE_CLIMATE
bool APIServerConnectionBase::send_list_entities_climate_response(const ListEntitiesClimateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_climate_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesClimateResponse>(msg, 46);
}
#endif
#ifdef USE_CLIMATE
bool APIServerConnectionBase::send_climate_state_response(const ClimateStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_climate_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ClimateStateResponse>(msg, 47);
}
#endif
#ifdef USE_CLIMATE
#endif
#ifdef USE_NUMBER
bool APIServerConnectionBase::send_list_entities_number_response(const ListEntitiesNumberResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_number_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesNumberResponse>(msg, 49);
}
#endif
#ifdef USE_NUMBER
bool APIServerConnectionBase::send_number_state_response(const NumberStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_number_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<NumberStateResponse>(msg, 50);
}
#endif
#ifdef USE_NUMBER
#endif
#ifdef USE_SELECT
bool APIServerConnectionBase::send_list_entities_select_response(const ListEntitiesSelectResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_select_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesSelectResponse>(msg, 52);
}
#endif
#ifdef USE_SELECT
bool APIServerConnectionBase::send_select_state_response(const SelectStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_select_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<SelectStateResponse>(msg, 53);
}
#endif
#ifdef USE_SELECT
#endif
#ifdef USE_BUTTON
bool APIServerConnectionBase::send_list_entities_button_response(const ListEntitiesButtonResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_button_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesButtonResponse>(msg, 61);
}
#endif
#ifdef USE_BUTTON
#endif
bool APIServerConnectionBase::read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) {
  switch (msg_type) {
    case 1: {
      HelloRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_hello_request: %s", msg.dump().c_str());
#endif
      this->on_hello_request(msg);
      break;
    }
    case 3: {
      ConnectRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_connect_request: %s", msg.dump().c_str());
#endif
      this->on_connect_request(msg);
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
    case 9: {
      DeviceInfoRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_device_info_request: %s", msg.dump().c_str());
#endif
      this->on_device_info_request(msg);
      break;
    }
    case 11: {
      ListEntitiesRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_request: %s", msg.dump().c_str());
#endif
      this->on_list_entities_request(msg);
      break;
    }
    case 20: {
      SubscribeStatesRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_states_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_states_request(msg);
      break;
    }
    case 28: {
      SubscribeLogsRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_logs_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_logs_request(msg);
      break;
    }
    case 30: {
#ifdef USE_COVER
      CoverCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_cover_command_request: %s", msg.dump().c_str());
#endif
      this->on_cover_command_request(msg);
#endif
      break;
    }
    case 31: {
#ifdef USE_FAN
      FanCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_fan_command_request: %s", msg.dump().c_str());
#endif
      this->on_fan_command_request(msg);
#endif
      break;
    }
    case 32: {
#ifdef USE_LIGHT
      LightCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_light_command_request: %s", msg.dump().c_str());
#endif
      this->on_light_command_request(msg);
#endif
      break;
    }
    case 33: {
#ifdef USE_SWITCH
      SwitchCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_switch_command_request: %s", msg.dump().c_str());
#endif
      this->on_switch_command_request(msg);
#endif
      break;
    }
    case 34: {
      SubscribeHomeassistantServicesRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_homeassistant_services_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_homeassistant_services_request(msg);
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
    case 38: {
      SubscribeHomeAssistantStatesRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_home_assistant_states_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_home_assistant_states_request(msg);
      break;
    }
    case 40: {
      HomeAssistantStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_home_assistant_state_response: %s", msg.dump().c_str());
#endif
      this->on_home_assistant_state_response(msg);
      break;
    }
    case 42: {
      ExecuteServiceRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_execute_service_request: %s", msg.dump().c_str());
#endif
      this->on_execute_service_request(msg);
      break;
    }
    case 45: {
#ifdef USE_ESP32_CAMERA
      CameraImageRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_camera_image_request: %s", msg.dump().c_str());
#endif
      this->on_camera_image_request(msg);
#endif
      break;
    }
    case 48: {
#ifdef USE_CLIMATE
      ClimateCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_climate_command_request: %s", msg.dump().c_str());
#endif
      this->on_climate_command_request(msg);
#endif
      break;
    }
    case 51: {
#ifdef USE_NUMBER
      NumberCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_number_command_request: %s", msg.dump().c_str());
#endif
      this->on_number_command_request(msg);
#endif
      break;
    }
    case 54: {
#ifdef USE_SELECT
      SelectCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_select_command_request: %s", msg.dump().c_str());
#endif
      this->on_select_command_request(msg);
#endif
      break;
    }
    case 62: {
#ifdef USE_BUTTON
      ButtonCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_button_command_request: %s", msg.dump().c_str());
#endif
      this->on_button_command_request(msg);
#endif
      break;
    }
    default:
      return false;
  }
  return true;
}

void APIServerConnection::on_hello_request(const HelloRequest &msg) {
  HelloResponse ret = this->hello(msg);
  if (!this->send_hello_response(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_connect_request(const ConnectRequest &msg) {
  ConnectResponse ret = this->connect(msg);
  if (!this->send_connect_response(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_disconnect_request(const DisconnectRequest &msg) {
  DisconnectResponse ret = this->disconnect(msg);
  if (!this->send_disconnect_response(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_ping_request(const PingRequest &msg) {
  PingResponse ret = this->ping(msg);
  if (!this->send_ping_response(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_device_info_request(const DeviceInfoRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  DeviceInfoResponse ret = this->device_info(msg);
  if (!this->send_device_info_response(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_list_entities_request(const ListEntitiesRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->list_entities(msg);
}
void APIServerConnection::on_subscribe_states_request(const SubscribeStatesRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->subscribe_states(msg);
}
void APIServerConnection::on_subscribe_logs_request(const SubscribeLogsRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->subscribe_logs(msg);
}
void APIServerConnection::on_subscribe_homeassistant_services_request(
    const SubscribeHomeassistantServicesRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->subscribe_homeassistant_services(msg);
}
void APIServerConnection::on_subscribe_home_assistant_states_request(const SubscribeHomeAssistantStatesRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->subscribe_home_assistant_states(msg);
}
void APIServerConnection::on_get_time_request(const GetTimeRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  GetTimeResponse ret = this->get_time(msg);
  if (!this->send_get_time_response(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_execute_service_request(const ExecuteServiceRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->execute_service(msg);
}
#ifdef USE_COVER
void APIServerConnection::on_cover_command_request(const CoverCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->cover_command(msg);
}
#endif
#ifdef USE_FAN
void APIServerConnection::on_fan_command_request(const FanCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->fan_command(msg);
}
#endif
#ifdef USE_LIGHT
void APIServerConnection::on_light_command_request(const LightCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->light_command(msg);
}
#endif
#ifdef USE_SWITCH
void APIServerConnection::on_switch_command_request(const SwitchCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->switch_command(msg);
}
#endif
#ifdef USE_ESP32_CAMERA
void APIServerConnection::on_camera_image_request(const CameraImageRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->camera_image(msg);
}
#endif
#ifdef USE_CLIMATE
void APIServerConnection::on_climate_command_request(const ClimateCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->climate_command(msg);
}
#endif
#ifdef USE_NUMBER
void APIServerConnection::on_number_command_request(const NumberCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->number_command(msg);
}
#endif
#ifdef USE_SELECT
void APIServerConnection::on_select_command_request(const SelectCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->select_command(msg);
}
#endif
#ifdef USE_BUTTON
void APIServerConnection::on_button_command_request(const ButtonCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->button_command(msg);
}
#endif

}  // namespace api
}  // namespace esphome
