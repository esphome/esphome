#include "api_connection.h"
#ifdef USE_API
#ifdef USE_API_NOISE
#include "api_frame_helper_noise.h"
#endif
#ifdef USE_API_PLAINTEXT
#include "api_frame_helper_plaintext.h"
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
#include "user_services.h"
#endif
#include <cerrno>
#include <cinttypes>
#include <functional>
#include <limits>
#include <utility>
#ifdef USE_ESP8266
#include <pgmspace.h>
#endif
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
#ifdef USE_CLIMATE
#include "esphome/components/climate/climate_mode.h"
#endif
#ifdef USE_VOICE_ASSISTANT
#include "esphome/components/voice_assistant/voice_assistant.h"
#endif
#ifdef USE_ZWAVE_PROXY
#include "esphome/components/zwave_proxy/zwave_proxy.h"
#endif

namespace esphome::api {

// Read a maximum of 5 messages per loop iteration to prevent starving other components.
// This is a balance between API responsiveness and allowing other components to run.
// Since each message could contain multiple protobuf messages when using packet batching,
// this limits the number of messages processed, not the number of TCP packets.
static constexpr uint8_t MAX_MESSAGES_PER_LOOP = 5;
static constexpr uint8_t MAX_PING_RETRIES = 60;
static constexpr uint16_t PING_RETRY_INTERVAL = 1000;
static constexpr uint32_t KEEPALIVE_DISCONNECT_TIMEOUT = (KEEPALIVE_TIMEOUT_MS * 5) / 2;

static constexpr auto ESPHOME_VERSION_REF = StringRef::from_lit(ESPHOME_VERSION);

static const char *const TAG = "api.connection";
#ifdef USE_CAMERA
static const int CAMERA_STOP_STREAM = 5000;
#endif

#ifdef USE_DEVICES
// Helper macro for entity command handlers - gets entity by key and device_id, returns if not found, and creates call
// object
#define ENTITY_COMMAND_MAKE_CALL(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key, msg.device_id); \
  if ((entity_var) == nullptr) \
    return; \
  auto call = (entity_var)->make_call();

// Helper macro for entity command handlers that don't use make_call() - gets entity by key and device_id and returns if
// not found
#define ENTITY_COMMAND_GET(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key, msg.device_id); \
  if ((entity_var) == nullptr) \
    return;
#else  // No device support, use simpler macros
// Helper macro for entity command handlers - gets entity by key, returns if not found, and creates call
// object
#define ENTITY_COMMAND_MAKE_CALL(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key); \
  if ((entity_var) == nullptr) \
    return; \
  auto call = (entity_var)->make_call();

// Helper macro for entity command handlers that don't use make_call() - gets entity by key and returns if
// not found
#define ENTITY_COMMAND_GET(entity_type, entity_var, getter_name) \
  entity_type *entity_var = App.get_##getter_name##_by_key(msg.key); \
  if ((entity_var) == nullptr) \
    return;
#endif  // USE_DEVICES

APIConnection::APIConnection(std::unique_ptr<socket::Socket> sock, APIServer *parent)
    : parent_(parent), initial_state_iterator_(this), list_entities_iterator_(this) {
#if defined(USE_API_PLAINTEXT) && defined(USE_API_NOISE)
  auto &noise_ctx = parent->get_noise_ctx();
  if (noise_ctx.has_psk()) {
    this->helper_ =
        std::unique_ptr<APIFrameHelper>{new APINoiseFrameHelper(std::move(sock), noise_ctx, &this->client_info_)};
  } else {
    this->helper_ = std::unique_ptr<APIFrameHelper>{new APIPlaintextFrameHelper(std::move(sock), &this->client_info_)};
  }
#elif defined(USE_API_PLAINTEXT)
  this->helper_ = std::unique_ptr<APIFrameHelper>{new APIPlaintextFrameHelper(std::move(sock), &this->client_info_)};
#elif defined(USE_API_NOISE)
  this->helper_ = std::unique_ptr<APIFrameHelper>{
      new APINoiseFrameHelper(std::move(sock), parent->get_noise_ctx(), &this->client_info_)};
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
    this->fatal_error_with_log_(LOG_STR("Helper init failed"), err);
    return;
  }
  this->client_info_.peername = helper_->getpeername();
  this->client_info_.name = this->client_info_.peername;
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
    this->fatal_error_with_log_(LOG_STR("Socket operation failed"), err);
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
        this->fatal_error_with_log_(LOG_STR("Reading failed"), err);
        return;
      } else {
        this->last_traffic_ = now;
        // read a packet
        this->read_message(buffer.data_len, buffer.type, buffer.data);
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
      // Release excess memory from buffers that grew during initial sync
      this->deferred_batch_.release_buffer();
      this->helper_->release_buffers();
    }
  }

  if (this->flags_.sent_ping) {
    // Disconnect if not responded within 2.5*keepalive
    if (now - this->last_traffic_ > KEEPALIVE_DISCONNECT_TIMEOUT) {
      on_fatal_error();
      ESP_LOGW(TAG, "%s (%s) is unresponsive; disconnecting", this->client_info_.name.c_str(),
               this->client_info_.peername.c_str());
    }
  } else if (now - this->last_traffic_ > KEEPALIVE_TIMEOUT_MS && !this->flags_.remove) {
    // Only send ping if we're not disconnecting
    ESP_LOGVV(TAG, "Sending keepalive PING");
    PingRequest req;
    this->flags_.sent_ping = this->send_message(req, PingRequest::MESSAGE_TYPE);
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

    CameraImageResponse msg;
    msg.key = camera::Camera::instance()->get_object_id_hash();
    msg.set_data(this->image_reader_->peek_data_buffer(), to_send);
    msg.done = done;
#ifdef USE_DEVICES
    msg.device_id = camera::Camera::instance()->get_device_id();
#endif

    if (this->send_message_(msg, CameraImageResponse::MESSAGE_TYPE)) {
      this->image_reader_->consume_data(to_send);
      if (done) {
        this->image_reader_->return_image();
      }
    }
  }
#endif

#ifdef USE_API_HOMEASSISTANT_STATES
  if (state_subs_at_ >= 0) {
    this->process_state_subscriptions_();
  }
