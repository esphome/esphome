// This file was automatically generated with a tool.
// See scripts/api_protobuf/api_protobuf.py
#include "api_pb2_service.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

static const char *const TAG = "api.service";

bool APIServerConnectionBase::send_hello_response(const HelloResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_hello_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<HelloResponse>(msg, 2);
}
bool APIServerConnectionBase::send_connect_response(const ConnectResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_connect_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ConnectResponse>(msg, 4);
}
bool APIServerConnectionBase::send_disconnect_request(const DisconnectRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_disconnect_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<DisconnectRequest>(msg, 5);
}
bool APIServerConnectionBase::send_disconnect_response(const DisconnectResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_disconnect_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<DisconnectResponse>(msg, 6);
}
bool APIServerConnectionBase::send_ping_request(const PingRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_ping_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<PingRequest>(msg, 7);
}
bool APIServerConnectionBase::send_ping_response(const PingResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_ping_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<PingResponse>(msg, 8);
}
bool APIServerConnectionBase::send_device_info_response(const DeviceInfoResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_device_info_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<DeviceInfoResponse>(msg, 10);
}
bool APIServerConnectionBase::send_list_entities_done_response(const ListEntitiesDoneResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_done_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesDoneResponse>(msg, 19);
}
#ifdef USE_BINARY_SENSOR
bool APIServerConnectionBase::send_list_entities_binary_sensor_response(const ListEntitiesBinarySensorResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_binary_sensor_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesBinarySensorResponse>(msg, 12);
}
#endif
#ifdef USE_BINARY_SENSOR
bool APIServerConnectionBase::send_binary_sensor_state_response(const BinarySensorStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_binary_sensor_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BinarySensorStateResponse>(msg, 21);
}
#endif
#ifdef USE_COVER
bool APIServerConnectionBase::send_list_entities_cover_response(const ListEntitiesCoverResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_cover_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesCoverResponse>(msg, 13);
}
#endif
#ifdef USE_COVER
bool APIServerConnectionBase::send_cover_state_response(const CoverStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_cover_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<CoverStateResponse>(msg, 22);
}
#endif
#ifdef USE_COVER
#endif
#ifdef USE_FAN
bool APIServerConnectionBase::send_list_entities_fan_response(const ListEntitiesFanResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_fan_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesFanResponse>(msg, 14);
}
#endif
#ifdef USE_FAN
bool APIServerConnectionBase::send_fan_state_response(const FanStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_fan_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<FanStateResponse>(msg, 23);
}
#endif
#ifdef USE_FAN
#endif
#ifdef USE_LIGHT
bool APIServerConnectionBase::send_list_entities_light_response(const ListEntitiesLightResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_light_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesLightResponse>(msg, 15);
}
#endif
#ifdef USE_LIGHT
bool APIServerConnectionBase::send_light_state_response(const LightStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_light_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<LightStateResponse>(msg, 24);
}
#endif
#ifdef USE_LIGHT
#endif
#ifdef USE_SENSOR
bool APIServerConnectionBase::send_list_entities_sensor_response(const ListEntitiesSensorResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_sensor_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesSensorResponse>(msg, 16);
}
#endif
#ifdef USE_SENSOR
bool APIServerConnectionBase::send_sensor_state_response(const SensorStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_sensor_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<SensorStateResponse>(msg, 25);
}
#endif
#ifdef USE_SWITCH
bool APIServerConnectionBase::send_list_entities_switch_response(const ListEntitiesSwitchResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_switch_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesSwitchResponse>(msg, 17);
}
#endif
#ifdef USE_SWITCH
bool APIServerConnectionBase::send_switch_state_response(const SwitchStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_switch_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<SwitchStateResponse>(msg, 26);
}
#endif
#ifdef USE_SWITCH
#endif
#ifdef USE_TEXT_SENSOR
bool APIServerConnectionBase::send_list_entities_text_sensor_response(const ListEntitiesTextSensorResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_text_sensor_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesTextSensorResponse>(msg, 18);
}
#endif
#ifdef USE_TEXT_SENSOR
bool APIServerConnectionBase::send_text_sensor_state_response(const TextSensorStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_text_sensor_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<TextSensorStateResponse>(msg, 27);
}
#endif
bool APIServerConnectionBase::send_subscribe_logs_response(const SubscribeLogsResponse &msg) {
  return this->send_message_<SubscribeLogsResponse>(msg, 29);
}
bool APIServerConnectionBase::send_homeassistant_service_response(const HomeassistantServiceResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_homeassistant_service_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<HomeassistantServiceResponse>(msg, 35);
}
bool APIServerConnectionBase::send_subscribe_home_assistant_state_response(
    const SubscribeHomeAssistantStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_subscribe_home_assistant_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<SubscribeHomeAssistantStateResponse>(msg, 39);
}
bool APIServerConnectionBase::send_get_time_request(const GetTimeRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_get_time_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<GetTimeRequest>(msg, 36);
}
bool APIServerConnectionBase::send_get_time_response(const GetTimeResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_get_time_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<GetTimeResponse>(msg, 37);
}
bool APIServerConnectionBase::send_list_entities_services_response(const ListEntitiesServicesResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_services_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesServicesResponse>(msg, 41);
}
#ifdef USE_ESP32_CAMERA
bool APIServerConnectionBase::send_list_entities_camera_response(const ListEntitiesCameraResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_camera_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesCameraResponse>(msg, 43);
}
#endif
#ifdef USE_ESP32_CAMERA
bool APIServerConnectionBase::send_camera_image_response(const CameraImageResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_camera_image_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<CameraImageResponse>(msg, 44);
}
#endif
#ifdef USE_ESP32_CAMERA
#endif
#ifdef USE_CLIMATE
bool APIServerConnectionBase::send_list_entities_climate_response(const ListEntitiesClimateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_climate_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesClimateResponse>(msg, 46);
}
#endif
#ifdef USE_CLIMATE
bool APIServerConnectionBase::send_climate_state_response(const ClimateStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_climate_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ClimateStateResponse>(msg, 47);
}
#endif
#ifdef USE_CLIMATE
#endif
#ifdef USE_NUMBER
bool APIServerConnectionBase::send_list_entities_number_response(const ListEntitiesNumberResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_number_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesNumberResponse>(msg, 49);
}
#endif
#ifdef USE_NUMBER
bool APIServerConnectionBase::send_number_state_response(const NumberStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_number_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<NumberStateResponse>(msg, 50);
}
#endif
#ifdef USE_NUMBER
#endif
#ifdef USE_SELECT
bool APIServerConnectionBase::send_list_entities_select_response(const ListEntitiesSelectResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_select_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesSelectResponse>(msg, 52);
}
#endif
#ifdef USE_SELECT
bool APIServerConnectionBase::send_select_state_response(const SelectStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_select_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<SelectStateResponse>(msg, 53);
}
#endif
#ifdef USE_SELECT
#endif
#ifdef USE_LOCK
bool APIServerConnectionBase::send_list_entities_lock_response(const ListEntitiesLockResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_lock_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesLockResponse>(msg, 58);
}
#endif
#ifdef USE_LOCK
bool APIServerConnectionBase::send_lock_state_response(const LockStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_lock_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<LockStateResponse>(msg, 59);
}
#endif
#ifdef USE_LOCK
#endif
#ifdef USE_BUTTON
bool APIServerConnectionBase::send_list_entities_button_response(const ListEntitiesButtonResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_button_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesButtonResponse>(msg, 61);
}
#endif
#ifdef USE_BUTTON
#endif
#ifdef USE_MEDIA_PLAYER
bool APIServerConnectionBase::send_list_entities_media_player_response(const ListEntitiesMediaPlayerResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_media_player_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesMediaPlayerResponse>(msg, 63);
}
#endif
#ifdef USE_MEDIA_PLAYER
bool APIServerConnectionBase::send_media_player_state_response(const MediaPlayerStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_media_player_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<MediaPlayerStateResponse>(msg, 64);
}
#endif
#ifdef USE_MEDIA_PLAYER
#endif
#ifdef USE_BLUETOOTH_PROXY
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_le_advertisement_response(const BluetoothLEAdvertisementResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_le_advertisement_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothLEAdvertisementResponse>(msg, 67);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_le_raw_advertisements_response(
    const BluetoothLERawAdvertisementsResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_le_raw_advertisements_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothLERawAdvertisementsResponse>(msg, 93);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_device_connection_response(const BluetoothDeviceConnectionResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_device_connection_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothDeviceConnectionResponse>(msg, 69);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_gatt_get_services_response(const BluetoothGATTGetServicesResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_gatt_get_services_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothGATTGetServicesResponse>(msg, 71);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_gatt_get_services_done_response(
    const BluetoothGATTGetServicesDoneResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_gatt_get_services_done_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothGATTGetServicesDoneResponse>(msg, 72);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_gatt_read_response(const BluetoothGATTReadResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_gatt_read_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothGATTReadResponse>(msg, 74);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
#endif
#ifdef USE_BLUETOOTH_PROXY
#endif
#ifdef USE_BLUETOOTH_PROXY
#endif
#ifdef USE_BLUETOOTH_PROXY
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_gatt_notify_data_response(const BluetoothGATTNotifyDataResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_gatt_notify_data_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothGATTNotifyDataResponse>(msg, 79);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_connections_free_response(const BluetoothConnectionsFreeResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_connections_free_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothConnectionsFreeResponse>(msg, 81);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_gatt_error_response(const BluetoothGATTErrorResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_gatt_error_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothGATTErrorResponse>(msg, 82);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_gatt_write_response(const BluetoothGATTWriteResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_gatt_write_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothGATTWriteResponse>(msg, 83);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_gatt_notify_response(const BluetoothGATTNotifyResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_gatt_notify_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothGATTNotifyResponse>(msg, 84);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_device_pairing_response(const BluetoothDevicePairingResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_device_pairing_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothDevicePairingResponse>(msg, 85);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_device_unpairing_response(const BluetoothDeviceUnpairingResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_device_unpairing_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothDeviceUnpairingResponse>(msg, 86);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
#endif
#ifdef USE_BLUETOOTH_PROXY
bool APIServerConnectionBase::send_bluetooth_device_clear_cache_response(const BluetoothDeviceClearCacheResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_bluetooth_device_clear_cache_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<BluetoothDeviceClearCacheResponse>(msg, 88);
}
#endif
#ifdef USE_VOICE_ASSISTANT
#endif
#ifdef USE_VOICE_ASSISTANT
bool APIServerConnectionBase::send_voice_assistant_request(const VoiceAssistantRequest &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_voice_assistant_request: %s", msg.dump().c_str());
#endif
  return this->send_message_<VoiceAssistantRequest>(msg, 90);
}
#endif
#ifdef USE_VOICE_ASSISTANT
#endif
#ifdef USE_VOICE_ASSISTANT
#endif
#ifdef USE_VOICE_ASSISTANT
bool APIServerConnectionBase::send_voice_assistant_audio(const VoiceAssistantAudio &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_voice_assistant_audio: %s", msg.dump().c_str());
#endif
  return this->send_message_<VoiceAssistantAudio>(msg, 106);
}
#endif
#ifdef USE_VOICE_ASSISTANT
#endif
#ifdef USE_VOICE_ASSISTANT
#endif
#ifdef USE_VOICE_ASSISTANT
bool APIServerConnectionBase::send_voice_assistant_announce_finished(const VoiceAssistantAnnounceFinished &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_voice_assistant_announce_finished: %s", msg.dump().c_str());
#endif
  return this->send_message_<VoiceAssistantAnnounceFinished>(msg, 120);
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
bool APIServerConnectionBase::send_list_entities_alarm_control_panel_response(
    const ListEntitiesAlarmControlPanelResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_alarm_control_panel_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesAlarmControlPanelResponse>(msg, 94);
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
bool APIServerConnectionBase::send_alarm_control_panel_state_response(const AlarmControlPanelStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_alarm_control_panel_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<AlarmControlPanelStateResponse>(msg, 95);
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
#endif
#ifdef USE_TEXT
bool APIServerConnectionBase::send_list_entities_text_response(const ListEntitiesTextResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_text_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesTextResponse>(msg, 97);
}
#endif
#ifdef USE_TEXT
bool APIServerConnectionBase::send_text_state_response(const TextStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_text_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<TextStateResponse>(msg, 98);
}
#endif
#ifdef USE_TEXT
#endif
#ifdef USE_DATETIME_DATE
bool APIServerConnectionBase::send_list_entities_date_response(const ListEntitiesDateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_date_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesDateResponse>(msg, 100);
}
#endif
#ifdef USE_DATETIME_DATE
bool APIServerConnectionBase::send_date_state_response(const DateStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_date_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<DateStateResponse>(msg, 101);
}
#endif
#ifdef USE_DATETIME_DATE
#endif
#ifdef USE_DATETIME_TIME
bool APIServerConnectionBase::send_list_entities_time_response(const ListEntitiesTimeResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_time_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesTimeResponse>(msg, 103);
}
#endif
#ifdef USE_DATETIME_TIME
bool APIServerConnectionBase::send_time_state_response(const TimeStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_time_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<TimeStateResponse>(msg, 104);
}
#endif
#ifdef USE_DATETIME_TIME
#endif
#ifdef USE_EVENT
bool APIServerConnectionBase::send_list_entities_event_response(const ListEntitiesEventResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_event_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesEventResponse>(msg, 107);
}
#endif
#ifdef USE_EVENT
bool APIServerConnectionBase::send_event_response(const EventResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_event_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<EventResponse>(msg, 108);
}
#endif
#ifdef USE_VALVE
bool APIServerConnectionBase::send_list_entities_valve_response(const ListEntitiesValveResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_valve_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesValveResponse>(msg, 109);
}
#endif
#ifdef USE_VALVE
bool APIServerConnectionBase::send_valve_state_response(const ValveStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_valve_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ValveStateResponse>(msg, 110);
}
#endif
#ifdef USE_VALVE
#endif
#ifdef USE_DATETIME_DATETIME
bool APIServerConnectionBase::send_list_entities_date_time_response(const ListEntitiesDateTimeResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_date_time_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesDateTimeResponse>(msg, 112);
}
#endif
#ifdef USE_DATETIME_DATETIME
bool APIServerConnectionBase::send_date_time_state_response(const DateTimeStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_date_time_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<DateTimeStateResponse>(msg, 113);
}
#endif
#ifdef USE_DATETIME_DATETIME
#endif
#ifdef USE_UPDATE
bool APIServerConnectionBase::send_list_entities_update_response(const ListEntitiesUpdateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_list_entities_update_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<ListEntitiesUpdateResponse>(msg, 116);
}
#endif
#ifdef USE_UPDATE
bool APIServerConnectionBase::send_update_state_response(const UpdateStateResponse &msg) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  ESP_LOGVV(TAG, "send_update_state_response: %s", msg.dump().c_str());
#endif
  return this->send_message_<UpdateStateResponse>(msg, 117);
}
#endif
#ifdef USE_UPDATE
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
    default:
      return false;
  }
  return true;
}

void APIServerConnection::on_hello_request(const HelloRequest &msg) {
  HelloResponse ret = this->hello(msg);
  if (!this->send_hello_response(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_connect_request(const ConnectRequest &msg) {
  ConnectResponse ret = this->connect(msg);
  if (!this->send_connect_response(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_disconnect_request(const DisconnectRequest &msg) {
  DisconnectResponse ret = this->disconnect(msg);
  if (!this->send_disconnect_response(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_ping_request(const PingRequest &msg) {
  PingResponse ret = this->ping(msg);
  if (!this->send_ping_response(ret)) {
    this->on_fatal_error();
  }
}
void APIServerConnection::on_device_info_request(const DeviceInfoRequest &msg) {
  if (!this->is_connection_setup()) {
    this->on_no_setup_connection();
    return;
  }
  DeviceInfoResponse ret = this->device_info(msg);
  if (!this->send_device_info_response(ret)) {
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
  if (!this->send_get_time_response(ret)) {
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
  if (!this->send_bluetooth_connections_free_response(ret)) {
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
