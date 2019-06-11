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
  ~APIConnection();

  void disconnect_client();
  void loop();

  bool send_list_info_done() {
    ListEntitiesDoneResponse resp;
    return this->send_list_entities_done_response(resp);
  }
#ifdef USE_BINARY_SENSOR
  bool send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor, bool state);
  bool send_binary_sensor_info(binary_sensor::BinarySensor *binary_sensor) {
    ListEntitiesBinarySensorResponse msg;
    msg.object_id = binary_sensor->get_object_id();
    msg.key = binary_sensor->get_object_id_hash();
    msg.name = binary_sensor->get_name();
    msg.unique_id = "";  // TODO
    msg.device_class = binary_sensor->get_device_class();
    msg.is_status_binary_sensor = binary_sensor->is_status_binary_sensor();
    // buffer.encode_string(4, get_default_unique_id("binary_sensor", binary_sensor));
    return this->send_list_entities_binary_sensor_response(msg);
  }
#endif
#ifdef USE_COVER
  bool send_cover_state(cover::Cover *cover);
  bool send_cover_info(cover::Cover *cover) {
    auto traits = cover->get_traits();
    ListEntitiesCoverResponse msg;
    msg.key = cover->get_object_id_hash();
    msg.object_id = cover->get_object_id();
    msg.name = cover->get_name();
    msg.unique_id = "";  // TODO
    msg.assumed_state = traits.get_is_assumed_state();
    msg.supports_position = traits.get_supports_position();
    msg.supports_tilt = traits.get_supports_tilt();
    msg.device_class = cover->get_device_class();
    return this->send_list_entities_cover_response(msg);
  }
  void cover_command(const CoverCommandRequest &msg) override {
    cover::Cover *cover = App.get_cover_by_key(msg.key);
    if (cover == nullptr)
      return;

    auto call = cover->make_call();
    if (msg.has_legacy_command) {
      switch (msg.legacy_command) {
        case LEGACY_COVER_COMMAND_OPEN:
          call.set_command_open();
          break;
        case LEGACY_COVER_COMMAND_CLOSE:
          call.set_command_close();
          break;
        case LEGACY_COVER_COMMAND_STOP:
          call.set_command_stop();
          break;
      }
    }
    if (msg.has_position)
      call.set_position(msg.position);
    if (msg.has_tilt)
      call.set_tilt(msg.tilt);
    if (msg.stop)
      call.set_command_stop();
    call.perform();
  }
#endif
#ifdef USE_FAN
  bool send_fan_state(fan::FanState *fan);
  bool send_fan_info(fan::FanState *fan) {
    auto traits = fan->get_traits();
    ListEntitiesFanResponse msg;
    msg.key = fan->get_object_id_hash();
    msg.object_id = fan->get_object_id();
    msg.name = fan->get_name();
    msg.unique_id = "";  // TODO
    msg.supports_oscillation = traits.supports_oscillation();
    msg.supports_speed = traits.supports_speed();
    return this->send_list_entities_fan_response(msg);
  }
  void fan_command(const FanCommandRequest &msg) override {
    fan::FanState *fan = App.get_fan_by_key(msg.key);
    if (fan == nullptr)
      return;

    auto call = fan->make_call();
    if (msg.has_state)
      call.set_state(msg.state);
    if (msg.has_oscillating)
      call.set_oscillating(msg.oscillating);
    if (msg.has_speed)
      call.set_speed(static_cast<fan::FanSpeed>(msg.speed));
    call.perform();
  }
#endif
#ifdef USE_LIGHT
  bool send_light_state(light::LightState *light);
  bool send_light_info(light::LightState *light) {
    auto traits = light->get_traits();
    ListEntitiesLightResponse msg;
    msg.key = light->get_object_id_hash();
    msg.object_id = light->get_object_id();
    msg.name = light->get_name();
    msg.unique_id = "";  // TODO
    msg.supports_brightness = traits.get_supports_brightness();
    msg.supports_rgb = traits.get_supports_rgb();
    msg.supports_white_value = traits.get_supports_rgb_white_value();
    msg.supports_color_temperature = traits.get_supports_color_temperature();
    if (msg.supports_color_temperature) {
      msg.min_mireds = traits.get_min_mireds();
      msg.max_mireds = traits.get_max_mireds();
    }
    if (light->supports_effects()) {
      for (auto *effect : light->get_effects())
        msg.effects.push_back(effect->get_name());
    }
    return this->send_list_entities_light_response(msg);
  }
  void light_command(const LightCommandRequest &msg) override {
    light::LightState *light = App.get_light_by_key(msg.key);
    if (light == nullptr)
      return;

    auto call = light->make_call();
    if (msg.has_state)
      call.set_state(msg.state);
    if (msg.has_brightness)
      call.set_brightness(msg.brightness);
    if (msg.has_rgb) {
      call.set_red(msg.red);
      call.set_green(msg.green);
      call.set_blue(msg.blue);
    }
    if (msg.has_white)
      call.set_white(msg.white);
    if (msg.has_color_temperature)
      call.set_color_temperature(msg.color_temperature);
    if (msg.has_transition_length)
      call.set_transition_length(msg.transition_length);
    if (msg.has_flash_length)
      call.set_flash_length(msg.flash_length);
    if (msg.has_effect)
      call.set_effect(msg.effect);
    call.perform();
  }
#endif
#ifdef USE_SENSOR
  bool send_sensor_state(sensor::Sensor *sensor, float state);
  bool send_sensor_info(sensor::Sensor *sensor) {
    ListEntitiesSensorResponse msg;
    msg.key = sensor->get_object_id_hash();
    msg.object_id = sensor->get_object_id();
    msg.name = sensor->get_name();
    msg.unique_id = sensor->unique_id();  // TODO
    msg.icon = sensor->get_icon();
    msg.unit_of_measurement = sensor->get_unit_of_measurement();
    msg.accuracy_decimals = sensor->get_accuracy_decimals();
    return this->send_list_entities_sensor_response(msg);
  }