#endif
}

bool APIConnection::send_disconnect_response(const DisconnectRequest &msg) {
  // remote initiated disconnect_client
  // don't close yet, we still need to send the disconnect response
  // close will happen on next loop
  ESP_LOGD(TAG, "%s (%s) disconnected", this->client_info_.name.c_str(), this->client_info_.peername.c_str());
  this->flags_.next_close = true;
  DisconnectResponse resp;
  return this->send_message(resp, DisconnectResponse::MESSAGE_TYPE);
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
  ProtoSize size_calc;
  msg.calculate_size(size_calc);
  uint32_t calculated_size = size_calc.get_size();

  // Cache frame sizes to avoid repeated virtual calls
  const uint8_t header_padding = conn->helper_->frame_header_padding();
  const uint8_t footer_size = conn->helper_->frame_footer_size();

  // Calculate total size with padding for buffer allocation
  size_t total_calculated_size = calculated_size + header_padding + footer_size;

  // Check if it fits
  if (total_calculated_size > remaining_size) {
    return 0;  // Doesn't fit
  }

  // Get buffer size after allocation (which includes header padding)
  std::vector<uint8_t> &shared_buf = conn->parent_->get_shared_buffer_ref();

  if (is_single || conn->flags_.batch_first_message) {
    // Single message or first batch message
    conn->prepare_first_message_buffer(shared_buf, header_padding, total_calculated_size);
    if (conn->flags_.batch_first_message) {
      conn->flags_.batch_first_message = false;
    }
  } else {
    // Batch message second or later
    // Add padding for previous message footer + this message header
    size_t current_size = shared_buf.size();
    shared_buf.reserve(current_size + total_calculated_size);
    shared_buf.resize(current_size + footer_size + header_padding);
  }

  // Encode directly into buffer
  size_t size_before_encode = shared_buf.size();
  msg.encode({&shared_buf});

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
  return fill_and_encode_entity_state(binary_sensor, resp, BinarySensorStateResponse::MESSAGE_TYPE, conn,
                                      remaining_size, is_single);
}

