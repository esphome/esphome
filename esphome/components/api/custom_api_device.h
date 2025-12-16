#pragma once

#include <map>
#include "api_server.h"
#ifdef USE_API
#ifdef USE_API_USER_DEFINED_ACTIONS
#include "user_services.h"
#endif
namespace esphome::api {

#ifdef USE_API_USER_DEFINED_ACTIONS
template<typename T, typename... Ts> class CustomAPIDeviceService : public UserServiceDynamic<Ts...> {
 public:
  CustomAPIDeviceService(const std::string &name, const std::array<std::string, sizeof...(Ts)> &arg_names, T *obj,
                         void (T::*callback)(Ts...))
      : UserServiceDynamic<Ts...>(name, arg_names), obj_(obj), callback_(callback) {}

 protected:
  // CustomAPIDevice services don't support action responses - ignore call_id and return_response
  void execute(uint32_t /*call_id*/, bool /*return_response*/, Ts... x) override {
    (this->obj_->*this->callback_)(x...);  // NOLINT
  }

  T *obj_;
  void (T::*callback_)(Ts...);
};
#endif  // USE_API_USER_DEFINED_ACTIONS

class CustomAPIDevice {
 public:
  /// Return if a client (such as Home Assistant) is connected to the native API.
  bool is_connected() const { return global_api_server->is_connected(); }

  /** Register a custom native API service that will show up in Home Assistant.
   *
   * Usage:
   *
   * ```cpp
   * void setup() override {
   *   register_service(&CustomNativeAPI::on_start_washer_cycle, "start_washer_cycle",
   *                    {"cycle_length"});
   * }
   *
   * void on_start_washer_cycle(int cycle_length) {
   *   // Start washer cycle.
   * }
   * ```
   *
   * @tparam T The class type creating the service, automatically deduced from the function pointer.
   * @tparam Ts The argument types for the service, automatically deduced from the function arguments.
   * @param callback The member function to call when the service is triggered.
   * @param name The name of the service to register.
   * @param arg_names The name of the arguments for the service, must match the arguments of the function.
   */
#ifdef USE_API_USER_DEFINED_ACTIONS
  template<typename T, typename... Ts>
  void register_service(void (T::*callback)(Ts...), const std::string &name,
                        const std::array<std::string, sizeof...(Ts)> &arg_names) {
#ifdef USE_API_CUSTOM_SERVICES
    auto *service = new CustomAPIDeviceService<T, Ts...>(name, arg_names, (T *) this, callback);  // NOLINT
    global_api_server->register_user_service(service);
#else
    static_assert(
        sizeof(T) == 0,
        "register_service() requires 'custom_services: true' in the 'api:' section of your YAML configuration");
#endif
  }
#else
  template<typename T, typename... Ts>
  void register_service(void (T::*callback)(Ts...), const std::string &name,
                        const std::array<std::string, sizeof...(Ts)> &arg_names) {
    static_assert(
        sizeof(T) == 0,
        "register_service() requires 'custom_services: true' in the 'api:' section of your YAML configuration");
  }
#endif

  /** Register a custom native API service that will show up in Home Assistant.
   *
   * Usage:
   *
   * ```cpp
   * void setup() override {
   *   register_service(&CustomNativeAPI::on_hello_world, "hello_world");
   * }
   *
   * void on_hello_world() {
   *   // Hello World service called.
   * }
   * ```
   *
   * @tparam T The class type creating the service, automatically deduced from the function pointer.
   * @param callback The member function to call when the service is triggered.
   * @param name The name of the arguments for the service, must match the arguments of the function.
   */
#ifdef USE_API_USER_DEFINED_ACTIONS
  template<typename T> void register_service(void (T::*callback)(), const std::string &name) {
#ifdef USE_API_CUSTOM_SERVICES
    auto *service = new CustomAPIDeviceService<T>(name, {}, (T *) this, callback);  // NOLINT
    global_api_server->register_user_service(service);
#else
    static_assert(
        sizeof(T) == 0,
        "register_service() requires 'custom_services: true' in the 'api:' section of your YAML configuration");
#endif
  }
#else
  template<typename T> void register_service(void (T::*callback)(), const std::string &name) {
    static_assert(
        sizeof(T) == 0,
        "register_service() requires 'custom_services: true' in the 'api:' section of your YAML configuration");
  }
#endif

#ifdef USE_API_HOMEASSISTANT_STATES
  /** Subscribe to the state (or attribute state) of an entity from Home Assistant.
   *
   * Usage:
   *
   * ```cpp
   * void setup() override {
   *   subscribe_homeassistant_state(&CustomNativeAPI::on_state_changed, "climate.kitchen", "current_temperature");
   * }
   *
   * void on_state_changed(std::string state) {
   *   // State of sensor.weather_forecast is `state`
   * }
   * ```
   *
   * @tparam T The class type creating the service, automatically deduced from the function pointer.
   * @param callback The member function to call when the entity state changes.
   * @param entity_id The entity_id to track.
   * @param attribute The entity state attribute to track.
   */
  template<typename T>
  void subscribe_homeassistant_state(void (T::*callback)(std::string), const std::string &entity_id,
                                     const std::string &attribute = "") {
    auto f = std::bind(callback, (T *) this, std::placeholders::_1);
    global_api_server->subscribe_home_assistant_state(entity_id, optional<std::string>(attribute), f);
  }