#endif
#ifdef USE_SWITCH
  bool send_switch_state(switch_::Switch *a_switch, bool state);
  bool send_switch_info(switch_::Switch *a_switch) {
    ListEntitiesSwitchResponse msg;
    msg.key = a_switch->get_object_id_hash();
    msg.object_id = a_switch->get_object_id();
    msg.name = a_switch->get_name();
    msg.unique_id = "";  // TODO
    msg.icon = a_switch->get_icon();
    msg.assumed_state = a_switch->assumed_state();
    return this->send_list_entities_switch_response(msg);
  }
  void switch_command(const SwitchCommandRequest &msg) override {
    switch_::Switch *a_switch = App.get_switch_by_key(msg.key);
    if (a_switch == nullptr)
      return;

    if (msg.state)
      a_switch->turn_on();
    else
      a_switch->turn_off();
  }
#endif
#ifdef USE_TEXT_SENSOR
  bool send_text_sensor_state(text_sensor::TextSensor *text_sensor, std::string state);
  bool send_text_sensor_info(text_sensor::TextSensor *text_sensor) {
    ListEntitiesTextSensorResponse msg;
    msg.key = text_sensor->get_object_id_hash();
    msg.object_id = text_sensor->get_object_id();
    msg.name = text_sensor->get_name();
    msg.unique_id = text_sensor->unique_id();  // TODO
    msg.icon = text_sensor->get_icon();
    return this->send_list_entities_text_sensor_response(msg);
  }
#endif
#ifdef USE_ESP32_CAMERA
  void send_camera_state(std::shared_ptr<esp32_camera::CameraImage> image);
  bool send_camera_info() {
    // TODO
  }
  void camera_image(const CameraImageRequest &msg) override {
    if (esp32_camera::global_esp32_camera == nullptr)
      return;

    if (msg.single)
      esp32_camera::global_esp32_camera->request_image();
    if (msg.stream)
      esp32_camera::global_esp32_camera->request_stream();
  }
#endif
#ifdef USE_CLIMATE
  bool send_climate_state(climate::Climate *climate);
  bool send_climate_info(climate::Climate *climate) {
    auto traits = climate->get_traits();
    ListEntitiesClimateResponse msg;
    msg.key = climate->get_object_id_hash();
    msg.object_id = climate->get_object_id();
    msg.name = climate->get_name();
    msg.unique_id = "";  // TODO
    msg.supports_current_temperature = traits.get_supports_current_temperature();
    msg.supports_two_point_target_temperature = traits.get_supports_two_point_target_temperature();
    for (auto mode : {climate::CLIMATE_MODE_AUTO, climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL,
                      climate::CLIMATE_MODE_HEAT}) {
      if (traits.supports_mode(mode))
        msg.supported_modes.push_back(static_cast<ClimateMode>(mode));
    }
    msg.visual_min_temperature = traits.get_visual_min_temperature();
    msg.visual_max_temperature = traits.get_visual_max_temperature();
    msg.visual_temperature_step = traits.get_visual_temperature_step();
    msg.supports_away = traits.get_supports_away();
    return this->send_list_entities_climate_response(msg);
  }
  void climate_command(const ClimateCommandRequest &msg) override {
    climate::Climate *climate = App.get_climate_by_key(msg.key);
    if (climate == nullptr)
      return;

    auto call = climate->make_call();
    if (msg.has_mode)
      call.set_mode(static_cast<climate::ClimateMode>(msg.mode));
    if (msg.has_target_temperature)
      call.set_target_temperature(msg.target_temperature);
    if (msg.has_target_temperature_low)
      call.set_target_temperature_low(msg.target_temperature_low);
    if (msg.has_target_temperature_high)
      call.set_target_temperature_high(msg.target_temperature_high);
    if (msg.has_away)
      call.set_away(msg.away);
    call.perform();
  }
#endif
  bool send_log_message(int level, const char *tag, const char *line);
  void send_service_call(const ServiceCallResponse &call) {
    if (!this->service_call_subscription_)
      return;
    this->send_service_call_response(call);
  }
#ifdef USE_HOMEASSISTANT_TIME
  void send_time_request() {
    GetTimeRequest req;
    this->send_get_time_request(req);
  }
#endif

 protected:
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
  PingResponse ping(const PingRequest &msg) override {
    return {};
  }
  DeviceInfoResponse device_info(const DeviceInfoRequest &msg) override;
  void list_entities(const ListEntitiesRequest &msg) override {
    this->list_entities_iterator_.begin();
  }
  void subscribe_states(const SubscribeStatesRequest &msg) override {
    this->state_subscription_ = true;
    this->initial_state_iterator_.begin();
  }
  void subscribe_logs(const SubscribeLogsRequest &msg) override {
    this->log_subscription_ = msg.level;
    if (msg.dump_config)
      App.schedule_dump_config();
  }
  void subscribe_service_calls(const SubscribeServiceCallsRequest &msg) override {
    this->service_call_subscription_ = true;
  }
  void subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) override;
  GetTimeResponse get_time(const GetTimeRequest &msg) override {
    // TODO
    return {};
  }
  void execute_service(const ExecuteServiceRequest &msg) override;
  bool is_authenticated() override {
    return this->connection_state_ == ConnectionState::AUTHENTICATED;
  }
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
  bool next_close_{false};
  AsyncClient *client_;
  APIServer *parent_;
  InitialStateIterator initial_state_iterator_;
  ListEntitiesIterator list_entities_iterator_;
};

}  // namespace api
}  // namespace esphome
