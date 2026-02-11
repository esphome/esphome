// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2_service.h"
#include "esphome/core/log.h"

namespace esphome::api {

static const char *const TAG = "api.service";

#ifdef HAS_PROTO_MESSAGE_DUMP
void APIServerConnectionBase::log_send_message_(const char *name, const char *dump) {
  ESP_LOGVV(TAG, "send_message %s: %s", name, dump);
}
void APIServerConnectionBase::log_receive_message_(const LogString *name, const ProtoMessage &msg) {
  DumpBuffer dump_buf;
  ESP_LOGVV(TAG, "%s: %s", LOG_STR_ARG(name), msg.dump_to(dump_buf));
}
void APIServerConnectionBase::log_receive_message_(const LogString *name) {
  ESP_LOGVV(TAG, "%s: {}", LOG_STR_ARG(name));
}
#endif

void APIServerConnectionBase::read_message(uint32_t msg_size, uint32_t msg_type, const uint8_t *msg_data) {
  // Check authentication/connection requirements
  switch (msg_type) {
    case HelloRequest::MESSAGE_TYPE:       // No setup required
    case DisconnectRequest::MESSAGE_TYPE:  // No setup required
    case PingRequest::MESSAGE_TYPE:        // No setup required
      break;
    case 9 /* DeviceInfoRequest is empty */:  // Connection setup only
      if (!this->check_connection_setup_()) {
        return;
      }
      break;
    default:
      if (!this->check_authenticated_()) {
        return;
      }
      break;
  }
  switch (msg_type) {
    case HelloRequest::MESSAGE_TYPE: {
      HelloRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_hello_request"), msg);
#endif
      this->on_hello_request(msg);
      break;
    }
    case DisconnectRequest::MESSAGE_TYPE: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_disconnect_request"));
#endif
      this->on_disconnect_request();
      break;
    }
    case DisconnectResponse::MESSAGE_TYPE: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_disconnect_response"));
#endif
      this->on_disconnect_response();
      break;
    }
    case PingRequest::MESSAGE_TYPE: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_ping_request"));
#endif
      this->on_ping_request();
      break;
    }
    case PingResponse::MESSAGE_TYPE: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_ping_response"));
#endif
      this->on_ping_response();
      break;
    }
    case 9 /* DeviceInfoRequest is empty */: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_device_info_request"));
#endif
      this->on_device_info_request();
      break;
    }
    case 11 /* ListEntitiesRequest is empty */: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_list_entities_request"));
#endif
      this->on_list_entities_request();
      break;
    }
    case 20 /* SubscribeStatesRequest is empty */: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_subscribe_states_request"));
#endif
      this->on_subscribe_states_request();
      break;
    }
    case SubscribeLogsRequest::MESSAGE_TYPE: {
      SubscribeLogsRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_subscribe_logs_request"), msg);
#endif
      this->on_subscribe_logs_request(msg);
      break;
    }
#ifdef USE_COVER
    case CoverCommandRequest::MESSAGE_TYPE: {
      CoverCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_cover_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_fan_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_light_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_switch_command_request"), msg);
#endif
      this->on_switch_command_request(msg);
      break;
    }
#endif
#ifdef USE_API_HOMEASSISTANT_SERVICES
    case 34 /* SubscribeHomeassistantServicesRequest is empty */: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_subscribe_homeassistant_services_request"));
#endif
      this->on_subscribe_homeassistant_services_request();
      break;
    }
#endif
    case GetTimeResponse::MESSAGE_TYPE: {
      GetTimeResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_get_time_response"), msg);
#endif
      this->on_get_time_response(msg);
      break;
    }
#ifdef USE_API_HOMEASSISTANT_STATES
    case 38 /* SubscribeHomeAssistantStatesRequest is empty */: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_subscribe_home_assistant_states_request"));
#endif
      this->on_subscribe_home_assistant_states_request();
      break;
    }
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
    case HomeAssistantStateResponse::MESSAGE_TYPE: {
      HomeAssistantStateResponse msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_home_assistant_state_response"), msg);
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
      this->log_receive_message_(LOG_STR("on_execute_service_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_camera_image_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_climate_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_number_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_select_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_siren_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_lock_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_button_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_media_player_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_subscribe_bluetooth_le_advertisements_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_bluetooth_device_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_bluetooth_gatt_get_services_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_bluetooth_gatt_read_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_bluetooth_gatt_write_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_bluetooth_gatt_read_descriptor_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_bluetooth_gatt_write_descriptor_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_bluetooth_gatt_notify_request"), msg);
#endif
      this->on_bluetooth_gatt_notify_request(msg);
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case 80 /* SubscribeBluetoothConnectionsFreeRequest is empty */: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_subscribe_bluetooth_connections_free_request"));
#endif
      this->on_subscribe_bluetooth_connections_free_request();
      break;
    }
#endif
#ifdef USE_BLUETOOTH_PROXY
    case 87 /* UnsubscribeBluetoothLEAdvertisementsRequest is empty */: {
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_unsubscribe_bluetooth_le_advertisements_request"));
#endif
      this->on_unsubscribe_bluetooth_le_advertisements_request();
      break;
    }
#endif
#ifdef USE_VOICE_ASSISTANT
    case SubscribeVoiceAssistantRequest::MESSAGE_TYPE: {
      SubscribeVoiceAssistantRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_subscribe_voice_assistant_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_voice_assistant_response"), msg);
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
      this->log_receive_message_(LOG_STR("on_voice_assistant_event_response"), msg);
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
      this->log_receive_message_(LOG_STR("on_alarm_control_panel_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_text_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_date_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_time_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_voice_assistant_audio"), msg);
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
      this->log_receive_message_(LOG_STR("on_valve_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_date_time_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_voice_assistant_timer_event_response"), msg);
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
      this->log_receive_message_(LOG_STR("on_update_command_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_voice_assistant_announce_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_voice_assistant_configuration_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_voice_assistant_set_configuration"), msg);
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
      this->log_receive_message_(LOG_STR("on_noise_encryption_set_key_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_bluetooth_scanner_set_mode_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_z_wave_proxy_frame"), msg);
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
      this->log_receive_message_(LOG_STR("on_z_wave_proxy_request"), msg);
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
      this->log_receive_message_(LOG_STR("on_homeassistant_action_response"), msg);
#endif
      this->on_homeassistant_action_response(msg);
      break;
    }
#endif
#ifdef USE_WATER_HEATER
    case WaterHeaterCommandRequest::MESSAGE_TYPE: {
      WaterHeaterCommandRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_water_heater_command_request"), msg);
#endif
      this->on_water_heater_command_request(msg);
      break;
    }
#endif
#ifdef USE_IR_RF
    case InfraredRFTransmitRawTimingsRequest::MESSAGE_TYPE: {
      InfraredRFTransmitRawTimingsRequest msg;
      msg.decode(msg_data, msg_size);
#ifdef HAS_PROTO_MESSAGE_DUMP
      this->log_receive_message_(LOG_STR("on_infrared_rf_transmit_raw_timings_request"), msg);
#endif
      this->on_infrared_rf_transmit_raw_timings_request(msg);
      break;
    }
#endif
    default:
      break;
  }
}

}  // namespace esphome::api