uint16_t APIConnection::try_send_binary_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                    bool is_single) {
  auto *binary_sensor = static_cast<binary_sensor::BinarySensor *>(entity);
  ListEntitiesBinarySensorResponse msg;
  msg.set_device_class(binary_sensor->get_device_class_ref());
  msg.is_status_binary_sensor = binary_sensor->is_status_binary_sensor();
  return fill_and_encode_entity_info(binary_sensor, msg, ListEntitiesBinarySensorResponse::MESSAGE_TYPE, conn,
                                     remaining_size, is_single);
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
  msg.position = cover->position;
  if (traits.get_supports_tilt())
    msg.tilt = cover->tilt;
  msg.current_operation = static_cast<enums::CoverOperation>(cover->current_operation);
  return fill_and_encode_entity_state(cover, msg, CoverStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
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
  msg.set_device_class(cover->get_device_class_ref());
  return fill_and_encode_entity_info(cover, msg, ListEntitiesCoverResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
}
void APIConnection::cover_command(const CoverCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(cover::Cover, cover, cover)
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
  if (traits.supports_preset_modes() && fan->has_preset_mode())
    msg.set_preset_mode(StringRef(fan->get_preset_mode()));
  return fill_and_encode_entity_state(fan, msg, FanStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
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
  msg.supported_preset_modes = &traits.supported_preset_modes();
  return fill_and_encode_entity_info(fan, msg, ListEntitiesFanResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
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
  if (light->supports_effects()) {
    resp.set_effect(light->get_effect_name_ref());
  }
  return fill_and_encode_entity_state(light, resp, LightStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_light_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *light = static_cast<light::LightState *>(entity);
  ListEntitiesLightResponse msg;
  auto traits = light->get_traits();
  auto supported_modes = traits.get_supported_color_modes();
  // Pass pointer to ColorModeMask so the iterator can encode actual ColorMode enum values
  msg.supported_color_modes = &supported_modes;
  if (traits.supports_color_capability(light::ColorCapability::COLOR_TEMPERATURE) ||
      traits.supports_color_capability(light::ColorCapability::COLD_WARM_WHITE)) {
    msg.min_mireds = traits.get_min_mireds();
    msg.max_mireds = traits.get_max_mireds();
  }
  FixedVector<const char *> effects_list;
  if (light->supports_effects()) {
    auto &light_effects = light->get_effects();
    effects_list.init(light_effects.size() + 1);
    effects_list.push_back("None");
    for (auto *effect : light_effects) {
      effects_list.push_back(effect->get_name());
    }
  }
  msg.effects = &effects_list;
  return fill_and_encode_entity_info(light, msg, ListEntitiesLightResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
    call.set_effect(reinterpret_cast<const char *>(msg.effect), msg.effect_len);
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
  return fill_and_encode_entity_state(sensor, resp, SensorStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *sensor = static_cast<sensor::Sensor *>(entity);
  ListEntitiesSensorResponse msg;
  msg.set_unit_of_measurement(sensor->get_unit_of_measurement_ref());
  msg.accuracy_decimals = sensor->get_accuracy_decimals();
  msg.force_update = sensor->get_force_update();
  msg.set_device_class(sensor->get_device_class_ref());
  msg.state_class = static_cast<enums::SensorStateClass>(sensor->get_state_class());
  return fill_and_encode_entity_info(sensor, msg, ListEntitiesSensorResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  return fill_and_encode_entity_state(a_switch, resp, SwitchStateResponse::MESSAGE_TYPE, conn, remaining_size,
                                      is_single);
}

uint16_t APIConnection::try_send_switch_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *a_switch = static_cast<switch_::Switch *>(entity);
  ListEntitiesSwitchResponse msg;
  msg.assumed_state = a_switch->assumed_state();
  msg.set_device_class(a_switch->get_device_class_ref());
  return fill_and_encode_entity_info(a_switch, msg, ListEntitiesSwitchResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  resp.set_state(StringRef(text_sensor->state));
  resp.missing_state = !text_sensor->has_state();
  return fill_and_encode_entity_state(text_sensor, resp, TextSensorStateResponse::MESSAGE_TYPE, conn, remaining_size,
                                      is_single);
}
uint16_t APIConnection::try_send_text_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                  bool is_single) {
  auto *text_sensor = static_cast<text_sensor::TextSensor *>(entity);
  ListEntitiesTextSensorResponse msg;
  msg.set_device_class(text_sensor->get_device_class_ref());
  return fill_and_encode_entity_info(text_sensor, msg, ListEntitiesTextSensorResponse::MESSAGE_TYPE, conn,
                                     remaining_size, is_single);
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
  auto traits = climate->get_traits();
  resp.mode = static_cast<enums::ClimateMode>(climate->mode);
  resp.action = static_cast<enums::ClimateAction>(climate->action);
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE))
    resp.current_temperature = climate->current_temperature;
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE |
                               climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
    resp.target_temperature_low = climate->target_temperature_low;
    resp.target_temperature_high = climate->target_temperature_high;
  } else {
    resp.target_temperature = climate->target_temperature;
  }
  if (traits.get_supports_fan_modes() && climate->fan_mode.has_value())
    resp.fan_mode = static_cast<enums::ClimateFanMode>(climate->fan_mode.value());
  if (!traits.get_supported_custom_fan_modes().empty() && climate->has_custom_fan_mode()) {
    resp.set_custom_fan_mode(StringRef(climate->get_custom_fan_mode()));
  }
  if (traits.get_supports_presets() && climate->preset.has_value()) {
    resp.preset = static_cast<enums::ClimatePreset>(climate->preset.value());
  }
  if (!traits.get_supported_custom_presets().empty() && climate->has_custom_preset()) {
    resp.set_custom_preset(StringRef(climate->get_custom_preset()));
  }
  if (traits.get_supports_swing_modes())
    resp.swing_mode = static_cast<enums::ClimateSwingMode>(climate->swing_mode);
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY))
    resp.current_humidity = climate->current_humidity;
  if (traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY))
    resp.target_humidity = climate->target_humidity;
  return fill_and_encode_entity_state(climate, resp, ClimateStateResponse::MESSAGE_TYPE, conn, remaining_size,
                                      is_single);
}
uint16_t APIConnection::try_send_climate_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single) {
  auto *climate = static_cast<climate::Climate *>(entity);
  ListEntitiesClimateResponse msg;
  auto traits = climate->get_traits();
  // Flags set for backward compatibility, deprecated in 2025.11.0
  msg.supports_current_temperature = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  msg.supports_current_humidity = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY);
  msg.supports_two_point_target_temperature = traits.has_feature_flags(
      climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE | climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
  msg.supports_target_humidity = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TARGET_HUMIDITY);
  msg.supports_action = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
  // Current feature flags and other supported parameters
  msg.feature_flags = traits.get_feature_flags();
  msg.supported_modes = &traits.get_supported_modes();
  msg.visual_min_temperature = traits.get_visual_min_temperature();
  msg.visual_max_temperature = traits.get_visual_max_temperature();
  msg.visual_target_temperature_step = traits.get_visual_target_temperature_step();
  msg.visual_current_temperature_step = traits.get_visual_current_temperature_step();
  msg.visual_min_humidity = traits.get_visual_min_humidity();
  msg.visual_max_humidity = traits.get_visual_max_humidity();
  msg.supported_fan_modes = &traits.get_supported_fan_modes();
  msg.supported_custom_fan_modes = &traits.get_supported_custom_fan_modes();
  msg.supported_presets = &traits.get_supported_presets();
  msg.supported_custom_presets = &traits.get_supported_custom_presets();
  msg.supported_swing_modes = &traits.get_supported_swing_modes();
  return fill_and_encode_entity_info(climate, msg, ListEntitiesClimateResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  return fill_and_encode_entity_state(number, resp, NumberStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_number_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *number = static_cast<number::Number *>(entity);
  ListEntitiesNumberResponse msg;
  msg.set_unit_of_measurement(number->traits.get_unit_of_measurement_ref());
  msg.mode = static_cast<enums::NumberMode>(number->traits.get_mode());
  msg.set_device_class(number->traits.get_device_class_ref());
  msg.min_value = number->traits.get_min_value();
  msg.max_value = number->traits.get_max_value();
  msg.step = number->traits.get_step();
  return fill_and_encode_entity_info(number, msg, ListEntitiesNumberResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  return fill_and_encode_entity_state(date, resp, DateStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_date_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                           bool is_single) {
  auto *date = static_cast<datetime::DateEntity *>(entity);
  ListEntitiesDateResponse msg;
  return fill_and_encode_entity_info(date, msg, ListEntitiesDateResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  return fill_and_encode_entity_state(time, resp, TimeStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_time_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                           bool is_single) {
  auto *time = static_cast<datetime::TimeEntity *>(entity);
  ListEntitiesTimeResponse msg;
  return fill_and_encode_entity_info(time, msg, ListEntitiesTimeResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  return fill_and_encode_entity_state(datetime, resp, DateTimeStateResponse::MESSAGE_TYPE, conn, remaining_size,
                                      is_single);
}
uint16_t APIConnection::try_send_datetime_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                               bool is_single) {
  auto *datetime = static_cast<datetime::DateTimeEntity *>(entity);
  ListEntitiesDateTimeResponse msg;
  return fill_and_encode_entity_info(datetime, msg, ListEntitiesDateTimeResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  resp.set_state(StringRef(text->state));
  resp.missing_state = !text->has_state();
  return fill_and_encode_entity_state(text, resp, TextStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_text_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                           bool is_single) {
  auto *text = static_cast<text::Text *>(entity);
  ListEntitiesTextResponse msg;
  msg.mode = static_cast<enums::TextMode>(text->traits.get_mode());
  msg.min_length = text->traits.get_min_length();
  msg.max_length = text->traits.get_max_length();
  msg.set_pattern(text->traits.get_pattern_ref());
  return fill_and_encode_entity_info(text, msg, ListEntitiesTextResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  resp.set_state(StringRef(select->current_option()));
  resp.missing_state = !select->has_state();
  return fill_and_encode_entity_state(select, resp, SelectStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_select_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *select = static_cast<select::Select *>(entity);
  ListEntitiesSelectResponse msg;
  msg.options = &select->traits.get_options();
  return fill_and_encode_entity_info(select, msg, ListEntitiesSelectResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
}
void APIConnection::select_command(const SelectCommandRequest &msg) {
  ENTITY_COMMAND_MAKE_CALL(select::Select, select, select)
  call.set_option(reinterpret_cast<const char *>(msg.state), msg.state_len);
  call.perform();
}
#endif

#ifdef USE_BUTTON
uint16_t APIConnection::try_send_button_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *button = static_cast<button::Button *>(entity);
  ListEntitiesButtonResponse msg;
  msg.set_device_class(button->get_device_class_ref());
  return fill_and_encode_entity_info(button, msg, ListEntitiesButtonResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  return fill_and_encode_entity_state(a_lock, resp, LockStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_lock_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                           bool is_single) {
  auto *a_lock = static_cast<lock::Lock *>(entity);
  ListEntitiesLockResponse msg;
  msg.assumed_state = a_lock->traits.get_assumed_state();
  msg.supports_open = a_lock->traits.get_supports_open();
  msg.requires_code = a_lock->traits.get_requires_code();
  return fill_and_encode_entity_info(a_lock, msg, ListEntitiesLockResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  return fill_and_encode_entity_state(valve, resp, ValveStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_valve_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *valve = static_cast<valve::Valve *>(entity);
  ListEntitiesValveResponse msg;
  auto traits = valve->get_traits();
  msg.set_device_class(valve->get_device_class_ref());
  msg.assumed_state = traits.get_is_assumed_state();
  msg.supports_position = traits.get_supports_position();
  msg.supports_stop = traits.get_supports_stop();
  return fill_and_encode_entity_info(valve, msg, ListEntitiesValveResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  return fill_and_encode_entity_state(media_player, resp, MediaPlayerStateResponse::MESSAGE_TYPE, conn, remaining_size,
                                      is_single);
}
uint16_t APIConnection::try_send_media_player_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                   bool is_single) {
  auto *media_player = static_cast<media_player::MediaPlayer *>(entity);
  ListEntitiesMediaPlayerResponse msg;
  auto traits = media_player->get_traits();
  msg.supports_pause = traits.get_supports_pause();
  msg.feature_flags = traits.get_feature_flags();
  for (auto &supported_format : traits.get_supported_formats()) {
    msg.supported_formats.emplace_back();
    auto &media_format = msg.supported_formats.back();
    media_format.set_format(StringRef(supported_format.format));
    media_format.sample_rate = supported_format.sample_rate;
    media_format.num_channels = supported_format.num_channels;
    media_format.purpose = static_cast<enums::MediaPlayerFormatPurpose>(supported_format.purpose);
    media_format.sample_bytes = supported_format.sample_bytes;
  }
  return fill_and_encode_entity_info(media_player, msg, ListEntitiesMediaPlayerResponse::MESSAGE_TYPE, conn,
                                     remaining_size, is_single);
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
  return fill_and_encode_entity_info(camera, msg, ListEntitiesCameraResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  if (homeassistant::global_homeassistant_time != nullptr) {
    homeassistant::global_homeassistant_time->set_epoch_time(value.epoch_seconds);
#ifdef USE_TIME_TIMEZONE
    if (value.timezone_len > 0) {
      homeassistant::global_homeassistant_time->set_timezone(reinterpret_cast<const char *>(value.timezone),
                                                             value.timezone_len);
    }
#endif
  }
}
#endif

#ifdef USE_BLUETOOTH_PROXY
void APIConnection::subscribe_bluetooth_le_advertisements(const SubscribeBluetoothLEAdvertisementsRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->subscribe_api_connection(this, msg.flags);
}
void APIConnection::unsubscribe_bluetooth_le_advertisements(const UnsubscribeBluetoothLEAdvertisementsRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->unsubscribe_api_connection(this);
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

bool APIConnection::send_subscribe_bluetooth_connections_free_response(
    const SubscribeBluetoothConnectionsFreeRequest &msg) {
  bluetooth_proxy::global_bluetooth_proxy->send_connections_free(this);
  return true;
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

bool APIConnection::send_voice_assistant_get_configuration_response(const VoiceAssistantConfigurationRequest &msg) {
  VoiceAssistantConfigurationResponse resp;
  if (!this->check_voice_assistant_api_connection_()) {
    return this->send_message(resp, VoiceAssistantConfigurationResponse::MESSAGE_TYPE);
  }

  auto &config = voice_assistant::global_voice_assistant->get_configuration();
  for (auto &wake_word : config.available_wake_words) {
    resp.available_wake_words.emplace_back();
    auto &resp_wake_word = resp.available_wake_words.back();
    resp_wake_word.set_id(StringRef(wake_word.id));
    resp_wake_word.set_wake_word(StringRef(wake_word.wake_word));
    for (const auto &lang : wake_word.trained_languages) {
      resp_wake_word.trained_languages.push_back(lang);
    }
  }

  // Filter external wake words
  for (auto &wake_word : msg.external_wake_words) {
    if (wake_word.model_type != "micro") {
      // microWakeWord only
      continue;
    }

    resp.available_wake_words.emplace_back();
    auto &resp_wake_word = resp.available_wake_words.back();
    resp_wake_word.set_id(StringRef(wake_word.id));
    resp_wake_word.set_wake_word(StringRef(wake_word.wake_word));
    for (const auto &lang : wake_word.trained_languages) {
      resp_wake_word.trained_languages.push_back(lang);
    }
  }

  resp.active_wake_words = &config.active_wake_words;
  resp.max_active_wake_words = config.max_active_wake_words;
  return this->send_message(resp, VoiceAssistantConfigurationResponse::MESSAGE_TYPE);
}

void APIConnection::voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) {
  if (this->check_voice_assistant_api_connection_()) {
    voice_assistant::global_voice_assistant->on_set_configuration(msg.active_wake_words);
  }
}
#endif

#ifdef USE_ZWAVE_PROXY
void APIConnection::zwave_proxy_frame(const ZWaveProxyFrame &msg) {
  zwave_proxy::global_zwave_proxy->send_frame(msg.data, msg.data_len);
}

void APIConnection::zwave_proxy_request(const ZWaveProxyRequest &msg) {
  zwave_proxy::global_zwave_proxy->zwave_proxy_request(this, msg.type);
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
  return fill_and_encode_entity_state(a_alarm_control_panel, resp, AlarmControlPanelStateResponse::MESSAGE_TYPE, conn,
                                      remaining_size, is_single);
}
uint16_t APIConnection::try_send_alarm_control_panel_info(EntityBase *entity, APIConnection *conn,
                                                          uint32_t remaining_size, bool is_single) {
  auto *a_alarm_control_panel = static_cast<alarm_control_panel::AlarmControlPanel *>(entity);
  ListEntitiesAlarmControlPanelResponse msg;
  msg.supported_features = a_alarm_control_panel->get_supported_features();
  msg.requires_code = a_alarm_control_panel->get_requires_code();
  msg.requires_code_to_arm = a_alarm_control_panel->get_requires_code_to_arm();
  return fill_and_encode_entity_info(a_alarm_control_panel, msg, ListEntitiesAlarmControlPanelResponse::MESSAGE_TYPE,
                                     conn, remaining_size, is_single);
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
void APIConnection::send_event(event::Event *event, const char *event_type) {
  this->send_message_smart_(event, MessageCreator(event_type), EventResponse::MESSAGE_TYPE,
                            EventResponse::ESTIMATED_SIZE);
}
uint16_t APIConnection::try_send_event_response(event::Event *event, const char *event_type, APIConnection *conn,
                                                uint32_t remaining_size, bool is_single) {
  EventResponse resp;
  resp.set_event_type(StringRef(event_type));
  return fill_and_encode_entity_state(event, resp, EventResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}

uint16_t APIConnection::try_send_event_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single) {
  auto *event = static_cast<event::Event *>(entity);
  ListEntitiesEventResponse msg;
  msg.set_device_class(event->get_device_class_ref());
  msg.event_types = &event->get_event_types();
  return fill_and_encode_entity_info(event, msg, ListEntitiesEventResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
    resp.set_current_version(StringRef(update->update_info.current_version));
    resp.set_latest_version(StringRef(update->update_info.latest_version));
    resp.set_title(StringRef(update->update_info.title));
    resp.set_release_summary(StringRef(update->update_info.summary));
    resp.set_release_url(StringRef(update->update_info.release_url));
  }
  return fill_and_encode_entity_state(update, resp, UpdateStateResponse::MESSAGE_TYPE, conn, remaining_size, is_single);
}
uint16_t APIConnection::try_send_update_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single) {
  auto *update = static_cast<update::UpdateEntity *>(entity);
  ListEntitiesUpdateResponse msg;
  msg.set_device_class(update->get_device_class_ref());
  return fill_and_encode_entity_info(update, msg, ListEntitiesUpdateResponse::MESSAGE_TYPE, conn, remaining_size,
                                     is_single);
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
  SubscribeLogsResponse msg;
  msg.level = static_cast<enums::LogLevel>(level);
  msg.set_message(reinterpret_cast<const uint8_t *>(line), message_len);
  return this->send_message_(msg, SubscribeLogsResponse::MESSAGE_TYPE);
}

void APIConnection::complete_authentication_() {
  // Early return if already authenticated
  if (this->flags_.connection_state == static_cast<uint8_t>(ConnectionState::AUTHENTICATED)) {
    return;
  }

  this->flags_.connection_state = static_cast<uint8_t>(ConnectionState::AUTHENTICATED);
  ESP_LOGD(TAG, "%s (%s) connected", this->client_info_.name.c_str(), this->client_info_.peername.c_str());
#ifdef USE_API_CLIENT_CONNECTED_TRIGGER
  this->parent_->get_client_connected_trigger()->trigger(this->client_info_.name, this->client_info_.peername);
#endif
#ifdef USE_HOMEASSISTANT_TIME
  if (homeassistant::global_homeassistant_time != nullptr) {
    this->send_time_request();
  }
#endif
#ifdef USE_ZWAVE_PROXY
  if (zwave_proxy::global_zwave_proxy != nullptr) {
    zwave_proxy::global_zwave_proxy->api_connection_authenticated(this);
  }
#endif
}

bool APIConnection::send_hello_response(const HelloRequest &msg) {
  this->client_info_.name.assign(reinterpret_cast<const char *>(msg.client_info), msg.client_info_len);
  this->client_info_.peername = this->helper_->getpeername();
  this->client_api_version_major_ = msg.api_version_major;
  this->client_api_version_minor_ = msg.api_version_minor;
  ESP_LOGV(TAG, "Hello from client: '%s' | %s | API Version %" PRIu32 ".%" PRIu32, this->client_info_.name.c_str(),
           this->client_info_.peername.c_str(), this->client_api_version_major_, this->client_api_version_minor_);

  HelloResponse resp;
  resp.api_version_major = 1;
  resp.api_version_minor = 13;
  // Send only the version string - the client only logs this for debugging and doesn't use it otherwise
  resp.set_server_info(ESPHOME_VERSION_REF);
  resp.set_name(StringRef(App.get_name()));

#ifdef USE_API_PASSWORD
  // Password required - wait for authentication
  this->flags_.connection_state = static_cast<uint8_t>(ConnectionState::CONNECTED);
#else
  // No password configured - auto-authenticate
  this->complete_authentication_();
#endif

  return this->send_message(resp, HelloResponse::MESSAGE_TYPE);
}
#ifdef USE_API_PASSWORD
bool APIConnection::send_authenticate_response(const AuthenticationRequest &msg) {
  AuthenticationResponse resp;
  // bool invalid_password = 1;
  resp.invalid_password = !this->parent_->check_password(msg.password, msg.password_len);
  if (!resp.invalid_password) {
    this->complete_authentication_();
  }
  return this->send_message(resp, AuthenticationResponse::MESSAGE_TYPE);
}
#endif  // USE_API_PASSWORD

bool APIConnection::send_ping_response(const PingRequest &msg) {
  PingResponse resp;
  return this->send_message(resp, PingResponse::MESSAGE_TYPE);
}

bool APIConnection::send_device_info_response(const DeviceInfoRequest &msg) {
  DeviceInfoResponse resp{};
#ifdef USE_API_PASSWORD
  resp.uses_password = true;
#endif
  resp.set_name(StringRef(App.get_name()));
  resp.set_friendly_name(StringRef(App.get_friendly_name()));
#ifdef USE_AREAS
  resp.set_suggested_area(StringRef(App.get_area()));
#endif
  // Stack buffer for MAC address (XX:XX:XX:XX:XX:XX\0 = 18 bytes)
  char mac_address[18];
  uint8_t mac[6];
  get_mac_address_raw(mac);
  format_mac_addr_upper(mac, mac_address);
  resp.set_mac_address(StringRef(mac_address));

  resp.set_esphome_version(ESPHOME_VERSION_REF);

  resp.set_compilation_time(App.get_compilation_time_ref());

  // Manufacturer string - define once, handle ESP8266 PROGMEM separately
#if defined(USE_ESP8266) || defined(USE_ESP32)
#define ESPHOME_MANUFACTURER "Espressif"
#elif defined(USE_RP2040)
#define ESPHOME_MANUFACTURER "Raspberry Pi"
#elif defined(USE_BK72XX)
#define ESPHOME_MANUFACTURER "Beken"
#elif defined(USE_LN882X)
#define ESPHOME_MANUFACTURER "Lightning"
#elif defined(USE_NRF52)
#define ESPHOME_MANUFACTURER "Nordic Semiconductor"
#elif defined(USE_RTL87XX)
#define ESPHOME_MANUFACTURER "Realtek"
#elif defined(USE_HOST)
#define ESPHOME_MANUFACTURER "Host"
#endif

#ifdef USE_ESP8266
  // ESP8266 requires PROGMEM for flash storage, copy to stack for memcpy compatibility
  static const char MANUFACTURER_PROGMEM[] PROGMEM = ESPHOME_MANUFACTURER;
  char manufacturer_buf[sizeof(MANUFACTURER_PROGMEM)];
  memcpy_P(manufacturer_buf, MANUFACTURER_PROGMEM, sizeof(MANUFACTURER_PROGMEM));
  resp.set_manufacturer(StringRef(manufacturer_buf, sizeof(MANUFACTURER_PROGMEM) - 1));
#else
  static constexpr auto MANUFACTURER = StringRef::from_lit(ESPHOME_MANUFACTURER);
  resp.set_manufacturer(MANUFACTURER);
#endif
#undef ESPHOME_MANUFACTURER

#ifdef USE_ESP8266
  static const char MODEL_PROGMEM[] PROGMEM = ESPHOME_BOARD;
  char model_buf[sizeof(MODEL_PROGMEM)];
  memcpy_P(model_buf, MODEL_PROGMEM, sizeof(MODEL_PROGMEM));
  resp.set_model(StringRef(model_buf, sizeof(MODEL_PROGMEM) - 1));
#else
  static constexpr auto MODEL = StringRef::from_lit(ESPHOME_BOARD);
  resp.set_model(MODEL);
#endif
#ifdef USE_DEEP_SLEEP
  resp.has_deep_sleep = deep_sleep::global_has_deep_sleep;
#endif
#ifdef ESPHOME_PROJECT_NAME
#ifdef USE_ESP8266
  static const char PROJECT_NAME_PROGMEM[] PROGMEM = ESPHOME_PROJECT_NAME;
  static const char PROJECT_VERSION_PROGMEM[] PROGMEM = ESPHOME_PROJECT_VERSION;
  char project_name_buf[sizeof(PROJECT_NAME_PROGMEM)];
  char project_version_buf[sizeof(PROJECT_VERSION_PROGMEM)];
  memcpy_P(project_name_buf, PROJECT_NAME_PROGMEM, sizeof(PROJECT_NAME_PROGMEM));
  memcpy_P(project_version_buf, PROJECT_VERSION_PROGMEM, sizeof(PROJECT_VERSION_PROGMEM));
  resp.set_project_name(StringRef(project_name_buf, sizeof(PROJECT_NAME_PROGMEM) - 1));
  resp.set_project_version(StringRef(project_version_buf, sizeof(PROJECT_VERSION_PROGMEM) - 1));
#else
  static constexpr auto PROJECT_NAME = StringRef::from_lit(ESPHOME_PROJECT_NAME);
  static constexpr auto PROJECT_VERSION = StringRef::from_lit(ESPHOME_PROJECT_VERSION);
  resp.set_project_name(PROJECT_NAME);
  resp.set_project_version(PROJECT_VERSION);
#endif
#endif
#ifdef USE_WEBSERVER
  resp.webserver_port = USE_WEBSERVER_PORT;
#endif
#ifdef USE_BLUETOOTH_PROXY
  resp.bluetooth_proxy_feature_flags = bluetooth_proxy::global_bluetooth_proxy->get_feature_flags();
  // Stack buffer for Bluetooth MAC address (XX:XX:XX:XX:XX:XX\0 = 18 bytes)
  char bluetooth_mac[18];
  bluetooth_proxy::global_bluetooth_proxy->get_bluetooth_mac_address_pretty(bluetooth_mac);
  resp.set_bluetooth_mac_address(StringRef(bluetooth_mac));
#endif
#ifdef USE_VOICE_ASSISTANT
  resp.voice_assistant_feature_flags = voice_assistant::global_voice_assistant->get_feature_flags();
#endif
#ifdef USE_ZWAVE_PROXY
  resp.zwave_proxy_feature_flags = zwave_proxy::global_zwave_proxy->get_feature_flags();
  resp.zwave_home_id = zwave_proxy::global_zwave_proxy->get_home_id();
#endif
#ifdef USE_API_NOISE
  resp.api_encryption_supported = true;
#endif
#ifdef USE_DEVICES
  size_t device_index = 0;
  for (auto const &device : App.get_devices()) {
    if (device_index >= ESPHOME_DEVICE_COUNT)
      break;
    auto &device_info = resp.devices[device_index++];
    device_info.device_id = device->get_device_id();
    device_info.set_name(StringRef(device->get_name()));
    device_info.area_id = device->get_area_id();
  }
#endif
#ifdef USE_AREAS
  size_t area_index = 0;
  for (auto const &area : App.get_areas()) {
    if (area_index >= ESPHOME_AREA_COUNT)
      break;
    auto &area_info = resp.areas[area_index++];
    area_info.area_id = area->get_area_id();
    area_info.set_name(StringRef(area->get_name()));
  }
#endif

  return this->send_message(resp, DeviceInfoResponse::MESSAGE_TYPE);
}

#ifdef USE_API_HOMEASSISTANT_STATES
void APIConnection::on_home_assistant_state_response(const HomeAssistantStateResponse &msg) {
  for (auto &it : this->parent_->get_state_subs()) {
    // Compare entity_id and attribute with message fields
    bool entity_match = (strcmp(it.entity_id, msg.entity_id.c_str()) == 0);
    bool attribute_match = (it.attribute != nullptr && strcmp(it.attribute, msg.attribute.c_str()) == 0) ||
                           (it.attribute == nullptr && msg.attribute.empty());

    if (entity_match && attribute_match) {
      it.callback(msg.state);
    }
  }
}
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
void APIConnection::execute_service(const ExecuteServiceRequest &msg) {
  bool found = false;
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  // Register the call and get a unique server-generated action_call_id
  // This avoids collisions when multiple clients use the same call_id
  uint32_t action_call_id = 0;
  if (msg.call_id != 0) {
    action_call_id = this->parent_->register_active_action_call(msg.call_id, this);
  }
  // Use the overload that passes action_call_id separately (avoids copying msg)
  for (auto *service : this->parent_->get_user_services()) {
    if (service->execute_service(msg, action_call_id)) {
      found = true;
    }
  }
#else
  for (auto *service : this->parent_->get_user_services()) {
    if (service->execute_service(msg)) {
      found = true;
    }
  }
#endif
  if (!found) {
    ESP_LOGV(TAG, "Could not find service");
  }
  // Note: For services with supports_response != none, the call is unregistered
  // by an automatically appended APIUnregisterServiceCallAction at the end of
  // the action list. This ensures async actions (delays, waits) complete first.
}
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
void APIConnection::send_execute_service_response(uint32_t call_id, bool success, const std::string &error_message) {
  ExecuteServiceResponse resp;
  resp.call_id = call_id;
  resp.success = success;
  resp.set_error_message(StringRef(error_message));
  this->send_message(resp, ExecuteServiceResponse::MESSAGE_TYPE);
}
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
void APIConnection::send_execute_service_response(uint32_t call_id, bool success, const std::string &error_message,
                                                  const uint8_t *response_data, size_t response_data_len) {
  ExecuteServiceResponse resp;
  resp.call_id = call_id;
  resp.success = success;
  resp.set_error_message(StringRef(error_message));
  resp.response_data = response_data;
  resp.response_data_len = response_data_len;
  this->send_message(resp, ExecuteServiceResponse::MESSAGE_TYPE);
}
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES
#endif

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
void APIConnection::on_homeassistant_action_response(const HomeassistantActionResponse &msg) {
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  if (msg.response_data_len > 0) {
    this->parent_->handle_action_response(msg.call_id, msg.success, msg.error_message, msg.response_data,
                                          msg.response_data_len);
  } else
#endif
  {
    this->parent_->handle_action_response(msg.call_id, msg.success, msg.error_message);
  }
};
#endif
#ifdef USE_API_NOISE
bool APIConnection::send_noise_encryption_set_key_response(const NoiseEncryptionSetKeyRequest &msg) {
  NoiseEncryptionSetKeyResponse resp;
  resp.success = false;

  psk_t psk{};
  if (msg.key.empty()) {
    if (this->parent_->clear_noise_psk(true)) {
      resp.success = true;
    } else {
      ESP_LOGW(TAG, "Failed to clear encryption key");
    }
  } else if (base64_decode(msg.key, psk.data(), psk.size()) != psk.size()) {
    ESP_LOGW(TAG, "Invalid encryption key length");
  } else if (!this->parent_->save_noise_psk(psk, true)) {
    ESP_LOGW(TAG, "Failed to save encryption key");
  } else {
    resp.success = true;
  }

  return this->send_message(resp, NoiseEncryptionSetKeyResponse::MESSAGE_TYPE);
}
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
void APIConnection::subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) {
  state_subs_at_ = 0;
}
#endif
bool APIConnection::try_to_clear_buffer(bool log_out_of_space) {
  if (this->flags_.remove)
    return false;
  if (this->helper_->can_write_without_blocking())
    return true;
  delay(0);
  APIError err = this->helper_->loop();
  if (err != APIError::OK) {
    this->fatal_error_with_log_(LOG_STR("Socket operation failed"), err);
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
    this->fatal_error_with_log_(LOG_STR("Packet write failed"), err);
    return false;
  }
  // Do not set last_traffic_ on send
  return true;
}
#ifdef USE_API_PASSWORD
void APIConnection::on_unauthenticated_access() {
  this->on_fatal_error();
  ESP_LOGD(TAG, "%s (%s) no authentication", this->client_info_.name.c_str(), this->client_info_.peername.c_str());
}
#endif
void APIConnection::on_no_setup_connection() {
  this->on_fatal_error();
  ESP_LOGD(TAG, "%s (%s) no connection setup", this->client_info_.name.c_str(), this->client_info_.peername.c_str());
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
      // Replace with new creator
      item.creator = creator;
      return;
    }
  }

  // No existing item found, add new one
  items.emplace_back(entity, creator, message_type, estimated_size);
}

void APIConnection::DeferredBatch::add_item_front(EntityBase *entity, MessageCreator creator, uint8_t message_type,
                                                  uint8_t estimated_size) {
  // Add high priority message and swap to front
  // This avoids expensive vector::insert which shifts all elements
  // Note: We only ever have one high-priority message at a time (ping OR disconnect)
  // If we're disconnecting, pings are blocked, so this simple swap is sufficient
  items.emplace_back(entity, creator, message_type, estimated_size);
  if (items.size() > 1) {
    // Swap the new high-priority item to the front
    std::swap(items.front(), items.back());
  }
}

bool APIConnection::schedule_batch_() {
  if (!this->flags_.batch_scheduled) {
    this->flags_.batch_scheduled = true;
    this->deferred_batch_.batch_start_time = App.get_loop_component_start_time();
  }
  return true;
}

void APIConnection::process_batch_() {
  // Ensure PacketInfo remains trivially destructible for our placement new approach
  static_assert(std::is_trivially_destructible<PacketInfo>::value,
                "PacketInfo must remain trivially destructible with this placement-new approach");

  if (this->deferred_batch_.empty()) {
    this->flags_.batch_scheduled = false;
    return;
  }

  // Try to clear buffer first
  if (!this->try_to_clear_buffer(true)) {
    // Can't write now, we'll try again later
    return;
  }

  // Get shared buffer reference once to avoid multiple calls
  auto &shared_buf = this->parent_->get_shared_buffer_ref();
  size_t num_items = this->deferred_batch_.size();

  // Fast path for single message - allocate exact size needed
  if (num_items == 1) {
    const auto &item = this->deferred_batch_[0];

    // Let the creator calculate size and encode if it fits
    uint16_t payload_size =
        item.creator(item.entity, this, std::numeric_limits<uint16_t>::max(), true, item.message_type);

    if (payload_size > 0 && this->send_buffer(ProtoWriteBuffer{&shared_buf}, item.message_type)) {
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

  size_t packets_to_process = std::min(num_items, MAX_PACKETS_PER_BATCH);

  // Stack-allocated array for packet info
  alignas(PacketInfo) char packet_info_storage[MAX_PACKETS_PER_BATCH * sizeof(PacketInfo)];
  PacketInfo *packet_info = reinterpret_cast<PacketInfo *>(packet_info_storage);
  size_t packet_count = 0;

  // Cache these values to avoid repeated virtual calls
  const uint8_t header_padding = this->helper_->frame_header_padding();
  const uint8_t footer_size = this->helper_->frame_footer_size();

  // Initialize buffer and tracking variables
  shared_buf.clear();

  // Pre-calculate exact buffer size needed based on message types
  uint32_t total_estimated_size = num_items * (header_padding + footer_size);
  for (size_t i = 0; i < this->deferred_batch_.size(); i++) {
    const auto &item = this->deferred_batch_[i];
    total_estimated_size += item.estimated_size;
  }

  // Calculate total overhead for all messages
  // Reserve based on estimated size (much more accurate than 24-byte worst-case)
  shared_buf.reserve(total_estimated_size);
  this->flags_.batch_first_message = true;

  size_t items_processed = 0;
  uint16_t remaining_size = std::numeric_limits<uint16_t>::max();

  // Track where each message's header padding begins in the buffer
  // For plaintext: this is where the 6-byte header padding starts
  // For noise: this is where the 7-byte header padding starts
  // The actual message data follows after the header padding
  uint32_t current_offset = 0;

  // Process items and encode directly to buffer (up to our limit)
  for (size_t i = 0; i < packets_to_process; i++) {
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
    // Use placement new to construct PacketInfo in pre-allocated stack array
    // This avoids default-constructing all MAX_PACKETS_PER_BATCH elements
    // Explicit destruction is not needed because PacketInfo is trivially destructible,
    // as ensured by the static_assert in its definition.
    new (&packet_info[packet_count++]) PacketInfo(item.message_type, current_offset, proto_payload_size);

    // Update tracking variables
    items_processed++;
    // After first message, set remaining size to MAX_BATCH_PACKET_SIZE to avoid fragmentation
    if (items_processed == 1) {
      remaining_size = MAX_BATCH_PACKET_SIZE;
    }
    remaining_size -= payload_size;
    // Calculate where the next message's header padding will start
    // Current buffer size + footer space for this message
    current_offset = shared_buf.size() + footer_size;
  }

  if (items_processed == 0) {
    this->deferred_batch_.clear();
    return;
  }

  // Add footer space for the last message (for Noise protocol MAC)
  if (footer_size > 0) {
    shared_buf.resize(shared_buf.size() + footer_size);
  }

  // Send all collected packets
  APIError err = this->helper_->write_protobuf_packets(ProtoWriteBuffer{&shared_buf},
                                                       std::span<const PacketInfo>(packet_info, packet_count));
  if (err != APIError::OK && err != APIError::WOULD_BLOCK) {
    this->fatal_error_with_log_(LOG_STR("Batch write failed"), err);
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
    // Remove processed items from the beginning
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
  // Special case: EventResponse uses const char * pointer
  if (message_type == EventResponse::MESSAGE_TYPE) {
    auto *e = static_cast<event::Event *>(entity);
    return APIConnection::try_send_event_response(e, data_.const_char_ptr, conn, remaining_size, is_single);
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

#ifdef USE_API_HOMEASSISTANT_STATES
void APIConnection::process_state_subscriptions_() {
  const auto &subs = this->parent_->get_state_subs();
  if (this->state_subs_at_ >= static_cast<int>(subs.size())) {
    this->state_subs_at_ = -1;
    return;
  }

  const auto &it = subs[this->state_subs_at_];
  SubscribeHomeAssistantStateResponse resp;
  resp.set_entity_id(StringRef(it.entity_id));

  // Avoid string copy by using the const char* pointer if it exists
  resp.set_attribute(it.attribute != nullptr ? StringRef(it.attribute) : StringRef(""));

  resp.once = it.once;
  if (this->send_message(resp, SubscribeHomeAssistantStateResponse::MESSAGE_TYPE)) {
    this->state_subs_at_++;
  }
}
#endif  // USE_API_HOMEASSISTANT_STATES

void APIConnection::log_warning_(const LogString *message, APIError err) {
  ESP_LOGW(TAG, "%s (%s): %s %s errno=%d", this->client_info_.name.c_str(), this->client_info_.peername.c_str(),
           LOG_STR_ARG(message), LOG_STR_ARG(api_error_to_logstr(err)), errno);
}

}  // namespace esphome::api
#endif
