#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API
#include "api_frame_helper.h"
#include "api_pb2.h"
#include "api_pb2_service.h"
#include "api_server.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"

#include <vector>

namespace esphome {
namespace api {

using send_message_t = bool(APIConnection *, void *);

/*
  This class holds a pointer to the source component that wants to publish a message, and a pointer to a function that
  will lazily publish that message.  The two pointers allow dedup in the deferred queue if multiple publishes for the
  same component are backed up, and take up only 8 bytes of memory.  The entry in the deferred queue (a std::vector) is
  the DeferredMessage instance itself (not a pointer to one elsewhere in heap) so still only 8 bytes per entry.  Even
  100 backed up messages (you'd have to have at least 100 sensors publishing because of dedup) would take up only 0.8
  kB.
*/
class DeferredMessageQueue {
  struct DeferredMessage {
    friend class DeferredMessageQueue;

   protected:
    void *source_;
    send_message_t *send_message_;

   public:
    DeferredMessage(void *source, send_message_t *send_message) : source_(source), send_message_(send_message) {}
    bool operator==(const DeferredMessage &test) const {
      return (source_ == test.source_ && send_message_ == test.send_message_);
    }
  } __attribute__((packed));

 protected:
  // vector is used very specifically for its zero memory overhead even though items are popped from the front (memory
  // footprint is more important than speed here)
  std::vector<DeferredMessage> deferred_queue_;
  APIConnection *api_connection_;

  // helper for allowing only unique entries in the queue
  void dmq_push_back_with_dedup_(void *source, send_message_t *send_message);

 public:
  DeferredMessageQueue(APIConnection *api_connection) : api_connection_(api_connection) {}
  void process_queue();
  void defer(void *source, send_message_t *send_message);
};

class APIConnection : public APIServerConnection {
 public:
  APIConnection(std::unique_ptr<socket::Socket> socket, APIServer *parent);
  virtual ~APIConnection();

  void start();
  void loop();

