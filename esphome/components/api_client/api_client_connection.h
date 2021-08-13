#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "../api/api_pb2.h"
#include "api_pb2_client.h"
#include <map>

namespace esphome {
namespace api {

class APIClientConnection : public APIClientConnectionBase, public Component {
 public:
  APIClientConnection();
  virtual ~APIClientConnection();

  virtual void disconnect_client() = 0;
  void setup();
  float get_setup_priority() const override;
  virtual void loop();
  virtual size_t space() = 0;

  virtual void on_hello_response(const HelloResponse &value);
  virtual void on_connect_response(const ConnectResponse &value);
  virtual void on_disconnect_request(const DisconnectRequest &value);
  virtual void on_disconnect_response(const DisconnectResponse &value);
  virtual void on_ping_request(const PingRequest &value);
  virtual void on_ping_response(const PingResponse &value);
  virtual void on_device_info_response(const DeviceInfoResponse &value);
  virtual void on_list_entities_done_response(const ListEntitiesDoneResponse &value);
  virtual void on_subscribe_logs_response(const SubscribeLogsResponse &value);
  virtual void on_homeassistant_service_response(const HomeassistantServiceResponse &value);
  virtual void on_subscribe_home_assistant_state_response(const SubscribeHomeAssistantStateResponse &value);
  virtual void on_get_time_request(const GetTimeRequest &value);
  virtual void on_get_time_response(const GetTimeResponse &value);
  virtual void on_list_entities_services_response(const ListEntitiesServicesResponse &value);

#ifdef USE_BINARY_SENSOR
  virtual void on_list_entities_binary_sensor_response(const ListEntitiesBinarySensorResponse &value);
  virtual void on_binary_sensor_state_response(const BinarySensorStateResponse &value);
  virtual void register_binary_sensor(uint32_t key, binary_sensor::BinarySensor *obj);
#endif
#ifdef USE_COVER
  virtual void on_list_entities_cover_response(const ListEntitiesCoverResponse &value);
  virtual void on_cover_state_response(const CoverStateResponse &value);
#endif
#ifdef USE_FAN
  virtual void on_list_entities_fan_response(const ListEntitiesFanResponse &value);
  virtual void on_fan_state_response(const FanStateResponse &value);
#endif
#ifdef USE_LIGHT
  virtual void on_list_entities_light_response(const ListEntitiesLightResponse &value);
  virtual void on_light_state_response(const LightStateResponse &value);
  virtual void register_light(uint32_t key, light::Light *obj);
#endif
#ifdef USE_SENSOR
  virtual void on_list_entities_sensor_response(const ListEntitiesSensorResponse &value);
  virtual void on_sensor_state_response(const SensorStateResponse &value);
  virtual void register_sensor(uint32_t key, sensor::Sensor *obj);
#endif
#ifdef USE_SWITCH
  virtual void on_list_entities_switch_response(const ListEntitiesSwitchResponse &value);
  virtual void on_switch_state_response(const SwitchStateResponse &value);
  virtual void register_switch(uint32_t key, switch_::Switch *obj);
#endif
#ifdef USE_TEXT_SENSOR
  virtual void on_list_entities_text_sensor_response(const ListEntitiesTextSensorResponse &value);
  virtual void on_text_sensor_state_response(const TextSensorStateResponse &value);
  virtual void register_text_sensor(uint32_t key, text_sensor::TextSensor *obj);
#endif
#ifdef USE_ESP32_CAMERA
  virtual void on_list_entities_camera_response(const ListEntitiesCameraResponse &value);
  virtual void on_camera_image_response(const CameraImageResponse &value);
#endif
#ifdef USE_CLIMATE
  virtual void on_list_entities_climate_response(const ListEntitiesClimateResponse &value);
  virtual void on_climate_state_response(const ClimateStateResponse &value);
#endif

  ProtoWriteBuffer create_buffer() override {
    this->send_buffer_.clear();
    return {&this->send_buffer_};
  }
  bool send_buffer(ProtoWriteBuffer buffer, uint32_t message_type);
  virtual bool send_buffer(std::vector<uint8_t> header, ProtoWriteBuffer buffer) = 0;
  bool is_authenticated() { return authenticated_; }
  bool is_connection_setup() { return connected_; }
  void set_password(std::string password) { this->password_ = password; }

 protected:
  void parse_recv_buffer_();
  void register_mapping_(uint32_t key, Nameable *obj);

  enum class ConnectionState {
    WAITING_FOR_HELLO,
    CONNECTED,
    AUTHENTICATED,
  } connection_state_{ConnectionState::WAITING_FOR_HELLO};

  bool authenticated_{false};
  bool connected_{false};
  
  std::vector<uint8_t> send_buffer_;
  std::vector<uint8_t> recv_buffer_;

  std::string server_info_;
  std::string password_;

  uint32_t last_traffic_;
  bool sent_ping_{false};

  std::map<uint32_t, Nameable *> key_map_{};
};

class StreamAPIClientConnection : public APIClientConnection {
 public:
  StreamAPIClientConnection();
  virtual ~StreamAPIClientConnection();
  void on_disconnect_response(const DisconnectResponse &value) override {
    // just reset connection_state_
    this->connection_state_ = ConnectionState::WAITING_FOR_HELLO;
  }
  DisconnectResponse disconnect(const DisconnectRequest &msg) {
    // remote initiated disconnect_client
    this->connection_state_ = ConnectionState::WAITING_FOR_HELLO;
    DisconnectResponse resp;
    return resp;
  }
  bool send_buffer(std::vector<uint8_t> header, ProtoWriteBuffer buffer) override;
  void set_password(std::string password) {
    this->password_ = password;
    if (password == "") {
      this->authenticated_ = true;
      this->connected_ = true;
    }
  }
  void set_stream(Stream *stream) {
    this->client_ = stream;
  }
  void on_fatal_error() override;

  void disconnect_client() override;
  void loop() override;
  size_t space() { return 256; }  

 protected:
  Stream *client_;
  bool is_connected_() {
    const uint32_t timeout = 30000;
    if (millis() - this->last_traffic_ > timeout) {
      return false;
    }
    return true;
  }
};

template<typename... Ts> class APIClientConnectedCondition : public Condition<Ts...> {
 public:
  bool check(Ts... x) override { return true; }
};

}  // namespace api
}  // namespace esphome
