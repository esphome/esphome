#include "api_connection.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/version.h"

#ifdef USE_DEEP_SLEEP
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif
#ifdef USE_HOMEASSISTANT_TIME
#include "esphome/components/homeassistant/time/homeassistant_time.h"
#endif
#ifdef USE_FAN
#include "esphome/components/fan/fan_helpers.h"
#endif

namespace esphome {
namespace api {

static const char *const TAG = "api.connection";

APIConnection::APIConnection(AsyncClient *client, APIServer *parent)
    : client_(client), parent_(parent), initial_state_iterator_(parent, this), list_entities_iterator_(parent, this) {
  this->client_->onError([](void *s, AsyncClient *c, int8_t error) { ((APIConnection *) s)->on_error_(error); }, this);
  this->client_->onDisconnect([](void *s, AsyncClient *c) { ((APIConnection *) s)->on_disconnect_(); }, this);
  this->client_->onTimeout([](void *s, AsyncClient *c, uint32_t time) { ((APIConnection *) s)->on_timeout_(time); },
                           this);
  this->client_->onData([](void *s, AsyncClient *c, void *buf,
                           size_t len) { ((APIConnection *) s)->on_data_(reinterpret_cast<uint8_t *>(buf), len); },
                        this);

  this->send_buffer_.reserve(64);
  this->recv_buffer_.reserve(32);
  this->client_info_ = this->client_->remoteIP().toString().c_str();
  this->last_traffic_ = millis();
}
APIConnection::~APIConnection() { delete this->client_; }
void APIConnection::on_error_(int8_t error) { this->remove_ = true; }
void APIConnection::on_disconnect_() { this->remove_ = true; }
void APIConnection::on_timeout_(uint32_t time) { this->on_fatal_error(); }
void APIConnection::on_data_(uint8_t *buf, size_t len) {
  if (len == 0 || buf == nullptr)
    return;
  this->recv_buffer_.insert(this->recv_buffer_.end(), buf, buf + len);
}
void APIConnection::parse_recv_buffer_() {
  if (this->recv_buffer_.empty() || this->remove_)
    return;

  while (!this->recv_buffer_.empty()) {
    if (this->recv_buffer_[0] != 0x00) {
      ESP_LOGW(TAG, "Invalid preamble from %s", this->client_info_.c_str());
      this->on_fatal_error();
      return;
    }
    uint32_t i = 1;
    const uint32_t size = this->recv_buffer_.size();
    uint32_t consumed;
    auto msg_size_varint = ProtoVarInt::parse(&this->recv_buffer_[i], size - i, &consumed);
    if (!msg_size_varint.has_value())
      // not enough data there yet
      return;
    i += consumed;
    uint32_t msg_size = msg_size_varint->as_uint32();

    auto msg_type_varint = ProtoVarInt::parse(&this->recv_buffer_[i], size - i, &consumed);
    if (!msg_type_varint.has_value())
      // not enough data there yet
      return;
    i += consumed;
    uint32_t msg_type = msg_type_varint->as_uint32();

    if (size - i < msg_size)
      // message body not fully received
      return;

    uint8_t *msg = &this->recv_buffer_[i];
    this->read_message(msg_size, msg_type, msg);
    if (this->remove_)
      return;
    // pop front
    uint32_t total = i + msg_size;
    this->recv_buffer_.erase(this->recv_buffer_.begin(), this->recv_buffer_.begin() + total);
    this->last_traffic_ = millis();
  }
}

void APIConnection::disconnect_client() {
  this->client_->close();
  this->remove_ = true;
}

void APIConnection::loop() {
  if (this->remove_)
    return;

  if (this->next_close_) {
    this->disconnect_client();
    return;
  }

  if (!network_is_connected()) {
    // when network is disconnected force disconnect immediately
    // don't wait for timeout
    this->on_fatal_error();
    return;
  }
  if (this->client_->disconnected()) {
    // failsafe for disconnect logic
    this->on_disconnect_();
    return;
  }
  this->parse_recv_buffer_();

  this->list_entities_iterator_.advance();
  this->initial_state_iterator_.advance();

  const uint32_t keepalive = 60000;
  if (this->sent_ping_) {
    // Disconnect if not responded within 2.5*keepalive
    if (millis() - this->last_traffic_ > (keepalive * 5) / 2) {
      ESP_LOGW(TAG, "'%s' didn't respond to ping request in time. Disconnecting...", this->client_info_.c_str());
      this->disconnect_client();
    }
  } else if (millis() - this->last_traffic_ > keepalive) {
    this->sent_ping_ = true;
    this->send_ping_request(PingRequest());
  }

#ifdef USE_ESP32_CAMERA
  if (this->image_reader_.available()) {
    uint32_t space = this->client_->space();
    // reserve 15 bytes for metadata, and at least 64 bytes of data
    if (space >= 15 + 64) {
      uint32_t to_send = std::min(space - 15, this->image_reader_.available());
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
  }
#endif
}

std::string get_default_unique_id(const std::string &component_type, Nameable *nameable) {
  return App.get_name() + component_type + nameable->get_object_id();
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
  msg.name = binary_sensor->get_name();
  msg.unique_id = get_default_unique_id("binary_sensor", binary_sensor);
  msg.device_class = binary_sensor->get_device_class();
  msg.is_status_binary_sensor = binary_sensor->is_status_binary_sensor();
  msg.disabled_by_default = binary_sensor->is_disabled_by_default();
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
  msg.name = cover->get_name();
  msg.unique_id = get_default_unique_id("cover", cover);
  msg.assumed_state = traits.get_is_assumed_state();
  msg.supports_position = traits.get_supports_position();
  msg.supports_tilt = traits.get_supports_tilt();
  msg.device_class = cover->get_device_class();
  msg.disabled_by_default = cover->is_disabled_by_default();
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
bool APIConnection::send_fan_state(fan::FanState *fan) {
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
    resp.speed = static_cast<enums::FanSpeed>(fan::speed_level_to_enum(fan->speed, traits.supported_speed_count()));
  }
  if (traits.supports_direction())
    resp.direction = static_cast<enums::FanDirection>(fan->direction);
  return this->send_fan_state_response(resp);
}
bool APIConnection::send_fan_info(fan::FanState *fan) {
  auto traits = fan->get_traits();
  ListEntitiesFanResponse msg;
  msg.key = fan->get_object_id_hash();
  msg.object_id = fan->get_object_id();
  msg.name = fan->get_name();
  msg.unique_id = get_default_unique_id("fan", fan);
  msg.supports_oscillation = traits.supports_oscillation();
  msg.supports_speed = traits.supports_speed();
  msg.supports_direction = traits.supports_direction();
  msg.supported_speed_count = traits.supported_speed_count();
  msg.disabled_by_default = fan->is_disabled_by_default();
  return this->send_list_entities_fan_response(msg);
}
void APIConnection::fan_command(const FanCommandRequest &msg) {
  fan::FanState *fan = App.get_fan_by_key(msg.key);
  if (fan == nullptr)
    return;

  auto traits = fan->get_traits();

  auto call = fan->make_call();
  if (msg.has_state)
    call.set_state(msg.state);
  if (msg.has_oscillating)
    call.set_oscillating(msg.oscillating);
  if (msg.has_speed_level) {
    // Prefer level
    call.set_speed(msg.speed_level);
  } else if (msg.has_speed) {
    call.set_speed(fan::speed_enum_to_level(static_cast<fan::FanSpeed>(msg.speed), traits.supported_speed_count()));
  }
  if (msg.has_direction)
    call.set_direction(static_cast<fan::FanDirection>(msg.direction));
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
  msg.name = light->get_name();
  msg.unique_id = get_default_unique_id("light", light);

  msg.disabled_by_default = light->is_disabled_by_default();

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
  msg.name = sensor->get_name();
  msg.unique_id = sensor->unique_id();
  if (msg.unique_id.empty())
    msg.unique_id = get_default_unique_id("sensor", sensor);
  msg.icon = sensor->get_icon();
  msg.unit_of_measurement = sensor->get_unit_of_measurement();
  msg.accuracy_decimals = sensor->get_accuracy_decimals();
  msg.force_update = sensor->get_force_update();
  msg.device_class = sensor->get_device_class();
  msg.state_class = static_cast<enums::SensorStateClass>(sensor->state_class);
  msg.last_reset_type = static_cast<enums::SensorLastResetType>(sensor->last_reset_type);
  msg.disabled_by_default = sensor->is_disabled_by_default();

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
  msg.name = a_switch->get_name();
  msg.unique_id = get_default_unique_id("switch", a_switch);
  msg.icon = a_switch->get_icon();
  msg.assumed_state = a_switch->assumed_state();
  msg.disabled_by_default = a_switch->is_disabled_by_default();
  return this->send_list_entities_switch_response(msg);
}
void APIConnection::switch_command(const SwitchCommandRequest &msg) {
  switch_::Switch *a_switch = App.get_switch_by_key(msg.key);
  if (a_switch == nullptr)
    return;

  if (msg.state)
    a_switch->turn_on();
  else
    a_switch->turn_off();
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
    resp.legacy_away = resp.preset == enums::CLIMATE_PRESET_AWAY;
  }
  if (!traits.get_supported_custom_presets().empty() && climate->custom_preset.has_value())
    resp.custom_preset = climate->custom_preset.value();
  if (traits.get_supports_swing_modes())
    resp.swing_mode = static_cast<enums::ClimateSwingMode>(climate->swing_mode);
  return this->send_climate_state_response(resp);
}
bool APIConnection::send_climate_info(climate::Climate *climate) {
  auto traits = climate->get_traits();
  ListEntitiesClimateResponse msg;
  msg.key = climate->get_object_id_hash();
  msg.object_id = climate->get_object_id();
  msg.name = climate->get_name();
  msg.unique_id = get_default_unique_id("climate", climate);

  msg.disabled_by_default = climate->is_disabled_by_default();

  msg.supports_current_temperature = traits.get_supports_current_temperature();
  msg.supports_two_point_target_temperature = traits.get_supports_two_point_target_temperature();

  for (auto mode : traits.get_supported_modes())
    msg.supported_modes.push_back(static_cast<enums::ClimateMode>(mode));

  msg.visual_min_temperature = traits.get_visual_min_temperature();
  msg.visual_max_temperature = traits.get_visual_max_temperature();
  msg.visual_temperature_step = traits.get_visual_temperature_step();
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
  if (msg.has_legacy_away)
    call.set_preset(msg.legacy_away ? climate::CLIMATE_PRESET_AWAY : climate::CLIMATE_PRESET_HOME);
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
  msg.name = number->get_name();
  msg.unique_id = get_default_unique_id("number", number);
  msg.icon = number->traits.get_icon();
  msg.disabled_by_default = number->is_disabled_by_default();

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
  msg.name = select->get_name();
  msg.unique_id = get_default_unique_id("select", select);
  msg.icon = select->traits.get_icon();
  msg.disabled_by_default = select->is_disabled_by_default();

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

#ifdef USE_ESP32_CAMERA
void APIConnection::send_camera_state(std::shared_ptr<esp32_camera::CameraImage> image) {
  if (!this->state_subscription_)
    return;
  if (this->image_reader_.available())
    return;
  this->image_reader_.set_image(image);
}
bool APIConnection::send_camera_info(esp32_camera::ESP32Camera *camera) {
  ListEntitiesCameraResponse msg;
  msg.key = camera->get_object_id_hash();
  msg.object_id = camera->get_object_id();
  msg.name = camera->get_name();
  msg.unique_id = get_default_unique_id("camera", camera);
  msg.disabled_by_default = camera->is_disabled_by_default();
  return this->send_list_entities_camera_response(msg);
}
void APIConnection::camera_image(const CameraImageRequest &msg) {
  if (esp32_camera::global_esp32_camera == nullptr)
    return;

  if (msg.single)
    esp32_camera::global_esp32_camera->request_image();
  if (msg.stream)
    esp32_camera::global_esp32_camera->request_stream();
}
#endif

#ifdef USE_HOMEASSISTANT_TIME
void APIConnection::on_get_time_response(const GetTimeResponse &value) {
  if (homeassistant::global_homeassistant_time != nullptr)
    homeassistant::global_homeassistant_time->set_epoch_time(value.epoch_seconds);
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
  bool success = this->send_buffer(buffer, 29);
  if (!success) {
    buffer = this->create_buffer();
    // bool send_failed = 4;
    buffer.encode_bool(4, true);
    return this->send_buffer(buffer, 29);
  } else {
    return true;
  }
}

HelloResponse APIConnection::hello(const HelloRequest &msg) {
  this->client_info_ = msg.client_info + " (" + this->client_->remoteIP().toString().c_str();
  this->client_info_ += ")";
  ESP_LOGV(TAG, "Hello from client: '%s'", this->client_info_.c_str());

  HelloResponse resp;
  resp.api_version_major = 1;
  resp.api_version_minor = 6;
  resp.server_info = App.get_name() + " (esphome v" ESPHOME_VERSION ")";
  this->connection_state_ = ConnectionState::CONNECTED;
  return resp;
}
ConnectResponse APIConnection::connect(const ConnectRequest &msg) {
  bool correct = this->parent_->check_password(msg.password);

  ConnectResponse resp;
  // bool invalid_password = 1;
  resp.invalid_password = !correct;
  if (correct) {
    ESP_LOGD(TAG, "Client '%s' connected successfully!", this->client_info_.c_str());
    this->connection_state_ = ConnectionState::AUTHENTICATED;

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
  resp.mac_address = get_mac_address_pretty();
  resp.esphome_version = ESPHOME_VERSION;
  resp.compilation_time = App.get_compilation_time();
#ifdef ARDUINO_BOARD
  resp.model = ARDUINO_BOARD;
#endif
#ifdef USE_DEEP_SLEEP
  resp.has_deep_sleep = deep_sleep::global_has_deep_sleep;
#endif
#ifdef ESPHOME_PROJECT_NAME
  resp.project_name = ESPHOME_PROJECT_NAME;
  resp.project_version = ESPHOME_PROJECT_VERSION;
#endif
  return resp;
}
void APIConnection::on_home_assistant_state_response(const HomeAssistantStateResponse &msg) {
  for (auto &it : this->parent_->get_state_subs())
    if (it.entity_id == msg.entity_id && it.attribute.value() == msg.attribute) {
      it.callback(msg.state);
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
  for (auto &it : this->parent_->get_state_subs()) {
    SubscribeHomeAssistantStateResponse resp;
    resp.entity_id = it.entity_id;
    resp.attribute = it.attribute.value();
    if (!this->send_subscribe_home_assistant_state_response(resp)) {
      this->on_fatal_error();
      return;
    }
  }
}
bool APIConnection::send_buffer(ProtoWriteBuffer buffer, uint32_t message_type) {
  if (this->remove_)
    return false;

  std::vector<uint8_t> header;
  header.push_back(0x00);
  ProtoVarInt(buffer.get_buffer()->size()).encode(header);
  ProtoVarInt(message_type).encode(header);

  size_t needed_space = buffer.get_buffer()->size() + header.size();

  if (needed_space > this->client_->space()) {
    delay(0);
    if (needed_space > this->client_->space()) {
      // SubscribeLogsResponse
      if (message_type != 29) {
        ESP_LOGV(TAG, "Cannot send message because of TCP buffer space");
      }
      delay(0);
      return false;
    }
  }

  this->client_->add(reinterpret_cast<char *>(header.data()), header.size(),
                     ASYNC_WRITE_FLAG_COPY | ASYNC_WRITE_FLAG_MORE);
  this->client_->add(reinterpret_cast<char *>(buffer.get_buffer()->data()), buffer.get_buffer()->size(),
                     ASYNC_WRITE_FLAG_COPY);
  bool ret = this->client_->send();
  return ret;
}
void APIConnection::on_unauthenticated_access() {
  ESP_LOGD(TAG, "'%s' tried to access without authentication.", this->client_info_.c_str());
  this->on_fatal_error();
}
void APIConnection::on_no_setup_connection() {
  ESP_LOGD(TAG, "'%s' tried to access without full connection.", this->client_info_.c_str());
  this->on_fatal_error();
}
void APIConnection::on_fatal_error() {
  ESP_LOGV(TAG, "Error: Disconnecting %s", this->client_info_.c_str());
  this->client_->close();
  this->remove_ = true;
}

}  // namespace api
}  // namespace esphome
