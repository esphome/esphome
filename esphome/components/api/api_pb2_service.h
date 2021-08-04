// This file was automatically generated with a tool.
// See scripts/api_protobuf/api_protobuf.py
#pragma once

#include "api_pb2.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace api {

class APIServerConnectionBase : public ProtoService {
 public:
  virtual void on_hello_request(const HelloRequest &value){};
  bool send_hello_response(const HelloResponse &msg);
  virtual void on_connect_request(const ConnectRequest &value){};
  bool send_connect_response(const ConnectResponse &msg);
  bool send_disconnect_request(const DisconnectRequest &msg);
  virtual void on_disconnect_request(const DisconnectRequest &value){};
  bool send_disconnect_response(const DisconnectResponse &msg);
  virtual void on_disconnect_response(const DisconnectResponse &value){};
  bool send_ping_request(const PingRequest &msg);
  virtual void on_ping_request(const PingRequest &value){};
  bool send_ping_response(const PingResponse &msg);
  virtual void on_ping_response(const PingResponse &value){};
  virtual void on_device_info_request(const DeviceInfoRequest &value){};
  bool send_device_info_response(const DeviceInfoResponse &msg);
  virtual void on_list_entities_request(const ListEntitiesRequest &value){};
  bool send_list_entities_done_response(const ListEntitiesDoneResponse &msg);
  virtual void on_subscribe_states_request(const SubscribeStatesRequest &value){};
#ifdef USE_BINARY_SENSOR
  bool send_list_entities_binary_sensor_response(const ListEntitiesBinarySensorResponse &msg);
#endif
#ifdef USE_BINARY_SENSOR
  bool send_binary_sensor_state_response(const BinarySensorStateResponse &msg);
#endif
#ifdef USE_COVER
  bool send_list_entities_cover_response(const ListEntitiesCoverResponse &msg);
#endif
#ifdef USE_COVER
  bool send_cover_state_response(const CoverStateResponse &msg);
#endif
#ifdef USE_COVER
  virtual void on_cover_command_request(const CoverCommandRequest &value){};
#endif
#ifdef USE_FAN
  bool send_list_entities_fan_response(const ListEntitiesFanResponse &msg);
#endif
#ifdef USE_FAN
  bool send_fan_state_response(const FanStateResponse &msg);
#endif
#ifdef USE_FAN
  virtual void on_fan_command_request(const FanCommandRequest &value){};
#endif
#ifdef USE_LIGHT
  bool send_list_entities_light_response(const ListEntitiesLightResponse &msg);
#endif
#ifdef USE_LIGHT
  bool send_light_state_response(const LightStateResponse &msg);
#endif
#ifdef USE_LIGHT
  virtual void on_light_command_request(const LightCommandRequest &value){};
#endif
#ifdef USE_SENSOR
  bool send_list_entities_sensor_response(const ListEntitiesSensorResponse &msg);
#endif
#ifdef USE_SENSOR
  bool send_sensor_state_response(const SensorStateResponse &msg);
#endif
#ifdef USE_SWITCH
  bool send_list_entities_switch_response(const ListEntitiesSwitchResponse &msg);
#endif
#ifdef USE_SWITCH
  bool send_switch_state_response(const SwitchStateResponse &msg);
#endif
#ifdef USE_SWITCH
  virtual void on_switch_command_request(const SwitchCommandRequest &value){};
#endif
#ifdef USE_TEXT_SENSOR
  bool send_list_entities_text_sensor_response(const ListEntitiesTextSensorResponse &msg);
#endif
#ifdef USE_TEXT_SENSOR
  bool send_text_sensor_state_response(const TextSensorStateResponse &msg);
#endif
  virtual void on_subscribe_logs_request(const SubscribeLogsRequest &value){};
  bool send_subscribe_logs_response(const SubscribeLogsResponse &msg);
  virtual void on_subscribe_homeassistant_services_request(const SubscribeHomeassistantServicesRequest &value){};
  bool send_homeassistant_service_response(const HomeassistantServiceResponse &msg);
  virtual void on_subscribe_home_assistant_states_request(const SubscribeHomeAssistantStatesRequest &value){};
  bool send_subscribe_home_assistant_state_response(const SubscribeHomeAssistantStateResponse &msg);
  virtual void on_home_assistant_state_response(const HomeAssistantStateResponse &value){};
  bool send_get_time_request(const GetTimeRequest &msg);
  virtual void on_get_time_request(const GetTimeRequest &value){};
  bool send_get_time_response(const GetTimeResponse &msg);
  virtual void on_get_time_response(const GetTimeResponse &value){};
  bool send_list_entities_services_response(const ListEntitiesServicesResponse &msg);
  virtual void on_execute_service_request(const ExecuteServiceRequest &value){};
#ifdef USE_ESP32_CAMERA
  bool send_list_entities_camera_response(const ListEntitiesCameraResponse &msg);
#endif
#ifdef USE_ESP32_CAMERA
  bool send_camera_image_response(const CameraImageResponse &msg);
#endif
#ifdef USE_ESP32_CAMERA
  virtual void on_camera_image_request(const CameraImageRequest &value){};
#endif
#ifdef USE_CLIMATE
  bool send_list_entities_climate_response(const ListEntitiesClimateResponse &msg);
#endif
#ifdef USE_CLIMATE
  bool send_climate_state_response(const ClimateStateResponse &msg);
#endif
#ifdef USE_CLIMATE
  virtual void on_climate_command_request(const ClimateCommandRequest &value){};
#endif
#ifdef USE_NUMBER
  bool send_list_entities_number_response(const ListEntitiesNumberResponse &msg);
#endif
#ifdef USE_NUMBER
  bool send_number_state_response(const NumberStateResponse &msg);
#endif
#ifdef USE_NUMBER
  virtual void on_number_command_request(const NumberCommandRequest &value){};
#endif
#ifdef USE_SELECT
  bool send_list_entities_select_response(const ListEntitiesSelectResponse &msg);
#endif
#ifdef USE_SELECT
  bool send_select_state_response(const SelectStateResponse &msg);
#endif
#ifdef USE_SELECT
  virtual void on_select_command_request(const SelectCommandRequest &value){};
#endif
 protected:
  bool read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) override;
};

