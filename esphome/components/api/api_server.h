#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "util.h"
#include "api_message.h"
#include "basic_messages.h"
#include "list_entities.h"
#include "subscribe_state.h"
#include "subscribe_logs.h"
#include "command_messages.h"
#include "service_call_message.h"
#include "user_services.h"

#ifdef ARDUINO_ARCH_ESP32
#include <AsyncTCP.h>
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include <ESPAsyncTCP.h>
#endif

namespace esphome {
namespace api {

class APIServer;

class APIConnection {
 public:
  APIConnection(AsyncClient *client, APIServer *parent);
  ~APIConnection();

  void disconnect_client();
  APIBuffer get_buffer();
  bool send_buffer(APIMessageType type);
  bool send_message(APIMessage &msg);
  bool send_empty_message(APIMessageType type);
  void loop();

#ifdef USE_BINARY_SENSOR
  bool send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor, bool state);
#endif
#ifdef USE_COVER
  bool send_cover_state(cover::Cover *cover);
#endif
#ifdef USE_FAN
  bool send_fan_state(fan::FanState *fan);
#endif
#ifdef USE_LIGHT
  bool send_light_state(light::LightState *light);
#endif
#ifdef USE_SENSOR
  bool send_sensor_state(sensor::Sensor *sensor, float state);
#endif
#ifdef USE_SWITCH
  bool send_switch_state(switch_::Switch *a_switch, bool state);
#endif
#ifdef USE_TEXT_SENSOR
  bool send_text_sensor_state(text_sensor::TextSensor *text_sensor, std::string state);
#endif
#ifdef USE_ESP32_CAMERA
  void send_camera_state(std::shared_ptr<esp32_camera::CameraImage> image);
#endif
#ifdef USE_CLIMATE
  bool send_climate_state(climate::Climate *climate);
#endif
  bool send_log_message(int level, const char *tag, const char *line);
  bool send_disconnect_request();
  bool send_ping_request();
  void send_service_call(ServiceCallResponse &call);
#ifdef USE_HOMEASSISTANT_TIME
  void send_time_request();
#endif

 protected:
  friend APIServer;

  void on_error_(int8_t error);
  void on_disconnect_();
  void on_timeout_(uint32_t time);
  void on_data_(uint8_t *buf, size_t len);
  void fatal_error_();
  bool valid_rx_message_type_(uint32_t msg_type);
  void read_message_(uint32_t size, uint32_t type, uint8_t *msg);
  void parse_recv_buffer_();

  // request types
  void on_hello_request_(const HelloRequest &req);
  void on_connect_request_(const ConnectRequest &req);
  void on_disconnect_request_(const DisconnectRequest &req);
  void on_disconnect_response_(const DisconnectResponse &req);
  void on_ping_request_(const PingRequest &req);
  void on_ping_response_(const PingResponse &req);
  void on_device_info_request_(const DeviceInfoRequest &req);
  void on_list_entities_request_(const ListEntitiesRequest &req);
  void on_subscribe_states_request_(const SubscribeStatesRequest &req);
  void on_subscribe_logs_request_(const SubscribeLogsRequest &req);
#ifdef USE_COVER
  void on_cover_command_request_(const CoverCommandRequest &req);
#endif
#ifdef USE_FAN
  void on_fan_command_request_(const FanCommandRequest &req);
#endif
#ifdef USE_LIGHT
  void on_light_command_request_(const LightCommandRequest &req);
#endif
#ifdef USE_SWITCH
  void on_switch_command_request_(const SwitchCommandRequest &req);
#endif
#ifdef USE_CLIMATE
  void on_climate_command_request_(const ClimateCommandRequest &req);
#endif
  void on_subscribe_service_calls_request_(const SubscribeServiceCallsRequest &req);
  void on_subscribe_home_assistant_states_request_(const SubscribeHomeAssistantStatesRequest &req);
  void on_home_assistant_state_response_(const HomeAssistantStateResponse &req);
  void on_execute_service_(const ExecuteServiceRequest &req);
#ifdef USE_ESP32_CAMERA
  void on_camera_image_request_(const CameraImageRequest &req);
#endif

  enum class ConnectionState {
    WAITING_FOR_HELLO,
    WAITING_FOR_CONNECT,
    CONNECTED,
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
  AsyncClient *client_;
  APIServer *parent_;
  InitialStateIterator initial_state_iterator_;
  ListEntitiesIterator list_entities_iterator_;
};

template<typename... Ts> class HomeAssistantServiceCallAction;

class APIServer : public Component, public Controller {
 public:
  APIServer();
  void setup() override;
  uint16_t get_port() const;
  float get_setup_priority() const override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  bool check_password(const std::string &password) const;
  bool uses_password() const;
  void set_port(uint16_t port);
  void set_password(const std::string &password);
  void set_reboot_timeout(uint32_t reboot_timeout);
  void handle_disconnect(APIConnection *conn);
#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *obj, bool state) override;
#endif
#ifdef USE_COVER
  void on_cover_update(cover::Cover *obj) override;
#endif
#ifdef USE_FAN
  void on_fan_update(fan::FanState *obj) override;
#endif
#ifdef USE_LIGHT
  void on_light_update(light::LightState *obj) override;
#endif
#ifdef USE_SENSOR
  void on_sensor_update(sensor::Sensor *obj, float state) override;
#endif
#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *obj, bool state) override;
#endif
#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *obj, std::string state) override;
#endif
#ifdef USE_CLIMATE
  void on_climate_update(climate::Climate *obj) override;
#endif
  void send_service_call(ServiceCallResponse &call);
  void register_user_service(UserServiceDescriptor *descriptor) { this->user_services_.push_back(descriptor); }
#ifdef USE_HOMEASSISTANT_TIME
  void request_time();
#endif

  bool is_connected() const;

  struct HomeAssistantStateSubscription {
    std::string entity_id;
    std::function<void(std::string)> callback;
  };

  void subscribe_home_assistant_state(std::string entity_id, std::function<void(std::string)> f);
  const std::vector<HomeAssistantStateSubscription> &get_state_subs() const;
  const std::vector<UserServiceDescriptor *> &get_user_services() const { return this->user_services_; }

 protected:
  AsyncServer server_{0};
  uint16_t port_{6053};
  uint32_t reboot_timeout_{300000};
  uint32_t last_connected_{0};
  std::vector<APIConnection *> clients_;
  std::string password_;
  std::vector<HomeAssistantStateSubscription> state_subs_;
  std::vector<UserServiceDescriptor *> user_services_;
};

extern APIServer *global_api_server;

template<typename... Ts> class HomeAssistantServiceCallAction : public Action<Ts...> {
 public:
  explicit HomeAssistantServiceCallAction(APIServer *parent) : parent_(parent) {}
  void set_service(const std::string &service) { this->resp_.set_service(service); }
  void set_data(const std::vector<KeyValuePair> &data) { this->resp_.set_data(data); }
  void set_data_template(const std::vector<KeyValuePair> &data_template) {
    this->resp_.set_data_template(data_template);
  }
  void set_variables(const std::vector<TemplatableKeyValuePair> &variables) { this->resp_.set_variables(variables); }
  void play(Ts... x) override { this->parent_->send_service_call(this->resp_); }

 protected:
  APIServer *parent_;
  ServiceCallResponse resp_;
};

template<typename... Ts> class APIConnectedCondition : public Condition<Ts...> {
 public:
  bool check(Ts... x) override { return global_api_server->is_connected(); }
};

}  // namespace api
}  // namespace esphome
