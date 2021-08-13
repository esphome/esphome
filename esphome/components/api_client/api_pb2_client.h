// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#pragma once

#include "proto_client.h"
#include "../api/api_pb2.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace api {

class APIClientConnectionBase : public ProtoClient {
 public:
  bool send_hello_request(const HelloRequest &msg);
  virtual void on_hello_response(const HelloResponse &value){};
  bool send_connect_request(const ConnectRequest &msg);
  virtual void on_connect_response(const ConnectResponse &value){};
  bool send_disconnect_request(const DisconnectRequest &msg);
  virtual void on_disconnect_request(const DisconnectRequest &value){};
  bool send_disconnect_response(const DisconnectResponse &msg);
  virtual void on_disconnect_response(const DisconnectResponse &value){};
  bool send_ping_request(const PingRequest &msg);
  virtual void on_ping_request(const PingRequest &value){};
  bool send_ping_response(const PingResponse &msg);
  virtual void on_ping_response(const PingResponse &value){};
  bool send_device_info_request(const DeviceInfoRequest &msg);
  virtual void on_device_info_response(const DeviceInfoResponse &value){};
  bool send_list_entities_request(const ListEntitiesRequest &msg);
  virtual void on_list_entities_done_response(const ListEntitiesDoneResponse &value){};
  bool send_subscribe_states_request(const SubscribeStatesRequest &msg);
#ifdef USE_BINARY_SENSOR
  virtual void on_list_entities_binary_sensor_response(const ListEntitiesBinarySensorResponse &value){};
#endif
#ifdef USE_BINARY_SENSOR
  virtual void on_binary_sensor_state_response(const BinarySensorStateResponse &value){};
#endif
#ifdef USE_COVER
  virtual void on_list_entities_cover_response(const ListEntitiesCoverResponse &value){};
#endif
#ifdef USE_COVER
  virtual void on_cover_state_response(const CoverStateResponse &value){};
#endif
#ifdef USE_COVER
  bool send_cover_command_request(const CoverCommandRequest &msg);
#endif
#ifdef USE_FAN
  virtual void on_list_entities_fan_response(const ListEntitiesFanResponse &value){};
#endif
#ifdef USE_FAN
  virtual void on_fan_state_response(const FanStateResponse &value){};
#endif
#ifdef USE_FAN
  bool send_fan_command_request(const FanCommandRequest &msg);
#endif
#ifdef USE_LIGHT
  virtual void on_list_entities_light_response(const ListEntitiesLightResponse &value){};
#endif
#ifdef USE_LIGHT
  virtual void on_light_state_response(const LightStateResponse &value){};
#endif
#ifdef USE_LIGHT
  bool send_light_command_request(const LightCommandRequest &msg);
#endif
#ifdef USE_SENSOR
  virtual void on_list_entities_sensor_response(const ListEntitiesSensorResponse &value){};
#endif
#ifdef USE_SENSOR
  virtual void on_sensor_state_response(const SensorStateResponse &value){};
#endif
#ifdef USE_SWITCH
  virtual void on_list_entities_switch_response(const ListEntitiesSwitchResponse &value){};
#endif
#ifdef USE_SWITCH
  virtual void on_switch_state_response(const SwitchStateResponse &value){};
#endif
#ifdef USE_SWITCH
  bool send_switch_command_request(const SwitchCommandRequest &msg);
#endif
#ifdef USE_TEXT_SENSOR
  virtual void on_list_entities_text_sensor_response(const ListEntitiesTextSensorResponse &value){};
#endif
#ifdef USE_TEXT_SENSOR
  virtual void on_text_sensor_state_response(const TextSensorStateResponse &value){};
#endif
  bool send_subscribe_logs_request(const SubscribeLogsRequest &msg);
  virtual void on_subscribe_logs_response(const SubscribeLogsResponse &value){};
  bool send_subscribe_homeassistant_services_request(const SubscribeHomeassistantServicesRequest &msg);
  virtual void on_homeassistant_service_response(const HomeassistantServiceResponse &value){};
  bool send_subscribe_home_assistant_states_request(const SubscribeHomeAssistantStatesRequest &msg);
  virtual void on_subscribe_home_assistant_state_response(const SubscribeHomeAssistantStateResponse &value){};
  bool send_home_assistant_state_response(const HomeAssistantStateResponse &msg);
  bool send_get_time_request(const GetTimeRequest &msg);
  virtual void on_get_time_request(const GetTimeRequest &value){};
  bool send_get_time_response(const GetTimeResponse &msg);
  virtual void on_get_time_response(const GetTimeResponse &value){};
  virtual void on_list_entities_services_response(const ListEntitiesServicesResponse &value){};
  bool send_execute_service_request(const ExecuteServiceRequest &msg);
#ifdef USE_ESP32_CAMERA
  virtual void on_list_entities_camera_response(const ListEntitiesCameraResponse &value){};
#endif
#ifdef USE_ESP32_CAMERA
  virtual void on_camera_image_response(const CameraImageResponse &value){};
#endif
#ifdef USE_ESP32_CAMERA
  bool send_camera_image_request(const CameraImageRequest &msg);
#endif
#ifdef USE_CLIMATE
  virtual void on_list_entities_climate_response(const ListEntitiesClimateResponse &value){};
#endif
#ifdef USE_CLIMATE
  virtual void on_climate_state_response(const ClimateStateResponse &value){};
#endif
#ifdef USE_CLIMATE
  bool send_climate_command_request(const ClimateCommandRequest &msg);
#endif
#ifdef USE_NUMBER
  virtual void on_list_entities_number_response(const ListEntitiesNumberResponse &value){};
#endif
#ifdef USE_NUMBER
  virtual void on_number_state_response(const NumberStateResponse &value){};
#endif
#ifdef USE_NUMBER
  bool send_number_command_request(const NumberCommandRequest &msg);
#endif
#ifdef USE_SELECT
  virtual void on_list_entities_select_response(const ListEntitiesSelectResponse &value){};
#endif
#ifdef USE_SELECT
  virtual void on_select_state_response(const SelectStateResponse &value){};
#endif
#ifdef USE_SELECT
  bool send_select_command_request(const SelectCommandRequest &msg);
#endif
 protected:
  bool read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) override;
};

}  // namespace api
}  // namespace esphome
