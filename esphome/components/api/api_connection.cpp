#include "api_connection.h"
#include <cerrno>
#include <cinttypes>
#include <utility>
#include "esphome/components/network/util.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#ifdef USE_DEEP_SLEEP
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif
#ifdef USE_HOMEASSISTANT_TIME
#include "esphome/components/homeassistant/time/homeassistant_time.h"
#endif
#ifdef USE_BLUETOOTH_PROXY
#include "esphome/components/bluetooth_proxy/bluetooth_proxy.h"
#endif
#ifdef USE_VOICE_ASSISTANT
#include "esphome/components/voice_assistant/voice_assistant.h"
#endif

namespace esphome {
namespace api {

static const char *const TAG = "api.connection";
static const int ESP32_CAMERA_STOP_STREAM = 5000;

APIConnection::APIConnection(std::unique_ptr<socket::Socket> sock, APIServer *parent)
    : parent_(parent), initial_state_iterator_(this), list_entities_iterator_(this) {
  this->proto_write_buffer_.reserve(64);

#if defined(USE_API_PLAINTEXT)
  this->helper_ = std::unique_ptr<APIFrameHelper>{new APIPlaintextFrameHelper(std::move(sock))};
#elif defined(USE_API_NOISE)
  this->helper_ = std::unique_ptr<APIFrameHelper>{new APINoiseFrameHelper(std::move(sock), parent->get_noise_ctx())};
#else
#error "No frame helper defined"
#endif
}
void APIConnection::start() {
  this->last_traffic_ = millis();

  APIError err = this->helper_->init();
  if (err != APIError::OK) {
    on_fatal_error();
    ESP_LOGW(TAG, "%s: Helper init failed: %s errno=%d", this->client_combined_info_.c_str(), api_error_to_str(err),
             errno);
    return;
  }
  this->client_info_ = helper_->getpeername();
  this->client_peername_ = this->client_info_;
  this->helper_->set_log_info(this->client_info_);
}

APIConnection::~APIConnection() {
#ifdef USE_BLUETOOTH_PROXY
  if (bluetooth_proxy::global_bluetooth_proxy->get_api_connection() == this) {
    bluetooth_proxy::global_bluetooth_proxy->unsubscribe_api_connection(this);
  }
#endif
#ifdef USE_VOICE_ASSISTANT
  if (voice_assistant::global_voice_assistant->get_api_connection() == this) {
    voice_assistant::global_voice_assistant->client_subscription(this, false);
  }
#endif
}

void APIConnection::loop() {
  if (this->remove_)
    return;

  if (!network::is_connected()) {
    // when network is disconnected force disconnect immediately
    // don't wait for timeout
    this->on_fatal_error();
    ESP_LOGW(TAG, "%s: Network unavailable, disconnecting", this->client_combined_info_.c_str());
    return;
  }
  if (this->next_close_) {
    // requested a disconnect
    this->helper_->close();
    this->remove_ = true;
    return;
  }

  APIError err = this->helper_->loop();
  if (err != APIError::OK) {
    on_fatal_error();
    ESP_LOGW(TAG, "%s: Socket operation failed: %s errno=%d", this->client_combined_info_.c_str(),
             api_error_to_str(err), errno);
    return;
  }
  ReadPacketBuffer buffer;
  err = this->helper_->read_packet(&buffer);
  if (err == APIError::WOULD_BLOCK) {
    // pass
  } else if (err != APIError::OK) {
    on_fatal_error();
    if (err == APIError::SOCKET_READ_FAILED && errno == ECONNRESET) {
      ESP_LOGW(TAG, "%s: Connection reset", this->client_combined_info_.c_str());
    } else if (err == APIError::CONNECTION_CLOSED) {
      ESP_LOGW(TAG, "%s: Connection closed", this->client_combined_info_.c_str());
    } else {
      ESP_LOGW(TAG, "%s: Reading failed: %s errno=%d", this->client_combined_info_.c_str(), api_error_to_str(err),
               errno);
    }
    return;
  } else {
    this->last_traffic_ = millis();
    // read a packet
    this->read_message(buffer.data_len, buffer.type, &buffer.container[buffer.data_offset]);
    if (this->remove_)
      return;
  }

  this->list_entities_iterator_.advance();
  this->initial_state_iterator_.advance();

  static uint32_t keepalive = 60000;
  static uint8_t max_ping_retries = 60;
  static uint16_t ping_retry_interval = 1000;
  const uint32_t now = millis();
  if (this->sent_ping_) {
    // Disconnect if not responded within 2.5*keepalive
    if (now - this->last_traffic_ > (keepalive * 5) / 2) {
      on_fatal_error();
      ESP_LOGW(TAG, "%s didn't respond to ping request in time. Disconnecting...", this->client_combined_info_.c_str());
    }
  } else if (now - this->last_traffic_ > keepalive && now > this->next_ping_retry_) {
    ESP_LOGVV(TAG, "Sending keepalive PING...");
    this->sent_ping_ = this->send_ping_request(PingRequest());
    if (!this->sent_ping_) {
      this->next_ping_retry_ = now + ping_retry_interval;
      this->ping_retries_++;
      if (this->ping_retries_ >= max_ping_retries) {
        on_fatal_error();
        ESP_LOGE(TAG, "%s: Sending keepalive failed %d time(s). Disconnecting...", this->client_combined_info_.c_str(),
                 this->ping_retries_);
      } else if (this->ping_retries_ >= 10) {
        ESP_LOGW(TAG, "%s: Sending keepalive failed %d time(s), will retry in %d ms",
                 this->client_combined_info_.c_str(), this->ping_retries_, ping_retry_interval);
      } else {
        ESP_LOGD(TAG, "%s: Sending keepalive failed %d time(s), will retry in %d ms",
                 this->client_combined_info_.c_str(), this->ping_retries_, ping_retry_interval);
      }
    }
  }

#ifdef USE_ESP32_CAMERA
  if (this->image_reader_.available() && this->helper_->can_write_without_blocking()) {
    uint32_t to_send = std::min((size_t) 1024, this->image_reader_.available());
    auto buffer = this->create_buffer();
    // fixed32 key = 1;
    buffer.encode_fixed32(1, esp32_camera::global_esp32_camera->get_object_id_hash());
    // bytes data = 2;
    buffer.encode_bytes(2, this->image_reader_.peek_data_buffer(), to_send);
    // bool done = 3;
    bool done = this->image_reader_.available() == to_send;
    buffer.encode_bool(3, done);
    bool success = this->send_buffer(buffer, 44);

    if (success) {
      this->image_reader_.consume_data(to_send);
    }
    if (success && done) {
      this->image_reader_.return_image();
    }
  }
#endif

  if (state_subs_at_ != -1) {
    const auto &subs = this->parent_->get_state_subs();
    if (state_subs_at_ >= (int) subs.size()) {
      state_subs_at_ = -1;
    } else {
      auto &it = subs[state_subs_at_];
      SubscribeHomeAssistantStateResponse resp;
      resp.entity_id = it.entity_id;
      resp.attribute = it.attribute.value();
      if (this->send_subscribe_home_assistant_state_response(resp)) {
        state_subs_at_++;
      }
    }
  }
}

std::string get_default_unique_id(const std::string &component_type, EntityBase *entity) {
  return App.get_name() + component_type + entity->get_object_id();
}

DisconnectResponse APIConnection::disconnect(const DisconnectRequest &msg) {
  // remote initiated disconnect_client
  // don't close yet, we still need to send the disconnect response
  // close will happen on next loop
  ESP_LOGD(TAG, "%s requested disconnected", this->client_combined_info_.c_str());
  this->next_close_ = true;
  DisconnectResponse resp;
  return resp;
}
void APIConnection::on_disconnect_response(const DisconnectResponse &value) {
  // pass
}

#ifdef USE_BINARY_SENSOR
bool APIConnection::send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor, bool state) {
  if (!this->state_subscription_)
    return false;

  BinarySensorStateResponse resp;
  resp.key = binary_sensor->get_object_id_hash();
  resp.state = state;
  resp.missing_state = !binary_sensor->has_state();
  return this->send_binary_sensor_state_response(resp);
}
bool APIConnection::send_binary_sensor_info(binary_sensor::BinarySensor *binary_sensor) {
  ListEntitiesBinarySensorResponse msg;
  msg.object_id = binary_sensor->get_object_id();
  msg.key = binary_sensor->get_object_id_hash();
  if (binary_sensor->has_own_name())
    msg.name = binary_sensor->get_name();
  msg.unique_id = get_default_unique_id("binary_sensor", binary_sensor);
  msg.device_class = binary_sensor->get_device_class();
  msg.is_status_binary_sensor = binary_sensor->is_status_binary_sensor();
  msg.disabled_by_default = binary_sensor->is_disabled_by_default();
  msg.icon = binary_sensor->get_icon();
  msg.entity_category = static_cast<enums::EntityCategory>(binary_sensor->get_entity_category());
  return this->send_list_entities_binary_sensor_response(msg);
}
#endif

#ifdef USE_COVER
bool APIConnection::send_cover_state(cover::Cover *cover) {
  if (!this->state_subscription_)
    return false;

  auto traits = cover->get_traits();
  CoverStateResponse resp{};
  resp.key = cover->get_object_id_hash();
  resp.legacy_state =
      (cover->position == cover::COVER_OPEN) ? enums::LEGACY_COVER_STATE_OPEN : enums::LEGACY_COVER_STATE_CLOSED;
  resp.position = cover->position;
  if (traits.get_supports_tilt())
    resp.tilt = cover->tilt;
  resp.current_operation = static_cast<enums::CoverOperation>(cover->current_operation);
  return this->send_cover_state_response(resp);
}
bool APIConnection::send_cover_info(cover::Cover *cover) {
  auto traits = cover->get_traits();
  ListEntitiesCoverResponse msg;
  msg.key = cover->get_object_id_hash();
  msg.object_id = cover->get_object_id();
  if (cover->has_own_name())
    msg.name = cover->get_name();
  msg.unique_id = get_default_unique_id("cover", cover);
  msg.assumed_state = traits.get_is_assumed_state();
  msg.supports_position = traits.get_supports_position();
  msg.supports_tilt = traits.get_supports_tilt();
  msg.supports_stop = traits.get_supports_stop();
  msg.device_class = cover->get_device_class();
  msg.disabled_by_default = cover->is_disabled_by_default();
  msg.icon = cover->get_icon();
  msg.entity_category = static_cast<enums::EntityCategory>(cover->get_entity_category());
  return this->send_list_entities_cover_response(msg);
}
void APIConnection::cover_command(const CoverCommandRequest &msg) {
  cover::Cover *cover = App.get_cover_by_key(msg.key);
  if (cover == nullptr)
    return;

  auto call = cover->make_call();
  if (msg.has_legacy_command) {
    switch (msg.legacy_command) {
      case enums::LEGACY_COVER_COMMAND_OPEN:
        call.set_command_open();
        break;
      case enums::LEGACY_COVER_COMMAND_CLOSE:
        call.set_command_close();
        break;
      case enums::LEGACY_COVER_COMMAND_STOP:
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
bool APIConnection::send_fan_state(fan::Fan *fan) {
  if (!this->state_subscription_)
    return false;

  auto traits = fan->get_traits();
  FanStateResponse resp{};
  resp.key = fan->get_object_id_hash();
  resp.state = fan->state;
  if (traits.supports_oscillation())
    resp.oscillating = fan->oscillating;
  if (traits.supports_speed()) {
    resp.speed_level = fan->speed;
  }
  if (traits.supports_direction())
    resp.direction = static_cast<enums::FanDirection>(fan->direction);
  if (traits.supports_preset_modes())
    resp.preset_mode = fan->preset_mode;
  return this->send_fan_state_response(resp);
}
bool APIConnection::send_fan_info(fan::Fan *fan) {
  auto traits = fan->get_traits();
  ListEntitiesFanResponse msg;
  msg.key = fan->get_object_id_hash();
  msg.object_id = fan->get_object_id();
  if (fan->has_own_name())
    msg.name = fan->get_name();
  msg.unique_id = get_default_unique_id("fan", fan);
  msg.supports_oscillation = traits.supports_oscillation();
  msg.supports_speed = traits.supports_speed();
  msg.supports_direction = traits.supports_direction();
  msg.supported_speed_count = traits.supported_speed_count();
  for (auto const &preset : traits.supported_preset_modes())
    msg.supported_preset_modes.push_back(preset);
  msg.disabled_by_default = fan->is_disabled_by_default();
  msg.icon = fan->get_icon();
  msg.entity_category = static_cast<enums::EntityCategory>(fan->get_entity_category());
  return this->send_list_entities_fan_response(msg);
}
void APIConnection::fan_command(const FanCommandRequest &msg) {
  fan::Fan *fan = App.get_fan_by_key(msg.key);
  if (fan == nullptr)
    return;

  auto call = fan->make_call();
  if (msg.has_state)
    call.set_state(msg.state);
  if (msg.has_oscillating)
    call.set_oscillating(msg.oscillating);
  if (msg.has_speed_level) {
    // Prefer level
    call.set_speed(msg.speed_level);
  }
  if (msg.has_direction)
    call.set_direction(static_cast<fan::FanDirection>(msg.direction));
  if (msg.has_preset_mode)
    call.set_preset_mode(msg.preset_mode);
  call.perform();
}
#endif

#ifdef USE_LIGHT
bool APIConnection::send_light_state(light::LightState *light) {
  if (!this->state_subscription_)
    return false;

  auto traits = light->get_traits();
  auto values = light->remote_values;
  auto color_mode = values.get_color_mode();
  LightStateResponse resp{};

  resp.key = light->get_object_id_hash();
  resp.state = values.is_on();
  resp.color_mode = static_cast<enums::ColorMode>(color_mode);
  resp.brightness = values.get_brightness();
  resp.color_brightness = values.get_color_brightness();
  resp.red = values.get_red();
  resp.green = values.get_green();
  resp.blue = values.get_blue();
  resp.white = values.get_white();
  resp.color_temperature = values.get_color_temperature();
  resp.cold_white = values.get_cold_white();
  resp.warm_white = values.get_warm_white();
  if (light->supports_effects())
    resp.effect = light->get_effect_name();
  return this->send_light_state_response(resp);
}
bool APIConnection::send_light_info(light::LightState *light) {
  auto traits = light->get_traits();
  ListEntitiesLightResponse msg;
  msg.key = light->get_object_id_hash();
  msg.object_id = light->get_object_id();
  if (light->has_own_name())
    msg.name = light->get_name();
  msg.unique_id = get_default_unique_id("light", light);

  msg.disabled_by_default = light->is_disabled_by_default();
  msg.icon = light->get_icon();
  msg.entity_category = static_cast<enums::EntityCategory>(light->get_entity_category());

  for (auto mode : traits.get_supported_color_modes())
    msg.supported_color_modes.push_back(static_cast<enums::ColorMode>(mode));

  msg.legacy_supports_brightness = traits.supports_color_capability(light::ColorCapability::BRIGHTNESS);
  msg.legacy_supports_rgb = traits.supports_color_capability(light::ColorCapability::RGB);
  msg.legacy_supports_white_value =
      msg.legacy_supports_rgb && (traits.supports_color_capability(light::ColorCapability::WHITE) ||
                                  traits.supports_color_capability(light::ColorCapability::COLD_WARM_WHITE));
  msg.legacy_supports_color_temperature = traits.supports_color_capability(light::ColorCapability::COLOR_TEMPERATURE) ||
                                          traits.supports_color_capability(light::ColorCapability::COLD_WARM_WHITE);

  if (msg.legacy_supports_color_temperature) {
    msg.min_mireds = traits.get_min_mireds();
    msg.max_mireds = traits.get_max_mireds();
  }
  if (light->supports_effects()) {
    msg.effects.emplace_back("None");
    for (auto *effect : light->get_effects())
      msg.effects.push_back(effect->get_name());
  }
  return this->send_list_entities_light_response(msg);
}
void APIConnection::light_command(const LightCommandRequest &msg) {
  light::LightState *light = App.get_light_by_key(msg.key);
  if (light == nullptr)
    return;

  auto call = light->make_call();
  if (msg.has_state)
    call.set_state(msg.state);
  if (msg.has_brightness)
    call.set_brightness(msg.brightness);
  if (msg.has_color_mode)
    call.set_color_mode(static_cast<light::ColorMode>(msg.color_mode));
  if (msg.has_color_brightness)
    call.set_color_brightness(msg.color_brightness);
  if (msg.has_rgb) {
    call.set_red(msg.red);
    call.set_green(msg.green);
    call.set_blue(msg.blue);
  }
  if (msg.has_white)
    call.set_white(msg.white);
  if (msg.has_color_temperature)
    call.set_color_temperature(msg.color_temperature);
  if (msg.has_cold_white)
    call.set_cold_white(msg.cold_white);
  if (msg.has_warm_white)
    call.set_warm_white(msg.warm_white);
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
bool APIConnection::send_sensor_state(sensor::Sensor *sensor, float state) {
  if (!this->state_subscription_)
    return false;

  SensorStateResponse resp{};
  resp.key = sensor->get_object_id_hash();
  resp.state = state;
  resp.missing_state = !sensor->has_state();
  return this->send_sensor_state_response(resp);
}
bool APIConnection::send_sensor_info(sensor::Sensor *sensor) {
  ListEntitiesSensorResponse msg;
  msg.key = sensor->get_object_id_hash();
  msg.object_id = sensor->get_object_id();
  if (sensor->has_own_name())
    msg.name = sensor->get_name();
  msg.unique_id = sensor->unique_id();
  if (msg.unique_id.empty())
    msg.unique_id = get_default_unique_id("sensor", sensor);
  msg.icon = sensor->get_icon();
  msg.unit_of_measurement = sensor->get_unit_of_measurement();
  msg.accuracy_decimals = sensor->get_accuracy_decimals();
  msg.force_update = sensor->get_force_update();
  msg.device_class = sensor->get_device_class();
  msg.state_class = static_cast<enums::SensorStateClass>(sensor->get_state_class());
  msg.disabled_by_default = sensor->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(sensor->get_entity_category());
  return this->send_list_entities_sensor_response(msg);
}
#endif

#ifdef USE_SWITCH
bool APIConnection::send_switch_state(switch_::Switch *a_switch, bool state) {
  if (!this->state_subscription_)
    return false;

  SwitchStateResponse resp{};
  resp.key = a_switch->get_object_id_hash();
  resp.state = state;
  return this->send_switch_state_response(resp);
}
bool APIConnection::send_switch_info(switch_::Switch *a_switch) {
  ListEntitiesSwitchResponse msg;
  msg.key = a_switch->get_object_id_hash();
  msg.object_id = a_switch->get_object_id();
  if (a_switch->has_own_name())
    msg.name = a_switch->get_name();
  msg.unique_id = get_default_unique_id("switch", a_switch);
  msg.icon = a_switch->get_icon();
  msg.assumed_state = a_switch->assumed_state();
  msg.disabled_by_default = a_switch->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(a_switch->get_entity_category());
  msg.device_class = a_switch->get_device_class();
  return this->send_list_entities_switch_response(msg);
}
void APIConnection::switch_command(const SwitchCommandRequest &msg) {
  switch_::Switch *a_switch = App.get_switch_by_key(msg.key);
  if (a_switch == nullptr)
    return;

  if (msg.state) {
    a_switch->turn_on();
  } else {
    a_switch->turn_off();
  }
}
#endif

#ifdef USE_TEXT_SENSOR
bool APIConnection::send_text_sensor_state(text_sensor::TextSensor *text_sensor, std::string state) {
  if (!this->state_subscription_)
    return false;

  TextSensorStateResponse resp{};
  resp.key = text_sensor->get_object_id_hash();
  resp.state = std::move(state);
  resp.missing_state = !text_sensor->has_state();
  return this->send_text_sensor_state_response(resp);
}
bool APIConnection::send_text_sensor_info(text_sensor::TextSensor *text_sensor) {
  ListEntitiesTextSensorResponse msg;
  msg.key = text_sensor->get_object_id_hash();
  msg.object_id = text_sensor->get_object_id();
  msg.name = text_sensor->get_name();
  msg.unique_id = text_sensor->unique_id();
  if (msg.unique_id.empty())
    msg.unique_id = get_default_unique_id("text_sensor", text_sensor);
  msg.icon = text_sensor->get_icon();
  msg.disabled_by_default = text_sensor->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(text_sensor->get_entity_category());
  msg.device_class = text_sensor->get_device_class();
  return this->send_list_entities_text_sensor_response(msg);
}
#endif

#ifdef USE_CLIMATE
bool APIConnection::send_climate_state(climate::Climate *climate) {
  if (!this->state_subscription_)
    return false;

  auto traits = climate->get_traits();
  ClimateStateResponse resp{};
  resp.key = climate->get_object_id_hash();
  resp.mode = static_cast<enums::ClimateMode>(climate->mode);
  resp.action = static_cast<enums::ClimateAction>(climate->action);
  if (traits.get_supports_current_temperature())
    resp.current_temperature = climate->current_temperature;
  if (traits.get_supports_two_point_target_temperature()) {
    resp.target_temperature_low = climate->target_temperature_low;
    resp.target_temperature_high = climate->target_temperature_high;
  } else {
    resp.target_temperature = climate->target_temperature;
  }
  if (traits.get_supports_fan_modes() && climate->fan_mode.has_value())
    resp.fan_mode = static_cast<enums::ClimateFanMode>(climate->fan_mode.value());
  if (!traits.get_supported_custom_fan_modes().empty() && climate->custom_fan_mode.has_value())
    resp.custom_fan_mode = climate->custom_fan_mode.value();
  if (traits.get_supports_presets() && climate->preset.has_value()) {
    resp.preset = static_cast<enums::ClimatePreset>(climate->preset.value());
  }
  if (!traits.get_supported_custom_presets().empty() && climate->custom_preset.has_value())
    resp.custom_preset = climate->custom_preset.value();
  if (traits.get_supports_swing_modes())
    resp.swing_mode = static_cast<enums::ClimateSwingMode>(climate->swing_mode);
  if (traits.get_supports_current_humidity())
    resp.current_humidity = climate->current_humidity;
  if (traits.get_supports_target_humidity())
    resp.target_humidity = climate->target_humidity;
  return this->send_climate_state_response(resp);
}
bool APIConnection::send_climate_info(climate::Climate *climate) {
  auto traits = climate->get_traits();
  ListEntitiesClimateResponse msg;
  msg.key = climate->get_object_id_hash();
  msg.object_id = climate->get_object_id();
  if (climate->has_own_name())
    msg.name = climate->get_name();
  msg.unique_id = get_default_unique_id("climate", climate);

  msg.disabled_by_default = climate->is_disabled_by_default();
  msg.icon = climate->get_icon();
  msg.entity_category = static_cast<enums::EntityCategory>(climate->get_entity_category());

  msg.supports_current_temperature = traits.get_supports_current_temperature();
  msg.supports_current_humidity = traits.get_supports_current_humidity();
  msg.supports_two_point_target_temperature = traits.get_supports_two_point_target_temperature();
  msg.supports_target_humidity = traits.get_supports_target_humidity();

  for (auto mode : traits.get_supported_modes())
    msg.supported_modes.push_back(static_cast<enums::ClimateMode>(mode));

  msg.visual_min_temperature = traits.get_visual_min_temperature();
  msg.visual_max_temperature = traits.get_visual_max_temperature();
  msg.visual_target_temperature_step = traits.get_visual_target_temperature_step();
  msg.visual_current_temperature_step = traits.get_visual_current_temperature_step();
  msg.visual_min_humidity = traits.get_visual_min_humidity();
  msg.visual_max_humidity = traits.get_visual_max_humidity();

  msg.legacy_supports_away = traits.supports_preset(climate::CLIMATE_PRESET_AWAY);
  msg.supports_action = traits.get_supports_action();

  for (auto fan_mode : traits.get_supported_fan_modes())
    msg.supported_fan_modes.push_back(static_cast<enums::ClimateFanMode>(fan_mode));
  for (auto const &custom_fan_mode : traits.get_supported_custom_fan_modes())
    msg.supported_custom_fan_modes.push_back(custom_fan_mode);
  for (auto preset : traits.get_supported_presets())
    msg.supported_presets.push_back(static_cast<enums::ClimatePreset>(preset));
  for (auto const &custom_preset : traits.get_supported_custom_presets())
    msg.supported_custom_presets.push_back(custom_preset);
  for (auto swing_mode : traits.get_supported_swing_modes())
    msg.supported_swing_modes.push_back(static_cast<enums::ClimateSwingMode>(swing_mode));
  return this->send_list_entities_climate_response(msg);
}
void APIConnection::climate_command(const ClimateCommandRequest &msg) {
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
  if (msg.has_target_humidity)
    call.set_target_humidity(msg.target_humidity);
  if (msg.has_fan_mode)
    call.set_fan_mode(static_cast<climate::ClimateFanMode>(msg.fan_mode));
  if (msg.has_custom_fan_mode)
    call.set_fan_mode(msg.custom_fan_mode);
  if (msg.has_preset)
    call.set_preset(static_cast<climate::ClimatePreset>(msg.preset));
  if (msg.has_custom_preset)
    call.set_preset(msg.custom_preset);
  if (msg.has_swing_mode)
    call.set_swing_mode(static_cast<climate::ClimateSwingMode>(msg.swing_mode));
  call.perform();
}
#endif

#ifdef USE_NUMBER
bool APIConnection::send_number_state(number::Number *number, float state) {
  if (!this->state_subscription_)
    return false;

  NumberStateResponse resp{};
  resp.key = number->get_object_id_hash();
  resp.state = state;
  resp.missing_state = !number->has_state();
  return this->send_number_state_response(resp);
}
bool APIConnection::send_number_info(number::Number *number) {
  ListEntitiesNumberResponse msg;
  msg.key = number->get_object_id_hash();
  msg.object_id = number->get_object_id();
  if (number->has_own_name())
    msg.name = number->get_name();
  msg.unique_id = get_default_unique_id("number", number);
  msg.icon = number->get_icon();
  msg.disabled_by_default = number->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(number->get_entity_category());
  msg.unit_of_measurement = number->traits.get_unit_of_measurement();
  msg.mode = static_cast<enums::NumberMode>(number->traits.get_mode());
  msg.device_class = number->traits.get_device_class();

  msg.min_value = number->traits.get_min_value();
  msg.max_value = number->traits.get_max_value();
  msg.step = number->traits.get_step();

  return this->send_list_entities_number_response(msg);
}
void APIConnection::number_command(const NumberCommandRequest &msg) {
  number::Number *number = App.get_number_by_key(msg.key);
  if (number == nullptr)
    return;

  auto call = number->make_call();
  call.set_value(msg.state);
  call.perform();
}
#endif

#ifdef USE_DATETIME_DATE
bool APIConnection::send_date_state(datetime::DateEntity *date) {
  if (!this->state_subscription_)
    return false;

  DateStateResponse resp{};
  resp.key = date->get_object_id_hash();
  resp.missing_state = !date->has_state();
  resp.year = date->year;
  resp.month = date->month;
  resp.day = date->day;
  return this->send_date_state_response(resp);
}
bool APIConnection::send_date_info(datetime::DateEntity *date) {
  ListEntitiesDateResponse msg;
  msg.key = date->get_object_id_hash();
  msg.object_id = date->get_object_id();
  if (date->has_own_name())
    msg.name = date->get_name();
  msg.unique_id = get_default_unique_id("date", date);
  msg.icon = date->get_icon();
  msg.disabled_by_default = date->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(date->get_entity_category());

  return this->send_list_entities_date_response(msg);
}
void APIConnection::date_command(const DateCommandRequest &msg) {
  datetime::DateEntity *date = App.get_date_by_key(msg.key);
  if (date == nullptr)
    return;

  auto call = date->make_call();
  call.set_date(msg.year, msg.month, msg.day);
  call.perform();
}
#endif

#ifdef USE_DATETIME_TIME
bool APIConnection::send_time_state(datetime::TimeEntity *time) {
  if (!this->state_subscription_)
    return false;

  TimeStateResponse resp{};
  resp.key = time->get_object_id_hash();
  resp.missing_state = !time->has_state();
  resp.hour = time->hour;
  resp.minute = time->minute;
  resp.second = time->second;
  return this->send_time_state_response(resp);
}
bool APIConnection::send_time_info(datetime::TimeEntity *time) {
  ListEntitiesTimeResponse msg;
  msg.key = time->get_object_id_hash();
  msg.object_id = time->get_object_id();
  if (time->has_own_name())
    msg.name = time->get_name();
  msg.unique_id = get_default_unique_id("time", time);
  msg.icon = time->get_icon();
  msg.disabled_by_default = time->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(time->get_entity_category());

  return this->send_list_entities_time_response(msg);
}
void APIConnection::time_command(const TimeCommandRequest &msg) {
  datetime::TimeEntity *time = App.get_time_by_key(msg.key);
  if (time == nullptr)
    return;

  auto call = time->make_call();
  call.set_time(msg.hour, msg.minute, msg.second);
  call.perform();
}
#endif

#ifdef USE_DATETIME_DATETIME
bool APIConnection::send_datetime_state(datetime::DateTimeEntity *datetime) {
  if (!this->state_subscription_)
    return false;

  DateTimeStateResponse resp{};
  resp.key = datetime->get_object_id_hash();
  resp.missing_state = !datetime->has_state();
  if (datetime->has_state()) {
    ESPTime state = datetime->state_as_esptime();
    resp.epoch_seconds = state.timestamp;
  }
  return this->send_date_time_state_response(resp);
}
bool APIConnection::send_datetime_info(datetime::DateTimeEntity *datetime) {
  ListEntitiesDateTimeResponse msg;
  msg.key = datetime->get_object_id_hash();
  msg.object_id = datetime->get_object_id();
  if (datetime->has_own_name())
    msg.name = datetime->get_name();
  msg.unique_id = get_default_unique_id("datetime", datetime);
  msg.icon = datetime->get_icon();
  msg.disabled_by_default = datetime->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(datetime->get_entity_category());

  return this->send_list_entities_date_time_response(msg);
}
void APIConnection::datetime_command(const DateTimeCommandRequest &msg) {
  datetime::DateTimeEntity *datetime = App.get_datetime_by_key(msg.key);
  if (datetime == nullptr)
    return;

  auto call = datetime->make_call();
  call.set_datetime(msg.epoch_seconds);
  call.perform();
}
#endif

#ifdef USE_TEXT
bool APIConnection::send_text_state(text::Text *text, std::string state) {
  if (!this->state_subscription_)
    return false;

  TextStateResponse resp{};
  resp.key = text->get_object_id_hash();
  resp.state = std::move(state);
  resp.missing_state = !text->has_state();
  return this->send_text_state_response(resp);
}
bool APIConnection::send_text_info(text::Text *text) {
  ListEntitiesTextResponse msg;
  msg.key = text->get_object_id_hash();
  msg.object_id = text->get_object_id();
  msg.name = text->get_name();
  msg.icon = text->get_icon();
  msg.disabled_by_default = text->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(text->get_entity_category());
  msg.mode = static_cast<enums::TextMode>(text->traits.get_mode());

  msg.min_length = text->traits.get_min_length();
  msg.max_length = text->traits.get_max_length();
  msg.pattern = text->traits.get_pattern();

  return this->send_list_entities_text_response(msg);
}
void APIConnection::text_command(const TextCommandRequest &msg) {
  text::Text *text = App.get_text_by_key(msg.key);
  if (text == nullptr)
    return;

  auto call = text->make_call();
  call.set_value(msg.state);
  call.perform();
}
#endif

#ifdef USE_SELECT
bool APIConnection::send_select_state(select::Select *select, std::string state) {
  if (!this->state_subscription_)
    return false;

  SelectStateResponse resp{};
  resp.key = select->get_object_id_hash();
  resp.state = std::move(state);
  resp.missing_state = !select->has_state();
  return this->send_select_state_response(resp);
}
bool APIConnection::send_select_info(select::Select *select) {
  ListEntitiesSelectResponse msg;
  msg.key = select->get_object_id_hash();
  msg.object_id = select->get_object_id();
  if (select->has_own_name())
    msg.name = select->get_name();
  msg.unique_id = get_default_unique_id("select", select);
  msg.icon = select->get_icon();
  msg.disabled_by_default = select->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(select->get_entity_category());

  for (const auto &option : select->traits.get_options())
    msg.options.push_back(option);

  return this->send_list_entities_select_response(msg);
}
void APIConnection::select_command(const SelectCommandRequest &msg) {
  select::Select *select = App.get_select_by_key(msg.key);
  if (select == nullptr)
    return;

  auto call = select->make_call();
  call.set_option(msg.state);
  call.perform();
}
#endif

#ifdef USE_BUTTON
bool APIConnection::send_button_info(button::Button *button) {
  ListEntitiesButtonResponse msg;
  msg.key = button->get_object_id_hash();
  msg.object_id = button->get_object_id();
  if (button->has_own_name())
    msg.name = button->get_name();
  msg.unique_id = get_default_unique_id("button", button);
  msg.icon = button->get_icon();
  msg.disabled_by_default = button->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(button->get_entity_category());
  msg.device_class = button->get_device_class();
  return this->send_list_entities_button_response(msg);
}
void APIConnection::button_command(const ButtonCommandRequest &msg) {
  button::Button *button = App.get_button_by_key(msg.key);
  if (button == nullptr)
    return;

  button->press();
}
#endif

#ifdef USE_LOCK
bool APIConnection::send_lock_state(lock::Lock *a_lock, lock::LockState state) {
  if (!this->state_subscription_)
    return false;

  LockStateResponse resp{};
  resp.key = a_lock->get_object_id_hash();
  resp.state = static_cast<enums::LockState>(state);
  return this->send_lock_state_response(resp);
}
bool APIConnection::send_lock_info(lock::Lock *a_lock) {
  ListEntitiesLockResponse msg;
  msg.key = a_lock->get_object_id_hash();
  msg.object_id = a_lock->get_object_id();
  if (a_lock->has_own_name())
    msg.name = a_lock->get_name();
  msg.unique_id = get_default_unique_id("lock", a_lock);
  msg.icon = a_lock->get_icon();
  msg.assumed_state = a_lock->traits.get_assumed_state();
  msg.disabled_by_default = a_lock->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(a_lock->get_entity_category());
  msg.supports_open = a_lock->traits.get_supports_open();
  msg.requires_code = a_lock->traits.get_requires_code();
  return this->send_list_entities_lock_response(msg);
}
void APIConnection::lock_command(const LockCommandRequest &msg) {
  lock::Lock *a_lock = App.get_lock_by_key(msg.key);
  if (a_lock == nullptr)
    return;

  switch (msg.command) {
    case enums::LOCK_UNLOCK:
      a_lock->unlock();
      break;
    case enums::LOCK_LOCK:
      a_lock->lock();
      break;
    case enums::LOCK_OPEN:
      a_lock->open();
      break;
  }
}
#endif

#ifdef USE_VALVE
bool APIConnection::send_valve_state(valve::Valve *valve) {
  if (!this->state_subscription_)
    return false;

  ValveStateResponse resp{};
  resp.key = valve->get_object_id_hash();
  resp.position = valve->position;
  resp.current_operation = static_cast<enums::ValveOperation>(valve->current_operation);
  return this->send_valve_state_response(resp);
}
bool APIConnection::send_valve_info(valve::Valve *valve) {
  auto traits = valve->get_traits();
  ListEntitiesValveResponse msg;
  msg.key = valve->get_object_id_hash();
  msg.object_id = valve->get_object_id();
  if (valve->has_own_name())
    msg.name = valve->get_name();
  msg.unique_id = get_default_unique_id("valve", valve);
  msg.icon = valve->get_icon();
  msg.disabled_by_default = valve->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(valve->get_entity_category());
  msg.device_class = valve->get_device_class();
  msg.assumed_state = traits.get_is_assumed_state();
  msg.supports_position = traits.get_supports_position();
  msg.supports_stop = traits.get_supports_stop();
  return this->send_list_entities_valve_response(msg);
}
void APIConnection::valve_command(const ValveCommandRequest &msg) {
  valve::Valve *valve = App.get_valve_by_key(msg.key);
  if (valve == nullptr)
    return;

  auto call = valve->make_call();
  if (msg.has_position)
    call.set_position(msg.position);
  if (msg.stop)
    call.set_command_stop();
  call.perform();
}
#endif

#ifdef USE_MEDIA_PLAYER
bool APIConnection::send_media_player_state(media_player::MediaPlayer *media_player) {
  if (!this->state_subscription_)
    return false;

  MediaPlayerStateResponse resp{};
  resp.key = media_player->get_object_id_hash();

  media_player::MediaPlayerState report_state = media_player->state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING
                                                    ? media_player::MEDIA_PLAYER_STATE_PLAYING
                                                    : media_player->state;
  resp.state = static_cast<enums::MediaPlayerState>(report_state);
  resp.volume = media_player->volume;
  resp.muted = media_player->is_muted();
  return this->send_media_player_state_response(resp);
}
bool APIConnection::send_media_player_info(media_player::MediaPlayer *media_player) {
  ListEntitiesMediaPlayerResponse msg;
  msg.key = media_player->get_object_id_hash();
  msg.object_id = media_player->get_object_id();
  if (media_player->has_own_name())
    msg.name = media_player->get_name();
  msg.unique_id = get_default_unique_id("media_player", media_player);
  msg.icon = media_player->get_icon();
  msg.disabled_by_default = media_player->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(media_player->get_entity_category());

  auto traits = media_player->get_traits();
  msg.supports_pause = traits.get_supports_pause();

  return this->send_list_entities_media_player_response(msg);
}
void APIConnection::media_player_command(const MediaPlayerCommandRequest &msg) {
  media_player::MediaPlayer *media_player = App.get_media_player_by_key(msg.key);
  if (media_player == nullptr)
    return;

  auto call = media_player->make_call();
  if (msg.has_command) {
    call.set_command(static_cast<media_player::MediaPlayerCommand>(msg.command));
  }
  if (msg.has_volume) {
    call.set_volume(msg.volume);
  }
  if (msg.has_media_url) {
    call.set_media_url(msg.media_url);
  }
  if (msg.has_announcement) {
    call.set_announcement(msg.announcement);
  }
  call.perform();
}
#endif

#ifdef USE_ESP32_CAMERA
void APIConnection::send_camera_state(std::shared_ptr<esp32_camera::CameraImage> image) {
  if (!this->state_subscription_)
    return;
  if (this->image_reader_.available())
    return;
  if (image->was_requested_by(esphome::esp32_camera::API_REQUESTER) ||
      image->was_requested_by(esphome::esp32_camera::IDLE))
    this->image_reader_.set_image(std::move(image));
}
bool APIConnection::send_camera_info(esp32_camera::ESP32Camera *camera) {
  ListEntitiesCameraResponse msg;
  msg.key = camera->get_object_id_hash();
  msg.object_id = camera->get_object_id();
  if (camera->has_own_name())
    msg.name = camera->get_name();
  msg.unique_id = get_default_unique_id("camera", camera);
  msg.disabled_by_default = camera->is_disabled_by_default();
  msg.icon = camera->get_icon();
  msg.entity_category = static_cast<enums::EntityCategory>(camera->get_entity_category());
  return this->send_list_entities_camera_response(msg);
}
void APIConnection::camera_image(const CameraImageRequest &msg) {
  if (esp32_camera::global_esp32_camera == nullptr)
    return;

  if (msg.single)
    esp32_camera::global_esp32_camera->request_image(esphome::esp32_camera::API_REQUESTER);
  if (msg.stream) {
    esp32_camera::global_esp32_camera->start_stream(esphome::esp32_camera::API_REQUESTER);

    App.scheduler.set_timeout(this->parent_, "api_esp32_camera_stop_stream", ESP32_CAMERA_STOP_STREAM, []() {
      esp32_camera::global_esp32_camera->stop_stream(esphome::esp32_camera::API_REQUESTER);
    });
  }
}
#endif

#ifdef USE_HOMEASSISTANT_TIME
void APIConnection::on_get_time_response(const GetTimeResponse &value) {
  if (homeassistant::global_homeassistant_time != nullptr)
    homeassistant::global_homeassistant_time->set_epoch_time(value.epoch_seconds);
}
#endif

#ifdef USE_BLUETOOTH_PROXY
void APIConnection::subscribe_bluetooth_le_advertisements(const SubscribeBluetoothLEAdvertisementsRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->subscribe_api_connection(this, msg.flags);
}
void APIConnection::unsubscribe_bluetooth_le_advertisements(const UnsubscribeBluetoothLEAdvertisementsRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->unsubscribe_api_connection(this);
}
bool APIConnection::send_bluetooth_le_advertisement(const BluetoothLEAdvertisementResponse &msg) {
  if (this->client_api_version_major_ < 1 || this->client_api_version_minor_ < 7) {
    BluetoothLEAdvertisementResponse resp = msg;
    for (auto &service : resp.service_data) {
      service.legacy_data.assign(service.data.begin(), service.data.end());
      service.data.clear();
    }
    for (auto &manufacturer_data : resp.manufacturer_data) {
      manufacturer_data.legacy_data.assign(manufacturer_data.data.begin(), manufacturer_data.data.end());
      manufacturer_data.data.clear();
    }
    return this->send_bluetooth_le_advertisement_response(resp);
  }
  return this->send_bluetooth_le_advertisement_response(msg);
}
void APIConnection::bluetooth_device_request(const BluetoothDeviceRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_device_request(msg);
}
void APIConnection::bluetooth_gatt_read(const BluetoothGATTReadRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_read(msg);
}
void APIConnection::bluetooth_gatt_write(const BluetoothGATTWriteRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_write(msg);
}
void APIConnection::bluetooth_gatt_read_descriptor(const BluetoothGATTReadDescriptorRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_read_descriptor(msg);
}
void APIConnection::bluetooth_gatt_write_descriptor(const BluetoothGATTWriteDescriptorRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_write_descriptor(msg);
}
void APIConnection::bluetooth_gatt_get_services(const BluetoothGATTGetServicesRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_send_services(msg);
}

void APIConnection::bluetooth_gatt_notify(const BluetoothGATTNotifyRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_gatt_notify(msg);
}

BluetoothConnectionsFreeResponse APIConnection::subscribe_bluetooth_connections_free(
    const SubscribeBluetoothConnectionsFreeRequest &msg) {
  BluetoothConnectionsFreeResponse resp;
  resp.free = bluetooth_proxy::global_bluetooth_proxy->get_bluetooth_connections_free();
  resp.limit = bluetooth_proxy::global_bluetooth_proxy->get_bluetooth_connections_limit();
  return resp;
}
#endif

#ifdef USE_VOICE_ASSISTANT
void APIConnection::subscribe_voice_assistant(const SubscribeVoiceAssistantRequest &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    voice_assistant::global_voice_assistant->client_subscription(this, msg.subscribe);
  }
}
void APIConnection::on_voice_assistant_response(const VoiceAssistantResponse &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return;
    }

    if (msg.error) {
      voice_assistant::global_voice_assistant->failed_to_start();
      return;
    }
    if (msg.port == 0) {
      // Use API Audio
      voice_assistant::global_voice_assistant->start_streaming();
    } else {
      struct sockaddr_storage storage;
      socklen_t len = sizeof(storage);
      this->helper_->getpeername((struct sockaddr *) &storage, &len);
      voice_assistant::global_voice_assistant->start_streaming(&storage, msg.port);
    }
  }
};
void APIConnection::on_voice_assistant_event_response(const VoiceAssistantEventResponse &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return;
    }

    voice_assistant::global_voice_assistant->on_event(msg);
  }
}
void APIConnection::on_voice_assistant_audio(const VoiceAssistantAudio &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return;
    }

    voice_assistant::global_voice_assistant->on_audio(msg);
  }
};
void APIConnection::on_voice_assistant_timer_event_response(const VoiceAssistantTimerEventResponse &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    if (voice_assistant::global_voice_assistant->get_api_connection() != this) {
      return;
    }

    voice_assistant::global_voice_assistant->on_timer_event(msg);
  }
};

