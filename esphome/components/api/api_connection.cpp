#include "api_connection.h"
#ifdef USE_API
#include <cerrno>
#include <cinttypes>
#include <utility>
#include <functional>
#include <limits>
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
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

// Read a maximum of 5 messages per loop iteration to prevent starving other components.
// This is a balance between API responsiveness and allowing other components to run.
// Since each message could contain multiple protobuf messages when using packet batching,
// this limits the number of messages processed, not the number of TCP packets.
static constexpr uint8_t MAX_MESSAGES_PER_LOOP = 5;
static constexpr uint8_t MAX_PING_RETRIES = 60;
static constexpr uint16_t PING_RETRY_INTERVAL = 1000;
static constexpr uint32_t KEEPALIVE_DISCONNECT_TIMEOUT = (KEEPALIVE_TIMEOUT_MS * 5) / 2;

static const char *const TAG = "api.connection";
#ifdef USE_CAMERA
static const int CAMERA_STOP_STREAM = 5000;
#endif

// Helper macro for entity command handlers - gets entity by key, returns if not found, and creates call object
#define ENTITY_COMMAND_MAKE_CALL(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key); \
  if ((entity_var) == nullptr) \
    return; \
  auto call = (entity_var)->make_call();

// Helper macro for entity command handlers that don't use make_call() - gets entity by key and returns if not found
#define ENTITY_COMMAND_GET(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key); \
  if ((entity_var) == nullptr) \
    return;

APIConnection::APIConnection(std::unique_ptr<socket::Socket> sock, APIServer *parent)
    : parent_(parent), initial_state_iterator_(this), list_entities_iterator_(this) {
#if defined(USE_API_PLAINTEXT) && defined(USE_API_NOISE)
  auto noise_ctx = parent->get_noise_ctx();
  if (noise_ctx->has_psk()) {
    this->helper_ = std::unique_ptr<APIFrameHelper>{new APINoiseFrameHelper(std::move(sock), noise_ctx)};
  } else {
    this->helper_ = std::unique_ptr<APIFrameHelper>{new APIPlaintextFrameHelper(std::move(sock))};
  }
#elif defined(USE_API_PLAINTEXT)
  this->helper_ = std::unique_ptr<APIFrameHelper>{new APIPlaintextFrameHelper(std::move(sock))};
#elif defined(USE_API_NOISE)
  this->helper_ = std::unique_ptr<APIFrameHelper>{new APINoiseFrameHelper(std::move(sock), parent->get_noise_ctx())};
#else
#error "No frame helper defined"
#endif
#ifdef USE_CAMERA
  if (camera::Camera::instance() != nullptr) {
    this->image_reader_ = std::unique_ptr<camera::CameraImageReader>{camera::Camera::instance()->create_image_reader()};
  }
#endif
}

uint32_t APIConnection::get_batch_delay_ms_() const { return this->parent_->get_batch_delay(); }

