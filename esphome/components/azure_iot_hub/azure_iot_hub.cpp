#include "azure_iot_hub.h"
#include <math.h>

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

void AzureIoTHub::setup() {
  this->setup_controller();
  this->http_request_.setup();
}

std::string AzureIoTHub::get_iot_hub_device_id() const { return this->iot_hub_device_id_; }
const char *AzureIoTHub::get_iot_hub_rest_url() const { return this->iot_hub_rest_url_.c_str(); }
std::string AzureIoTHub::get_iot_hub_sas_token() const { return this->iot_hub_sas_token_; }

void AzureIoTHub::set_iot_hub_device_id(const std::string &device_id) { this->iot_hub_device_id_ = device_id; }
void AzureIoTHub::set_iot_hub_sas_token(const std::string &sas_token) { this->iot_hub_sas_token_ = sas_token; }
void AzureIoTHub::set_iot_hub_rest_url(const std::string &rest_url) { this->iot_hub_rest_url_ = rest_url; }
void AzureIoTHub::set_iot_hub_sas_token_expiration_string(const std::string &expiration_string) {
  this->iot_hub_sas_token_expiration_string_ = expiration_string;
}

bool AzureIoTHub::post_json_to_iot_hub_(const std::string json_payload) {
  ESP_LOGD(TAG, "Posting to Azure IoT Hub");

  char length_buffer[8];  // 8 characters are more than sufficient, since they allow 9999999 chars
  itoa(json_payload.length(), length_buffer, 10);

  std::list<esphome::http_request::Header> headers{{.name = "Authorization", .value = this->iot_hub_sas_token_.c_str()},
                                                   {.name = "Content-Type", .value = "application/json"},
                                                   {.name = "Content-Length", .value = length_buffer}};

  this->http_request_.set_method("post");
  this->http_request_.set_url(this->get_iot_hub_rest_url());
  this->http_request_.set_headers(headers);
  this->http_request_.set_body(json_payload);

  this->http_request_.send();

  ESP_LOGD(TAG, "HTTP Request to Azure IoT Hub completed %s",
           this->http_request_.status_has_warning() ? "unsuccessfully" : "successfully");

  return !this->http_request_.status_has_warning();
}

#ifdef USE_BINARY_SENSOR
void AzureIoTHub::on_binary_sensor_update(binary_sensor::BinarySensor *sensor, bool state) {
  if (sensor->is_internal())
    return;

  std::string payload = json::build_json([this, sensor, state](JsonObject &root) {
    root["deviceId"] = this->get_iot_hub_device_id();
    JsonObject &sensor_node = root.createNestedObject(sensor->get_object_id());
    sensor_node["type"] = "binary_sensor";
    std::string name = sensor->get_name();
    if (!name.empty()) {
      sensor_node["name"] = name;
    }
    sensor_node["state"] = state;
    sensor_node["missingState"] = !sensor->has_state();
  });

  this->post_json_to_iot_hub_(payload);
}
#endif

#ifdef USE_COVER
void AzureIoTHub::on_cover_update(cover::Cover *cover) {
  if (cover->is_internal())
    return;

  std::string payload = json::build_json([this, cover](JsonObject &root) {
    auto traits = cover->get_traits();
    root["deviceId"] = this->get_iot_hub_device_id();
    JsonObject &cover_node = root.createNestedObject(cover->get_object_id());
    cover_node["type"] = "cover";
    std::string name = cover->get_name();
    if (!name.empty()) {
      cover_node["name"] = name;
    }
    if (traits.get_supports_tilt())
      cover_node["tilt"] = cover->tilt;
    if (traits.get_supports_position())
      cover_node["position"] = cover->position;

    cover_node["state"] = (cover->position == cover::COVER_OPEN) ? "open" : "closed";
    cover_node["currentOperation"] =
        (cover->current_operation == cover::CoverOperation::COVER_OPERATION_OPENING)
            ? "opening"
            : cover->current_operation == cover::CoverOperation::COVER_OPERATION_CLOSING ? "closing" : "idle";
  });

  this->post_json_to_iot_hub_(payload);
}
#endif

#ifdef USE_FAN
void AzureIoTHub::on_fan_update(fan::FanState *fan) {
  if (fan->is_internal())
    return;

  std::string payload = json::build_json([this, fan](JsonObject &root) {
    auto traits = fan->get_traits();
    root["deviceId"] = this->get_iot_hub_device_id();
    JsonObject &fan_node = root.createNestedObject(fan->get_object_id());
    fan_node["type"] = "fan";
    std::string name = fan->get_name();
    if (!name.empty()) {
      fan_node["name"] = name;
    }
    fan_node["state"] = fan->state;

    if (traits.supports_oscillation())
      fan_node["oscillating"] = fan->oscillating;

    if (traits.supports_speed())
      fan_node["speed"] = fan->speed == esphome::fan::FanSpeed::FAN_SPEED_LOW
                              ? "low"
                              : fan->speed == esphome::fan::FanSpeed::FAN_SPEED_MEDIUM ? "medium" : "high";
  });

  this->post_json_to_iot_hub_(payload);
}
#endif

