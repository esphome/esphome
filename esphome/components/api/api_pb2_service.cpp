// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2_service.h"
#include "esphome/core/log.h"

namespace esphome::api {

static const char *const TAG = "api.service";

#ifdef HAS_PROTO_MESSAGE_DUMP
void APIServerConnectionBase::log_send_message_(const char *name, const std::string &dump) {
  ESP_LOGVV(TAG, "send_message %s: %s", name, dump.c_str());
}
#endif

void APIServerConnectionBase::read_message(uint32_t msg_size, uint32_t msg_type, const uint8_t *msg_data) {
  switch (msg_type) {
    case HelloRequest::MESSAGE_TYPE: {
      HelloRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_hello_request: %s", msg.dump().c_str());
#endif
      this->on_hello_request(msg);
      break;
    }
#ifdef USE_API_PASSWORD
    case AuthenticationRequest::MESSAGE_TYPE: {
      AuthenticationRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_authentication_request: %s", msg.dump().c_str());
#endif
      this->on_authentication_request(msg);
      break;
    }
#endif
    case DisconnectRequest::MESSAGE_TYPE: {
      DisconnectRequest msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_disconnect_request: %s", msg.dump().c_str());
#endif
      this->on_disconnect_request(msg);
      break;
    }
    case DisconnectResponse::MESSAGE_TYPE: {
      DisconnectResponse msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_disconnect_response: %s", msg.dump().c_str());
#endif
      this->on_disconnect_response(msg);
      break;
    }
    case PingRequest::MESSAGE_TYPE: {
      PingRequest msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_ping_request: %s", msg.dump().c_str());
#endif
      this->on_ping_request(msg);
      break;
    }
    case PingResponse::MESSAGE_TYPE: {
      PingResponse msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_ping_response: %s", msg.dump().c_str());
#endif
      this->on_ping_response(msg);
      break;
    }
    case DeviceInfoRequest::MESSAGE_TYPE: {
      DeviceInfoRequest msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_device_info_request: %s", msg.dump().c_str());
#endif
      this->on_device_info_request(msg);
      break;
    }
    case ListEntitiesRequest::MESSAGE_TYPE: {
      ListEntitiesRequest msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_list_entities_request: %s", msg.dump().c_str());
#endif
      this->on_list_entities_request(msg);
      break;
    }
    case SubscribeStatesRequest::MESSAGE_TYPE: {
      SubscribeStatesRequest msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_states_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_states_request(msg);
      break;
    }
    case SubscribeLogsRequest::MESSAGE_TYPE: {
      SubscribeLogsRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_logs_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_logs_request(msg);
      break;
    }
#ifdef USE_COVER
    case CoverCommandRequest::MESSAGE_TYPE: {
      CoverCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_cover_command_request: %s", msg.dump().c_str());
#endif
      this->on_cover_command_request(msg);
      break;
    }
#endif
#ifdef USE_FAN
    case FanCommandRequest::MESSAGE_TYPE: {
      FanCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_fan_command_request: %s", msg.dump().c_str());
#endif
      this->on_fan_command_request(msg);
      break;
    }
#endif
#ifdef USE_LIGHT
    case LightCommandRequest::MESSAGE_TYPE: {
      LightCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_light_command_request: %s", msg.dump().c_str());
#endif
      this->on_light_command_request(msg);
      break;
    }
#endif
#ifdef USE_SWITCH
    case SwitchCommandRequest::MESSAGE_TYPE: {
      SwitchCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_switch_command_request: %s", msg.dump().c_str());
#endif
      this->on_switch_command_request(msg);
      break;
    }
#endif
#ifdef USE_API_HOMEASSISTANT_SERVICES
    case SubscribeHomeassistantServicesRequest::MESSAGE_TYPE: {
      SubscribeHomeassistantServicesRequest msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_homeassistant_services_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_homeassistant_services_request(msg);
      break;
    }
#endif
    case GetTimeResponse::MESSAGE_TYPE: {
      GetTimeResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_get_time_response: %s", msg.dump().c_str());
#endif
      this->on_get_time_response(msg);
      break;
    }
#ifdef USE_API_HOMEASSISTANT_STATES
    case SubscribeHomeAssistantStatesRequest::MESSAGE_TYPE: {
      SubscribeHomeAssistantStatesRequest msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_home_assistant_states_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_home_assistant_states_request(msg);
      break;
    }
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
    case HomeAssistantStateResponse::MESSAGE_TYPE: {
      HomeAssistantStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_home_assistant_state_response: %s", msg.dump().c_str());
#endif
      this->on_home_assistant_state_response(msg);
      break;
    }
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
    case ExecuteServiceRequest::MESSAGE_TYPE: {
      ExecuteServiceRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_execute_service_request: %s", msg.dump().c_str());
#endif
      this->on_execute_service_request(msg);
      break;
    }
#endif
#ifdef USE_CAMERA
    case CameraImageRequest::MESSAGE_TYPE: {
      CameraImageRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_camera_image_request: %s", msg.dump().c_str());
#endif
      this->on_camera_image_request(msg);
      break;
    }
#endif
#ifdef USE_CLIMATE
    case ClimateCommandRequest::MESSAGE_TYPE: {
      ClimateCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_climate_command_request: %s", msg.dump().c_str());
#endif
      this->on_climate_command_request(msg);
      break;
    }
#endif
#ifdef USE_NUMBER
    case NumberCommandRequest::MESSAGE_TYPE: {
      NumberCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_number_command_request: %s", msg.dump().c_str());
#endif
      this->on_number_command_request(msg);
      break;
    }
#endif
#ifdef USE_SELECT
    case SelectCommandRequest::MESSAGE_TYPE: {
      SelectCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_select_command_request: %s", msg.dump().c_str());
#endif
      this->on_select_command_request(msg);
      break;
    }
#endif
#ifdef USE_SIREN
    case SirenCommandRequest::MESSAGE_TYPE: {
      SirenCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_siren_command_request: %s", msg.dump().c_str());
#endif
      this->on_siren_command_request(msg);
      break;
    }
#endif
#ifdef USE_LOCK
    case LockCommandRequest::MESSAGE_TYPE: {
      LockCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_lock_command_request: %s", msg.dump().c_str());
#endif
      this->on_lock_command_request(msg);
      break;
    }
#endif
#ifdef USE_BUTTON
    case ButtonCommandRequest::MESSAGE_TYPE: {
      ButtonCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_button_command_request: %s", msg.dump().c_str());
#endif
      this->on_button_command_request(msg);
      break;
    }
#endif
#ifdef USE_MEDIA_PLAYER
    case MediaPlayerCommandRequest::MESSAGE_TYPE: {
      MediaPlayerCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_media_player_command_request: %s", msg.dump().c_str());
#endif
      this->on_media_player_command_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case SubscribeBluetoothLEAdvertisementsRequest::MESSAGE_TYPE: {
      SubscribeBluetoothLEAdvertisementsRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_bluetooth_le_advertisements_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_bluetooth_le_advertisements_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case BluetoothDeviceRequest::MESSAGE_TYPE: {
      BluetoothDeviceRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_device_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_device_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case BluetoothGATTGetServicesRequest::MESSAGE_TYPE: {
      BluetoothGATTGetServicesRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_get_services_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_get_services_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case BluetoothGATTReadRequest::MESSAGE_TYPE: {
      BluetoothGATTReadRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_read_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_read_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case BluetoothGATTWriteRequest::MESSAGE_TYPE: {
      BluetoothGATTWriteRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_write_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_write_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case BluetoothGATTReadDescriptorRequest::MESSAGE_TYPE: {
      BluetoothGATTReadDescriptorRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_read_descriptor_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_read_descriptor_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case BluetoothGATTWriteDescriptorRequest::MESSAGE_TYPE: {
      BluetoothGATTWriteDescriptorRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_write_descriptor_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_write_descriptor_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case BluetoothGATTNotifyRequest::MESSAGE_TYPE: {
      BluetoothGATTNotifyRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_notify_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_notify_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case SubscribeBluetoothConnectionsFreeRequest::MESSAGE_TYPE: {
      SubscribeBluetoothConnectionsFreeRequest msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_bluetooth_connections_free_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_bluetooth_connections_free_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case UnsubscribeBluetoothLEAdvertisementsRequest::MESSAGE_TYPE: {
      UnsubscribeBluetoothLEAdvertisementsRequest msg;
      // Empty message: no decode needed
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_unsubscribe_bluetooth_le_advertisements_request: %s", msg.dump().c_str());
#endif
      this->on_unsubscribe_bluetooth_le_advertisements_request(msg);
      break;
    }
#endif
#ifdef USE_VOICE_ASSISTANT
    case SubscribeVoiceAssistantRequest::MESSAGE_TYPE: {
      SubscribeVoiceAssistantRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_voice_assistant_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_voice_assistant_request(msg);
      break;
    }
#endif
#ifdef USE_VOICE_ASSISTANT
    case VoiceAssistantResponse::MESSAGE_TYPE: {
      VoiceAssistantResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_response: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_response(msg);
      break;
    }
#endif
#ifdef USE_VOICE_ASSISTANT
    case VoiceAssistantEventResponse::MESSAGE_TYPE: {
      VoiceAssistantEventResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_event_response: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_event_response(msg);
      break;
    }
#endif
#ifdef USE_ALARM_CONTROL_PANEL
    case AlarmControlPanelCommandRequest::MESSAGE_TYPE: {
      AlarmControlPanelCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_alarm_control_panel_command_request: %s", msg.dump().c_str());
#endif
      this->on_alarm_control_panel_command_request(msg);
      break;
    }
#endif
#ifdef USE_TEXT
    case TextCommandRequest::MESSAGE_TYPE: {
      TextCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_text_command_request: %s", msg.dump().c_str());
#endif
      this->on_text_command_request(msg);
      break;
    }
#endif
#ifdef USE_DATETIME_DATE
    case DateCommandRequest::MESSAGE_TYPE: {
      DateCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_date_command_request: %s", msg.dump().c_str());
#endif
      this->on_date_command_request(msg);
      break;
    }
#endif
#ifdef USE_DATETIME_TIME
    case TimeCommandRequest::MESSAGE_TYPE: {
      TimeCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_time_command_request: %s", msg.dump().c_str());
#endif
      this->on_time_command_request(msg);
      break;
    }
#endif
#ifdef USE_VOICE_ASSISTANT
    case VoiceAssistantAudio::MESSAGE_TYPE: {
      VoiceAssistantAudio msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_audio: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_audio(msg);
      break;
    }
#endif
#ifdef USE_VALVE
    case ValveCommandRequest::MESSAGE_TYPE: {
      ValveCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_valve_command_request: %s", msg.dump().c_str());
#endif
      this->on_valve_command_request(msg);
      break;
    }
#endif
#ifdef USE_DATETIME_DATETIME
    case DateTimeCommandRequest::MESSAGE_TYPE: {
      DateTimeCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_date_time_command_request: %s", msg.dump().c_str());
#endif
      this->on_date_time_command_request(msg);
      break;
    }
#endif
#ifdef USE_VOICE_ASSISTANT
    case VoiceAssistantTimerEventResponse::MESSAGE_TYPE: {
      VoiceAssistantTimerEventResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_timer_event_response: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_timer_event_response(msg);
      break;
    }
#endif
#ifdef USE_UPDATE
    case UpdateCommandRequest::MESSAGE_TYPE: {
      UpdateCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_update_command_request: %s", msg.dump().c_str());
#endif
      this->on_update_command_request(msg);
      break;
    }
#endif
#ifdef USE_VOICE_ASSISTANT
    case VoiceAssistantAnnounceRequest::MESSAGE_TYPE: {
      VoiceAssistantAnnounceRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_announce_request: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_announce_request(msg);
      break;
    }
#endif
#ifdef USE_VOICE_ASSISTANT
    case VoiceAssistantConfigurationRequest::MESSAGE_TYPE: {
      VoiceAssistantConfigurationRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_configuration_request: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_configuration_request(msg);
      break;
    }
#endif
#ifdef USE_VOICE_ASSISTANT
    case VoiceAssistantSetConfiguration::MESSAGE_TYPE: {
      VoiceAssistantSetConfiguration msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_set_configuration: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_set_configuration(msg);
      break;
    }
#endif
#ifdef USE_API_NOISE
    case NoiseEncryptionSetKeyRequest::MESSAGE_TYPE: {
      NoiseEncryptionSetKeyRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_noise_encryption_set_key_request: %s", msg.dump().c_str());
#endif
      this->on_noise_encryption_set_key_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case BluetoothScannerSetModeRequest::MESSAGE_TYPE: {
      BluetoothScannerSetModeRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_scanner_set_mode_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_scanner_set_mode_request(msg);
      break;
    }
#endif
#ifdef USE_ZWAVE_PROXY
    case ZWaveProxyFrame::MESSAGE_TYPE: {
      ZWaveProxyFrame msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_z_wave_proxy_frame: %s", msg.dump().c_str());
#endif
      this->on_z_wave_proxy_frame(msg);
      break;
    }
#endif
#ifdef USE_ZWAVE_PROXY
    case ZWaveProxyRequest::MESSAGE_TYPE: {
      ZWaveProxyRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_z_wave_proxy_request: %s", msg.dump().c_str());
#endif
      this->on_z_wave_proxy_request(msg);
      break;
    }
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
    case HomeassistantActionResponse::MESSAGE_TYPE: {
      HomeassistantActionResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_homeassistant_action_response: %s", msg.dump().c_str());
#endif
      this->on_homeassistant_action_response(msg);
      break;
    }
#endif
    default:
      break;
  }
}

void APIServerConnection::on_hello_request(const HelloRequest &msg) {
  if (!this->send_hello_response(msg)) {
    this->on_fatal_error();
  }
}
#ifdef USE_API_PASSWORD
void APIServerConnection::on_authentication_request(const AuthenticationRequest &msg) {
  if (!this->send_authenticate_response(msg)) {
    this->on_fatal_error();
  }
}
#endif
void APIServerConnection::on_disconnect_request(const DisconnectRequest &msg) {
  if (!this->send_disconnect_response(msg)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_ping_request(const PingRequest &msg) {
  if (!this->send_ping_response(msg)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_device_info_request(const DeviceInfoRequest &msg) {
  if (!this->send_device_info_response(msg)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_list_entities_request(const ListEntitiesRequest &msg) { this->list_entities(msg); }
void APIServerConnection::on_subscribe_states_request(const SubscribeStatesRequest &msg) {
  this->subscribe_states(msg);
}
void APIServerConnection::on_subscribe_logs_request(const SubscribeLogsRequest &msg) { this->subscribe_logs(msg); }
#ifdef USE_API_HOMEASSISTANT_SERVICES
void APIServerConnection::on_subscribe_homeassistant_services_request(
    const SubscribeHomeassistantServicesRequest &msg) {
  this->subscribe_homeassistant_services(msg);
}
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
void APIServerConnection::on_subscribe_home_assistant_states_request(const SubscribeHomeAssistantStatesRequest &msg) {
  this->subscribe_home_assistant_states(msg);
}
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
void APIServerConnection::on_execute_service_request(const ExecuteServiceRequest &msg) { this->execute_service(msg); }
#endif
#ifdef USE_API_NOISE
void APIServerConnection::on_noise_encryption_set_key_request(const NoiseEncryptionSetKeyRequest &msg) {
  if (!this->send_noise_encryption_set_key_response(msg)) {
    this->on_fatal_error();
  }
}
#endif
#ifdef USE_BUTTON
void APIServerConnection::on_button_command_request(const ButtonCommandRequest &msg) { this->button_command(msg); }
#endif
#ifdef USE_CAMERA
void APIServerConnection::on_camera_image_request(const CameraImageRequest &msg) { this->camera_image(msg); }
#endif
#ifdef USE_CLIMATE
void APIServerConnection::on_climate_command_request(const ClimateCommandRequest &msg) { this->climate_command(msg); }
#endif
#ifdef USE_COVER
void APIServerConnection::on_cover_command_request(const CoverCommandRequest &msg) { this->cover_command(msg); }
#endif
#ifdef USE_DATETIME_DATE
void APIServerConnection::on_date_command_request(const DateCommandRequest &msg) { this->date_command(msg); }
#endif
#ifdef USE_DATETIME_DATETIME
void APIServerConnection::on_date_time_command_request(const DateTimeCommandRequest &msg) {
  this->datetime_command(msg);
}
#endif
#ifdef USE_FAN
void APIServerConnection::on_fan_command_request(const FanCommandRequest &msg) { this->fan_command(msg); }
#endif
#ifdef USE_LIGHT
void APIServerConnection::on_light_command_request(const LightCommandRequest &msg) { this->light_command(msg); }
#endif
#ifdef USE_LOCK
void APIServerConnection::on_lock_command_request(const LockCommandRequest &msg) { this->lock_command(msg); }
#endif
#ifdef USE_MEDIA_PLAYER
void APIServerConnection::on_media_player_command_request(const MediaPlayerCommandRequest &msg) {
  this->media_player_command(msg);
}
#endif
#ifdef USE_NUMBER
void APIServerConnection::on_number_command_request(const NumberCommandRequest &msg) { this->number_command(msg); }
#endif
#ifdef USE_SELECT
void APIServerConnection::on_select_command_request(const SelectCommandRequest &msg) { this->select_command(msg); }
#endif
#ifdef USE_SIREN
void APIServerConnection::on_siren_command_request(const SirenCommandRequest &msg) { this->siren_command(msg); }
#endif
#ifdef USE_SWITCH
void APIServerConnection::on_switch_command_request(const SwitchCommandRequest &msg) { this->switch_command(msg); }
#endif
#ifdef USE_TEXT
void APIServerConnection::on_text_command_request(const TextCommandRequest &msg) { this->text_command(msg); }
#endif
#ifdef USE_DATETIME_TIME
void APIServerConnection::on_time_command_request(const TimeCommandRequest &msg) { this->time_command(msg); }
#endif
#ifdef USE_UPDATE
void APIServerConnection::on_update_command_request(const UpdateCommandRequest &msg) { this->update_command(msg); }
#endif
#ifdef USE_VALVE
void APIServerConnection::on_valve_command_request(const ValveCommandRequest &msg) { this->valve_command(msg); }
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_subscribe_bluetooth_le_advertisements_request(
    const SubscribeBluetoothLEAdvertisementsRequest &msg) {
  this->subscribe_bluetooth_le_advertisements(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_device_request(const BluetoothDeviceRequest &msg) {
  this->bluetooth_device_request(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_get_services_request(const BluetoothGATTGetServicesRequest &msg) {
  this->bluetooth_gatt_get_services(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_read_request(const BluetoothGATTReadRequest &msg) {
  this->bluetooth_gatt_read(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_write_request(const BluetoothGATTWriteRequest &msg) {
  this->bluetooth_gatt_write(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_read_descriptor_request(const BluetoothGATTReadDescriptorRequest &msg) {
  this->bluetooth_gatt_read_descriptor(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_write_descriptor_request(const BluetoothGATTWriteDescriptorRequest &msg) {
  this->bluetooth_gatt_write_descriptor(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_notify_request(const BluetoothGATTNotifyRequest &msg) {
  this->bluetooth_gatt_notify(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_subscribe_bluetooth_connections_free_request(
    const SubscribeBluetoothConnectionsFreeRequest &msg) {
  if (!this->send_subscribe_bluetooth_connections_free_response(msg)) {
    this->on_fatal_error();
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_unsubscribe_bluetooth_le_advertisements_request(
    const UnsubscribeBluetoothLEAdvertisementsRequest &msg) {
  this->unsubscribe_bluetooth_le_advertisements(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_scanner_set_mode_request(const BluetoothScannerSetModeRequest &msg) {
  this->bluetooth_scanner_set_mode(msg);
}
#endif
#ifdef USE_VOICE_ASSISTANT
void APIServerConnection::on_subscribe_voice_assistant_request(const SubscribeVoiceAssistantRequest &msg) {
  this->subscribe_voice_assistant(msg);
}
#endif
#ifdef USE_VOICE_ASSISTANT
void APIServerConnection::on_voice_assistant_configuration_request(const VoiceAssistantConfigurationRequest &msg) {
  if (!this->send_voice_assistant_get_configuration_response(msg)) {
    this->on_fatal_error();
  }
}
#endif
#ifdef USE_VOICE_ASSISTANT
void APIServerConnection::on_voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) {
  this->voice_assistant_set_configuration(msg);
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
void APIServerConnection::on_alarm_control_panel_command_request(const AlarmControlPanelCommandRequest &msg) {
  this->alarm_control_panel_command(msg);
}
#endif
#ifdef USE_ZWAVE_PROXY
void APIServerConnection::on_z_wave_proxy_frame(const ZWaveProxyFrame &msg) { this->zwave_proxy_frame(msg); }
#endif
#ifdef USE_ZWAVE_PROXY
void APIServerConnection::on_z_wave_proxy_request(const ZWaveProxyRequest &msg) { this->zwave_proxy_request(msg); }
#endif

void APIServerConnection::read_message(uint32_t msg_size, uint32_t msg_type, const uint8_t *msg_data) {
  // Check authentication/connection requirements for messages
  switch (msg_type) {
    case HelloRequest::MESSAGE_TYPE:  // No setup required
#ifdef USE_API_PASSWORD
    case AuthenticationRequest::MESSAGE_TYPE:  // No setup required
#endif
    case DisconnectRequest::MESSAGE_TYPE:  // No setup required
    case PingRequest::MESSAGE_TYPE:        // No setup required
      break;                               // Skip all checks for these messages
    case DeviceInfoRequest::MESSAGE_TYPE:  // Connection setup only
      if (!this->check_connection_setup_()) {
        return;  // Connection not setup
      }
      break;
    default:
      // All other messages require authentication (which includes connection check)
      if (!this->check_authenticated_()) {
        return;  // Authentication failed
      }
      break;
  }

  // Call base implementation to process the message
  APIServerConnectionBase::read_message(msg_size, msg_type, msg_data);
}

}  // namespace esphome::api