class APIServerConnection : public APIServerConnectionBase {
 public:
  virtual HelloResponse hello(const HelloRequest &msg) = 0;
  virtual ConnectResponse connect(const ConnectRequest &msg) = 0;
  virtual DisconnectResponse disconnect(const DisconnectRequest &msg) = 0;
  virtual PingResponse ping(const PingRequest &msg) = 0;
  virtual DeviceInfoResponse device_info(const DeviceInfoRequest &msg) = 0;
  virtual void list_entities(const ListEntitiesRequest &msg) = 0;
  virtual void subscribe_states(const SubscribeStatesRequest &msg) = 0;
  virtual void subscribe_logs(const SubscribeLogsRequest &msg) = 0;
  virtual void subscribe_homeassistant_services(const SubscribeHomeassistantServicesRequest &msg) = 0;
  virtual void subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) = 0;
  virtual GetTimeResponse get_time(const GetTimeRequest &msg) = 0;
  virtual void execute_service(const ExecuteServiceRequest &msg) = 0;
#ifdef USE_COVER
  virtual void cover_command(const CoverCommandRequest &msg) = 0;
#endif
#ifdef USE_FAN
  virtual void fan_command(const FanCommandRequest &msg) = 0;
#endif
#ifdef USE_LIGHT
  virtual void light_command(const LightCommandRequest &msg) = 0;
#endif
#ifdef USE_SWITCH
  virtual void switch_command(const SwitchCommandRequest &msg) = 0;
#endif
#ifdef USE_ESP32_CAMERA
  virtual void camera_image(const CameraImageRequest &msg) = 0;
#endif
#ifdef USE_CLIMATE
  virtual void climate_command(const ClimateCommandRequest &msg) = 0;
#endif
#ifdef USE_NUMBER
  virtual void number_command(const NumberCommandRequest &msg) = 0;
#endif
#ifdef USE_SELECT
  virtual void select_command(const SelectCommandRequest &msg) = 0;
#endif
 protected:
  void on_hello_request(const HelloRequest &msg) override;
  void on_connect_request(const ConnectRequest &msg) override;
  void on_disconnect_request(const DisconnectRequest &msg) override;
  void on_ping_request(const PingRequest &msg) override;
  void on_device_info_request(const DeviceInfoRequest &msg) override;
  void on_list_entities_request(const ListEntitiesRequest &msg) override;
  void on_subscribe_states_request(const SubscribeStatesRequest &msg) override;
  void on_subscribe_logs_request(const SubscribeLogsRequest &msg) override;
  void on_subscribe_homeassistant_services_request(const SubscribeHomeassistantServicesRequest &msg) override;
  void on_subscribe_home_assistant_states_request(const SubscribeHomeAssistantStatesRequest &msg) override;
  void on_get_time_request(const GetTimeRequest &msg) override;
  void on_execute_service_request(const ExecuteServiceRequest &msg) override;
#ifdef USE_COVER
  void on_cover_command_request(const CoverCommandRequest &msg) override;
#endif
#ifdef USE_FAN
  void on_fan_command_request(const FanCommandRequest &msg) override;
#endif
#ifdef USE_LIGHT
  void on_light_command_request(const LightCommandRequest &msg) override;
#endif
#ifdef USE_SWITCH
  void on_switch_command_request(const SwitchCommandRequest &msg) override;
#endif
#ifdef USE_ESP32_CAMERA
  void on_camera_image_request(const CameraImageRequest &msg) override;
#endif
#ifdef USE_CLIMATE
  void on_climate_command_request(const ClimateCommandRequest &msg) override;
#endif
#ifdef USE_NUMBER
  void on_number_command_request(const NumberCommandRequest &msg) override;
#endif
#ifdef USE_SELECT
  void on_select_command_request(const SelectCommandRequest &msg) override;
#endif
};

}  // namespace api
}  // namespace esphome
