// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#pragma once

#include "esphome/core/defines.h"

#include "api_pb2.h"

namespace esphome::api {

class APIServerConnectionBase {
 public:
#ifdef HAS_PROTO_MESSAGE_DUMP
 protected:
  void log_send_message_(const LogString *name, const char *dump);
  void log_receive_message_(const LogString *name, const ProtoMessage &msg);
  void log_receive_message_(const LogString *name);

 public:
#endif

  void on_hello_request(const HelloRequest &value){};

  void on_disconnect_request(){};
  void on_disconnect_response(){};
  void on_ping_request(){};
  void on_ping_response(){};
  void on_device_info_request(){};

  void on_list_entities_request(){};

  void on_subscribe_states_request(){};

#ifdef USE_COVER
  void on_cover_command_request(const CoverCommandRequest &value){};
#endif

#ifdef USE_FAN
  void on_fan_command_request(const FanCommandRequest &value){};
#endif

#ifdef USE_LIGHT
  void on_light_command_request(const LightCommandRequest &value){};
#endif

#ifdef USE_SWITCH
  void on_switch_command_request(const SwitchCommandRequest &value){};
#endif

  void on_subscribe_logs_request(const SubscribeLogsRequest &value){};

#ifdef USE_API_NOISE
  void on_noise_encryption_set_key_request(const NoiseEncryptionSetKeyRequest &value){};
#endif

#ifdef USE_API_HOMEASSISTANT_SERVICES
  void on_subscribe_homeassistant_services_request(){};
#endif

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  void on_homeassistant_action_response(const HomeassistantActionResponse &value){};
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
  void on_subscribe_home_assistant_states_request(){};
#endif

#ifdef USE_API_HOMEASSISTANT_STATES
  void on_home_assistant_state_response(const HomeAssistantStateResponse &value){};
#endif

  void on_get_time_response(const GetTimeResponse &value){};

#ifdef USE_API_USER_DEFINED_ACTIONS
  void on_execute_service_request(const ExecuteServiceRequest &value){};
#endif

#ifdef USE_CAMERA
  void on_camera_image_request(const CameraImageRequest &value){};
#endif

#ifdef USE_CLIMATE
  void on_climate_command_request(const ClimateCommandRequest &value){};
#endif

#ifdef USE_WATER_HEATER
  void on_water_heater_command_request(const WaterHeaterCommandRequest &value){};
#endif

#ifdef USE_NUMBER
  void on_number_command_request(const NumberCommandRequest &value){};
#endif

#ifdef USE_SELECT
  void on_select_command_request(const SelectCommandRequest &value){};
#endif

#ifdef USE_SIREN
  void on_siren_command_request(const SirenCommandRequest &value){};
#endif

#ifdef USE_LOCK
  void on_lock_command_request(const LockCommandRequest &value){};
#endif

#ifdef USE_BUTTON
  void on_button_command_request(const ButtonCommandRequest &value){};
#endif

#ifdef USE_MEDIA_PLAYER
  void on_media_player_command_request(const MediaPlayerCommandRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_subscribe_bluetooth_le_advertisements_request(const SubscribeBluetoothLEAdvertisementsRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_device_request(const BluetoothDeviceRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_get_services_request(const BluetoothGATTGetServicesRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_read_request(const BluetoothGATTReadRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_write_request(const BluetoothGATTWriteRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_read_descriptor_request(const BluetoothGATTReadDescriptorRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_write_descriptor_request(const BluetoothGATTWriteDescriptorRequest &value){};
#endif
#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_gatt_notify_request(const BluetoothGATTNotifyRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  void on_subscribe_bluetooth_connections_free_request(){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  void on_unsubscribe_bluetooth_le_advertisements_request(){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_scanner_set_mode_request(const BluetoothScannerSetModeRequest &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  void on_subscribe_voice_assistant_request(const SubscribeVoiceAssistantRequest &value){};
#endif

#ifdef USE_VOICE_ASSISTANT
  void on_voice_assistant_response(const VoiceAssistantResponse &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  void on_voice_assistant_event_response(const VoiceAssistantEventResponse &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  void on_voice_assistant_audio(const VoiceAssistantAudio &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  void on_voice_assistant_timer_event_response(const VoiceAssistantTimerEventResponse &value){};
#endif
#ifdef USE_VOICE_ASSISTANT
  void on_voice_assistant_announce_request(const VoiceAssistantAnnounceRequest &value){};
#endif

#ifdef USE_VOICE_ASSISTANT
  void on_voice_assistant_configuration_request(const VoiceAssistantConfigurationRequest &value){};
#endif

#ifdef USE_VOICE_ASSISTANT
  void on_voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &value){};
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  void on_alarm_control_panel_command_request(const AlarmControlPanelCommandRequest &value){};
#endif

#ifdef USE_TEXT
  void on_text_command_request(const TextCommandRequest &value){};
#endif

#ifdef USE_DATETIME_DATE
  void on_date_command_request(const DateCommandRequest &value){};
#endif

#ifdef USE_DATETIME_TIME
  void on_time_command_request(const TimeCommandRequest &value){};
#endif

#ifdef USE_VALVE
  void on_valve_command_request(const ValveCommandRequest &value){};
#endif

#ifdef USE_DATETIME_DATETIME
  void on_date_time_command_request(const DateTimeCommandRequest &value){};
#endif

#ifdef USE_UPDATE
  void on_update_command_request(const UpdateCommandRequest &value){};
#endif
#ifdef USE_ZWAVE_PROXY
  void on_z_wave_proxy_frame(const ZWaveProxyFrame &value){};
#endif
#ifdef USE_ZWAVE_PROXY
  void on_z_wave_proxy_request(const ZWaveProxyRequest &value){};
#endif

#if defined(USE_IR_RF) || defined(USE_RADIO_FREQUENCY)
  void on_infrared_rf_transmit_raw_timings_request(const InfraredRFTransmitRawTimingsRequest &value){};
#endif

#ifdef USE_SERIAL_PROXY
  void on_serial_proxy_configure_request(const SerialProxyConfigureRequest &value){};
#endif

#ifdef USE_SERIAL_PROXY
  void on_serial_proxy_write_request(const SerialProxyWriteRequest &value){};
#endif
#ifdef USE_SERIAL_PROXY
  void on_serial_proxy_set_modem_pins_request(const SerialProxySetModemPinsRequest &value){};
#endif
#ifdef USE_SERIAL_PROXY
  void on_serial_proxy_get_modem_pins_request(const SerialProxyGetModemPinsRequest &value){};
#endif

#ifdef USE_SERIAL_PROXY
  void on_serial_proxy_request(const SerialProxyRequest &value){};
#endif

#ifdef USE_BLUETOOTH_PROXY
  void on_bluetooth_set_connection_params_request(const BluetoothSetConnectionParamsRequest &value){};
#endif
};

}  // namespace esphome::api
