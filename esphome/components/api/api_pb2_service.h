// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#pragma once

#include "esphome/core/defines.h"

#include "api_pb2.h"

namespace esphome::api {

class APIServerConnectionBase : public ProtoService {
 public:
#ifdef HAS_PROTO_MESSAGE_DUMP
 protected:
  void log_send_message_(const char *name, const std::string &dump);

 public:
#endif

  bool send_message(const ProtoMessage &msg, uint8_t message_type) {
#ifdef HAS_PROTO_MESSAGE_DUMP
    this->log_send_message_(msg.message_name(), msg.dump());
#endif
    return this->send_message_(msg, message_type);
  }

  virtual void on_hello_request(const HelloRequest &value){};

#ifdef USE_API_PASSWORD
  virtual void on_authentication_request(const AuthenticationRequest &value){};
#endif

  virtual void on_disconnect_request(const DisconnectRequest &value){};
  virtual void on_disconnect_response(const DisconnectResponse &value){};
  virtual void on_ping_request(const PingRequest &value){};
  virtual void on_ping_response(const PingResponse &value){};
  virtual void on_device_info_request(const DeviceInfoRequest &value){};

  virtual void on_list_entities_request(const ListEntitiesRequest &value){};

  virtual void on_subscribe_states_request(const SubscribeStatesRequest &value){};

#ifdef USE_COVER
  virtual void on_cover_command_request(const CoverCommandRequest &value){};
#endif

#ifdef USE_FAN
  virtual void on_fan_command_request(const FanCommandRequest &value){};
#endif

#ifdef USE_LIGHT
  virtual void on_light_command_request(const LightCommandRequest &value){};
#endif

#ifdef USE_SWITCH
  virtual void on_switch_command_request(const SwitchCommandRequest &value){};
#endif

  virtual void on_subscribe_logs_request(const SubscribeLogsRequest &value){};

#ifdef USE_API_NOISE
  virtual void on_noise_encryption_set_key_request(const NoiseEncryptionSetKeyRequest &value){};
#endif

#ifdef USE_API_HOMEASSISTANT_SERVICES
  virtual void on_subscribe_homeassistant_services_request(const SubscribeHomeassistantServicesRequest &value){};
#endif

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  virtual void on_homeassistant_action_response(const HomeassistantActionResponse &value){};
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
  virtual void on_subscribe_home_assistant_states_request(const SubscribeHomeAssistantStatesRequest &value){};
#endif

#ifdef USE_API_HOMEASSISTANT_STATES
  virtual void on_home_assistant_state_response(const HomeAssistantStateResponse &value){};
#endif