  bool send_list_info_done() {
    ListEntitiesDoneResponse resp;
    return this->send_list_entities_done_response(resp);
  }
#ifdef USE_BINARY_SENSOR
  bool send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor, bool state);
  void send_binary_sensor_info(binary_sensor::BinarySensor *binary_sensor);
  static bool try_send_binary_sensor_state(APIConnection *api, void *v_binary_sensor);
  static bool try_send_binary_sensor_state(APIConnection *api, binary_sensor::BinarySensor *binary_sensor, bool state);
  static bool try_send_binary_sensor_info(APIConnection *api, void *v_binary_sensor);
#endif
#ifdef USE_COVER
  bool send_cover_state(cover::Cover *cover);
  void send_cover_info(cover::Cover *cover);
  static bool try_send_cover_state(APIConnection *api, void *v_cover);
  static bool try_send_cover_info(APIConnection *api, void *v_cover);
  void cover_command(const CoverCommandRequest &msg) override;
#endif
#ifdef USE_FAN
  bool send_fan_state(fan::Fan *fan);
  void send_fan_info(fan::Fan *fan);
  static bool try_send_fan_state(APIConnection *api, void *v_fan);
  static bool try_send_fan_info(APIConnection *api, void *v_fan);
  void fan_command(const FanCommandRequest &msg) override;
#endif
#ifdef USE_LIGHT
  bool send_light_state(light::LightState *light);
  void send_light_info(light::LightState *light);
  static bool try_send_light_state(APIConnection *api, void *v_light);
  static bool try_send_light_info(APIConnection *api, void *v_light);
  void light_command(const LightCommandRequest &msg) override;
#endif
#ifdef USE_SENSOR
  bool send_sensor_state(sensor::Sensor *sensor, float state);
  void send_sensor_info(sensor::Sensor *sensor);
  static bool try_send_sensor_state(APIConnection *api, void *v_sensor);
  static bool try_send_sensor_state(APIConnection *api, sensor::Sensor *sensor, float state);
  static bool try_send_sensor_info(APIConnection *api, void *v_sensor);
#endif
#ifdef USE_SWITCH
  bool send_switch_state(switch_::Switch *a_switch, bool state);
  void send_switch_info(switch_::Switch *a_switch);
  static bool try_send_switch_state(APIConnection *api, void *v_a_switch);
  static bool try_send_switch_state(APIConnection *api, switch_::Switch *a_switch, bool state);
  static bool try_send_switch_info(APIConnection *api, void *v_a_switch);
  void switch_command(const SwitchCommandRequest &msg) override;
#endif
#ifdef USE_TEXT_SENSOR
  bool send_text_sensor_state(text_sensor::TextSensor *text_sensor, std::string state);
  void send_text_sensor_info(text_sensor::TextSensor *text_sensor);
  static bool try_send_text_sensor_state(APIConnection *api, void *v_text_sensor);
  static bool try_send_text_sensor_state(APIConnection *api, text_sensor::TextSensor *text_sensor, std::string state);
  static bool try_send_text_sensor_info(APIConnection *api, void *v_text_sensor);
#endif
#ifdef USE_ESP32_CAMERA
  void set_camera_state(std::shared_ptr<esp32_camera::CameraImage> image);
  void send_camera_info(esp32_camera::ESP32Camera *camera);
  static bool try_send_camera_info(APIConnection *api, void *v_camera);
  void camera_image(const CameraImageRequest &msg) override;
#endif
#ifdef USE_CLIMATE
  bool send_climate_state(climate::Climate *climate);
  void send_climate_info(climate::Climate *climate);
  static bool try_send_climate_state(APIConnection *api, void *v_climate);
  static bool try_send_climate_info(APIConnection *api, void *v_climate);
  void climate_command(const ClimateCommandRequest &msg) override;
#endif
#ifdef USE_NUMBER
  bool send_number_state(number::Number *number, float state);
  void send_number_info(number::Number *number);
  static bool try_send_number_state(APIConnection *api, void *v_number);
  static bool try_send_number_state(APIConnection *api, number::Number *number, float state);
  static bool try_send_number_info(APIConnection *api, void *v_number);
  void number_command(const NumberCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_DATE
  bool send_date_state(datetime::DateEntity *date);
  void send_date_info(datetime::DateEntity *date);
  static bool try_send_date_state(APIConnection *api, void *v_date);
  static bool try_send_date_info(APIConnection *api, void *v_date);
  void date_command(const DateCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_TIME
  bool send_time_state(datetime::TimeEntity *time);
  void send_time_info(datetime::TimeEntity *time);
  static bool try_send_time_state(APIConnection *api, void *v_time);
  static bool try_send_time_info(APIConnection *api, void *v_time);
  void time_command(const TimeCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_DATETIME
  bool send_datetime_state(datetime::DateTimeEntity *datetime);
  void send_datetime_info(datetime::DateTimeEntity *datetime);
  static bool try_send_datetime_state(APIConnection *api, void *v_datetime);
  static bool try_send_datetime_info(APIConnection *api, void *v_datetime);
  void datetime_command(const DateTimeCommandRequest &msg) override;
#endif
#ifdef USE_TEXT
  bool send_text_state(text::Text *text, std::string state);
  void send_text_info(text::Text *text);
  static bool try_send_text_state(APIConnection *api, void *v_text);
  static bool try_send_text_state(APIConnection *api, text::Text *text, std::string state);
  static bool try_send_text_info(APIConnection *api, void *v_text);
  void text_command(const TextCommandRequest &msg) override;
#endif
#ifdef USE_SELECT
  bool send_select_state(select::Select *select, std::string state);
  void send_select_info(select::Select *select);
  static bool try_send_select_state(APIConnection *api, void *v_select);
  static bool try_send_select_state(APIConnection *api, select::Select *select, std::string state);
  static bool try_send_select_info(APIConnection *api, void *v_select);
  void select_command(const SelectCommandRequest &msg) override;
#endif
#ifdef USE_BUTTON
  void send_button_info(button::Button *button);
  static bool try_send_button_info(APIConnection *api, void *v_button);
  void button_command(const ButtonCommandRequest &msg) override;
#endif
#ifdef USE_LOCK
  bool send_lock_state(lock::Lock *a_lock, lock::LockState state);
  void send_lock_info(lock::Lock *a_lock);
  static bool try_send_lock_state(APIConnection *api, void *v_a_lock);
  static bool try_send_lock_state(APIConnection *api, lock::Lock *a_lock, lock::LockState state);
  static bool try_send_lock_info(APIConnection *api, void *v_a_lock);
  void lock_command(const LockCommandRequest &msg) override;
#endif
#ifdef USE_VALVE
  bool send_valve_state(valve::Valve *valve);
  void send_valve_info(valve::Valve *valve);
  static bool try_send_valve_state(APIConnection *api, void *v_valve);
  static bool try_send_valve_info(APIConnection *api, void *v_valve);
  void valve_command(const ValveCommandRequest &msg) override;
#endif
#ifdef USE_MEDIA_PLAYER
  bool send_media_player_state(media_player::MediaPlayer *media_player);
  void send_media_player_info(media_player::MediaPlayer *media_player);
  static bool try_send_media_player_state(APIConnection *api, void *v_media_player);
  static bool try_send_media_player_info(APIConnection *api, void *v_media_player);
  void media_player_command(const MediaPlayerCommandRequest &msg) override;
#endif
  bool try_send_log_message(int level, const char *tag, const char *line);
  void send_homeassistant_service_call(const HomeassistantServiceResponse &call) {
    if (!this->service_call_subscription_)
      return;
    this->send_homeassistant_service_response(call);
  }
#ifdef USE_BLUETOOTH_PROXY
  void subscribe_bluetooth_le_advertisements(const SubscribeBluetoothLEAdvertisementsRequest &msg) override;
  void unsubscribe_bluetooth_le_advertisements(const UnsubscribeBluetoothLEAdvertisementsRequest &msg) override;
  bool send_bluetooth_le_advertisement(const BluetoothLEAdvertisementResponse &msg);

  void bluetooth_device_request(const BluetoothDeviceRequest &msg) override;
  void bluetooth_gatt_read(const BluetoothGATTReadRequest &msg) override;
  void bluetooth_gatt_write(const BluetoothGATTWriteRequest &msg) override;
  void bluetooth_gatt_read_descriptor(const BluetoothGATTReadDescriptorRequest &msg) override;
  void bluetooth_gatt_write_descriptor(const BluetoothGATTWriteDescriptorRequest &msg) override;
  void bluetooth_gatt_get_services(const BluetoothGATTGetServicesRequest &msg) override;
  void bluetooth_gatt_notify(const BluetoothGATTNotifyRequest &msg) override;
  BluetoothConnectionsFreeResponse subscribe_bluetooth_connections_free(
      const SubscribeBluetoothConnectionsFreeRequest &msg) override;

#endif
#ifdef USE_HOMEASSISTANT_TIME
  void send_time_request() {
    GetTimeRequest req;
    this->send_get_time_request(req);
  }
#endif

#ifdef USE_VOICE_ASSISTANT
  void subscribe_voice_assistant(const SubscribeVoiceAssistantRequest &msg) override;
  void on_voice_assistant_response(const VoiceAssistantResponse &msg) override;
  void on_voice_assistant_event_response(const VoiceAssistantEventResponse &msg) override;
  void on_voice_assistant_audio(const VoiceAssistantAudio &msg) override;
  void on_voice_assistant_timer_event_response(const VoiceAssistantTimerEventResponse &msg) override;
  void on_voice_assistant_announce_request(const VoiceAssistantAnnounceRequest &msg) override;
  VoiceAssistantConfigurationResponse voice_assistant_get_configuration(
      const VoiceAssistantConfigurationRequest &msg) override;
  void voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) override;
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  bool send_alarm_control_panel_state(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel);
  void send_alarm_control_panel_info(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel);
  static bool try_send_alarm_control_panel_state(APIConnection *api, void *v_a_alarm_control_panel);
  static bool try_send_alarm_control_panel_info(APIConnection *api, void *v_a_alarm_control_panel);
  void alarm_control_panel_command(const AlarmControlPanelCommandRequest &msg) override;
#endif

#ifdef USE_EVENT
  void send_event(event::Event *event, std::string event_type);
  void send_event_info(event::Event *event);
  static bool try_send_event(APIConnection *api, void *v_event);
  static bool try_send_event(APIConnection *api, event::Event *event, std::string event_type);
  static bool try_send_event_info(APIConnection *api, void *v_event);
#endif

#ifdef USE_UPDATE
  bool send_update_state(update::UpdateEntity *update);
  void send_update_info(update::UpdateEntity *update);
  static bool try_send_update_state(APIConnection *api, void *v_update);
  static bool try_send_update_info(APIConnection *api, void *v_update);
  void update_command(const UpdateCommandRequest &msg) override;
#endif

  void on_disconnect_response(const DisconnectResponse &value) override;
  void on_ping_response(const PingResponse &value) override {
    // we initiated ping
    this->ping_retries_ = 0;
    this->sent_ping_ = false;
  }
  void on_home_assistant_state_response(const HomeAssistantStateResponse &msg) override;
#ifdef USE_HOMEASSISTANT_TIME
  void on_get_time_response(const GetTimeResponse &value) override;
#endif
  HelloResponse hello(const HelloRequest &msg) override;
  ConnectResponse connect(const ConnectRequest &msg) override;
  DisconnectResponse disconnect(const DisconnectRequest &msg) override;
  PingResponse ping(const PingRequest &msg) override { return {}; }
  DeviceInfoResponse device_info(const DeviceInfoRequest &msg) override;
  void list_entities(const ListEntitiesRequest &msg) override { this->list_entities_iterator_.begin(); }
  void subscribe_states(const SubscribeStatesRequest &msg) override {
    this->state_subscription_ = true;
    this->initial_state_iterator_.begin();
  }
  void subscribe_logs(const SubscribeLogsRequest &msg) override {
    this->log_subscription_ = msg.level;
    if (msg.dump_config)
      App.schedule_dump_config();
  }
  void subscribe_homeassistant_services(const SubscribeHomeassistantServicesRequest &msg) override {
    this->service_call_subscription_ = true;
  }
  void subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) override;
  GetTimeResponse get_time(const GetTimeRequest &msg) override {
    // TODO
    return {};
  }
  void execute_service(const ExecuteServiceRequest &msg) override;

  bool is_authenticated() override { return this->connection_state_ == ConnectionState::AUTHENTICATED; }
  bool is_connection_setup() override {
    return this->connection_state_ == ConnectionState ::CONNECTED || this->is_authenticated();
  }
  void on_fatal_error() override;
  void on_unauthenticated_access() override;
  void on_no_setup_connection() override;
  ProtoWriteBuffer create_buffer() override {
    // FIXME: ensure no recursive writes can happen
    this->proto_write_buffer_.clear();
    return {&this->proto_write_buffer_};
  }
  bool send_buffer(ProtoWriteBuffer buffer, uint32_t message_type) override;

  std::string get_client_combined_info() const { return this->client_combined_info_; }

 protected:
  friend APIServer;

  bool send_(const void *buf, size_t len, bool force);

  enum class ConnectionState {
    WAITING_FOR_HELLO,
    CONNECTED,
    AUTHENTICATED,
  } connection_state_{ConnectionState::WAITING_FOR_HELLO};

  bool remove_{false};

  // Buffer used to encode proto messages
  // Re-use to prevent allocations
  std::vector<uint8_t> proto_write_buffer_;
  std::unique_ptr<APIFrameHelper> helper_;

  std::string client_info_;
  std::string client_peername_;
  std::string client_combined_info_;
  uint32_t client_api_version_major_{0};
  uint32_t client_api_version_minor_{0};
#ifdef USE_ESP32_CAMERA
  esp32_camera::CameraImageReader image_reader_;
#endif

  bool state_subscription_{false};
  int log_subscription_{ESPHOME_LOG_LEVEL_NONE};
  uint32_t last_traffic_;
  uint32_t next_ping_retry_{0};
  uint8_t ping_retries_{0};
  bool sent_ping_{false};
  bool service_call_subscription_{false};
  bool next_close_ = false;
  APIServer *parent_;
  DeferredMessageQueue deferred_message_queue_;
  InitialStateIterator initial_state_iterator_;
  ListEntitiesIterator list_entities_iterator_;
  int state_subs_at_ = -1;
};

}  // namespace api
}  // namespace esphome
#endif