  /** Subscribe to the state (or attribute state) of an entity from Home Assistant.
   *
   * Usage:
   *
   * ```cpp
   * void setup() override {
   *   subscribe_homeassistant_state(&CustomNativeAPI::on_state_changed, "sensor.weather_forecast");
   * }
   *
   * void on_state_changed(std::string entity_id, std::string state) {
   *   // State of `entity_id` is `state`
   * }
   * ```
   *
   * @tparam T The class type creating the service, automatically deduced from the function pointer.
   * @param callback The member function to call when the entity state changes.
   * @param entity_id The entity_id to track.
   * @param attribute The entity state attribute to track.
   */
  template<typename T>
  void subscribe_homeassistant_state(void (T::*callback)(std::string, std::string), const std::string &entity_id,
                                     const std::string &attribute = "") {
    auto f = std::bind(callback, (T *) this, entity_id, std::placeholders::_1);
    global_api_server->subscribe_home_assistant_state(entity_id, optional<std::string>(attribute), f);
  }
#else
  template<typename T>
  void subscribe_homeassistant_state(void (T::*callback)(std::string), const std::string &entity_id,
                                     const std::string &attribute = "") {
    static_assert(sizeof(T) == 0,
                  "subscribe_homeassistant_state() requires 'homeassistant_states: true' in the 'api:' section "
                  "of your YAML configuration");
  }

  template<typename T>
  void subscribe_homeassistant_state(void (T::*callback)(std::string, std::string), const std::string &entity_id,
                                     const std::string &attribute = "") {
    static_assert(sizeof(T) == 0,
                  "subscribe_homeassistant_state() requires 'homeassistant_states: true' in the 'api:' section "
                  "of your YAML configuration");
  }
#endif

#ifdef USE_API_HOMEASSISTANT_SERVICES
  /** Call a Home Assistant service from ESPHome.
   *
   * Usage:
   *
   * ```cpp
   * call_homeassistant_service("homeassistant.restart");
   * ```
   *
   * @param service_name The service to call.
   */
  void call_homeassistant_service(const std::string &service_name) {
    HomeassistantActionRequest resp;
    resp.set_service(StringRef(service_name));
    global_api_server->send_homeassistant_action(resp);
  }

  /** Call a Home Assistant service from ESPHome.
   *
   * Usage:
   *
   * ```cpp
   * call_homeassistant_service("light.turn_on", {
   *   {"entity_id", "light.my_light"},
   *   {"brightness", "127"},
   * });
   * ```
   *
   * @param service_name The service to call.
   * @param data The data for the service call, mapping from string to string.
   */
  void call_homeassistant_service(const std::string &service_name, const std::map<std::string, std::string> &data) {
    HomeassistantActionRequest resp;
    resp.set_service(StringRef(service_name));
    resp.data.init(data.size());
    for (auto &it : data) {
      auto &kv = resp.data.emplace_back();
      kv.set_key(StringRef(it.first));
      kv.value = it.second;
    }
    global_api_server->send_homeassistant_action(resp);
  }

  /** Fire an ESPHome event in Home Assistant.
   *
   * Usage:
   *
   * ```cpp
   * fire_homeassistant_event("esphome.something_happened");
   * ```
   *
   * @param event_name The event to fire.
   */
  void fire_homeassistant_event(const std::string &event_name) {
    HomeassistantActionRequest resp;
    resp.set_service(StringRef(event_name));
    resp.is_event = true;
    global_api_server->send_homeassistant_action(resp);
  }

  /** Fire an ESPHome event in Home Assistant.
   *
   * Usage:
   *
   * ```cpp
   * fire_homeassistant_event("esphome.something_happened", {
   *   {"my_value", "500"},
   * });
   * ```
   *
   * @param event_name The event to fire.
   * @param data The data for the event, mapping from string to string.
   */
  void fire_homeassistant_event(const std::string &service_name, const std::map<std::string, std::string> &data) {
    HomeassistantActionRequest resp;
    resp.set_service(StringRef(service_name));
    resp.is_event = true;
    resp.data.init(data.size());
    for (auto &it : data) {
      auto &kv = resp.data.emplace_back();
      kv.set_key(StringRef(it.first));
      kv.value = it.second;
    }
    global_api_server->send_homeassistant_action(resp);
  }
#else
  template<typename T = void> void call_homeassistant_service(const std::string &service_name) {
    static_assert(sizeof(T) == 0, "call_homeassistant_service() requires 'homeassistant_services: true' in the 'api:' "
                                  "section of your YAML configuration");
  }

  template<typename T = void>
  void call_homeassistant_service(const std::string &service_name, const std::map<std::string, std::string> &data) {
    static_assert(sizeof(T) == 0, "call_homeassistant_service() requires 'homeassistant_services: true' in the 'api:' "
                                  "section of your YAML configuration");
  }

  template<typename T = void> void fire_homeassistant_event(const std::string &event_name) {
    static_assert(sizeof(T) == 0, "fire_homeassistant_event() requires 'homeassistant_services: true' in the 'api:' "
                                  "section of your YAML configuration");
  }

  template<typename T = void>
  void fire_homeassistant_event(const std::string &service_name, const std::map<std::string, std::string> &data) {
    static_assert(sizeof(T) == 0, "fire_homeassistant_event() requires 'homeassistant_services: true' in the 'api:' "
                                  "section of your YAML configuration");
  }
#endif
};

}  // namespace esphome::api
#endif
