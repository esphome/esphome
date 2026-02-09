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
  void log_send_message_(const char *name, const char *dump);
  void log_receive_message_(const LogString *name, const ProtoMessage &msg);
  void log_receive_message_(const LogString *name);

 public:
#endif

  bool send_message(const ProtoMessage &msg, uint8_t message_type) {
#ifdef HAS_PROTO_MESSAGE_DUMP
    DumpBuffer dump_buf;
    this->log_send_message_(msg.message_name(), msg.dump_to(dump_buf));
#endif
    return this->send_message_impl(msg, message_type);
  }

  virtual void on_hello_request(const HelloRequest &value){};

  virtual void on_disconnect_request(){};
  virtual void on_disconnect_response(){};
  virtual void on_ping_request(){};
  virtual void on_ping_response(){};
  virtual void on_device_info_request(){};

  virtual void on_list_entities_request(){};

  virtual void on_subscribe_states_request(){};

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
  virtual void on_subscribe_homeassistant_services_request(){};
#endif

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  virtual void on_homeassistant_action_response(const HomeassistantActionResponse &value){};
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
  virtual void on_subscribe_home_assistant_states_request(){};
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

#ifdef USE_WATER_HEATER
  virtual void on_water_heater_command_request(const WaterHeaterCommandRequest &value){};
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
  virtual void on_subscribe_bluetooth_connections_free_request(){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  virtual void on_unsubscribe_bluetooth_le_advertisements_request(){};
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

#ifdef USE_IR_RF
  virtual void on_infrared_rf_transmit_raw_timings_request(const InfraredRFTransmitRawTimingsRequest &value){};
#endif

 protected:
  void read_message(uint32_t msg_size, uint32_t msg_type, const uint8_t *msg_data) override;
};

class APIServerConnection : public APIServerConnectionBase {
 protected:
  void read_message(uint32_t msg_size, uint32_t msg_type, const uint8_t *msg_data) override;
};

}  // namespace esphome::api
