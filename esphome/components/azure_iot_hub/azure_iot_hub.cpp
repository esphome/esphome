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
}

#ifdef ARDUINO_ARCH_ESP8266
static uint8_t htoi(unsigned char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'A' && c <= 'F')
    return 10 + c - 'A';
  else if (c >= 'a' && c <= 'f')
    return 10 + c - 'a';
  else
    return 255;
}

// Set a fingerprint by parsing an ASCII string,
// based on codebase of ESP8266 arduino core.
// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiClientSecureBearSSL.cpp
// Preserving this helps reduce setup and handle CLANG ESP8266 dependency not having relevant overload
bool AzureIoTHub::set_fingerprint_bytes(const char *fingerprint_string) {
  int idx = 0;
  uint8_t c, d;

  while (idx < 20) {
    c = *fingerprint_string++;
    if (!c)
      break;  // String ended, done processing
    d = *fingerprint_string++;
    if (!d)
      return false;  // Only half of the last hex digit, error
    c = htoi(c);
    d = htoi(d);
    if ((c > 15) || (d > 15)) {
      return false;  // Error in one of the hex characters
    }
    this->ssl_sha1_fingerprint_bytes_[idx++] = (c << 4) | d;

    // Skip 0 or more spaces or colons
    while (*fingerprint_string && (*fingerprint_string == ' ' || *fingerprint_string == ':')) {
      fingerprint_string++;
    }
  }
  if ((idx != 20)) {
    return false;  // Garbage at EOL or we didn't have enough hex digits
  } else {
    return true;
  }
}
#endif

void AzureIoTHub::setup() {
  this->setup_controller();
#ifdef ARDUINO_ARCH_ESP8266
  this->wifi_client_ = new BearSSL::WiFiClientSecure();

  if (this->ssl_fingerprint_supplied_) {
    ESP_LOGD(TAG, "Using SSL with SHA1 fingerprint verification");
    this->wifi_client_->setFingerprint(this->ssl_sha1_fingerprint_bytes_);
  } else {
    ESP_LOGD(TAG, "Using insecure SSL");
    this->wifi_client_->setInsecure();
  }
  this->wifi_client_->setBufferSizes(512, 512);
#endif

#ifdef ARDUINO_ARCH_ESP32
  this->wifi_client_ = new WiFiClientSecure();
  if (baltimore_root_pem_ != nullptr) {
    ESP_LOGD(TAG, "Using SSL with Baltimore root certificate for verification");
    this->wifi_client_->setCACert(baltimore_root_pem_);
  } else {
    ESP_LOGD(TAG, "Using insecure SSL");
  }
#endif
  this->http_client_.setReuse(true);
}

std::string AzureIoTHub::get_iot_hub_device_id() const { return this->iot_hub_device_id_; }

void AzureIoTHub::set_iot_hub_device_id(const std::string &device_id) { this->iot_hub_device_id_ = device_id; }
void AzureIoTHub::set_iot_hub_sas_token(const std::string &sas_token) { this->iot_hub_sas_token_ = sas_token; }
void AzureIoTHub::set_iot_hub_rest_url(const std::string &rest_url) { this->iot_hub_rest_url_ = rest_url; }
void AzureIoTHub::set_iot_hub_sas_token_expiration_string(const std::string &expirationString) {
  this->iot_hub_sas_token_expiration_string_ = expirationString;
}

#ifdef ARDUINO_ARCH_ESP8266
void AzureIoTHub::set_iot_hub_ssl_sha1_fingerprint(const std::string &fingerprint) {
  this->ssl_fingerprint_supplied_ = this->set_fingerprint_bytes(fingerprint.c_str());
}
#endif

bool AzureIoTHub::post_json_to_iot_hub(const std::string json_payload) {
  ESP_LOGD(TAG, "Posting to Azure IoT Hub");
  String url{this->iot_hub_rest_url_.c_str()};

#ifndef CLANG_TIDY
  this->http_client_.begin(*this->wifi_client_, url);
#endif

  this->http_client_.addHeader(F("Authorization"), this->iot_hub_sas_token_.c_str());
  this->http_client_.addHeader(F("Content-Type"), F("application/json"));
  this->http_client_.addHeader(F("Content-Length"), String(json_payload.length()));

  int httpCode = this->http_client_.POST(json_payload.c_str());
  this->http_client_.end();
  ESP_LOGD(TAG, "HTTP Request to Azure IoT Hub completed with code: %d", httpCode);
  bool success = httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT;
  if (!success) {
    this->status_set_error();
  } else if (this->status_has_error()) {
    this->status_clear_error();
  }

  return success;
}

