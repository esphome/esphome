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
#ifdef USE_LOCK
  bool send_list_entities_lock_response(const ListEntitiesLockResponse &msg);
#endif
#ifdef USE_LOCK
  bool send_lock_state_response(const LockStateResponse &msg);
#endif
#ifdef USE_LOCK
  virtual void on_lock_command_request(const LockCommandRequest &value){};
#endif
#ifdef USE_BUTTON
  bool send_list_entities_button_response(const ListEntitiesButtonResponse &msg);
#endif
#ifdef USE_BUTTON
  virtual void on_button_command_request(const ButtonCommandRequest &value){};
#endif
#ifdef USE_MEDIA_PLAYER
  bool send_list_entities_media_player_response(const ListEntitiesMediaPlayerResponse &msg);
#endif
#ifdef USE_MEDIA_PLAYER
  bool send_media_player_state_response(const MediaPlayerStateResponse &msg);
#endif
#ifdef USE_MEDIA_PLAYER
  virtual void on_media_player_command_request(const MediaPlayerCommandRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_subscribe_bluetooth_le_advertisements_request(
      const SubscribeBluetoothLEAdvertisementsRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_le_advertisement_response(const BluetoothLEAdvertisementResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_le_raw_advertisements_response(const BluetoothLERawAdvertisementsResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_device_request(const BluetoothDeviceRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_device_connection_response(const BluetoothDeviceConnectionResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_gatt_get_services_request(const BluetoothGATTGetServicesRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_gatt_get_services_response(const BluetoothGATTGetServicesResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_gatt_get_services_done_response(const BluetoothGATTGetServicesDoneResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_gatt_read_request(const BluetoothGATTReadRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_gatt_read_response(const BluetoothGATTReadResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_gatt_write_request(const BluetoothGATTWriteRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_gatt_read_descriptor_request(const BluetoothGATTReadDescriptorRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_gatt_write_descriptor_request(const BluetoothGATTWriteDescriptorRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_gatt_notify_request(const BluetoothGATTNotifyRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_gatt_notify_data_response(const BluetoothGATTNotifyDataResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_subscribe_bluetooth_connections_free_request(const SubscribeBluetoothConnectionsFreeRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_connections_free_response(const BluetoothConnectionsFreeResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_gatt_error_response(const BluetoothGATTErrorResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_gatt_write_response(const BluetoothGATTWriteResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_gatt_notify_response(const BluetoothGATTNotifyResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_device_pairing_response(const BluetoothDevicePairingResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_device_unpairing_response(const BluetoothDeviceUnpairingResponse &msg);
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_unsubscribe_bluetooth_le_advertisements_request(
      const UnsubscribeBluetoothLEAdvertisementsRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  bool send_bluetooth_device_clear_cache_response(const BluetoothDeviceClearCacheResponse &msg);
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void on_subscribe_voice_assistant_request(const SubscribeVoiceAssistantRequest &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  bool send_voice_assistant_request(const VoiceAssistantRequest &msg);
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void on_voice_assistant_response(const VoiceAssistantResponse &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void on_voice_assistant_event_response(const VoiceAssistantEventResponse &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  bool send_voice_assistant_audio(const VoiceAssistantAudio &msg);
  virtual void on_voice_assistant_audio(const VoiceAssistantAudio &value){};
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  bool send_list_entities_alarm_control_panel_response(const ListEntitiesAlarmControlPanelResponse &msg);
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  bool send_alarm_control_panel_state_response(const AlarmControlPanelStateResponse &msg);
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  virtual void on_alarm_control_panel_command_request(const AlarmControlPanelCommandRequest &value){};
#endif
#ifdef USE_TEXT
  bool send_list_entities_text_response(const ListEntitiesTextResponse &msg);
#endif
#ifdef USE_TEXT
  bool send_text_state_response(const TextStateResponse &msg);
#endif
#ifdef USE_TEXT
  virtual void on_text_command_request(const TextCommandRequest &value){};
#endif
#ifdef USE_DATETIME_DATE
  bool send_list_entities_date_response(const ListEntitiesDateResponse &msg);
#endif
#ifdef USE_DATETIME_DATE
  bool send_date_state_response(const DateStateResponse &msg);
#endif
#ifdef USE_DATETIME_DATE
  virtual void on_date_command_request(const DateCommandRequest &value){};
#endif
#ifdef USE_DATETIME_TIME
  bool send_list_entities_time_response(const ListEntitiesTimeResponse &msg);
#endif
#ifdef USE_DATETIME_TIME
  bool send_time_state_response(const TimeStateResponse &msg);
#endif
#ifdef USE_DATETIME_TIME
  virtual void on_time_command_request(const TimeCommandRequest &value){};
#endif
#ifdef USE_VALVE
  bool send_list_entities_valve_response(const ListEntitiesValveResponse &msg);
#endif
#ifdef USE_VALVE
  bool send_valve_state_response(const ValveStateResponse &msg);
#endif
#ifdef USE_VALVE
  virtual void on_valve_command_request(const ValveCommandRequest &value){};
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
#ifdef USE_TEXT
  virtual void text_command(const TextCommandRequest &msg) = 0;
#endif
#ifdef USE_SELECT
  virtual void select_command(const SelectCommandRequest &msg) = 0;
#endif
#ifdef USE_BUTTON
  virtual void button_command(const ButtonCommandRequest &msg) = 0;
#endif
#ifdef USE_LOCK
  virtual void lock_command(const LockCommandRequest &msg) = 0;
#endif
#ifdef USE_VALVE
  virtual void valve_command(const ValveCommandRequest &msg) = 0;
#endif
#ifdef USE_MEDIA_PLAYER
  virtual void media_player_command(const MediaPlayerCommandRequest &msg) = 0;
#endif
#ifdef USE_DATETIME_DATE
  virtual void date_command(const DateCommandRequest &msg) = 0;
#endif
#ifdef USE_DATETIME_TIME
  virtual void time_command(const TimeCommandRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void subscribe_bluetooth_le_advertisements(const SubscribeBluetoothLEAdvertisementsRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void bluetooth_device_request(const BluetoothDeviceRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void bluetooth_gatt_get_services(const BluetoothGATTGetServicesRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void bluetooth_gatt_read(const BluetoothGATTReadRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void bluetooth_gatt_write(const BluetoothGATTWriteRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void bluetooth_gatt_read_descriptor(const BluetoothGATTReadDescriptorRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void bluetooth_gatt_write_descriptor(const BluetoothGATTWriteDescriptorRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void bluetooth_gatt_notify(const BluetoothGATTNotifyRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual BluetoothConnectionsFreeResponse subscribe_bluetooth_connections_free(
      const SubscribeBluetoothConnectionsFreeRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void unsubscribe_bluetooth_le_advertisements(const UnsubscribeBluetoothLEAdvertisementsRequest &msg) = 0;
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void subscribe_voice_assistant(const SubscribeVoiceAssistantRequest &msg) = 0;
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  virtual void alarm_control_panel_command(const AlarmControlPanelCommandRequest &msg) = 0;
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
#ifdef USE_TEXT
  void on_text_command_request(const TextCommandRequest &msg) override;
#endif
#ifdef USE_SELECT
  void on_select_command_request(const SelectCommandRequest &msg) override;
#endif
#ifdef USE_BUTTON
  void on_button_command_request(const ButtonCommandRequest &msg) override;
#endif
#ifdef USE_LOCK
  void on_lock_command_request(const LockCommandRequest &msg) override;
#endif
#ifdef USE_VALVE
  void on_valve_command_request(const ValveCommandRequest &msg) override;
#endif
#ifdef USE_MEDIA_PLAYER
  void on_media_player_command_request(const MediaPlayerCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_DATE
  void on_date_command_request(const DateCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_TIME
  void on_time_command_request(const TimeCommandRequest &msg) override;
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_subscribe_bluetooth_le_advertisements_request(const SubscribeBluetoothLEAdvertisementsRequest &msg) override;
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_device_request(const BluetoothDeviceRequest &msg) override;
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_get_services_request(const BluetoothGATTGetServicesRequest &msg) override;
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_read_request(const BluetoothGATTReadRequest &msg) override;
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_write_request(const BluetoothGATTWriteRequest &msg) override;
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_read_descriptor_request(const BluetoothGATTReadDescriptorRequest &msg) override;
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_write_descriptor_request(const BluetoothGATTWriteDescriptorRequest &msg) override;
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_notify_request(const BluetoothGATTNotifyRequest &msg) override;
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_subscribe_bluetooth_connections_free_request(const SubscribeBluetoothConnectionsFreeRequest &msg) override;
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_unsubscribe_bluetooth_le_advertisements_request(
      const UnsubscribeBluetoothLEAdvertisementsRequest &msg) override;
#endif
#ifdef USE_VOICE_ASSISTANT
  void on_subscribe_voice_assistant_request(const SubscribeVoiceAssistantRequest &msg) override;
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  void on_alarm_control_panel_command_request(const AlarmControlPanelCommandRequest &msg) override;
#endif
};

}  // namespace api
}  // namespace esphome