#endif

#ifdef USE_ALARM_CONTROL_PANEL
bool APIConnection::send_alarm_control_panel_state(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
  if (!this->state_subscription_)
    return false;

  AlarmControlPanelStateResponse resp{};
  resp.key = a_alarm_control_panel->get_object_id_hash();
  resp.state = static_cast<enums::AlarmControlPanelState>(a_alarm_control_panel->get_state());
  return this->send_alarm_control_panel_state_response(resp);
}
bool APIConnection::send_alarm_control_panel_info(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
  ListEntitiesAlarmControlPanelResponse msg;
  msg.key = a_alarm_control_panel->get_object_id_hash();
  msg.object_id = a_alarm_control_panel->get_object_id();
  msg.name = a_alarm_control_panel->get_name();
  msg.unique_id = get_default_unique_id("alarm_control_panel", a_alarm_control_panel);
  msg.icon = a_alarm_control_panel->get_icon();
  msg.disabled_by_default = a_alarm_control_panel->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(a_alarm_control_panel->get_entity_category());
  msg.supported_features = a_alarm_control_panel->get_supported_features();
  msg.requires_code = a_alarm_control_panel->get_requires_code();
  msg.requires_code_to_arm = a_alarm_control_panel->get_requires_code_to_arm();
  return this->send_list_entities_alarm_control_panel_response(msg);
}
void APIConnection::alarm_control_panel_command(const AlarmControlPanelCommandRequest &msg) {
  alarm_control_panel::AlarmControlPanel *a_alarm_control_panel = App.get_alarm_control_panel_by_key(msg.key);
  if (a_alarm_control_panel == nullptr)
    return;

  auto call = a_alarm_control_panel->make_call();
  switch (msg.command) {
    case enums::ALARM_CONTROL_PANEL_DISARM:
      call.disarm();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_AWAY:
      call.arm_away();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_HOME:
      call.arm_home();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_NIGHT:
      call.arm_night();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_VACATION:
      call.arm_vacation();
      break;
    case enums::ALARM_CONTROL_PANEL_ARM_CUSTOM_BYPASS:
      call.arm_custom_bypass();
      break;
    case enums::ALARM_CONTROL_PANEL_TRIGGER:
      call.pending();
      break;
  }
  call.set_code(msg.code);
  call.perform();
}
#endif

