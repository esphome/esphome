#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/http_request/http_request.h"

namespace esphome {
namespace azure_iot_hub {

class AzureIoTHub : public Component, public Controller {
 public:
  AzureIoTHub() {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  std::string get_iot_hub_device_id() const;
  const char *get_iot_hub_rest_url() const;
  std::string get_iot_hub_sas_token() const;

  void set_iot_hub_device_id(const std::string &device_id);
  void set_iot_hub_sas_token(const std::string &sas_token);
  void set_iot_hub_rest_url(const std::string &rest_url);
#ifdef ARDUINO_ARCH_ESP8266
  // ESP32 doesn't currently support fingerprints in its SSL stack
  void set_iot_hub_ssl_sha1_fingerprint(const std::string &fingerprint);
#endif
  // Expiration string is only there for debug purposes. dump_config will allow to identify when token is expired
  void set_iot_hub_sas_token_expiration_string(const std::string &expiration_string);

#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *sensor, bool state) override;
#endif
#ifdef USE_COVER
  void on_cover_update(cover::Cover *cover) override;
#endif
#ifdef USE_FAN
  void on_fan_update(fan::FanState *fan) override;
#endif
#ifdef USE_LIGHT
  void on_light_update(light::LightState *light) override;
#endif
#ifdef USE_SENSOR
  void on_sensor_update(sensor::Sensor *sensor, float state) override;
#endif
#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *sw, bool state) override;
#endif
#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *text, std::string state) override;
#endif
#ifdef USE_CLIMATE
  void on_climate_update(climate::Climate *climate) override;
#endif

 protected:
  std::string iot_hub_sas_token_;
  std::string iot_hub_rest_url_;
  std::string iot_hub_device_id_;
  std::string iot_hub_sas_token_expiration_string_;
  bool post_json_to_iot_hub_(std::string json_payload);
  http_request::HttpRequestComponent http_request_;
};

}  // namespace azure_iot_hub
}  // namespace esphome
