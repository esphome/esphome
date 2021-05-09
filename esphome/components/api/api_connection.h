#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "api_pb2.h"
#include "api_pb2_service.h"
#include "api_server.h"

namespace esphome {
namespace api {

class APIConnection : public APIServerConnection {
 public:
  APIConnection(AsyncClient *client, APIServer *parent);
  virtual ~APIConnection();

  void disconnect_client();
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
  bool send_fan_state(fan::FanState *fan);
  bool send_fan_info(fan::FanState *fan);
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

  void on_disconnect_response(const DisconnectResponse &value) override {
    // we initiated disconnect_client
    this->next_close_ = true;
  }
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
  DisconnectResponse disconnect(const DisconnectRequest &msg) override {
    // remote initiated disconnect_client
    this->next_close_ = true;
    DisconnectResponse resp;
    return resp;
  }
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
    this->send_buffer_.clear();
    return {&this->send_buffer_};
  }
  bool send_buffer(ProtoWriteBuffer buffer, uint32_t message_type) override;

 protected:
  friend APIServer;

  void on_error_(int8_t error);
  void on_disconnect_();
  void on_timeout_(uint32_t time);
  void on_data_(uint8_t *buf, size_t len);
  void parse_recv_buffer_();

  enum class ConnectionState {
    WAITING_FOR_HELLO,
    CONNECTED,
    AUTHENTICATED,
  } connection_state_{ConnectionState::WAITING_FOR_HELLO};

  bool remove_{false};

  std::vector<uint8_t> send_buffer_;
  std::vector<uint8_t> recv_buffer_;

  std::string client_info_;
#ifdef USE_ESP32_CAMERA
  esp32_camera::CameraImageReader image_reader_;
#endif

  bool state_subscription_{false};
  int log_subscription_{ESPHOME_LOG_LEVEL_NONE};
  uint32_t last_traffic_;
  bool sent_ping_{false};
  bool service_call_subscription_{false};
  bool current_nodelay_{false};
  bool next_close_{false};
  AsyncClient *client_;
  APIServer *parent_;
  InitialStateIterator initial_state_iterator_;
  ListEntitiesIterator list_entities_iterator_;
};

}  // namespace api
}  // namespace esphome
