// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2_service.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

static const char *const TAG = "api.service";

#ifdef HAS_PROTO_MESSAGE_DUMP
void APIServerConnectionBase::log_send_message_(const char *name, const std::string &dump) {
  ESP_LOGVV(TAG, "send_message %s: %s", name, dump.c_str());
}
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
    case 57: {
#ifdef USE_SIREN
      SirenCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_siren_command_request: %s", msg.dump().c_str());
#endif
      this->on_siren_command_request(msg);
#endif
      break;
    }
    case 60: {
#ifdef USE_LOCK
      LockCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_lock_command_request: %s", msg.dump().c_str());
#endif
      this->on_lock_command_request(msg);
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
    case 65: {
#ifdef USE_MEDIA_PLAYER
      MediaPlayerCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_media_player_command_request: %s", msg.dump().c_str());
#endif
      this->on_media_player_command_request(msg);
#endif
      break;
    }
    case 66: {
#ifdef USE_BLUETOOTH_PROXY
      SubscribeBluetoothLEAdvertisementsRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_bluetooth_le_advertisements_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_bluetooth_le_advertisements_request(msg);
#endif
      break;
    }
    case 68: {
#ifdef USE_BLUETOOTH_PROXY
      BluetoothDeviceRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_device_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_device_request(msg);
#endif
      break;
    }
    case 70: {
#ifdef USE_BLUETOOTH_PROXY
      BluetoothGATTGetServicesRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_get_services_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_get_services_request(msg);
#endif
      break;
    }
    case 73: {
#ifdef USE_BLUETOOTH_PROXY
      BluetoothGATTReadRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_read_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_read_request(msg);
#endif
      break;
    }
    case 75: {
#ifdef USE_BLUETOOTH_PROXY
      BluetoothGATTWriteRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_write_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_write_request(msg);
#endif
      break;
    }
    case 76: {
#ifdef USE_BLUETOOTH_PROXY
      BluetoothGATTReadDescriptorRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_read_descriptor_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_read_descriptor_request(msg);
#endif
      break;
    }
    case 77: {
#ifdef USE_BLUETOOTH_PROXY
      BluetoothGATTWriteDescriptorRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_write_descriptor_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_write_descriptor_request(msg);
#endif
      break;
    }
    case 78: {
#ifdef USE_BLUETOOTH_PROXY
      BluetoothGATTNotifyRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_gatt_notify_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_gatt_notify_request(msg);
#endif
      break;
    }
    case 80: {
#ifdef USE_BLUETOOTH_PROXY
      SubscribeBluetoothConnectionsFreeRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_bluetooth_connections_free_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_bluetooth_connections_free_request(msg);
#endif
      break;
    }
    case 87: {
#ifdef USE_BLUETOOTH_PROXY
      UnsubscribeBluetoothLEAdvertisementsRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_unsubscribe_bluetooth_le_advertisements_request: %s", msg.dump().c_str());
#endif
      this->on_unsubscribe_bluetooth_le_advertisements_request(msg);
#endif
      break;
    }
    case 89: {
#ifdef USE_VOICE_ASSISTANT
      SubscribeVoiceAssistantRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_voice_assistant_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_voice_assistant_request(msg);
#endif
      break;
    }
    case 91: {
#ifdef USE_VOICE_ASSISTANT
      VoiceAssistantResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_response: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_response(msg);
#endif
      break;
    }
    case 92: {
#ifdef USE_VOICE_ASSISTANT
      VoiceAssistantEventResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_event_response: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_event_response(msg);
#endif
      break;
    }
    case 96: {
#ifdef USE_ALARM_CONTROL_PANEL
      AlarmControlPanelCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_alarm_control_panel_command_request: %s", msg.dump().c_str());
#endif
      this->on_alarm_control_panel_command_request(msg);
#endif
      break;
    }
    case 99: {
#ifdef USE_TEXT
      TextCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_text_command_request: %s", msg.dump().c_str());
#endif
      this->on_text_command_request(msg);
#endif
      break;
    }
    case 102: {
#ifdef USE_DATETIME_DATE
      DateCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_date_command_request: %s", msg.dump().c_str());
#endif
      this->on_date_command_request(msg);
#endif
      break;
    }
    case 105: {
#ifdef USE_DATETIME_TIME
      TimeCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_time_command_request: %s", msg.dump().c_str());
#endif
      this->on_time_command_request(msg);
#endif
      break;
    }
    case 106: {
#ifdef USE_VOICE_ASSISTANT
      VoiceAssistantAudio msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_audio: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_audio(msg);
#endif
      break;
    }
    case 111: {
#ifdef USE_VALVE
      ValveCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_valve_command_request: %s", msg.dump().c_str());
#endif
      this->on_valve_command_request(msg);
#endif
      break;
    }
    case 114: {
#ifdef USE_DATETIME_DATETIME
      DateTimeCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_date_time_command_request: %s", msg.dump().c_str());
#endif
      this->on_date_time_command_request(msg);
#endif
      break;
    }
    case 115: {
#ifdef USE_VOICE_ASSISTANT
      VoiceAssistantTimerEventResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_timer_event_response: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_timer_event_response(msg);
#endif
      break;
    }
    case 118: {
#ifdef USE_UPDATE
      UpdateCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_update_command_request: %s", msg.dump().c_str());
#endif
      this->on_update_command_request(msg);
#endif
      break;
    }
    case 119: {
#ifdef USE_VOICE_ASSISTANT
      VoiceAssistantAnnounceRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_announce_request: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_announce_request(msg);
#endif
      break;
    }
    case 121: {
#ifdef USE_VOICE_ASSISTANT
      VoiceAssistantConfigurationRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_configuration_request: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_configuration_request(msg);
#endif
      break;
    }
    case 123: {
#ifdef USE_VOICE_ASSISTANT
      VoiceAssistantSetConfiguration msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_voice_assistant_set_configuration: %s", msg.dump().c_str());
#endif
      this->on_voice_assistant_set_configuration(msg);
#endif
      break;
    }
    case 124: {
#ifdef USE_API_NOISE
      NoiseEncryptionSetKeyRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_noise_encryption_set_key_request: %s", msg.dump().c_str());
#endif
      this->on_noise_encryption_set_key_request(msg);
#endif
      break;
    }
    case 127: {
#ifdef USE_BLUETOOTH_PROXY
      BluetoothScannerSetModeRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_scanner_set_mode_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_scanner_set_mode_request(msg);
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
  if (!this->send_message(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_connect_request(const ConnectRequest &msg) {
  ConnectResponse ret = this->connect(msg);
  if (!this->send_message(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_disconnect_request(const DisconnectRequest &msg) {
  DisconnectResponse ret = this->disconnect(msg);
  if (!this->send_message(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_ping_request(const PingRequest &msg) {
  PingResponse ret = this->ping(msg);
  if (!this->send_message(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_device_info_request(const DeviceInfoRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  DeviceInfoResponse ret = this->device_info(msg);
  if (!this->send_message(ret)) {
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
  if (!this->send_message(ret)) {
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
#ifdef USE_API_NOISE
void APIServerConnection::on_noise_encryption_set_key_request(const NoiseEncryptionSetKeyRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  NoiseEncryptionSetKeyResponse ret = this->noise_encryption_set_key(msg);
  if (!this->send_message(ret)) {
    this->on_fatal_error();
  }
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
#ifdef USE_DATETIME_DATE
void APIServerConnection::on_date_command_request(const DateCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->date_command(msg);
}
#endif
#ifdef USE_DATETIME_DATETIME
void APIServerConnection::on_date_time_command_request(const DateTimeCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->datetime_command(msg);
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
#ifdef USE_LOCK
void APIServerConnection::on_lock_command_request(const LockCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->lock_command(msg);
}
#endif
#ifdef USE_MEDIA_PLAYER
void APIServerConnection::on_media_player_command_request(const MediaPlayerCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->media_player_command(msg);
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
#ifdef USE_SIREN
void APIServerConnection::on_siren_command_request(const SirenCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->siren_command(msg);
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
#ifdef USE_TEXT
void APIServerConnection::on_text_command_request(const TextCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->text_command(msg);
}
#endif
#ifdef USE_DATETIME_TIME
void APIServerConnection::on_time_command_request(const TimeCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->time_command(msg);
}
#endif
#ifdef USE_UPDATE
void APIServerConnection::on_update_command_request(const UpdateCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->update_command(msg);
}
#endif
#ifdef USE_VALVE
void APIServerConnection::on_valve_command_request(const ValveCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->valve_command(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_subscribe_bluetooth_le_advertisements_request(
    const SubscribeBluetoothLEAdvertisementsRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->subscribe_bluetooth_le_advertisements(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_device_request(const BluetoothDeviceRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->bluetooth_device_request(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_get_services_request(const BluetoothGATTGetServicesRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->bluetooth_gatt_get_services(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_read_request(const BluetoothGATTReadRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->bluetooth_gatt_read(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_write_request(const BluetoothGATTWriteRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->bluetooth_gatt_write(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_read_descriptor_request(const BluetoothGATTReadDescriptorRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->bluetooth_gatt_read_descriptor(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_write_descriptor_request(const BluetoothGATTWriteDescriptorRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->bluetooth_gatt_write_descriptor(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_notify_request(const BluetoothGATTNotifyRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->bluetooth_gatt_notify(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_subscribe_bluetooth_connections_free_request(
    const SubscribeBluetoothConnectionsFreeRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  BluetoothConnectionsFreeResponse ret = this->subscribe_bluetooth_connections_free(msg);
  if (!this->send_message(ret)) {
    this->on_fatal_error();
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_unsubscribe_bluetooth_le_advertisements_request(
    const UnsubscribeBluetoothLEAdvertisementsRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->unsubscribe_bluetooth_le_advertisements(msg);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_scanner_set_mode_request(const BluetoothScannerSetModeRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->bluetooth_scanner_set_mode(msg);
}
#endif
#ifdef USE_VOICE_ASSISTANT
void APIServerConnection::on_subscribe_voice_assistant_request(const SubscribeVoiceAssistantRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->subscribe_voice_assistant(msg);
}
#endif
#ifdef USE_VOICE_ASSISTANT
void APIServerConnection::on_voice_assistant_configuration_request(const VoiceAssistantConfigurationRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  VoiceAssistantConfigurationResponse ret = this->voice_assistant_get_configuration(msg);
  if (!this->send_message(ret)) {
    this->on_fatal_error();
  }
}
#endif
#ifdef USE_VOICE_ASSISTANT
void APIServerConnection::on_voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->voice_assistant_set_configuration(msg);
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
void APIServerConnection::on_alarm_control_panel_command_request(const AlarmControlPanelCommandRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  if (!this->is_authenticated()) {
    this->on_unauthenticated_access();
    return;
  }
  this->alarm_control_panel_command(msg);
}
#endif

}  // namespace api
}  // namespace esphome