#ifdef USE_EVENT
bool APIConnection::send_event(event::Event *event, std::string event_type) {
  EventResponse resp{};
  resp.key = event->get_object_id_hash();
  resp.event_type = std::move(event_type);
  return this->send_event_response(resp);
}
bool APIConnection::send_event_info(event::Event *event) {
  ListEntitiesEventResponse msg;
  msg.key = event->get_object_id_hash();
  msg.object_id = event->get_object_id();
  if (event->has_own_name())
    msg.name = event->get_name();
  msg.unique_id = get_default_unique_id("event", event);
  msg.icon = event->get_icon();
  msg.disabled_by_default = event->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(event->get_entity_category());
  msg.device_class = event->get_device_class();
  for (const auto &event_type : event->get_event_types())
    msg.event_types.push_back(event_type);
  return this->send_list_entities_event_response(msg);
}
#endif

#ifdef USE_UPDATE
bool APIConnection::send_update_state(update::UpdateEntity *update) {
  if (!this->state_subscription_)
    return false;

  UpdateStateResponse resp{};
  resp.key = update->get_object_id_hash();
  resp.missing_state = !update->has_state();
  if (update->has_state()) {
    resp.in_progress = update->state == update::UpdateState::UPDATE_STATE_INSTALLING;
    if (update->update_info.has_progress) {
      resp.has_progress = true;
      resp.progress = update->update_info.progress;
    }
    resp.current_version = update->update_info.current_version;
    resp.latest_version = update->update_info.latest_version;
    resp.title = update->update_info.title;
    resp.release_summary = update->update_info.summary;
    resp.release_url = update->update_info.release_url;
  }

  return this->send_update_state_response(resp);
}
bool APIConnection::send_update_info(update::UpdateEntity *update) {
  ListEntitiesUpdateResponse msg;
  msg.key = update->get_object_id_hash();
  msg.object_id = update->get_object_id();
  if (update->has_own_name())
    msg.name = update->get_name();
  msg.unique_id = get_default_unique_id("update", update);
  msg.icon = update->get_icon();
  msg.disabled_by_default = update->is_disabled_by_default();
  msg.entity_category = static_cast<enums::EntityCategory>(update->get_entity_category());
  msg.device_class = update->get_device_class();
  return this->send_list_entities_update_response(msg);
}
void APIConnection::update_command(const UpdateCommandRequest &msg) {
  update::UpdateEntity *update = App.get_update_by_key(msg.key);
  if (update == nullptr)
    return;

  switch (msg.command) {
    case enums::UPDATE_COMMAND_UPDATE:
      update->perform();
      break;
    case enums::UPDATE_COMMAND_CHECK:
      update->check();
      break;
    default:
      ESP_LOGW(TAG, "Unknown update command: %d", msg.command);
      break;
  }
}
#endif