#ifdef USE_BINARY_SENSOR
void AzureIoTHub::on_binary_sensor_update(binary_sensor::BinarySensor *sensor, bool state) {
  if (sensor->is_internal())
    return;

  std::string payload = json::build_json([this, sensor, state](JsonObject &root) {
    root[F("deviceId")] = this->get_iot_hub_device_id();
    JsonObject &sensorNode = root.createNestedObject(sensor->get_object_id());
    sensorNode[F("type")] = F("binary_sensor");
    std::string name = sensor->get_name();
    if (!name.empty()) {
      sensorNode[F("name")] = name;
    }
    sensorNode[F("state")] = state;
    sensorNode[F("missingState")] = !sensor->has_state();
  });

  this->post_json_to_iot_hub(payload);
}
#endif

#ifdef USE_COVER
void AzureIoTHub::on_cover_update(cover::Cover *cover) {
  if (cover->is_internal())
    return;

  std::string payload = json::build_json([this, cover](JsonObject &root) {
    auto traits = cover->get_traits();
    root[F("deviceId")] = this->get_iot_hub_device_id();
    JsonObject &coverNode = root.createNestedObject(cover->get_object_id());
    coverNode[F("type")] = F("cover");
    std::string name = cover->get_name();
    if (!name.empty()) {
      coverNode[F("name")] = name;
    }
    if (traits.get_supports_tilt())
      coverNode[F("tilt")] = cover->tilt;
    if (traits.get_supports_position())
      coverNode[F("position")] = cover->position;

    coverNode[F("state")] = (cover->position == cover::COVER_OPEN) ? "open" : "closed";
    coverNode[F("currentOperation")] =
        (cover->current_operation == cover::CoverOperation::COVER_OPERATION_OPENING)
            ? "opening"
            : cover->current_operation == cover::CoverOperation::COVER_OPERATION_CLOSING ? "closing" : "idle";
  });

  this->post_json_to_iot_hub(payload);
}
#endif

#ifdef USE_FAN
void AzureIoTHub::on_fan_update(fan::FanState *fan) {
  if (fan->is_internal())
    return;

  std::string payload = json::build_json([this, fan](JsonObject &root) {
    auto traits = fan->get_traits();
    root[F("deviceId")] = this->get_iot_hub_device_id();
    JsonObject &fanNode = root.createNestedObject(fan->get_object_id());
    fanNode[F("type")] = F("fan");
    std::string name = fan->get_name();
    if (!name.empty()) {
      fanNode[F("name")] = name;
    }
    fanNode[F("state")] = fan->state;

    if (traits.supports_oscillation())
      fanNode[F("oscillating")] = fan->oscillating;

    if (traits.supports_speed())
      fanNode[F("speed")] = fan->speed == esphome::fan::FanSpeed::FAN_SPEED_LOW
                                ? "low"
                                : fan->speed == esphome::fan::FanSpeed::FAN_SPEED_MEDIUM ? "medium" : "high";
  });

  this->post_json_to_iot_hub(payload);
}
#endif

#ifdef USE_LIGHT
void AzureIoTHub::on_light_update(light::LightState *light) {
  if (light->is_internal())
    return;

  std::string payload = json::build_json([this, light](JsonObject &root) {
    auto traits = light->get_traits();
    auto values = light->remote_values;
    root[F("deviceId")] = this->get_iot_hub_device_id();
    JsonObject &lightNode = root.createNestedObject(light->get_object_id());
    lightNode[F("type")] = F("light");
    std::string name = light->get_name();
    if (!name.empty()) {
      lightNode[F("name")] = name;
    }
    lightNode[F("state")] = values.is_on();

    if (traits.get_supports_brightness())
      lightNode[F("brightness")] = values.get_brightness();

    if (traits.get_supports_rgb()) {
      lightNode[F("red")] = values.get_red();
      lightNode[F("green")] = values.get_green();
      lightNode[F("blue")] = values.get_blue();
    }

    if (traits.get_supports_rgb_white_value())
      lightNode[F("white")] = values.get_white();

    if (traits.get_supports_color_temperature())
      lightNode[F("colorTemperature")] = values.get_color_temperature();

    if (light->supports_effects())
      lightNode[F("effect")] = light->get_effect_name();
  });

  this->post_json_to_iot_hub(payload);
}
#endif

