#pragma once

#include "esphome/core/defines.h"
#ifdef USE_MQTT

#include "esphome/core/component.h"
#include "mqtt_client.h"

namespace esphome {
namespace mqtt {

/** This class is a helper class for custom components that communicate using
 * MQTT. It has 5 helper functions that you can use (square brackets indicate optional):
 *
 *  - `subscribe(topic, function_pointer, [qos])`
 *  - `subscribe_json(topic, function_pointer, [qos])`
 *  - `publish(topic, payload, [qos], [retain])`
 *  - `publish_json(topic, payload_builder, [qos], [retain])`
 *  - `is_connected()`
 */
class CustomMQTTDevice {
 public:
  /** Subscribe to an MQTT topic with the given Quality of Service.
   *
   * Example:
   *
   * ```cpp
   * class MyCustomMQTTDevice : public Component, public mqtt:CustomMQTTDevice {
   *  public:
   *   void setup() override {
   *     subscribe("the/topic", &MyCustomMQTTDevice::on_message);
   *     pinMode(5, OUTPUT);
   *   }
   *
   *   // topic and payload parameters can be removed if not needed
   *   // e.g: void on_message() {
   *
   *   void on_message(const std::string &topic, const std::string &payload) {
   *     // do something with topic and payload
   *     if (payload == "ON") {
   *       digitalWrite(5, HIGH);
   *     } else {
   *       digitalWrite(5, LOW);
   *     }
   *   }
   * };
   * ```
   *
   * @tparam T A C++ template argument for determining the type of the callback.
   * @param topic The topic to subscribe to. Re-subscription on re-connects is automatically handled.
   * @param callback The callback (must be a class member) to subscribe with.
   * @param qos The Quality of Service to subscribe with. Defaults to 0.
   */
  template<typename T>
  void subscribe(const std::string &topic, void (T::*callback)(const std::string &, const std::string &),
                 uint8_t qos = 0);

  template<typename T>
  void subscribe(const std::string &topic, void (T::*callback)(const std::string &), uint8_t qos = 0);

  template<typename T> void subscribe(const std::string &topic, void (T::*callback)(), uint8_t qos = 0);

  /** Subscribe to an MQTT topic and call the callback if the payload can be decoded
   * as JSON with the given Quality of Service.
   *
   * Example:
   *
   * ```cpp
   * class MyCustomMQTTDevice : public Component, public mqtt:CustomMQTTDevice {
   *  public:
   *   void setup() override {
   *     subscribe_json("the/topic", &MyCustomMQTTDevice::on_json_message);
   *     pinMode(5, OUTPUT);
   *   }
   *
   *   // topic parameter can be remove if not needed:
   *   // e.g.: void on_json_message(JsonObject payload) {
   *
   *   void on_json_message(const std::string &topic, JsonObject payload) {
   *     // do something with topic and payload
   *     if (payload["number"] == 1) {
   *       digitalWrite(5, HIGH);
   *     } else {
   *       digitalWrite(5, LOW);
   *     }
   *   }
   * };
   * ```
   *
   * @tparam T A C++ template argument for determining the type of the callback.
   * @param topic The topic to subscribe to. Re-subscription on re-connects is automatically handled.
   * @param callback The callback (must be a class member) to subscribe with.
   * @param qos The Quality of Service to subscribe with. Defaults to 0.
   */
  template<typename T>
  void subscribe_json(const std::string &topic, void (T::*callback)(const std::string &, JsonObject), uint8_t qos = 0);

  template<typename T> void subscribe_json(const std::string &topic, void (T::*callback)(JsonObject), uint8_t qos = 0);

  /** Publish an MQTT message with the given payload and QoS and retain settings.
   *
   * Example:
   *
   * ```cpp
   * void in_some_method() {
   *   publish("the/topic", "The Payload", 0, true);
   * }
   * ```
   *
   * @param topic The topic to publish to.
   * @param payload The payload to publish.
   * @param qos The Quality of Service to publish with. Defaults to 0
   * @param retain Whether to retain the message. Defaults to false.
   */
  bool publish(const std::string &topic, const std::string &payload, uint8_t qos = 0, bool retain = false);

  /** Publish an MQTT message with the given floating point number and number of decimals.
   *
   * Example:
   *
   * ```cpp
   * void in_some_method() {
   *   publish("the/topic", 1.0);
   *   // with two digits after the decimal point
   *   publish("the/topic", 1.0, 2);
   * }
   * ```
   *
   * @param topic The topic to publish to.
   * @param payload The payload to publish.
   * @param number_decimals The number of digits after the decimal point to round to, defaults to 3 digits.
   */
  bool publish(const std::string &topic, float value, int8_t number_decimals = 3);

  /** Publish an MQTT message with the given integer as payload.
   *
   * Example:
   *
   * ```cpp
   * void in_some_method() {
   *   publish("the/topic", 42);
   * }
   * ```
   *
   * @param topic The topic to publish to.
   * @param payload The payload to publish.
   */
  bool publish(const std::string &topic, int value);

  /** Publish a JSON-encoded MQTT message with the given Quality of Service and retain settings.
   *
   * Example:
   *
   * ```cpp
   * void in_some_method() {
   *   publish("the/topic", [=](JsonObject root) {
   *     root["the_key"] = "Hello World!";
   *   }, 0, false);
   * }
   * ```
   *
   * @param topic The topic to publish to.
   * @param payload The payload to publish.
   * @param qos The Quality of Service to publish with.
   * @param retain Whether to retain the message.
   */
  bool publish_json(const std::string &topic, const json::json_build_t &f, uint8_t qos, bool retain);

  /** Publish a JSON-encoded MQTT message.
   *
   * Example:
   *
   * ```cpp
   * void in_some_method() {
   *   publish("the/topic", [=](JsonObject root) {
   *     root["the_key"] = "Hello World!";
   *   });
   * }
   * ```
   *
   * @param topic The topic to publish to.
   * @param payload The payload to publish.
   */
  bool publish_json(const std::string &topic, const json::json_build_t &f);

  /// Check whether the MQTT client is currently connected and messages can be published.
  bool is_connected();
};

template<typename T>
void CustomMQTTDevice::subscribe(const std::string &topic,
                                 void (T::*callback)(const std::string &, const std::string &), uint8_t qos) {
  auto f = std::bind(callback, (T *) this, std::placeholders::_1, std::placeholders::_2);
  global_mqtt_client->subscribe(topic, f, qos);
}
template<typename T>
void CustomMQTTDevice::subscribe(const std::string &topic, void (T::*callback)(const std::string &), uint8_t qos) {
  auto f = std::bind(callback, (T *) this, std::placeholders::_2);
  global_mqtt_client->subscribe(topic, f, qos);
}
template<typename T> void CustomMQTTDevice::subscribe(const std::string &topic, void (T::*callback)(), uint8_t qos) {
  auto f = std::bind(callback, (T *) this);
  global_mqtt_client->subscribe(topic, f, qos);
}
template<typename T>
void CustomMQTTDevice::subscribe_json(const std::string &topic, void (T::*callback)(const std::string &, JsonObject),
                                      uint8_t qos) {
  auto f = std::bind(callback, (T *) this, std::placeholders::_1, std::placeholders::_2);
  global_mqtt_client->subscribe_json(topic, f, qos);
}
template<typename T>
void CustomMQTTDevice::subscribe_json(const std::string &topic, void (T::*callback)(JsonObject), uint8_t qos) {
  auto f = std::bind(callback, (T *) this, std::placeholders::_2);
  global_mqtt_client->subscribe_json(topic, f, qos);
}

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_MQTT
