#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "api_pb2.h"
#include "api_pb2_service.h"
#include "api_server.h"
#include "api_frame_helper.h"

namespace esphome {
namespace api {

class APIConnection : public APIServerConnection {
 public:
  APIConnection(std::unique_ptr<socket::Socket> socket, APIServer *parent);
  virtual ~APIConnection() = default;

  void start();
  void loop();

  bool send_list_info_done() {
    ListEntitiesDoneResponse resp;
    return this->send_list_entities_done_response(resp);
  }
#ifdef USE_BINARY_SENSOR
  bool send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor, bool state);
  bool send_binary_sensor_info(binary_sensor::BinarySensor *binary_sensor);
#endif
#ifdef USE_COVER
  bool send_cover_state(cover::Cover *cover);
  bool send_cover_info(cover::Cover *cover);
  void cover_command(const CoverCommandRequest &msg) override;
#endif
#ifdef USE_FAN
  bool send_fan_state(fan::Fan *fan);
  bool send_fan_info(fan::Fan *fan);
  void fan_command(const FanCommandRequest &msg) override;
#endif
#ifdef USE_LIGHT
  bool send_light_state(light::LightState *light);
  bool send_light_info(light::LightState *light);
  void light_command(const LightCommandRequest &msg) override;
#endif
#ifdef USE_SENSOR
  bool send_sensor_state(sensor::Sensor *sensor, float state);
  bool send_sensor_info(sensor::Sensor *sensor);
#endif
#ifdef USE_SWITCH
  bool send_switch_state(switch_::Switch *a_switch, bool state);
  bool send_switch_info(switch_::Switch *a_switch);
  void switch_command(const SwitchCommandRequest &msg) override;
#endif
#ifdef USE_TEXT_SENSOR
  bool send_text_sensor_state(text_sensor::TextSensor *text_sensor, std::string state);
  bool send_text_sensor_info(text_sensor::TextSensor *text_sensor);
#endif
#ifdef USE_ESP32_CAMERA
  void send_camera_state(std::shared_ptr<esp32_camera::CameraImage> image);
  bool send_camera_info(esp32_camera::ESP32Camera *camera);
  void camera_image(const CameraImageRequest &msg) override;
#endif
#ifdef USE_CLIMATE
  bool send_climate_state(climate::Climate *climate);
  bool send_climate_info(climate::Climate *climate);
  void climate_command(const ClimateCommandRequest &msg) override;
#endif
#ifdef USE_NUMBER
  bool send_number_state(number::Number *number, float state);
  bool send_number_info(number::Number *number);
  void number_command(const NumberCommandRequest &msg) override;
#endif
#ifdef USE_SELECT
  bool send_select_state(select::Select *select, std::string state);
  bool send_select_info(select::Select *select);
  void select_command(const SelectCommandRequest &msg) override;
#endif
#ifdef USE_BUTTON
  bool send_button_info(button::Button *button);
  void button_command(const ButtonCommandRequest &msg) override;
#endif
#ifdef USE_LOCK
  bool send_lock_state(lock::Lock *a_lock, lock::LockState state);
  bool send_lock_info(lock::Lock *a_lock);
  void lock_command(const LockCommandRequest &msg) override;
#endif
  bool send_log_message(int level, const char *tag, const char *line);
  void send_homeassistant_service_call(const HomeassistantServiceResponse &call) {
    if (!this->service_call_subscription_)
      return;
    this->send_homeassistant_service_response(call);
  }
#ifdef USE_HOMEASSISTANT_TIME
  void send_time_request() {
    GetTimeRequest req;
    this->send_get_time_request(req);
  }
#endif

  void on_disconnect_response(const DisconnectResponse &value) override;
  void on_ping_response(const PingResponse &value) override {
    // we initiated ping
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
#ifdef USE_ESP32_CAMERA
  esp32_camera::CameraImageReader image_reader_;
#endif

  bool state_subscription_{false};
  int log_subscription_{ESPHOME_LOG_LEVEL_NONE};
  uint32_t last_traffic_;
  bool sent_ping_{false};
  bool service_call_subscription_{false};
  bool next_close_ = false;
  APIServer *parent_;
  InitialStateIterator initial_state_iterator_;
  ListEntitiesIterator list_entities_iterator_;
  int state_subs_at_ = -1;
};

}  // namespace api
}  // namespace esphome
