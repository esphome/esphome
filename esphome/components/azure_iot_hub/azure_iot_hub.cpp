#include "azure_iot_hub.h"

namespace esphome {
namespace azure_iot_hub {

static const char *TAG = "azure_iot_hub";

void AzureIoTHub::dump_config() {
    ESP_LOGCONFIG(TAG, "Azure IoT Hub:");
    ESP_LOGCONFIG(TAG, "  Device ID: %s", this->iot_hub_device_id_.c_str());
    ESP_LOGCONFIG(TAG, "  REST URL: %s", this->iot_hub_rest_url_.c_str());
    ESP_LOGCONFIG(TAG, "  SAS Token: %s", this->iot_hub_sas_token_.c_str());
    ESP_LOGCONFIG(TAG, "  SAS Token Expiration: %s", this->iot_hub_sas_token_expiration_string_.c_str());
    ESP_LOGCONFIG(TAG, "  SSL SHA1 Fingerprint: %s", this->iot_hub_ssl_sha1_fingerprint_.empty() ? "<NULL>" : this->iot_hub_ssl_sha1_fingerprint_.c_str());
}

#ifdef ARDUINO_ARCH_ESP8266
static uint8_t htoi (unsigned char c)
{
    if (c>='0' && c <='9') return c - '0';
    else if (c>='A' && c<='F') return 10 + c - 'A';
    else if (c>='a' && c<='f') return 10 + c - 'a';
    else return 255;
}

// Set a fingerprint by parsing an ASCII string,
// based on codebase of ESP8266 arduino core. 
// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiClientSecureBearSSL.cpp
// Preserving this helps reduce setup and handle CLANG ESP8266 dependency not having relevant overload
bool setFingerprintBytes(const char *fingerprint_string, uint8_t* fingerprint_bytes) {
    int idx = 0;
    uint8_t c, d;

    while (idx < 20) {
        c = *fingerprint_string++;
        if (!c) break; // String ended, done processing
        d = *fingerprint_string++;
        if (!d) return false; // Only half of the last hex digit, error
        c = htoi(c);
        d = htoi(d);
        if ((c>15) || (d>15)) {
        return false; // Error in one of the hex characters
        }
        fingerprint_bytes[idx++] = (c<<4)|d;

        // Skip 0 or more spaces or colons
        while ( *fingerprint_string && (*fingerprint_string == ' ' || *fingerprint_string == ':' ) ) {
        fingerprint_string++;
        }
    }
    if ((idx != 20)) {
        return false; // Garbage at EOL or we didn't have enough hex digits
    }
    else {
        return true;
    }
}
#endif

void AzureIoTHub::setup() {
    if (this->iot_hub_ssl_sha1_fingerprint_.length() >= 20) {
        this->secure_ssl_ = setFingerprintBytes(this->iot_hub_ssl_sha1_fingerprint_.c_str(), this->ssl_sha1_fingerprint_bytes_);
    }
    else {
        this->secure_ssl_ = false;
    }

#ifdef ARDUINO_ARCH_ESP8266
    this->wifi_client_ = new BearSSL::WiFiClientSecure();
    
    if (this->secure_ssl_) {
        this->wifi_client_->setFingerprint(this->ssl_sha1_fingerprint_bytes_);
    }
    else {
        this->wifi_client_->setInsecure();
    }
    this->wifi_client_->setBufferSizes(512, 512);
#endif
}


void AzureIoTHub::set_iot_hub_device_id(const std::string &device_id) { this->iot_hub_device_id_ = device_id; }
void AzureIoTHub::set_iot_hub_sas_token(const std::string &sas_token) { this->iot_hub_sas_token_ = sas_token; }
void AzureIoTHub::set_iot_hub_rest_url(const std::string &rest_url) { this->iot_hub_rest_url_ = rest_url; }
void AzureIoTHub::set_iot_hub_ssl_sha1_fingerprint(const std::string &fingerprint) { this->iot_hub_ssl_sha1_fingerprint_ = fingerprint; }
void AzureIoTHub::set_iot_hub_sas_token_expiration_string(const std::string &expirationString) { this->iot_hub_sas_token_expiration_string_ = expirationString; }

bool AzureIoTHub::post_json_to_iot_hub(const std::string json_payload) {
    String url{this->iot_hub_rest_url_.c_str()};
    
#if ARDUINO_ARCH_ESP8266
#ifndef CLANG_TIDY
    this->http_client_.begin(*this->wifi_client_, url);
#endif
#endif
#if ARDUINO_ARCH_ESP32
    if (this->secure_ssl_) {
        this->http_client_.begin(url, this->ssl_sha1_fingerprint_bytes_);
    }
    else {
        this->http_client_.begin(url);
    }
#endif
    this->http_client_.addHeader(F("Authorization"), this->iot_hub_sas_token_.c_str());
    this->http_client_.addHeader(F("Content-Type"), F("application/json"));
    this->http_client_.addHeader(F("Content-Length"), String(json_payload.length()));

    int httpCode = this->http_client_.POST(json_payload.c_str());
    this->http_client_.end();
    return httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT;
}

#ifdef USE_BINARY_SENSOR
void AzureIoTHub::on_binary_sensor_update(binary_sensor::BinarySensor *sensor, bool state) {
    if (sensor->is_internal())
        return;
  
    std::string id = sensor->get_object_id();
    std::string json_payload{"{ \"deviceId\":\""};
    json_payload.append(this->iot_hub_device_id_);
    json_payload.append("\", \"");
    json_payload.append(id);
    json_payload.append("\": { \"type\": \"binary_sensor\"");
    std::string name = sensor->get_name();
    if (!name.empty()) {
        json_payload.append(", \"name\": \"");
        json_payload.append(name);
        json_payload.append("\"");
    }

    json_payload.append(", \"state\": ");
    json_payload.append(state ? "true": "false");

    json_payload.append(", \"missingState\": ");
    json_payload.append(sensor->has_state() ? "false": "true");

    json_payload.append("} }");

    this->post_json_to_iot_hub(json_payload);
}
#endif

#ifdef USE_COVER
void AzureIoTHub::on_cover_update(cover::Cover *cover) {
    if (cover->is_internal())
        return;
    auto traits = cover->get_traits();
    std::string id = cover->get_object_id();
    std::string json_payload{"{ \"deviceId\":\""};
    json_payload.append(this->iot_hub_device_id_);
    json_payload.append("\", \"");
    json_payload.append(id);
    json_payload.append("\": { \"type\": \"cover\"");

    std::string name = cover->get_name();
    if (!name.empty()) {
        json_payload.append(", \"name\": \"");
        json_payload.append(name);
        json_payload.append("\"");
    }


    if (traits.get_supports_tilt()) {
        json_payload.append(", \"tilt\": ");
        json_payload.append(String(cover->tilt).c_str());
    }
    if (traits.get_supports_position()) {
        json_payload.append(", \"position\": ");
        json_payload.append(String(cover->position).c_str());
    }
    json_payload.append(", \"state\": \"");
    json_payload.append((cover->position == cover::COVER_OPEN) ? "open" : "closed");
    json_payload.append("\"");

    json_payload.append(", \"currentOperation\": \"");
    json_payload.append((cover->current_operation == cover::CoverOperation::COVER_OPERATION_OPENING) 
        ? "opening" 
        : cover->current_operation == cover::CoverOperation::COVER_OPERATION_CLOSING
            ? "closing"
            : "idle");
    json_payload.append("\"");

    json_payload.append("} }");

    this->post_json_to_iot_hub(json_payload);
}
#endif

#ifdef USE_FAN
void AzureIoTHub::on_fan_update(fan::FanState *fan) {
    if (fan->is_internal())
        return;
  
    std::string id = fan->get_object_id();
    std::string json_payload{"{ \"deviceId\":\""};
    json_payload.append(this->iot_hub_device_id_);
    json_payload.append("\", \"");
    json_payload.append(id);
    json_payload.append("\": { \"type\": \"fan\"");
    std::string name = fan->get_name();
    if (!name.empty()) {
        json_payload.append(", \"name\": \"");
        json_payload.append(name);
        json_payload.append("\"");
    }

    auto traits = fan->get_traits();

    json_payload.append(", \"state\": ");
    json_payload.append(fan->state ? "true" : "false");

    if (traits.supports_oscillation()) {
        json_payload.append(", \"oscillating\": ");
        json_payload.append(fan->oscillating ? "true" : "false");
    }

    if (traits.supports_speed()) {
        json_payload.append(", \"speed\": \"");
        json_payload.append(fan->speed == esphome::fan::FanSpeed::FAN_SPEED_LOW 
            ? "low"
            : fan->speed == esphome::fan::FanSpeed::FAN_SPEED_MEDIUM 
                ? "medium"
                : "high" );
        json_payload.append("\"");
    }

    json_payload.append("} }");

    this->post_json_to_iot_hub(json_payload);
}
#endif

#ifdef USE_LIGHT
void AzureIoTHub::on_light_update(light::LightState *light) {
    if (light->is_internal())
        return;

    auto traits = light->get_traits();
    auto values = light->remote_values;

    std::string id = light->get_object_id();
    std::string json_payload{"{ \"deviceId\":\""};
    json_payload.append(this->iot_hub_device_id_);
    json_payload.append("\", \"");
    json_payload.append(id);
    json_payload.append("\": { \"type\": \"light\"");
    std::string name = light->get_name();
    if (!name.empty()) {
        json_payload.append(", \"name\": \"");
        json_payload.append(name);
        json_payload.append("\"");
    }

    json_payload.append(", \"state\": ");
    json_payload.append(values.is_on() ? "true" : "false");

    if (traits.get_supports_brightness()) {
        json_payload.append(", \"brightness\": ");
        json_payload.append(String(values.get_brightness()).c_str());
    }

    if (traits.get_supports_rgb()) {
        json_payload.append(", \"red\": ");
        json_payload.append(String(values.get_red()).c_str());

        json_payload.append(", \"green\": ");
        json_payload.append(String(values.get_green()).c_str());

        json_payload.append(", \"blue\": ");
        json_payload.append(String(values.get_blue()).c_str());
    }

    if (traits.get_supports_rgb_white_value()) {
        json_payload.append(", \"white\": ");
        json_payload.append(String(values.get_white()).c_str());
    }

    if (traits.get_supports_color_temperature()) {
        json_payload.append(", \"colorTemperature\": ");
        json_payload.append(String(values.get_color_temperature()).c_str());
    }

    if (light->supports_effects()) {
        json_payload.append(", \"effect\": \"");
        json_payload.append(light->get_effect_name());
        json_payload.append("\"");
    }

        
    json_payload.append("} }");
    
    this->post_json_to_iot_hub(json_payload);
}
#endif

#ifdef USE_SENSOR
void AzureIoTHub::on_sensor_update(sensor::Sensor *sensor, float state) {
    if (sensor->is_internal())
        return;
    
    std::string id = sensor->get_object_id();
    std::string json_payload{"{ \"deviceId\":\""};
    json_payload.append(this->iot_hub_device_id_);
    json_payload.append("\", \"");
    json_payload.append(id);
    json_payload.append("\": { \"type\": \"sensor\"");
    std::string name = sensor->get_name();
    if (!name.empty()) {
        json_payload.append(", \"name\": \"");
        json_payload.append(name);
        json_payload.append("\"");
    }

    json_payload.append(", \"state\": ");
    json_payload.append(String(state).c_str());
    
    json_payload.append(", \"missingState\": ");
    json_payload.append(sensor->has_state() ? "false": "true");

    json_payload.append("} }");

    this->post_json_to_iot_hub(json_payload);
}
#endif

#ifdef USE_SWITCH
void AzureIoTHub::on_switch_update(switch_::Switch *sw, bool state) {
    if (sw->is_internal()) {
        return;
    }

    std::string id = sw->get_object_id();
    std::string json_payload{"{ \"deviceId\":\""};
    json_payload.append(this->iot_hub_device_id_);
    json_payload.append("\", \"");
    json_payload.append(id);
    json_payload.append("\": { \"type\": \"switch\"");
    std::string name = sw->get_name();
    if (!name.empty()) {
        json_payload.append(", \"name\": \"");
        json_payload.append(name);
        json_payload.append("\"");
    }

    json_payload.append(", \"state\": ");
    json_payload.append(state ? "true": "false");

    json_payload.append("} }");

    this->post_json_to_iot_hub(json_payload);
}
#endif

#ifdef USE_TEXT_SENSOR
void AzureIoTHub::on_text_sensor_update(text_sensor::TextSensor *text, std::string state) {
    if (text->is_internal())
        return;

    std::string id = text->get_object_id();
    std::string json_payload{"{ \"deviceId\":\""};
    json_payload.append(this->iot_hub_device_id_);
    json_payload.append("\", \"");
    json_payload.append(id);
    json_payload.append("\": { \"type\": \"text_sensor\"");
    std::string name = text->get_name();
    if (!name.empty()) {
        json_payload.append(", \"name\": \"");
        json_payload.append(name);
        json_payload.append("\"");
    }

    json_payload.append(", \"state\": \"");
    json_payload.append(state);
    json_payload.append("\"");

    json_payload.append(", \"missingState\": ");
    json_payload.append(text->has_state() ? "false": "true");


    json_payload.append("} }");

    this->post_json_to_iot_hub(json_payload);
}
#endif

#ifdef USE_CLIMATE
void AzureIoTHub::on_climate_update(climate::Climate *climate) {
    if (climate->is_internal())
        return;
    
    auto traits = climate->get_traits();
    
    std::string id = climate->get_object_id();
    std::string json_payload{"{ \"deviceId\":\""};
    json_payload.append(this->iot_hub_device_id_);
    json_payload.append("\", \"");
    json_payload.append(id);
    json_payload.append("\": { \"type\": \"climate\"");
    std::string name = climate->get_name();
    if (!name.empty()) {
        json_payload.append(", \"name\": \"");
        json_payload.append(name);
        json_payload.append("\"");
    }

    json_payload.append(", \"mode\": \"");
    json_payload.append(esphome::climate::climate_mode_to_string(climate->mode));
    json_payload.append("\"");
    
    json_payload.append(", \"action\": \"");
    json_payload.append(esphome::climate::climate_action_to_string(climate->action));
    json_payload.append("\"");
    
    if (traits.get_supports_current_temperature()) {
        json_payload.append(", \"currentTemperature\": \"");
        json_payload.append(String(climate->current_temperature).c_str());
        json_payload.append("\"");
    }

    if (traits.get_supports_two_point_target_temperature()) {
        json_payload.append(", \"targetTemperatureLow\": \"");
        json_payload.append(String(climate->target_temperature_low).c_str());
        json_payload.append("\"");

        json_payload.append(", \"targetTemperatureHigh\": \"");
        json_payload.append(String(climate->target_temperature_high).c_str());
        json_payload.append("\"");
    } else {
        json_payload.append(", \"targetTemperature\": \"");
        json_payload.append(String(climate->target_temperature).c_str());
        json_payload.append("\"");
    }

    if (traits.get_supports_away()) {
        json_payload.append(", \"away\": ");
        json_payload.append(climate->away ? "true" : "false");
    }

    if (traits.get_supports_fan_modes()) {
        json_payload.append(", \"fanMode\": \"");
        json_payload.append(esphome::climate::climate_fan_mode_to_string(climate->fan_mode));
        json_payload.append("\"");
    }

    if (traits.get_supports_swing_modes()) {
        json_payload.append(", \"swingMode\": \"");
        json_payload.append(esphome::climate::climate_swing_mode_to_string(climate->swing_mode));
        json_payload.append("\"");
    }

    
    json_payload.append("} }");

    this->post_json_to_iot_hub(json_payload);
}
#endif

}
}
