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

void APIServerConnectionBase::read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) {
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
#ifdef USE_COVER
    case 30: {
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
    case 31: {
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
    case 32: {
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
    case 33: {
      SwitchCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_switch_command_request: %s", msg.dump().c_str());
#endif
      this->on_switch_command_request(msg);
      break;
    }
#endif
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
#ifdef USE_API_SERVICES
    case 42: {
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
    case 45: {
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
    case 48: {
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
    case 51: {
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
    case 54: {
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
    case 57: {
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
    case 60: {
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
    case 62: {
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
    case 65: {
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
    case 66: {
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
    case 68: {
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
    case 70: {
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
    case 73: {
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
    case 75: {
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
    case 76: {
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
    case 77: {
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
    case 78: {
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
    case 80: {
      SubscribeBluetoothConnectionsFreeRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_subscribe_bluetooth_connections_free_request: %s", msg.dump().c_str());
#endif
      this->on_subscribe_bluetooth_connections_free_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case 87: {
      UnsubscribeBluetoothLEAdvertisementsRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_unsubscribe_bluetooth_le_advertisements_request: %s", msg.dump().c_str());
#endif
      this->on_unsubscribe_bluetooth_le_advertisements_request(msg);
      break;
    }
#endif
#ifdef USE_VOICE_ASSISTANT
    case 89: {
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
    case 91: {
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
    case 92: {
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
    case 96: {
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
    case 99: {
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
    case 102: {
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
    case 105: {
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
    case 106: {
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
    case 111: {
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
    case 114: {
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
    case 115: {
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
    case 118: {
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
    case 119: {
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
    case 121: {
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
    case 123: {
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
    case 124: {
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
    case 127: {
      BluetoothScannerSetModeRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      ESP_LOGVV(TAG, "on_bluetooth_scanner_set_mode_request: %s", msg.dump().c_str());
#endif
      this->on_bluetooth_scanner_set_mode_request(msg);
      break;
    }
#endif
    default:
      break;
  }
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
  if (this->check_connection_setup_()) {
    DeviceInfoResponse ret = this->device_info(msg);
    if (!this->send_message(ret)) {
      this->on_fatal_error();
    }
  }
}
void APIServerConnection::on_list_entities_request(const ListEntitiesRequest &msg) {
  if (this->check_authenticated_()) {
    this->list_entities(msg);
  }
}
void APIServerConnection::on_subscribe_states_request(const SubscribeStatesRequest &msg) {
  if (this->check_authenticated_()) {
    this->subscribe_states(msg);
  }
}
void APIServerConnection::on_subscribe_logs_request(const SubscribeLogsRequest &msg) {
  if (this->check_authenticated_()) {
    this->subscribe_logs(msg);
  }
}
void APIServerConnection::on_subscribe_homeassistant_services_request(
    const SubscribeHomeassistantServicesRequest &msg) {
  if (this->check_authenticated_()) {
    this->subscribe_homeassistant_services(msg);
  }
}
void APIServerConnection::on_subscribe_home_assistant_states_request(const SubscribeHomeAssistantStatesRequest &msg) {
  if (this->check_authenticated_()) {
    this->subscribe_home_assistant_states(msg);
  }
}
void APIServerConnection::on_get_time_request(const GetTimeRequest &msg) {
  if (this->check_connection_setup_()) {
    GetTimeResponse ret = this->get_time(msg);
    if (!this->send_message(ret)) {
      this->on_fatal_error();
    }
  }
}
#ifdef USE_API_SERVICES
void APIServerConnection::on_execute_service_request(const ExecuteServiceRequest &msg) {
  if (this->check_authenticated_()) {
    this->execute_service(msg);
  }
}
#endif
#ifdef USE_API_NOISE
void APIServerConnection::on_noise_encryption_set_key_request(const NoiseEncryptionSetKeyRequest &msg) {
  if (this->check_authenticated_()) {
    NoiseEncryptionSetKeyResponse ret = this->noise_encryption_set_key(msg);
    if (!this->send_message(ret)) {
      this->on_fatal_error();
    }
  }
}
#endif
#ifdef USE_BUTTON
void APIServerConnection::on_button_command_request(const ButtonCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->button_command(msg);
  }
}
#endif
#ifdef USE_CAMERA
void APIServerConnection::on_camera_image_request(const CameraImageRequest &msg) {
  if (this->check_authenticated_()) {
    this->camera_image(msg);
  }
}
#endif
#ifdef USE_CLIMATE
void APIServerConnection::on_climate_command_request(const ClimateCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->climate_command(msg);
  }
}
#endif
#ifdef USE_COVER
void APIServerConnection::on_cover_command_request(const CoverCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->cover_command(msg);
  }
}
#endif
#ifdef USE_DATETIME_DATE
void APIServerConnection::on_date_command_request(const DateCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->date_command(msg);
  }
}
#endif
#ifdef USE_DATETIME_DATETIME
void APIServerConnection::on_date_time_command_request(const DateTimeCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->datetime_command(msg);
  }
}
#endif
#ifdef USE_FAN
void APIServerConnection::on_fan_command_request(const FanCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->fan_command(msg);
  }
}
#endif
#ifdef USE_LIGHT
void APIServerConnection::on_light_command_request(const LightCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->light_command(msg);
  }
}
#endif
#ifdef USE_LOCK
void APIServerConnection::on_lock_command_request(const LockCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->lock_command(msg);
  }
}
#endif
#ifdef USE_MEDIA_PLAYER
void APIServerConnection::on_media_player_command_request(const MediaPlayerCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->media_player_command(msg);
  }
}
#endif
#ifdef USE_NUMBER
void APIServerConnection::on_number_command_request(const NumberCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->number_command(msg);
  }
}
#endif
#ifdef USE_SELECT
void APIServerConnection::on_select_command_request(const SelectCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->select_command(msg);
  }
}
#endif
#ifdef USE_SIREN
void APIServerConnection::on_siren_command_request(const SirenCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->siren_command(msg);
  }
}
#endif
#ifdef USE_SWITCH
void APIServerConnection::on_switch_command_request(const SwitchCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->switch_command(msg);
  }
}
#endif
#ifdef USE_TEXT
void APIServerConnection::on_text_command_request(const TextCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->text_command(msg);
  }
}
#endif
#ifdef USE_DATETIME_TIME
void APIServerConnection::on_time_command_request(const TimeCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->time_command(msg);
  }
}
#endif
#ifdef USE_UPDATE
void APIServerConnection::on_update_command_request(const UpdateCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->update_command(msg);
  }
}
#endif
#ifdef USE_VALVE
void APIServerConnection::on_valve_command_request(const ValveCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->valve_command(msg);
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_subscribe_bluetooth_le_advertisements_request(
    const SubscribeBluetoothLEAdvertisementsRequest &msg) {
  if (this->check_authenticated_()) {
    this->subscribe_bluetooth_le_advertisements(msg);
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_device_request(const BluetoothDeviceRequest &msg) {
  if (this->check_authenticated_()) {
    this->bluetooth_device_request(msg);
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_get_services_request(const BluetoothGATTGetServicesRequest &msg) {
  if (this->check_authenticated_()) {
    this->bluetooth_gatt_get_services(msg);
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_read_request(const BluetoothGATTReadRequest &msg) {
  if (this->check_authenticated_()) {
    this->bluetooth_gatt_read(msg);
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_write_request(const BluetoothGATTWriteRequest &msg) {
  if (this->check_authenticated_()) {
    this->bluetooth_gatt_write(msg);
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_read_descriptor_request(const BluetoothGATTReadDescriptorRequest &msg) {
  if (this->check_authenticated_()) {
    this->bluetooth_gatt_read_descriptor(msg);
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_write_descriptor_request(const BluetoothGATTWriteDescriptorRequest &msg) {
  if (this->check_authenticated_()) {
    this->bluetooth_gatt_write_descriptor(msg);
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_gatt_notify_request(const BluetoothGATTNotifyRequest &msg) {
  if (this->check_authenticated_()) {
    this->bluetooth_gatt_notify(msg);
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_subscribe_bluetooth_connections_free_request(
    const SubscribeBluetoothConnectionsFreeRequest &msg) {
  if (this->check_authenticated_()) {
    BluetoothConnectionsFreeResponse ret = this->subscribe_bluetooth_connections_free(msg);
    if (!this->send_message(ret)) {
      this->on_fatal_error();
    }
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_unsubscribe_bluetooth_le_advertisements_request(
    const UnsubscribeBluetoothLEAdvertisementsRequest &msg) {
  if (this->check_authenticated_()) {
    this->unsubscribe_bluetooth_le_advertisements(msg);
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void APIServerConnection::on_bluetooth_scanner_set_mode_request(const BluetoothScannerSetModeRequest &msg) {
  if (this->check_authenticated_()) {
    this->bluetooth_scanner_set_mode(msg);
  }
}
#endif
#ifdef USE_VOICE_ASSISTANT
void APIServerConnection::on_subscribe_voice_assistant_request(const SubscribeVoiceAssistantRequest &msg) {
  if (this->check_authenticated_()) {
    this->subscribe_voice_assistant(msg);
  }
}
#endif
#ifdef USE_VOICE_ASSISTANT
void APIServerConnection::on_voice_assistant_configuration_request(const VoiceAssistantConfigurationRequest &msg) {
  if (this->check_authenticated_()) {
    VoiceAssistantConfigurationResponse ret = this->voice_assistant_get_configuration(msg);
    if (!this->send_message(ret)) {
      this->on_fatal_error();
    }
  }
}
#endif
#ifdef USE_VOICE_ASSISTANT
void APIServerConnection::on_voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) {
  if (this->check_authenticated_()) {
    this->voice_assistant_set_configuration(msg);
  }
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
void APIServerConnection::on_alarm_control_panel_command_request(const AlarmControlPanelCommandRequest &msg) {
  if (this->check_authenticated_()) {
    this->alarm_control_panel_command(msg);
  }
}
#endif

}  // namespace api
}  // namespace esphome