#ifdef USE_SENSOR
void AzureIoTHub::on_sensor_update(sensor::Sensor *sensor, float state) {
  if (sensor->is_internal())
    return;

  std::string payload = json::build_json([this, sensor, state](JsonObject &root) {
    root[F("deviceId")] = this->get_iot_hub_device_id();
    JsonObject &sensorNode = root.createNestedObject(sensor->get_object_id());
    sensorNode[F("type")] = F("sensor");
    std::string name = sensor->get_name();
    if (!name.empty()) {
      sensorNode[F("name")] = name;
    }
    sensorNode[F("state")] = state;
    sensorNode[F("missingState")] = !sensor->has_state();
  });

  this->post_json_to_iot_hub(payload);
}
#endif

#ifdef USE_SWITCH
void AzureIoTHub::on_switch_update(switch_::Switch *sw, bool state) {
  if (sw->is_internal()) {
    return;
  }

  std::string payload = json::build_json([this, sw, state](JsonObject &root) {
    root[F("deviceId")] = this->get_iot_hub_device_id();
    JsonObject &switchNode = root.createNestedObject(sw->get_object_id());
    switchNode[F("type")] = F("switch");
    std::string name = sw->get_name();
    if (!name.empty()) {
      switchNode[F("name")] = name;
    }
    switchNode[F("state")] = state;
  });

  this->post_json_to_iot_hub(payload);
}
#endif

#ifdef USE_TEXT_SENSOR
void AzureIoTHub::on_text_sensor_update(text_sensor::TextSensor *text, std::string state) {
  if (text->is_internal())
    return;

  std::string payload = json::build_json([this, text, state](JsonObject &root) {
    root[F("deviceId")] = this->get_iot_hub_device_id();
    JsonObject &textNode = root.createNestedObject(text->get_object_id());
    textNode[F("type")] = F("text_sensor");
    std::string name = text->get_name();
    if (!name.empty()) {
      textNode[F("name")] = name;
    }
    textNode[F("state")] = state;
    textNode[F("missingState")] = !text->has_state();
  });

  this->post_json_to_iot_hub(payload);
}
#endif

#ifdef USE_CLIMATE
void AzureIoTHub::on_climate_update(climate::Climate *climate) {
  if (climate->is_internal())
    return;

  std::string payload = json::build_json([this, climate](JsonObject &root) {
    auto traits = climate->get_traits();
    root[F("deviceId")] = this->get_iot_hub_device_id();
    JsonObject &climateNode = root.createNestedObject(climate->get_object_id());
    climateNode[F("type")] = F("climate");
    std::string name = climate->get_name();
    if (!name.empty()) {
      climateNode[F("name")] = name;
    }

    climateNode[F("mode")] = esphome::climate::climate_mode_to_string(climate->mode);
    climateNode[F("action")] = esphome::climate::climate_action_to_string(climate->action);

    if (traits.get_supports_current_temperature())
      climateNode[F("currentTemperature")] = climate->current_temperature;

    if (traits.get_supports_two_point_target_temperature()) {
      climateNode[F("targetTemperatureLow")] = climate->target_temperature_low;
      climateNode[F("targetTemperatureHigh")] = climate->target_temperature_high;
    } else {
      climateNode[F("targetTemperature")] = climate->target_temperature;
    }

    if (traits.get_supports_away())
      climateNode[F("away")] = climate->away;

    if (traits.get_supports_fan_modes())
      climateNode[F("fanMode")] = esphome::climate::climate_fan_mode_to_string(climate->fan_mode);

    if (traits.get_supports_swing_modes())
      climateNode[F("swingMode")] = esphome::climate::climate_swing_mode_to_string(climate->swing_mode);
  });

  this->post_json_to_iot_hub(payload);
}
#endif

}  // namespace azure_iot_hub
}  // namespace esphome
