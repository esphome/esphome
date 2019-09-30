#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "api_pb2.h"
#include "api_pb2_service.h"
#include "util.h"
#include "list_entities.h"
#include "subscribe_state.h"
#include "homeassistant_service.h"
#include "user_services.h"

#ifdef ARDUINO_ARCH_ESP32
#include <AsyncTCP.h>
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include <ESPAsyncTCP.h>
#endif

namespace esphome {
namespace api {

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
  void send_homeassistant_service_call(const HomeassistantServiceResponse &call);
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

template<typename... Ts> class APIConnectedCondition : public Condition<Ts...> {
 public:
  bool check(Ts... x) override { return global_api_server->is_connected(); }
};

}  // namespace api
}  // namespace esphome