#ifdef USE_LIGHT
void AzureIoTHub::on_light_update(light::LightState *light) {
  if (light->is_internal())
    return;

  std::string payload = json::build_json([this, light](JsonObject &root) {
    auto traits = light->get_traits();
    auto values = light->remote_values;
    root["deviceId"] = this->get_iot_hub_device_id();
    JsonObject &light_node = root.createNestedObject(light->get_object_id());
    light_node["type"] = "light";
    std::string name = light->get_name();
    if (!name.empty()) {
      light_node["name"] = name;
    }
    light_node["state"] = values.is_on();

    if (traits.get_supports_brightness())
      light_node["brightness"] = values.get_brightness();

    if (traits.get_supports_rgb()) {
      light_node["red"] = values.get_red();
      light_node["green"] = values.get_green();
      light_node["blue"] = values.get_blue();
    }

    if (traits.get_supports_rgb_white_value())
      light_node["white"] = values.get_white();

    if (traits.get_supports_color_temperature())
      light_node["colorTemperature"] = values.get_color_temperature();

    if (light->supports_effects())
      light_node["effect"] = light->get_effect_name();
  });

  this->post_json_to_iot_hub_(payload);
}
#endif

#ifdef USE_SENSOR
void AzureIoTHub::on_sensor_update(sensor::Sensor *sensor, float state) {
  if (sensor->is_internal())
    return;

  std::string payload = json::build_json([this, sensor, state](JsonObject &root) {
    root["deviceId"] = this->get_iot_hub_device_id();
    JsonObject &sensor_node = root.createNestedObject(sensor->get_object_id());
    sensor_node["type"] = "sensor";
    std::string name = sensor->get_name();
    if (!name.empty()) {
      sensor_node["name"] = name;
    }

    // If sensor reports specific accuracy, truncate state to that many decimals
    int accuracy = sensor->get_accuracy_decimals();
    if (accuracy > 0) {
      sensor_node["state"] = roundf(state * 10 * accuracy) / 10 * accuracy;
    } else if (accuracy == 0) {
      sensor_node["state"] = roundf(state);
    } else {
      sensor_node["state"] = state;
    }
    sensor_node["missingState"] = !sensor->has_state();
  });

  this->post_json_to_iot_hub_(payload);
}
#endif

#ifdef USE_SWITCH
void AzureIoTHub::on_switch_update(switch_::Switch *sw, bool state) {
  if (sw->is_internal()) {
    return;
  }

  std::string payload = json::build_json([this, sw, state](JsonObject &root) {
    root["deviceId"] = this->get_iot_hub_device_id();
    JsonObject &switch_node = root.createNestedObject(sw->get_object_id());
    switch_node["type"] = "switch";
    std::string name = sw->get_name();
    if (!name.empty()) {
      switch_node["name"] = name;
    }
    switch_node["state"] = state;
  });

  this->post_json_to_iot_hub_(payload);
}
#endif

#ifdef USE_TEXT_SENSOR
void AzureIoTHub::on_text_sensor_update(text_sensor::TextSensor *text, std::string state) {
  if (text->is_internal())
    return;

  std::string payload = json::build_json([this, text, state](JsonObject &root) {
    root["deviceId"] = this->get_iot_hub_device_id();
    JsonObject &text_node = root.createNestedObject(text->get_object_id());
    text_node["type"] = "text_sensor";
    std::string name = text->get_name();
    if (!name.empty()) {
      text_node["name"] = name;
    }
    text_node["state"] = state;
    text_node["missingState"] = !text->has_state();
  });

  this->post_json_to_iot_hub_(payload);
}
#endif

#ifdef USE_CLIMATE
void AzureIoTHub::on_climate_update(climate::Climate *climate) {
  if (climate->is_internal())
    return;

  std::string payload = json::build_json([this, climate](JsonObject &root) {
    auto traits = climate->get_traits();
    root["deviceId"] = this->get_iot_hub_device_id();
    JsonObject &climate_node = root.createNestedObject(climate->get_object_id());
    climate_node["type"] = "climate";
    std::string name = climate->get_name();
    if (!name.empty()) {
      climate_node["name"] = name;
    }

    climate_node["mode"] = esphome::climate::climate_mode_to_string(climate->mode);
    climate_node["action"] = esphome::climate::climate_action_to_string(climate->action);

    if (traits.get_supports_current_temperature())
      climate_node["currentTemperature"] = climate->current_temperature;

    if (traits.get_supports_two_point_target_temperature()) {
      climate_node["targetTemperatureLow"] = climate->target_temperature_low;
      climate_node["targetTemperatureHigh"] = climate->target_temperature_high;
    } else {
      climate_node["targetTemperature"] = climate->target_temperature;
    }

    if (traits.get_supports_away())
      climate_node["away"] = climate->away;

    if (traits.get_supports_fan_modes())
      climate_node["fanMode"] = esphome::climate::climate_fan_mode_to_string(climate->fan_mode);

    if (traits.get_supports_swing_modes())
      climate_node["swingMode"] = esphome::climate::climate_swing_mode_to_string(climate->swing_mode);
  });

  this->post_json_to_iot_hub_(payload);
}
#endif

}  // namespace azure_iot_hub
}  // namespace esphome