  virtual void on_get_time_response(const GetTimeResponse &value){};

#ifdef USE_API_USER_DEFINED_ACTIONS
  virtual void on_execute_service_request(const ExecuteServiceRequest &value){};
#endif

#ifdef USE_CAMERA
  virtual void on_camera_image_request(const CameraImageRequest &value){};
#endif

#ifdef USE_CLIMATE
  virtual void on_climate_command_request(const ClimateCommandRequest &value){};
#endif

#ifdef USE_NUMBER
  virtual void on_number_command_request(const NumberCommandRequest &value){};
#endif

#ifdef USE_SELECT
  virtual void on_select_command_request(const SelectCommandRequest &value){};
#endif

#ifdef USE_SIREN
  virtual void on_siren_command_request(const SirenCommandRequest &value){};
#endif

#ifdef USE_LOCK
  virtual void on_lock_command_request(const LockCommandRequest &value){};
#endif

#ifdef USE_BUTTON
  virtual void on_button_command_request(const ButtonCommandRequest &value){};
#endif

#ifdef USE_MEDIA_PLAYER
  virtual void on_media_player_command_request(const MediaPlayerCommandRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void on_subscribe_bluetooth_le_advertisements_request(
      const SubscribeBluetoothLEAdvertisementsRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_device_request(const BluetoothDeviceRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_gatt_get_services_request(const BluetoothGATTGetServicesRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_gatt_read_request(const BluetoothGATTReadRequest &value){};
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
  virtual void on_subscribe_bluetooth_connections_free_request(const SubscribeBluetoothConnectionsFreeRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  virtual void on_unsubscribe_bluetooth_le_advertisements_request(
      const UnsubscribeBluetoothLEAdvertisementsRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  virtual void on_bluetooth_scanner_set_mode_request(const BluetoothScannerSetModeRequest &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void on_subscribe_voice_assistant_request(const SubscribeVoiceAssistantRequest &value){};
#endif

#ifdef USE_VOICE_ASSISTANT
  virtual void on_voice_assistant_response(const VoiceAssistantResponse &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void on_voice_assistant_event_response(const VoiceAssistantEventResponse &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void on_voice_assistant_audio(const VoiceAssistantAudio &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void on_voice_assistant_timer_event_response(const VoiceAssistantTimerEventResponse &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void on_voice_assistant_announce_request(const VoiceAssistantAnnounceRequest &value){};
#endif

#ifdef USE_VOICE_ASSISTANT
  virtual void on_voice_assistant_configuration_request(const VoiceAssistantConfigurationRequest &value){};
#endif

#ifdef USE_VOICE_ASSISTANT
  virtual void on_voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &value){};
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  virtual void on_alarm_control_panel_command_request(const AlarmControlPanelCommandRequest &value){};
#endif

#ifdef USE_TEXT
  virtual void on_text_command_request(const TextCommandRequest &value){};
#endif

#ifdef USE_DATETIME_DATE
  virtual void on_date_command_request(const DateCommandRequest &value){};
#endif

#ifdef USE_DATETIME_TIME
  virtual void on_time_command_request(const TimeCommandRequest &value){};
#endif

#ifdef USE_VALVE
  virtual void on_valve_command_request(const ValveCommandRequest &value){};
#endif

#ifdef USE_DATETIME_DATETIME
  virtual void on_date_time_command_request(const DateTimeCommandRequest &value){};
#endif

#ifdef USE_UPDATE
  virtual void on_update_command_request(const UpdateCommandRequest &value){};
#endif
#ifdef USE_ZWAVE_PROXY
  virtual void on_z_wave_proxy_frame(const ZWaveProxyFrame &value){};
#endif
#ifdef USE_ZWAVE_PROXY
  virtual void on_z_wave_proxy_request(const ZWaveProxyRequest &value){};
#endif
 protected:
  void read_message(uint32_t msg_size, uint32_t msg_type, const uint8_t *msg_data) override;
};

class APIServerConnection : public APIServerConnectionBase {
 public:
  virtual bool send_hello_response(const HelloRequest &msg) = 0;
#ifdef USE_API_PASSWORD
  virtual bool send_authenticate_response(const AuthenticationRequest &msg) = 0;
#endif
  virtual bool send_disconnect_response(const DisconnectRequest &msg) = 0;
  virtual bool send_ping_response(const PingRequest &msg) = 0;
  virtual bool send_device_info_response(const DeviceInfoRequest &msg) = 0;
  virtual void list_entities(const ListEntitiesRequest &msg) = 0;
  virtual void subscribe_states(const SubscribeStatesRequest &msg) = 0;
  virtual void subscribe_logs(const SubscribeLogsRequest &msg) = 0;
#ifdef USE_API_HOMEASSISTANT_SERVICES
  virtual void subscribe_homeassistant_services(const SubscribeHomeassistantServicesRequest &msg) = 0;
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
  virtual void subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) = 0;
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
  virtual void execute_service(const ExecuteServiceRequest &msg) = 0;
#endif
#ifdef USE_API_NOISE
  virtual bool send_noise_encryption_set_key_response(const NoiseEncryptionSetKeyRequest &msg) = 0;
#endif
#ifdef USE_BUTTON
  virtual void button_command(const ButtonCommandRequest &msg) = 0;
#endif
#ifdef USE_CAMERA
  virtual void camera_image(const CameraImageRequest &msg) = 0;
#endif
#ifdef USE_CLIMATE
  virtual void climate_command(const ClimateCommandRequest &msg) = 0;
#endif
#ifdef USE_COVER
  virtual void cover_command(const CoverCommandRequest &msg) = 0;
#endif
#ifdef USE_DATETIME_DATE
  virtual void date_command(const DateCommandRequest &msg) = 0;
#endif
#ifdef USE_DATETIME_DATETIME
  virtual void datetime_command(const DateTimeCommandRequest &msg) = 0;
#endif
#ifdef USE_FAN
  virtual void fan_command(const FanCommandRequest &msg) = 0;
#endif
#ifdef USE_LIGHT
  virtual void light_command(const LightCommandRequest &msg) = 0;
#endif
#ifdef USE_LOCK
  virtual void lock_command(const LockCommandRequest &msg) = 0;
#endif
#ifdef USE_MEDIA_PLAYER
  virtual void media_player_command(const MediaPlayerCommandRequest &msg) = 0;
#endif
#ifdef USE_NUMBER
  virtual void number_command(const NumberCommandRequest &msg) = 0;
#endif
#ifdef USE_SELECT
  virtual void select_command(const SelectCommandRequest &msg) = 0;
#endif
#ifdef USE_SIREN
  virtual void siren_command(const SirenCommandRequest &msg) = 0;
#endif
#ifdef USE_SWITCH
  virtual void switch_command(const SwitchCommandRequest &msg) = 0;
#endif
#ifdef USE_TEXT
  virtual void text_command(const TextCommandRequest &msg) = 0;
#endif
#ifdef USE_DATETIME_TIME
  virtual void time_command(const TimeCommandRequest &msg) = 0;
#endif
#ifdef USE_UPDATE
  virtual void update_command(const UpdateCommandRequest &msg) = 0;
#endif
#ifdef USE_VALVE
  virtual void valve_command(const ValveCommandRequest &msg) = 0;
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
  virtual bool send_subscribe_bluetooth_connections_free_response(
      const SubscribeBluetoothConnectionsFreeRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void unsubscribe_bluetooth_le_advertisements(const UnsubscribeBluetoothLEAdvertisementsRequest &msg) = 0;
#endif
#ifdef USE_BLUETOOTH_PROXY
  virtual void bluetooth_scanner_set_mode(const BluetoothScannerSetModeRequest &msg) = 0;
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void subscribe_voice_assistant(const SubscribeVoiceAssistantRequest &msg) = 0;
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual bool send_voice_assistant_get_configuration_response(const VoiceAssistantConfigurationRequest &msg) = 0;
#endif
#ifdef USE_VOICE_ASSISTANT
  virtual void voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) = 0;
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  virtual void alarm_control_panel_command(const AlarmControlPanelCommandRequest &msg) = 0;
#endif
#ifdef USE_ZWAVE_PROXY
  virtual void zwave_proxy_frame(const ZWaveProxyFrame &msg) = 0;
#endif
#ifdef USE_ZWAVE_PROXY
  virtual void zwave_proxy_request(const ZWaveProxyRequest &msg) = 0;
#endif
 protected:
  void on_hello_request(const HelloRequest &msg) override;
#ifdef USE_API_PASSWORD
  void on_authentication_request(const AuthenticationRequest &msg) override;
#endif
  void on_disconnect_request(const DisconnectRequest &msg) override;
  void on_ping_request(const PingRequest &msg) override;
  void on_device_info_request(const DeviceInfoRequest &msg) override;
  void on_list_entities_request(const ListEntitiesRequest &msg) override;
  void on_subscribe_states_request(const SubscribeStatesRequest &msg) override;
  void on_subscribe_logs_request(const SubscribeLogsRequest &msg) override;
#ifdef USE_API_HOMEASSISTANT_SERVICES
  void on_subscribe_homeassistant_services_request(const SubscribeHomeassistantServicesRequest &msg) override;
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
  void on_subscribe_home_assistant_states_request(const SubscribeHomeAssistantStatesRequest &msg) override;
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
  void on_execute_service_request(const ExecuteServiceRequest &msg) override;
#endif
#ifdef USE_API_NOISE
  void on_noise_encryption_set_key_request(const NoiseEncryptionSetKeyRequest &msg) override;
#endif
#ifdef USE_BUTTON
  void on_button_command_request(const ButtonCommandRequest &msg) override;
#endif
#ifdef USE_CAMERA
  void on_camera_image_request(const CameraImageRequest &msg) override;
#endif
#ifdef USE_CLIMATE
  void on_climate_command_request(const ClimateCommandRequest &msg) override;
#endif
#ifdef USE_COVER
  void on_cover_command_request(const CoverCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_DATE
  void on_date_command_request(const DateCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_DATETIME
  void on_date_time_command_request(const DateTimeCommandRequest &msg) override;
#endif
#ifdef USE_FAN
  void on_fan_command_request(const FanCommandRequest &msg) override;
#endif
#ifdef USE_LIGHT
  void on_light_command_request(const LightCommandRequest &msg) override;
#endif
#ifdef USE_LOCK
  void on_lock_command_request(const LockCommandRequest &msg) override;
#endif
#ifdef USE_MEDIA_PLAYER
  void on_media_player_command_request(const MediaPlayerCommandRequest &msg) override;
#endif
#ifdef USE_NUMBER
  void on_number_command_request(const NumberCommandRequest &msg) override;
#endif
#ifdef USE_SELECT
  void on_select_command_request(const SelectCommandRequest &msg) override;
#endif
#ifdef USE_SIREN
  void on_siren_command_request(const SirenCommandRequest &msg) override;
#endif
#ifdef USE_SWITCH
  void on_switch_command_request(const SwitchCommandRequest &msg) override;
#endif
#ifdef USE_TEXT
  void on_text_command_request(const TextCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_TIME
  void on_time_command_request(const TimeCommandRequest &msg) override;
#endif
#ifdef USE_UPDATE
  void on_update_command_request(const UpdateCommandRequest &msg) override;
#endif
#ifdef USE_VALVE
  void on_valve_command_request(const ValveCommandRequest &msg) override;
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
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_scanner_set_mode_request(const BluetoothScannerSetModeRequest &msg) override;
#endif
#ifdef USE_VOICE_ASSISTANT
  void on_subscribe_voice_assistant_request(const SubscribeVoiceAssistantRequest &msg) override;
#endif
#ifdef USE_VOICE_ASSISTANT
  void on_voice_assistant_configuration_request(const VoiceAssistantConfigurationRequest &msg) override;
#endif
#ifdef USE_VOICE_ASSISTANT
  void on_voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) override;
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  void on_alarm_control_panel_command_request(const AlarmControlPanelCommandRequest &msg) override;
#endif
#ifdef USE_ZWAVE_PROXY
  void on_z_wave_proxy_frame(const ZWaveProxyFrame &msg) override;
#endif
#ifdef USE_ZWAVE_PROXY
  void on_z_wave_proxy_request(const ZWaveProxyRequest &msg) override;
#endif
  void read_message(uint32_t msg_size, uint32_t msg_type, const uint8_t *msg_data) override;
};

}  // namespace esphome::api