bool APIConnection::send_log_message(int level, const char *tag, const char *line) {
  if (this->log_subscription_ < level)
    return false;

  // Send raw so that we don't copy too much
  auto buffer = this->create_buffer();
  // LogLevel level = 1;
  buffer.encode_uint32(1, static_cast<uint32_t>(level));
  // string message = 3;
  buffer.encode_string(3, line, strlen(line));
  // SubscribeLogsResponse - 29
  return this->send_buffer(buffer, 29);
}

HelloResponse APIConnection::hello(const HelloRequest &msg) {
  this->client_info_ = msg.client_info;
  this->client_peername_ = this->helper_->getpeername();
  this->client_combined_info_ = this->client_info_ + " (" + this->client_peername_ + ")";
  this->helper_->set_log_info(this->client_combined_info_);
  this->client_api_version_major_ = msg.api_version_major;
  this->client_api_version_minor_ = msg.api_version_minor;
  ESP_LOGV(TAG, "Hello from client: '%s' | %s | API Version %" PRIu32 ".%" PRIu32, this->client_info_.c_str(),
           this->client_peername_.c_str(), this->client_api_version_major_, this->client_api_version_minor_);

  HelloResponse resp;
  resp.api_version_major = 1;
  resp.api_version_minor = 10;
  resp.server_info = App.get_name() + " (esphome v" ESPHOME_VERSION ")";
  resp.name = App.get_name();

  this->connection_state_ = ConnectionState::CONNECTED;
  return resp;
}
ConnectResponse APIConnection::connect(const ConnectRequest &msg) {
  bool correct = this->parent_->check_password(msg.password);

  ConnectResponse resp;
  // bool invalid_password = 1;
  resp.invalid_password = !correct;
  if (correct) {
    ESP_LOGD(TAG, "%s: Connected successfully", this->client_combined_info_.c_str());
    this->connection_state_ = ConnectionState::AUTHENTICATED;
    this->parent_->get_client_connected_trigger()->trigger(this->client_info_, this->client_peername_);
#ifdef USE_HOMEASSISTANT_TIME
    if (homeassistant::global_homeassistant_time != nullptr) {
      this->send_time_request();
    }
#endif
  }
  return resp;
}
DeviceInfoResponse APIConnection::device_info(const DeviceInfoRequest &msg) {
  DeviceInfoResponse resp{};
  resp.uses_password = this->parent_->uses_password();
  resp.name = App.get_name();
  resp.friendly_name = App.get_friendly_name();
  resp.suggested_area = App.get_area();
  resp.mac_address = get_mac_address_pretty();
  resp.esphome_version = ESPHOME_VERSION;
  resp.compilation_time = App.get_compilation_time();
#if defined(USE_ESP8266) || defined(USE_ESP32)
  resp.manufacturer = "Espressif";
#elif defined(USE_RP2040)
  resp.manufacturer = "Raspberry Pi";
#elif defined(USE_BK72XX)
  resp.manufacturer = "Beken";
#elif defined(USE_RTL87XX)
  resp.manufacturer = "Realtek";
#elif defined(USE_HOST)
  resp.manufacturer = "Host";
#endif
  resp.model = ESPHOME_BOARD;
#ifdef USE_DEEP_SLEEP
  resp.has_deep_sleep = deep_sleep::global_has_deep_sleep;
#endif
#ifdef ESPHOME_PROJECT_NAME
  resp.project_name = ESPHOME_PROJECT_NAME;
  resp.project_version = ESPHOME_PROJECT_VERSION;
#endif
#ifdef USE_WEBSERVER
  resp.webserver_port = USE_WEBSERVER_PORT;
#endif
#ifdef USE_BLUETOOTH_PROXY
  resp.legacy_bluetooth_proxy_version = bluetooth_proxy::global_bluetooth_proxy->get_legacy_version();
  resp.bluetooth_proxy_feature_flags = bluetooth_proxy::global_bluetooth_proxy->get_feature_flags();
#endif
#ifdef USE_VOICE_ASSISTANT
  resp.legacy_voice_assistant_version = voice_assistant::global_voice_assistant->get_legacy_version();
  resp.voice_assistant_feature_flags = voice_assistant::global_voice_assistant->get_feature_flags();
#endif
  return resp;
}
void APIConnection::on_home_assistant_state_response(const HomeAssistantStateResponse &msg) {
  for (auto &it : this->parent_->get_state_subs()) {
    if (it.entity_id == msg.entity_id && it.attribute.value() == msg.attribute) {
      it.callback(msg.state);
    }
  }
}
void APIConnection::execute_service(const ExecuteServiceRequest &msg) {
  bool found = false;
  for (auto *service : this->parent_->get_user_services()) {
    if (service->execute_service(msg)) {
      found = true;
    }
  }
  if (!found) {
    ESP_LOGV(TAG, "Could not find matching service!");
  }
}
void APIConnection::subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) {
  state_subs_at_ = 0;
}
bool APIConnection::send_buffer(ProtoWriteBuffer buffer, uint32_t message_type) {
  if (this->remove_)
    return false;
  if (!this->helper_->can_write_without_blocking()) {
    delay(0);
    APIError err = this->helper_->loop();
    if (err != APIError::OK) {
      on_fatal_error();
      ESP_LOGW(TAG, "%s: Socket operation failed: %s errno=%d", this->client_combined_info_.c_str(),
               api_error_to_str(err), errno);
      return false;
    }
    if (!this->helper_->can_write_without_blocking()) {
      // SubscribeLogsResponse
      if (message_type != 29) {
        ESP_LOGV(TAG, "Cannot send message because of TCP buffer space");
      }
      delay(0);
      return false;
    }
  }

  APIError err = this->helper_->write_packet(message_type, buffer.get_buffer()->data(), buffer.get_buffer()->size());
  if (err == APIError::WOULD_BLOCK)
    return false;
  if (err != APIError::OK) {
    on_fatal_error();
    if (err == APIError::SOCKET_WRITE_FAILED && errno == ECONNRESET) {
      ESP_LOGW(TAG, "%s: Connection reset", this->client_combined_info_.c_str());
    } else {
      ESP_LOGW(TAG, "%s: Packet write failed %s errno=%d", this->client_combined_info_.c_str(), api_error_to_str(err),
               errno);
    }
    return false;
  }
  // Do not set last_traffic_ on send
  return true;
}
void APIConnection::on_unauthenticated_access() {
  this->on_fatal_error();
  ESP_LOGD(TAG, "%s: tried to access without authentication.", this->client_combined_info_.c_str());
}
void APIConnection::on_no_setup_connection() {
  this->on_fatal_error();
  ESP_LOGD(TAG, "%s: tried to access without full connection.", this->client_combined_info_.c_str());
}
void APIConnection::on_fatal_error() {
  this->helper_->close();
  this->remove_ = true;
}

}  // namespace api
}  // namespace esphome