void APIConnection::start() {
  this->last_traffic_ = App.get_loop_component_start_time();

  APIError err = this->helper_->init();
  if (err != APIError::OK) {
    on_fatal_error();
    ESP_LOGW(TAG, "%s: Helper init failed: %s errno=%d", this->get_client_combined_info().c_str(),
             api_error_to_str(err), errno);
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
  if (this->flags_.next_close) {
    // requested a disconnect
    this->helper_->close();
    this->flags_.remove = true;
    return;
  }

  APIError err = this->helper_->loop();
  if (err != APIError::OK) {
    on_fatal_error();
    ESP_LOGW(TAG, "%s: Socket operation failed: %s errno=%d", this->get_client_combined_info().c_str(),
             api_error_to_str(err), errno);
    return;
  }

  const uint32_t now = App.get_loop_component_start_time();
  // Check if socket has data ready before attempting to read
  if (this->helper_->is_socket_ready()) {
    // Read up to MAX_MESSAGES_PER_LOOP messages per loop to improve throughput
    for (uint8_t message_count = 0; message_count < MAX_MESSAGES_PER_LOOP; message_count++) {
      ReadPacketBuffer buffer;
      err = this->helper_->read_packet(&buffer);
      if (err == APIError::WOULD_BLOCK) {
        // No more data available
        break;
      } else if (err != APIError::OK) {
        on_fatal_error();
        if (err == APIError::SOCKET_READ_FAILED && errno == ECONNRESET) {
          ESP_LOGW(TAG, "%s: Connection reset", this->get_client_combined_info().c_str());
        } else if (err == APIError::CONNECTION_CLOSED) {
          ESP_LOGW(TAG, "%s: Connection closed", this->get_client_combined_info().c_str());
        } else {
          ESP_LOGW(TAG, "%s: Reading failed: %s errno=%d", this->get_client_combined_info().c_str(),
                   api_error_to_str(err), errno);
        }
        return;
      } else {
        this->last_traffic_ = now;
        // read a packet
        if (buffer.data_len > 0) {
          this->read_message(buffer.data_len, buffer.type, &buffer.container[buffer.data_offset]);
        } else {
          this->read_message(0, buffer.type, nullptr);
        }
        if (this->flags_.remove)
          return;
      }
    }
  }

  // Process deferred batch if scheduled and timer has expired
  if (this->flags_.batch_scheduled && now - this->deferred_batch_.batch_start_time >= this->get_batch_delay_ms_()) {
    this->process_batch_();
  }

  if (!this->list_entities_iterator_.completed()) {
    this->process_iterator_batch_(this->list_entities_iterator_);
  } else if (!this->initial_state_iterator_.completed()) {
    this->process_iterator_batch_(this->initial_state_iterator_);

    // If we've completed initial states, process any remaining and clear the flag
    if (this->initial_state_iterator_.completed()) {
      // Process any remaining batched messages immediately
      if (!this->deferred_batch_.empty()) {
        this->process_batch_();
      }
      // Now that everything is sent, enable immediate sending for future state changes
      this->flags_.should_try_send_immediately = true;
    }
  }

  if (this->flags_.sent_ping) {
    // Disconnect if not responded within 2.5*keepalive
    if (now - this->last_traffic_ > KEEPALIVE_DISCONNECT_TIMEOUT) {
      on_fatal_error();
      ESP_LOGW(TAG, "%s is unresponsive; disconnecting", this->get_client_combined_info().c_str());
    }
  } else if (now - this->last_traffic_ > KEEPALIVE_TIMEOUT_MS) {
    ESP_LOGVV(TAG, "Sending keepalive PING");
    this->flags_.sent_ping = this->send_message(PingRequest());
    if (!this->flags_.sent_ping) {
      // If we can't send the ping request directly (tx_buffer full),
      // schedule it at the front of the batch so it will be sent with priority
      ESP_LOGW(TAG, "Buffer full, ping queued");
      this->schedule_message_front_(nullptr, &APIConnection::try_send_ping_request, PingRequest::MESSAGE_TYPE,
                                    PingRequest::ESTIMATED_SIZE);
      this->flags_.sent_ping = true;  // Mark as sent to avoid scheduling multiple pings
    }
  }

#ifdef USE_CAMERA
  if (this->image_reader_ && this->image_reader_->available() && this->helper_->can_write_without_blocking()) {
    uint32_t to_send = std::min((size_t) MAX_BATCH_PACKET_SIZE, this->image_reader_->available());
    bool done = this->image_reader_->available() == to_send;
    uint32_t msg_size = 0;
    ProtoSize::add_fixed_field<4>(msg_size, 1, true);
    // partial message size calculated manually since its a special case
    // 1 for the data field, varint for the data size, and the data itself
    msg_size += 1 + ProtoSize::varint(to_send) + to_send;
    ProtoSize::add_bool_field(msg_size, 1, done);

    auto buffer = this->create_buffer(msg_size);
    // fixed32 key = 1;
    buffer.encode_fixed32(1, camera::Camera::instance()->get_object_id_hash());
    // bytes data = 2;
    buffer.encode_bytes(2, this->image_reader_->peek_data_buffer(), to_send);
    // bool done = 3;
    buffer.encode_bool(3, done);

    bool success = this->send_buffer(buffer, CameraImageResponse::MESSAGE_TYPE);

    if (success) {
      this->image_reader_->consume_data(to_send);
      if (done) {
        this->image_reader_->return_image();
      }
    }
  }
#endif

  if (state_subs_at_ >= 0) {
    const auto &subs = this->parent_->get_state_subs();
    if (state_subs_at_ < static_cast<int>(subs.size())) {
      auto &it = subs[state_subs_at_];
      SubscribeHomeAssistantStateResponse resp;
      resp.entity_id = it.entity_id;
      resp.attribute = it.attribute.value();
      resp.once = it.once;
      if (this->send_message(resp)) {
        state_subs_at_++;
      }
    } else {
      state_subs_at_ = -1;
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
  ESP_LOGD(TAG, "%s disconnected", this->get_client_combined_info().c_str());
  this->flags_.next_close = true;
  DisconnectResponse resp;
  return resp;
}
void APIConnection::on_disconnect_response(const DisconnectResponse &value) {
  this->helper_->close();
  this->flags_.remove = true;
}

// Encodes a message to the buffer and returns the total number of bytes used,
// including header and footer overhead. Returns 0 if the message doesn't fit.
uint16_t APIConnection::encode_message_to_buffer(ProtoMessage &msg, uint8_t message_type, APIConnection *conn,
                                                 uint32_t remaining_size, bool is_single) {
#ifdef HAS_PROTO_MESSAGE_DUMP
  // If in log-only mode, just log and return
  if (conn->flags_.log_only_mode) {
    conn->log_send_message_(msg.message_name(), msg.dump());
    return 1;  // Return non-zero to indicate "success" for logging
  }
#endif

  // Calculate size
  uint32_t calculated_size = 0;
  msg.calculate_size(calculated_size);

  // Cache frame sizes to avoid repeated virtual calls
  const uint8_t header_padding = conn->helper_->frame_header_padding();
  const uint8_t footer_size = conn->helper_->frame_footer_size();

  // Calculate total size with padding for buffer allocation
  size_t total_calculated_size = calculated_size + header_padding + footer_size;

  // Check if it fits
  if (total_calculated_size > remaining_size) {
    return 0;  // Doesn't fit
  }

  // Allocate buffer space - pass payload size, allocation functions add header/footer space
  ProtoWriteBuffer buffer = is_single ? conn->allocate_single_message_buffer(calculated_size)
                                      : conn->allocate_batch_message_buffer(calculated_size);

  // Get buffer size after allocation (which includes header padding)
  std::vector<uint8_t> &shared_buf = conn->parent_->get_shared_buffer_ref();
  size_t size_before_encode = shared_buf.size();

  // Encode directly into buffer
  msg.encode(buffer);

  // Calculate actual encoded size (not including header that was already added)
  size_t actual_payload_size = shared_buf.size() - size_before_encode;

  // Return actual total size (header + actual payload + footer)
  size_t actual_total_size = header_padding + actual_payload_size + footer_size;

  // Verify that calculate_size() returned the correct value
  assert(calculated_size == actual_payload_size);
  return static_cast<uint16_t>(actual_total_size);
}

#ifdef USE_BINARY_SENSOR
bool APIConnection::send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor) {
  return this->send_message_smart_(binary_sensor, &APIConnection::try_send_binary_sensor_state,
                                   BinarySensorStateResponse::MESSAGE_TYPE, BinarySensorStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_binary_sensor_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                     bool is_single) {
  auto *binary_sensor = static_cast<binary_sensor::BinarySensor *>(entity);
  BinarySensorStateResponse resp;
  resp.state = binary_sensor->state;
  resp.missing_state = !binary_sensor->has_state();
  fill_entity_state_base(binary_sensor, resp);
  return encode_message_to_buffer(resp, BinarySensorStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_binary_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                    bool is_single) {
  auto *binary_sensor = static_cast<binary_sensor::BinarySensor *>(entity);
  ListEntitiesBinarySensorResponse msg;
  msg.device_class = binary_sensor->get_device_class();
  msg.is_status_binary_sensor = binary_sensor->is_status_binary_sensor();
  msg.unique_id = get_default_unique_id("binary_sensor", binary_sensor);
  fill_entity_info_base(binary_sensor, msg);
  return encode_message_to_buffer(msg, ListEntitiesBinarySensorResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
#endif

#ifdef USE_COVER
bool APIConnection::send_cover_state(cover::Cover *cover) {
  return this->send_message_smart_(cover, &APIConnection::try_send_cover_state, CoverStateResponse::MESSAGE_TYPE,
                                   CoverStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_cover_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *cover = static_cast<cover::Cover *>(entity);
  CoverStateResponse msg;
  auto traits = cover->get_traits();
  msg.legacy_state =
      (cover->position == cover::COVER_OPEN) ? enums::LEGACY_COVER_STATE_OPEN : enums::LEGACY_COVER_STATE_CLOSED;
  msg.position = cover->position;
  if (traits.get_supports_tilt())
    msg.tilt = cover->tilt;
  msg.current_operation = static_cast<enums::CoverOperation>(cover->current_operation);
  fill_entity_state_base(cover, msg);
  return encode_message_to_buffer(msg, CoverStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_cover_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *cover = static_cast<cover::Cover *>(entity);
  ListEntitiesCoverResponse msg;
  auto traits = cover->get_traits();
  msg.assumed_state = traits.get_is_assumed_state();
  msg.supports_position = traits.get_supports_position();
  msg.supports_tilt = traits.get_supports_tilt();
  msg.supports_stop = traits.get_supports_stop();
  msg.device_class = cover->get_device_class();
  msg.unique_id = get_default_unique_id("cover", cover);
  fill_entity_info_base(cover, msg);
  return encode_message_to_buffer(msg, ListEntitiesCoverResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::cover_command(const CoverCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(cover::Cover, cover, cover)
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
  return this->send_message_smart_(fan, &APIConnection::try_send_fan_state, FanStateResponse::MESSAGE_TYPE,
                                   FanStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_fan_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                           bool is_single) {
  auto *fan = static_cast<fan::Fan *>(entity);
  FanStateResponse msg;
  auto traits = fan->get_traits();
  msg.state = fan->state;
  if (traits.supports_oscillation())
    msg.oscillating = fan->oscillating;
  if (traits.supports_speed()) {
    msg.speed_level = fan->speed;
  }
  if (traits.supports_direction())
    msg.direction = static_cast<enums::FanDirection>(fan->direction);
  if (traits.supports_preset_modes())
    msg.preset_mode = fan->preset_mode;
  fill_entity_state_base(fan, msg);
  return encode_message_to_buffer(msg, FanStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_fan_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                          bool is_single) {
  auto *fan = static_cast<fan::Fan *>(entity);
  ListEntitiesFanResponse msg;
  auto traits = fan->get_traits();
  msg.supports_oscillation = traits.supports_oscillation();
  msg.supports_speed = traits.supports_speed();
  msg.supports_direction = traits.supports_direction();
  msg.supported_speed_count = traits.supported_speed_count();
  for (auto const &preset : traits.supported_preset_modes())
    msg.supported_preset_modes.push_back(preset);
  msg.unique_id = get_default_unique_id("fan", fan);
  fill_entity_info_base(fan, msg);
  return encode_message_to_buffer(msg, ListEntitiesFanResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::fan_command(const FanCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(fan::Fan, fan, fan)
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
  return this->send_message_smart_(light, &APIConnection::try_send_light_state, LightStateResponse::MESSAGE_TYPE,
                                   LightStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_light_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *light = static_cast<light::LightState *>(entity);
  LightStateResponse resp;
  auto traits = light->get_traits();
  auto values = light->remote_values;
  auto color_mode = values.get_color_mode();
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
  fill_entity_state_base(light, resp);
  return encode_message_to_buffer(resp, LightStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_light_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *light = static_cast<light::LightState *>(entity);
  ListEntitiesLightResponse msg;
  auto traits = light->get_traits();
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
    for (auto *effect : light->get_effects()) {
      msg.effects.push_back(effect->get_name());
    }
  }
  msg.unique_id = get_default_unique_id("light", light);
  fill_entity_info_base(light, msg);
  return encode_message_to_buffer(msg, ListEntitiesLightResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::light_command(const LightCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(light::LightState, light, light)
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
bool APIConnection::send_sensor_state(sensor::Sensor *sensor) {
  return this->send_message_smart_(sensor, &APIConnection::try_send_sensor_state, SensorStateResponse::MESSAGE_TYPE,
                                   SensorStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_sensor_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single) {
  auto *sensor = static_cast<sensor::Sensor *>(entity);
  SensorStateResponse resp;
  resp.state = sensor->state;
  resp.missing_state = !sensor->has_state();
  fill_entity_state_base(sensor, resp);
  return encode_message_to_buffer(resp, SensorStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *sensor = static_cast<sensor::Sensor *>(entity);
  ListEntitiesSensorResponse msg;
  msg.unit_of_measurement = sensor->get_unit_of_measurement();
  msg.accuracy_decimals = sensor->get_accuracy_decimals();
  msg.force_update = sensor->get_force_update();
  msg.device_class = sensor->get_device_class();
  msg.state_class = static_cast<enums::SensorStateClass>(sensor->get_state_class());
  msg.unique_id = sensor->unique_id();
  if (msg.unique_id.empty())
    msg.unique_id = get_default_unique_id("sensor", sensor);
  fill_entity_info_base(sensor, msg);
  return encode_message_to_buffer(msg, ListEntitiesSensorResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
#endif

#ifdef USE_SWITCH
bool APIConnection::send_switch_state(switch_::Switch *a_switch) {
  return this->send_message_smart_(a_switch, &APIConnection::try_send_switch_state, SwitchStateResponse::MESSAGE_TYPE,
                                   SwitchStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_switch_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single) {
  auto *a_switch = static_cast<switch_::Switch *>(entity);
  SwitchStateResponse resp;
  resp.state = a_switch->state;
  fill_entity_state_base(a_switch, resp);
  return encode_message_to_buffer(resp, SwitchStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_switch_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *a_switch = static_cast<switch_::Switch *>(entity);
  ListEntitiesSwitchResponse msg;
  msg.assumed_state = a_switch->assumed_state();
  msg.device_class = a_switch->get_device_class();
  msg.unique_id = get_default_unique_id("switch", a_switch);
  fill_entity_info_base(a_switch, msg);
  return encode_message_to_buffer(msg, ListEntitiesSwitchResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::switch_command(const SwitchCommandRequest &msg) {
  ENTITY_COMMAND_GET(switch_::Switch, a_switch, switch)

  if (msg.state) {
    a_switch->turn_on();
  } else {
    a_switch->turn_off();
  }
}
#endif

#ifdef USE_TEXT_SENSOR
bool APIConnection::send_text_sensor_state(text_sensor::TextSensor *text_sensor) {
  return this->send_message_smart_(text_sensor, &APIConnection::try_send_text_sensor_state,
                                   TextSensorStateResponse::MESSAGE_TYPE, TextSensorStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_text_sensor_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                   bool is_single) {
  auto *text_sensor = static_cast<text_sensor::TextSensor *>(entity);
  TextSensorStateResponse resp;
  resp.state = text_sensor->state;
  resp.missing_state = !text_sensor->has_state();
  fill_entity_state_base(text_sensor, resp);
  return encode_message_to_buffer(resp, TextSensorStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_text_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                  bool is_single) {
  auto *text_sensor = static_cast<text_sensor::TextSensor *>(entity);
  ListEntitiesTextSensorResponse msg;
  msg.device_class = text_sensor->get_device_class();
  msg.unique_id = text_sensor->unique_id();
  if (msg.unique_id.empty())
    msg.unique_id = get_default_unique_id("text_sensor", text_sensor);
  fill_entity_info_base(text_sensor, msg);
  return encode_message_to_buffer(msg, ListEntitiesTextSensorResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
#endif

#ifdef USE_CLIMATE
bool APIConnection::send_climate_state(climate::Climate *climate) {
  return this->send_message_smart_(climate, &APIConnection::try_send_climate_state, ClimateStateResponse::MESSAGE_TYPE,
                                   ClimateStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_climate_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                               bool is_single) {
  auto *climate = static_cast<climate::Climate *>(entity);
  ClimateStateResponse resp;
  fill_entity_state_base(climate, resp);
  auto traits = climate->get_traits();
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
  return encode_message_to_buffer(resp, ClimateStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_climate_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single) {
  auto *climate = static_cast<climate::Climate *>(entity);
  ListEntitiesClimateResponse msg;
  auto traits = climate->get_traits();
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
  msg.unique_id = get_default_unique_id("climate", climate);
  fill_entity_info_base(climate, msg);
  return encode_message_to_buffer(msg, ListEntitiesClimateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::climate_command(const ClimateCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(climate::Climate, climate, climate)
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
bool APIConnection::send_number_state(number::Number *number) {
  return this->send_message_smart_(number, &APIConnection::try_send_number_state, NumberStateResponse::MESSAGE_TYPE,
                                   NumberStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_number_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single) {
  auto *number = static_cast<number::Number *>(entity);
  NumberStateResponse resp;
  resp.state = number->state;
  resp.missing_state = !number->has_state();
  fill_entity_state_base(number, resp);
  return encode_message_to_buffer(resp, NumberStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_number_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *number = static_cast<number::Number *>(entity);
  ListEntitiesNumberResponse msg;
  msg.unit_of_measurement = number->traits.get_unit_of_measurement();
  msg.mode = static_cast<enums::NumberMode>(number->traits.get_mode());
  msg.device_class = number->traits.get_device_class();
  msg.min_value = number->traits.get_min_value();
  msg.max_value = number->traits.get_max_value();
  msg.step = number->traits.get_step();
  msg.unique_id = get_default_unique_id("number", number);
  fill_entity_info_base(number, msg);
  return encode_message_to_buffer(msg, ListEntitiesNumberResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::number_command(const NumberCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(number::Number, number, number)
  call.set_value(msg.state);
  call.perform();
}
#endif

#ifdef USE_DATETIME_DATE
bool APIConnection::send_date_state(datetime::DateEntity *date) {
  return this->send_message_smart_(date, &APIConnection::try_send_date_state, DateStateResponse::MESSAGE_TYPE,
                                   DateStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_date_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *date = static_cast<datetime::DateEntity *>(entity);
  DateStateResponse resp;
  resp.missing_state = !date->has_state();
  resp.year = date->year;
  resp.month = date->month;
  resp.day = date->day;
  fill_entity_state_base(date, resp);
  return encode_message_to_buffer(resp, DateStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_date_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                           bool is_single) {
  auto *date = static_cast<datetime::DateEntity *>(entity);
  ListEntitiesDateResponse msg;
  msg.unique_id = get_default_unique_id("date", date);
  fill_entity_info_base(date, msg);
  return encode_message_to_buffer(msg, ListEntitiesDateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::date_command(const DateCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(datetime::DateEntity, date, date)
  call.set_date(msg.year, msg.month, msg.day);
  call.perform();
}
#endif

#ifdef USE_DATETIME_TIME
bool APIConnection::send_time_state(datetime::TimeEntity *time) {
  return this->send_message_smart_(time, &APIConnection::try_send_time_state, TimeStateResponse::MESSAGE_TYPE,
                                   TimeStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_time_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *time = static_cast<datetime::TimeEntity *>(entity);
  TimeStateResponse resp;
  resp.missing_state = !time->has_state();
  resp.hour = time->hour;
  resp.minute = time->minute;
  resp.second = time->second;
  fill_entity_state_base(time, resp);
  return encode_message_to_buffer(resp, TimeStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_time_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                           bool is_single) {
  auto *time = static_cast<datetime::TimeEntity *>(entity);
  ListEntitiesTimeResponse msg;
  msg.unique_id = get_default_unique_id("time", time);
  fill_entity_info_base(time, msg);
  return encode_message_to_buffer(msg, ListEntitiesTimeResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::time_command(const TimeCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(datetime::TimeEntity, time, time)
  call.set_time(msg.hour, msg.minute, msg.second);
  call.perform();
}
#endif

#ifdef USE_DATETIME_DATETIME
bool APIConnection::send_datetime_state(datetime::DateTimeEntity *datetime) {
  return this->send_message_smart_(datetime, &APIConnection::try_send_datetime_state,
                                   DateTimeStateResponse::MESSAGE_TYPE, DateTimeStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_datetime_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                bool is_single) {
  auto *datetime = static_cast<datetime::DateTimeEntity *>(entity);
  DateTimeStateResponse resp;
  resp.missing_state = !datetime->has_state();
  if (datetime->has_state()) {
    ESPTime state = datetime->state_as_esptime();
    resp.epoch_seconds = state.timestamp;
  }
  fill_entity_state_base(datetime, resp);
  return encode_message_to_buffer(resp, DateTimeStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_datetime_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                               bool is_single) {
  auto *datetime = static_cast<datetime::DateTimeEntity *>(entity);
  ListEntitiesDateTimeResponse msg;
  msg.unique_id = get_default_unique_id("datetime", datetime);
  fill_entity_info_base(datetime, msg);
  return encode_message_to_buffer(msg, ListEntitiesDateTimeResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::datetime_command(const DateTimeCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(datetime::DateTimeEntity, datetime, datetime)
  call.set_datetime(msg.epoch_seconds);
  call.perform();
}
#endif

#ifdef USE_TEXT
bool APIConnection::send_text_state(text::Text *text) {
  return this->send_message_smart_(text, &APIConnection::try_send_text_state, TextStateResponse::MESSAGE_TYPE,
                                   TextStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_text_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *text = static_cast<text::Text *>(entity);
  TextStateResponse resp;
  resp.state = text->state;
  resp.missing_state = !text->has_state();
  fill_entity_state_base(text, resp);
  return encode_message_to_buffer(resp, TextStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_text_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                           bool is_single) {
  auto *text = static_cast<text::Text *>(entity);
  ListEntitiesTextResponse msg;
  msg.mode = static_cast<enums::TextMode>(text->traits.get_mode());
  msg.min_length = text->traits.get_min_length();
  msg.max_length = text->traits.get_max_length();
  msg.pattern = text->traits.get_pattern();
  msg.unique_id = get_default_unique_id("text", text);
  fill_entity_info_base(text, msg);
  return encode_message_to_buffer(msg, ListEntitiesTextResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::text_command(const TextCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(text::Text, text, text)
  call.set_value(msg.state);
  call.perform();
}
#endif

#ifdef USE_SELECT
bool APIConnection::send_select_state(select::Select *select) {
  return this->send_message_smart_(select, &APIConnection::try_send_select_state, SelectStateResponse::MESSAGE_TYPE,
                                   SelectStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_select_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single) {
  auto *select = static_cast<select::Select *>(entity);
  SelectStateResponse resp;
  resp.state = select->state;
  resp.missing_state = !select->has_state();
  fill_entity_state_base(select, resp);
  return encode_message_to_buffer(resp, SelectStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_select_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *select = static_cast<select::Select *>(entity);
  ListEntitiesSelectResponse msg;
  for (const auto &option : select->traits.get_options())
    msg.options.push_back(option);
  msg.unique_id = get_default_unique_id("select", select);
  fill_entity_info_base(select, msg);
  return encode_message_to_buffer(msg, ListEntitiesSelectResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::select_command(const SelectCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(select::Select, select, select)
  call.set_option(msg.state);
  call.perform();
}
#endif

#ifdef USE_BUTTON
uint16_t APIConnection::try_send_button_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *button = static_cast<button::Button *>(entity);
  ListEntitiesButtonResponse msg;
  msg.device_class = button->get_device_class();
  msg.unique_id = get_default_unique_id("button", button);
  fill_entity_info_base(button, msg);
  return encode_message_to_buffer(msg, ListEntitiesButtonResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void esphome::api::APIConnection::button_command(const ButtonCommandRequest &msg) {
  ENTITY_COMMAND_GET(button::Button, button, button)
  button->press();
}
#endif

#ifdef USE_LOCK
bool APIConnection::send_lock_state(lock::Lock *a_lock) {
  return this->send_message_smart_(a_lock, &APIConnection::try_send_lock_state, LockStateResponse::MESSAGE_TYPE,
                                   LockStateResponse::ESTIMATED_SIZE);
}

uint16_t APIConnection::try_send_lock_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *a_lock = static_cast<lock::Lock *>(entity);
  LockStateResponse resp;
  resp.state = static_cast<enums::LockState>(a_lock->state);
  fill_entity_state_base(a_lock, resp);
  return encode_message_to_buffer(resp, LockStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_lock_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                           bool is_single) {
  auto *a_lock = static_cast<lock::Lock *>(entity);
  ListEntitiesLockResponse msg;
  msg.assumed_state = a_lock->traits.get_assumed_state();
  msg.supports_open = a_lock->traits.get_supports_open();
  msg.requires_code = a_lock->traits.get_requires_code();
  msg.unique_id = get_default_unique_id("lock", a_lock);
  fill_entity_info_base(a_lock, msg);
  return encode_message_to_buffer(msg, ListEntitiesLockResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::lock_command(const LockCommandRequest &msg) {
  ENTITY_COMMAND_GET(lock::Lock, a_lock, lock)

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
  return this->send_message_smart_(valve, &APIConnection::try_send_valve_state, ValveStateResponse::MESSAGE_TYPE,
                                   ValveStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_valve_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *valve = static_cast<valve::Valve *>(entity);
  ValveStateResponse resp;
  resp.position = valve->position;
  resp.current_operation = static_cast<enums::ValveOperation>(valve->current_operation);
  fill_entity_state_base(valve, resp);
  return encode_message_to_buffer(resp, ValveStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_valve_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *valve = static_cast<valve::Valve *>(entity);
  ListEntitiesValveResponse msg;
  auto traits = valve->get_traits();
  msg.device_class = valve->get_device_class();
  msg.assumed_state = traits.get_is_assumed_state();
  msg.supports_position = traits.get_supports_position();
  msg.supports_stop = traits.get_supports_stop();
  msg.unique_id = get_default_unique_id("valve", valve);
  fill_entity_info_base(valve, msg);
  return encode_message_to_buffer(msg, ListEntitiesValveResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::valve_command(const ValveCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(valve::Valve, valve, valve)
  if (msg.has_position)
    call.set_position(msg.position);
  if (msg.stop)
    call.set_command_stop();
  call.perform();
}
#endif

#ifdef USE_MEDIA_PLAYER
bool APIConnection::send_media_player_state(media_player::MediaPlayer *media_player) {
  return this->send_message_smart_(media_player, &APIConnection::try_send_media_player_state,
                                   MediaPlayerStateResponse::MESSAGE_TYPE, MediaPlayerStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_media_player_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                    bool is_single) {
  auto *media_player = static_cast<media_player::MediaPlayer *>(entity);
  MediaPlayerStateResponse resp;
  media_player::MediaPlayerState report_state = media_player->state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING
                                                    ? media_player::MEDIA_PLAYER_STATE_PLAYING
                                                    : media_player->state;
  resp.state = static_cast<enums::MediaPlayerState>(report_state);
  resp.volume = media_player->volume;
  resp.muted = media_player->is_muted();
  fill_entity_state_base(media_player, resp);
  return encode_message_to_buffer(resp, MediaPlayerStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_media_player_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                   bool is_single) {
  auto *media_player = static_cast<media_player::MediaPlayer *>(entity);
  ListEntitiesMediaPlayerResponse msg;
  auto traits = media_player->get_traits();
  msg.supports_pause = traits.get_supports_pause();
  for (auto &supported_format : traits.get_supported_formats()) {
    MediaPlayerSupportedFormat media_format;
    media_format.format = supported_format.format;
    media_format.sample_rate = supported_format.sample_rate;
    media_format.num_channels = supported_format.num_channels;
    media_format.purpose = static_cast<enums::MediaPlayerFormatPurpose>(supported_format.purpose);
    media_format.sample_bytes = supported_format.sample_bytes;
    msg.supported_formats.push_back(media_format);
  }
  msg.unique_id = get_default_unique_id("media_player", media_player);
  fill_entity_info_base(media_player, msg);
  return encode_message_to_buffer(msg, ListEntitiesMediaPlayerResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::media_player_command(const MediaPlayerCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(media_player::MediaPlayer, media_player, media_player)
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

#ifdef USE_CAMERA
void APIConnection::set_camera_state(std::shared_ptr<camera::CameraImage> image) {
  if (!this->flags_.state_subscription)
    return;
  if (!this->image_reader_)
    return;
  if (this->image_reader_->available())
    return;
  if (image->was_requested_by(esphome::camera::API_REQUESTER) || image->was_requested_by(esphome::camera::IDLE))
    this->image_reader_->set_image(std::move(image));
}
uint16_t APIConnection::try_send_camera_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *camera = static_cast<camera::Camera *>(entity);
  ListEntitiesCameraResponse msg;
  msg.unique_id = get_default_unique_id("camera", camera);
  fill_entity_info_base(camera, msg);
  return encode_message_to_buffer(msg, ListEntitiesCameraResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::camera_image(const CameraImageRequest &msg) {
  if (camera::Camera::instance() == nullptr)
    return;

  if (msg.single)
    camera::Camera::instance()->request_image(esphome::camera::API_REQUESTER);
  if (msg.stream) {
    camera::Camera::instance()->start_stream(esphome::camera::API_REQUESTER);

    App.scheduler.set_timeout(this->parent_, "api_camera_stop_stream", CAMERA_STOP_STREAM,
                              []() { camera::Camera::instance()->stop_stream(esphome::camera::API_REQUESTER); });
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
    return this->send_message(resp);
  }
  return this->send_message(msg);
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

void APIConnection::bluetooth_scanner_set_mode(const BluetoothScannerSetModeRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->bluetooth_scanner_set_mode(
      msg.mode == enums::BluetoothScannerMode::BLUETOOTH_SCANNER_MODE_ACTIVE);
}
#endif

#ifdef USE_VOICE_ASSISTANT
bool APIConnection::check_voice_assistant_api_connection_() const {
  return voice_assistant::global_voice_assistant != nullptr &&
         voice_assistant::global_voice_assistant->get_api_connection() == this;
}

void APIConnection::subscribe_voice_assistant(const SubscribeVoiceAssistantRequest &msg) {
  if (voice_assistant::global_voice_assistant != nullptr) {
    voice_assistant::global_voice_assistant->client_subscription(this, msg.subscribe);
  }
}
void APIConnection::on_voice_assistant_response(const VoiceAssistantResponse &msg) {
  if (!this->check_voice_assistant_api_connection_()) {
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
};
void APIConnection::on_voice_assistant_event_response(const VoiceAssistantEventResponse &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_event(msg);
  }
}
void APIConnection::on_voice_assistant_audio(const VoiceAssistantAudio &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_audio(msg);
  }
};
void APIConnection::on_voice_assistant_timer_event_response(const VoiceAssistantTimerEventResponse &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_timer_event(msg);
  }
};

void APIConnection::on_voice_assistant_announce_request(const VoiceAssistantAnnounceRequest &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_announce(msg);
  }
}

VoiceAssistantConfigurationResponse APIConnection::voice_assistant_get_configuration(
    const VoiceAssistantConfigurationRequest &msg) {
  VoiceAssistantConfigurationResponse resp;
  if (!this->check_voice_assistant_api_connection_()) {
    return resp;
  }

  auto &config = voice_assistant::global_voice_assistant->get_configuration();
  for (auto &wake_word : config.available_wake_words) {
    VoiceAssistantWakeWord resp_wake_word;
    resp_wake_word.id = wake_word.id;
    resp_wake_word.wake_word = wake_word.wake_word;
    for (const auto &lang : wake_word.trained_languages) {
      resp_wake_word.trained_languages.push_back(lang);
    }
    resp.available_wake_words.push_back(std::move(resp_wake_word));
  }
  for (auto &wake_word_id : config.active_wake_words) {
    resp.active_wake_words.push_back(wake_word_id);
  }
  resp.max_active_wake_words = config.max_active_wake_words;
  return resp;
}

void APIConnection::voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_set_configuration(msg.active_wake_words);
  }
}

#endif

#ifdef USE_ALARM_CONTROL_PANEL
bool APIConnection::send_alarm_control_panel_state(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel) {
  return this->send_message_smart_(a_alarm_control_panel, &APIConnection::try_send_alarm_control_panel_state,
                                   AlarmControlPanelStateResponse::MESSAGE_TYPE,
                                   AlarmControlPanelStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_alarm_control_panel_state(EntityBase *entity, APIConnection *conn,
                                                           uint32_t remaining_size, bool is_single) {
  auto *a_alarm_control_panel = static_cast<alarm_control_panel::AlarmControlPanel *>(entity);
  AlarmControlPanelStateResponse resp;
  resp.state = static_cast<enums::AlarmControlPanelState>(a_alarm_control_panel->get_state());
  fill_entity_state_base(a_alarm_control_panel, resp);
  return encode_message_to_buffer(resp, AlarmControlPanelStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_alarm_control_panel_info(EntityBase *entity, APIConnection *conn,
                                                          uint32_t remaining_size, bool is_single) {
  auto *a_alarm_control_panel = static_cast<alarm_control_panel::AlarmControlPanel *>(entity);
  ListEntitiesAlarmControlPanelResponse msg;
  msg.supported_features = a_alarm_control_panel->get_supported_features();
  msg.requires_code = a_alarm_control_panel->get_requires_code();
  msg.requires_code_to_arm = a_alarm_control_panel->get_requires_code_to_arm();
  msg.unique_id = get_default_unique_id("alarm_control_panel", a_alarm_control_panel);
  fill_entity_info_base(a_alarm_control_panel, msg);
  return encode_message_to_buffer(msg, ListEntitiesAlarmControlPanelResponse::MESSAGE_TYPE, conn, remaining_size,
                                  is_single);
}
void APIConnection::alarm_control_panel_command(const AlarmControlPanelCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(alarm_control_panel::AlarmControlPanel, a_alarm_control_panel, alarm_control_panel)
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
void APIConnection::send_event(event::Event *event, const std::string &event_type) {
  this->schedule_message_(event, MessageCreator(event_type), EventResponse::MESSAGE_TYPE,
                          EventResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_event_response(event::Event *event, const std::string &event_type, APIConnection *conn,
                                                uint32_t remaining_size, bool is_single) {
  EventResponse resp;
  resp.event_type = event_type;
  fill_entity_state_base(event, resp);
  return encode_message_to_buffer(resp, EventResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_event_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *event = static_cast<event::Event *>(entity);
  ListEntitiesEventResponse msg;
  msg.device_class = event->get_device_class();
  for (const auto &event_type : event->get_event_types())
    msg.event_types.push_back(event_type);
  msg.unique_id = get_default_unique_id("event", event);
  fill_entity_info_base(event, msg);
  return encode_message_to_buffer(msg, ListEntitiesEventResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
#endif

#ifdef USE_UPDATE
bool APIConnection::send_update_state(update::UpdateEntity *update) {
  return this->send_message_smart_(update, &APIConnection::try_send_update_state, UpdateStateResponse::MESSAGE_TYPE,
                                   UpdateStateResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_update_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single) {
  auto *update = static_cast<update::UpdateEntity *>(entity);
  UpdateStateResponse resp;
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
  fill_entity_state_base(update, resp);
  return encode_message_to_buffer(resp, UpdateStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_update_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *update = static_cast<update::UpdateEntity *>(entity);
  ListEntitiesUpdateResponse msg;
  msg.device_class = update->get_device_class();
  msg.unique_id = get_default_unique_id("update", update);
  fill_entity_info_base(update, msg);
  return encode_message_to_buffer(msg, ListEntitiesUpdateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
void APIConnection::update_command(const UpdateCommandRequest &msg) {
  ENTITY_COMMAND_GET(update::UpdateEntity, update, update)

  switch (msg.command) {
    case enums::UPDATE_COMMAND_UPDATE:
      update->perform();
      break;
    case enums::UPDATE_COMMAND_CHECK:
      update->check();
      break;
    case enums::UPDATE_COMMAND_NONE:
      ESP_LOGE(TAG, "UPDATE_COMMAND_NONE not handled; confirm command is correct");
      break;
    default:
      ESP_LOGW(TAG, "Unknown update command: %" PRIu32, msg.command);
      break;
  }
}
#endif

bool APIConnection::try_send_log_message(int level, const char *tag, const char *line, size_t message_len) {
  if (this->flags_.log_subscription < level)
    return false;

  // Pre-calculate message size to avoid reallocations
  uint32_t msg_size = 0;

  // Add size for level field (field ID 1, varint type)
  // 1 byte for field tag + size of the level varint
  msg_size += 1 + api::ProtoSize::varint(static_cast<uint32_t>(level));

  // Add size for string field (field ID 3, string type)
  // 1 byte for field tag + size of length varint + string length
  msg_size += 1 + api::ProtoSize::varint(static_cast<uint32_t>(message_len)) + message_len;

  // Create a pre-sized buffer
  auto buffer = this->create_buffer(msg_size);

  // Encode the message (SubscribeLogsResponse)
  buffer.encode_uint32(1, static_cast<uint32_t>(level));  // LogLevel level = 1
  buffer.encode_string(3, line, message_len);             // string message = 3

  // SubscribeLogsResponse - 29
  return this->send_buffer(buffer, SubscribeLogsResponse::MESSAGE_TYPE);
}

HelloResponse APIConnection::hello(const HelloRequest &msg) {
  this->client_info_ = msg.client_info;
  this->client_peername_ = this->helper_->getpeername();
  this->helper_->set_log_info(this->get_client_combined_info());
  this->client_api_version_major_ = msg.api_version_major;
  this->client_api_version_minor_ = msg.api_version_minor;
  ESP_LOGV(TAG, "Hello from client: '%s' | %s | API Version %" PRIu32 ".%" PRIu32, this->client_info_.c_str(),
           this->client_peername_.c_str(), this->client_api_version_major_, this->client_api_version_minor_);

  HelloResponse resp;
  resp.api_version_major = 1;
  resp.api_version_minor = 10;
  resp.server_info = App.get_name() + " (esphome v" ESPHOME_VERSION ")";
  resp.name = App.get_name();

  this->flags_.connection_state = static_cast<uint8_t>(ConnectionState::CONNECTED);
  return resp;
}
ConnectResponse APIConnection::connect(const ConnectRequest &msg) {
  bool correct = true;
#ifdef USE_API_PASSWORD
  correct = this->parent_->check_password(msg.password);
#endif

  ConnectResponse resp;
  // bool invalid_password = 1;
  resp.invalid_password = !correct;
  if (correct) {
    ESP_LOGD(TAG, "%s connected", this->get_client_combined_info().c_str());
    this->flags_.connection_state = static_cast<uint8_t>(ConnectionState::AUTHENTICATED);
#ifdef USE_API_CLIENT_CONNECTED_TRIGGER
    this->parent_->get_client_connected_trigger()->trigger(this->client_info_, this->client_peername_);
#endif
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
#ifdef USE_API_PASSWORD
  resp.uses_password = this->parent_->uses_password();
#else
  resp.uses_password = false;
#endif
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
#elif defined(USE_LN882X)
  resp.manufacturer = "Lightning";
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
  resp.bluetooth_mac_address = bluetooth_proxy::global_bluetooth_proxy->get_bluetooth_mac_address_pretty();
#endif
#ifdef USE_VOICE_ASSISTANT
  resp.legacy_voice_assistant_version = voice_assistant::global_voice_assistant->get_legacy_version();
  resp.voice_assistant_feature_flags = voice_assistant::global_voice_assistant->get_feature_flags();
#endif
#ifdef USE_API_NOISE
  resp.api_encryption_supported = true;
#endif
#ifdef USE_DEVICES
  for (auto const &device : App.get_devices()) {
    DeviceInfo device_info;
    device_info.device_id = device->get_device_id();
    device_info.name = device->get_name();
    device_info.area_id = device->get_area_id();
    resp.devices.push_back(device_info);
  }
#endif
#ifdef USE_AREAS
  for (auto const &area : App.get_areas()) {
    AreaInfo area_info;
    area_info.area_id = area->get_area_id();
    area_info.name = area->get_name();
    resp.areas.push_back(area_info);
  }
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
#ifdef USE_API_SERVICES
void APIConnection::execute_service(const ExecuteServiceRequest &msg) {
  bool found = false;
  for (auto *service : this->parent_->get_user_services()) {
    if (service->execute_service(msg)) {
      found = true;
    }
  }
  if (!found) {
    ESP_LOGV(TAG, "Could not find service");
  }
}
#endif
#ifdef USE_API_NOISE
NoiseEncryptionSetKeyResponse APIConnection::noise_encryption_set_key(const NoiseEncryptionSetKeyRequest &msg) {
  psk_t psk{};
  NoiseEncryptionSetKeyResponse resp;
  if (base64_decode(msg.key, psk.data(), msg.key.size()) != psk.size()) {
    ESP_LOGW(TAG, "Invalid encryption key length");
    resp.success = false;
    return resp;
  }

  if (!this->parent_->save_noise_psk(psk, true)) {
    ESP_LOGW(TAG, "Failed to save encryption key");
    resp.success = false;
    return resp;
  }

  resp.success = true;
  return resp;
}
#endif
void APIConnection::subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) {
  state_subs_at_ = 0;
}
bool APIConnection::try_to_clear_buffer(bool log_out_of_space) {
  if (this->flags_.remove)
    return false;
  if (this->helper_->can_write_without_blocking())
    return true;
  delay(0);
  APIError err = this->helper_->loop();
  if (err != APIError::OK) {
    on_fatal_error();
    ESP_LOGW(TAG, "%s: Socket operation failed: %s errno=%d", this->get_client_combined_info().c_str(),
             api_error_to_str(err), errno);
    return false;
  }
  if (this->helper_->can_write_without_blocking())
    return true;
  if (log_out_of_space) {
    ESP_LOGV(TAG, "Cannot send message because of TCP buffer space");
  }
  return false;
}
bool APIConnection::send_buffer(ProtoWriteBuffer buffer, uint8_t message_type) {
  if (!this->try_to_clear_buffer(message_type != SubscribeLogsResponse::MESSAGE_TYPE)) {  // SubscribeLogsResponse
    return false;
  }

  APIError err = this->helper_->write_protobuf_packet(message_type, buffer);
  if (err == APIError::WOULD_BLOCK)
    return false;
  if (err != APIError::OK) {
    on_fatal_error();
    if (err == APIError::SOCKET_WRITE_FAILED && errno == ECONNRESET) {
      ESP_LOGW(TAG, "%s: Connection reset", this->get_client_combined_info().c_str());
    } else {
      ESP_LOGW(TAG, "%s: Packet write failed %s errno=%d", this->get_client_combined_info().c_str(),
               api_error_to_str(err), errno);
    }
    return false;
  }
  // Do not set last_traffic_ on send
  return true;
}
void APIConnection::on_unauthenticated_access() {
  this->on_fatal_error();
  ESP_LOGD(TAG, "%s requested access without authentication", this->get_client_combined_info().c_str());
}
void APIConnection::on_no_setup_connection() {
  this->on_fatal_error();
  ESP_LOGD(TAG, "%s requested access without full connection", this->get_client_combined_info().c_str());
}
void APIConnection::on_fatal_error() {
  this->helper_->close();
  this->flags_.remove = true;
}

void APIConnection::DeferredBatch::add_item(EntityBase *entity, MessageCreator creator, uint8_t message_type,
                                            uint8_t estimated_size) {
  // Check if we already have a message of this type for this entity
  // This provides deduplication per entity/message_type combination
  // O(n) but optimized for RAM and not performance.
  for (auto &item : items) {
    if (item.entity == entity && item.message_type == message_type) {
      // Clean up old creator before replacing
      item.creator.cleanup(message_type);
      // Move assign the new creator
      item.creator = std::move(creator);
      return;
    }
  }

  // No existing item found, add new one
  items.emplace_back(entity, std::move(creator), message_type, estimated_size);
}

void APIConnection::DeferredBatch::add_item_front(EntityBase *entity, MessageCreator creator, uint8_t message_type,
                                                  uint8_t estimated_size) {
  // Insert at front for high priority messages (no deduplication check)
  items.insert(items.begin(), BatchItem(entity, std::move(creator), message_type, estimated_size));
}

bool APIConnection::schedule_batch_() {
  if (!this->flags_.batch_scheduled) {
    this->flags_.batch_scheduled = true;
    this->deferred_batch_.batch_start_time = App.get_loop_component_start_time();
  }
  return true;
}

ProtoWriteBuffer APIConnection::allocate_single_message_buffer(uint16_t size) { return this->create_buffer(size); }

ProtoWriteBuffer APIConnection::allocate_batch_message_buffer(uint16_t size) {
  ProtoWriteBuffer result = this->prepare_message_buffer(size, this->flags_.batch_first_message);
  this->flags_.batch_first_message = false;
  return result;
}

void APIConnection::process_batch_() {
  if (this->deferred_batch_.empty()) {
    this->flags_.batch_scheduled = false;
    return;
  }

  // Try to clear buffer first
  if (!this->try_to_clear_buffer(true)) {
    // Can't write now, we'll try again later
    return;
  }

  size_t num_items = this->deferred_batch_.size();

  // Fast path for single message - allocate exact size needed
  if (num_items == 1) {
    const auto &item = this->deferred_batch_[0];

    // Let the creator calculate size and encode if it fits
    uint16_t payload_size =
        item.creator(item.entity, this, std::numeric_limits<uint16_t>::max(), true, item.message_type);

    if (payload_size > 0 &&
        this->send_buffer(ProtoWriteBuffer{&this->parent_->get_shared_buffer_ref()}, item.message_type)) {
#ifdef HAS_PROTO_MESSAGE_DUMP
      // Log messages after send attempt for VV debugging
      // It's safe to use the buffer for logging at this point regardless of send result
      this->log_batch_item_(item);
#endif
      this->clear_batch_();
    } else if (payload_size == 0) {
      // Message too large
      ESP_LOGW(TAG, "Message too large to send: type=%u", item.message_type);
      this->clear_batch_();
    }
    return;
  }

  // Pre-allocate storage for packet info
  std::vector<PacketInfo> packet_info;
  packet_info.reserve(num_items);

  // Cache these values to avoid repeated virtual calls
  const uint8_t header_padding = this->helper_->frame_header_padding();
  const uint8_t footer_size = this->helper_->frame_footer_size();

  // Initialize buffer and tracking variables
  this->parent_->get_shared_buffer_ref().clear();

  // Pre-calculate exact buffer size needed based on message types
  uint32_t total_estimated_size = 0;
  for (size_t i = 0; i < this->deferred_batch_.size(); i++) {
    const auto &item = this->deferred_batch_[i];
    total_estimated_size += item.estimated_size;
  }

  // Calculate total overhead for all messages
  uint32_t total_overhead = (header_padding + footer_size) * num_items;

  // Reserve based on estimated size (much more accurate than 24-byte worst-case)
  this->parent_->get_shared_buffer_ref().reserve(total_estimated_size + total_overhead);
  this->flags_.batch_first_message = true;

  size_t items_processed = 0;
  uint16_t remaining_size = std::numeric_limits<uint16_t>::max();

  // Track where each message's header padding begins in the buffer
  // For plaintext: this is where the 6-byte header padding starts
  // For noise: this is where the 7-byte header padding starts
  // The actual message data follows after the header padding
  uint32_t current_offset = 0;

  // Process items and encode directly to buffer
  for (size_t i = 0; i < this->deferred_batch_.size(); i++) {
    const auto &item = this->deferred_batch_[i];
    // Try to encode message
    // The creator will calculate overhead to determine if the message fits
    uint16_t payload_size = item.creator(item.entity, this, remaining_size, false, item.message_type);

    if (payload_size == 0) {
      // Message won't fit, stop processing
      break;
    }

    // Message was encoded successfully
    // payload_size is header_padding + actual payload size + footer_size
    uint16_t proto_payload_size = payload_size - header_padding - footer_size;
    packet_info.emplace_back(item.message_type, current_offset, proto_payload_size);

    // Update tracking variables
    items_processed++;
    // After first message, set remaining size to MAX_BATCH_PACKET_SIZE to avoid fragmentation
    if (items_processed == 1) {
      remaining_size = MAX_BATCH_PACKET_SIZE;
    }
    remaining_size -= payload_size;
    // Calculate where the next message's header padding will start
    // Current buffer size + footer space (that prepare_message_buffer will add for this message)
    current_offset = this->parent_->get_shared_buffer_ref().size() + footer_size;
  }

  if (items_processed == 0) {
    this->deferred_batch_.clear();
    return;
  }

  // Add footer space for the last message (for Noise protocol MAC)
  if (footer_size > 0) {
    auto &shared_buf = this->parent_->get_shared_buffer_ref();
    shared_buf.resize(shared_buf.size() + footer_size);
  }

  // Send all collected packets
  APIError err =
      this->helper_->write_protobuf_packets(ProtoWriteBuffer{&this->parent_->get_shared_buffer_ref()}, packet_info);
  if (err != APIError::OK && err != APIError::WOULD_BLOCK) {
    on_fatal_error();
    if (err == APIError::SOCKET_WRITE_FAILED && errno == ECONNRESET) {
      ESP_LOGW(TAG, "%s: Connection reset during batch write", this->get_client_combined_info().c_str());
    } else {
      ESP_LOGW(TAG, "%s: Batch write failed %s errno=%d", this->get_client_combined_info().c_str(),
               api_error_to_str(err), errno);
    }
  }

#ifdef HAS_PROTO_MESSAGE_DUMP
  // Log messages after send attempt for VV debugging
  // It's safe to use the buffer for logging at this point regardless of send result
  for (size_t i = 0; i < items_processed; i++) {
    const auto &item = this->deferred_batch_[i];
    this->log_batch_item_(item);
  }
#endif

  // Handle remaining items more efficiently
  if (items_processed < this->deferred_batch_.size()) {
    // Remove processed items from the beginning with proper cleanup
    this->deferred_batch_.remove_front(items_processed);
    // Reschedule for remaining items
    this->schedule_batch_();
  } else {
    // All items processed
    this->clear_batch_();
  }
}

uint16_t APIConnection::MessageCreator::operator()(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                   bool is_single, uint8_t message_type) const {
#ifdef USE_EVENT
  // Special case: EventResponse uses string pointer
  if (message_type == EventResponse::MESSAGE_TYPE) {
    auto *e = static_cast<event::Event *>(entity);
    return APIConnection::try_send_event_response(e, *data_.string_ptr, conn, remaining_size, is_single);
  }
#endif

  // All other message types use function pointers
  return data_.function_ptr(entity, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_list_info_done(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                bool is_single) {
  ListEntitiesDoneResponse resp;
  return encode_message_to_buffer(resp, ListEntitiesDoneResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_disconnect_request(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                    bool is_single) {
  DisconnectRequest req;
  return encode_message_to_buffer(req, DisconnectRequest::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_ping_request(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single) {
  PingRequest req;
  return encode_message_to_buffer(req, PingRequest::MESSAGE_TYPE, conn, remaining_size, is_single);
}

}  // namespace api
}  // namespace esphome
#endif
