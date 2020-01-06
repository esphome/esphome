#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32
#include <HTTPClient.h>
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266HTTPClient.h>
#endif
#include <WiFiClientSecure.h>

namespace esphome {
namespace azure_iot_hub {


class AzureIoTHub : public Component, public Controller {
public:
    AzureIoTHub() { }

    void setup() override;
    void dump_config() override;

    void set_iot_hub_device_id(const std::string &device_id);
    void set_iot_hub_sas_token(const std::string &sas_token);
    void set_iot_hub_rest_url(const std::string &rest_url);
#ifdef ARDUINO_ARCH_ESP8266
    void set_iot_hub_ssl_sha1_fingerprint(const std::string &fingerprint);
#endif
#ifdef ARDUINO_ARCH_ESP32
    void set_baltimore_root_ca_pem(const std::string &baltimore__root_ca_pem);
#endif
    // Expiration string is only there for debug purposes. dump_config will allow to identify when token is expired
    void set_iot_hub_sas_token_expiration_string(const std::string &expirationString);
    
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

protected:
    std::string iot_hub_sas_token_;
    std::string iot_hub_rest_url_;
    std::string iot_hub_device_id_;
    std::string iot_hub_sas_token_expiration_string_;
    HTTPClient http_client_{};
    bool post_json_to_iot_hub(const std::string json_payload);

private:
#ifdef ARDUINO_ARCH_ESP8266
    std::string iot_hub_ssl_sha1_fingerprint_;
    uint8_t ssl_sha1_fingerprint_bytes_[20];
    BearSSL::WiFiClientSecure *wifi_client_;
    bool set_fingerprint_bytes(const char *fingerprint_string);
#endif
#ifdef ARDUINO_ARCH_ESP32
    std::string baltimore_root_ca_pem_;
    WiFiClientSecure *wifi_client_;
#endif
    
};


}  // namespace azure_iot_hub
}  // namespace esphome
