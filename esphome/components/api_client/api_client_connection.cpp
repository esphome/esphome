#include "api_client_connection.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "esphome/core/version.h"
#include "esphome/components/logger/logger.h"
#include <string>

namespace esphome {
namespace api {

static const char *TAG = "api.client_connection";

APIClientConnection::APIClientConnection() {
  this->send_buffer_.reserve(64);
  this->recv_buffer_.reserve(32);
}
APIClientConnection::~APIClientConnection() {}
void APIClientConnection::setup() {
    last_traffic_ = millis();
    HelloRequest req;
    req.client_info = App.get_name();
    this->send_hello_request(req);
}
void APIClientConnection::parse_recv_buffer_() {
    if (this->recv_buffer_.empty())
        return;

    ESP_LOGV(TAG, "Receive buffer size: %u", this->recv_buffer_.size());
    if (this->recv_buffer_[0] != 0x00) {
        ESP_LOGE(TAG, "Invalid preamble from %s : %02x", this->server_info_.c_str(), this->recv_buffer_[0]);
        this->on_fatal_error();
        return;
    }
    uint32_t i = 1;
    const uint32_t size = this->recv_buffer_.size();
    uint32_t consumed;
    auto msg_size_varint = ProtoVarInt::parse(&this->recv_buffer_[i], size - i, &consumed);
    if (!msg_size_varint.has_value()) {
        ESP_LOGVV(TAG, "Insufficient data to read msg_size");
        return;
    }
    i += consumed;
    uint32_t msg_size = msg_size_varint->as_uint32();
    ESP_LOGVV(TAG, "msg_size: %u", msg_size);

    auto msg_type_varint = ProtoVarInt::parse(&this->recv_buffer_[i], size - i, &consumed);
    if (!msg_type_varint.has_value()) {
        ESP_LOGVV(TAG, "Insufficient data to read msg_type");
        return;
    }
    i += consumed;
    uint32_t msg_type = msg_type_varint->as_uint32();
    ESP_LOGVV(TAG, "msg_type: %u", msg_type);

    static uint32_t loop_counter = 0;
    if (size - i < msg_size) {
        loop_counter++;
        ESP_LOGVV(TAG, "Insufficient data to read entire message, got %u, need %u", size - i, msg_size);
        if (loop_counter == 25) {
            ESP_LOGE(TAG, "Not making progress, clearing the buffer");
            this->recv_buffer_.clear();
        }
        return;
    } else {
        loop_counter = 0;
    }
    uint8_t *msg = &this->recv_buffer_[i];
    this->read_message(msg_size, msg_type, msg);
    // pop front
    uint32_t total = i + msg_size;
    this->recv_buffer_.erase(this->recv_buffer_.begin(), this->recv_buffer_.begin() + total);
    this->last_traffic_ = millis();
    ESP_LOGV(TAG, "Receive buffer size: %u", this->recv_buffer_.size());
}
void APIClientConnection::register_mapping_(uint32_t key, Nameable *obj) {
    Nameable *entity = key_map_[key];
    if (entity != nullptr) {
        ESP_LOGW(TAG, "Key %d already mapped to %s, attempt to overwrite with %s", 
                 key, entity->get_name().c_str(), obj->get_name().c_str());
        return;
    }
    key_map_[key] = obj;
}
bool APIClientConnection::send_buffer(ProtoWriteBuffer buffer, uint32_t message_type) {
  std::vector<uint8_t> header;
  header.push_back(0x00);
  ProtoVarInt(buffer.get_buffer()->size()).encode(header);
  ProtoVarInt(message_type).encode(header);

  size_t needed_space = buffer.get_buffer()->size() + header.size();

  if (needed_space > this->space()) {
    delay(0);
    if (needed_space > this->space()) {
      // SubscribeLogsResponse
      if (message_type != 29) {
        ESP_LOGV(TAG, "Cannot send message because of Send buffer space");
      }
      delay(0);
      return false;
    }
  }

  return this->send_buffer(header, buffer);
}
float APIClientConnection::get_setup_priority() const { return setup_priority::PROCESSOR; }
void APIClientConnection::loop() {
  this->parse_recv_buffer_();

  const uint32_t keepalive = 10000;
  if (this->sent_ping_) {
    // Disconnect if not responded within 2.5*keepalive
    if (millis() - this->last_traffic_ > (keepalive * 5) / 2) {
      ESP_LOGW(TAG, "'%s' didn't respond to ping request in time. Disconnecting...", this->server_info_.c_str());
      this->disconnect_client();
      return;
    }
  } else if (millis() - this->last_traffic_ > keepalive) {
    this->sent_ping_ = true;
    this->send_ping_request(PingRequest());
  }
}
#ifdef USE_BINARY_SENSOR
void APIClientConnection::on_binary_sensor_state_response(const BinarySensorStateResponse &value){
    Nameable *entity = key_map_[value.key];
    if (entity != nullptr) {
        uint32_t key = entity->get_object_id_hash();
        auto *obj = App.get_binary_sensor_by_key(key);
        if (obj != nullptr)
            obj->publish_state(value.state);
    }
}
void APIClientConnection::on_list_entities_binary_sensor_response(const ListEntitiesBinarySensorResponse &value){
    ESP_LOGD(TAG, "binary_sensor:");
    ESP_LOGD(TAG, "  - platform: api_relay");
    ESP_LOGD(TAG, "    id: %s", value.object_id.c_str());
    ESP_LOGD(TAG, "    key: %u", value.key);
    ESP_LOGD(TAG, "    name: %s", value.name.c_str());
    ESP_LOGD(TAG, "    device_class: %s", value.device_class.c_str());
    ESP_LOGD(TAG, "    is_status_binary_sensor: %s", value.is_status_binary_sensor ? "true" : "false");
}
void APIClientConnection::register_binary_sensor(uint32_t key, binary_sensor::BinarySensor *obj){
    this->register_mapping_(key, obj);
}
#endif
#ifdef USE_COVER
void APIClientConnection::on_list_entities_cover_response(const ListEntitiesCoverResponse &value){
}
void APIClientConnection::on_cover_state_response(const CoverStateResponse &value){
}
#endif
#ifdef USE_FAN
void APIClientConnection::on_list_entities_fan_response(const ListEntitiesFanResponse &value){
}
void APIClientConnection::on_fan_state_response(const FanStateResponse &value){
}
#endif
#ifdef USE_LIGHT
void APIClientConnection::on_list_entities_light_response(const ListEntitiesLightResponse &value){
    ESP_LOGD(TAG, "light:");
    ESP_LOGD(TAG, "  - platform: api_relay");
    ESP_LOGD(TAG, "    id: %s", value.object_id.c_str());
    ESP_LOGD(TAG, "    key: %u", value.key);
    ESP_LOGD(TAG, "    name: %s", value.name.c_str());
    ESP_LOGD(TAG, "    supports_brightness: %s", value.supports_brightness ? "true" : "false");
    ESP_LOGD(TAG, "    supports_rgb: %s", value.supports_rgb ? "true" : "false");
    ESP_LOGD(TAG, "    supports_white_value: %s", value.supports_white_value ? "true" : "false");
    ESP_LOGD(TAG, "    supports_color_temperature: %s", value.supports_color_temperature ? "true" : "false");
    ESP_LOGD(TAG, "    min_mireds: %f", value.min_mireds);
    ESP_LOGD(TAG, "    max_mireds: %f", value.max_mireds);
}
void APIClientConnection::on_light_state_response(const LightStateResponse &value){
    Nameable *entity = key_map_[value.key];
    if (entity != nullptr) {
        uint32_t key = entity->get_object_id_hash();
        auto *obj = App.get_light_by_key(key);
        if (obj != nullptr)
            obj->publish_state(value.state);
    }
}
void APIClientConnection::register_light(uint32_t key, light::Light *obj){
    this->register_mapping_(key, obj);
}
#endif
#ifdef USE_SENSOR
void APIClientConnection::on_list_entities_sensor_response(const ListEntitiesSensorResponse &value){
    ESP_LOGD(TAG, "sensor:");
    ESP_LOGD(TAG, "  - platform: api_relay");
    ESP_LOGD(TAG, "    id: %s", value.object_id.c_str());
    ESP_LOGD(TAG, "    key: %u", value.key);
    ESP_LOGD(TAG, "    name: %s", value.name.c_str());
    ESP_LOGD(TAG, "    icon: %s", value.icon.c_str());
    ESP_LOGD(TAG, "    unit_of_measurement: %s", value.unit_of_measurement.c_str());
    ESP_LOGD(TAG, "    accuracy_decimals: %d", value.accuracy_decimals);
    ESP_LOGD(TAG, "    device_class: %s", value.device_class.c_str());
}
void APIClientConnection::on_sensor_state_response(const SensorStateResponse &value){
    Nameable *entity = key_map_[value.key];
    if (entity != nullptr) {
        uint32_t key = entity->get_object_id_hash();
        auto *obj = App.get_sensor_by_key(key);
        if (obj != nullptr)
            obj->publish_state(value.state);
    }
}
void APIClientConnection::register_sensor(uint32_t key, sensor::Sensor *obj){
    this->register_mapping_(key, obj);
}
#endif
#ifdef USE_SWITCH
void APIClientConnection::on_list_entities_switch_response(const ListEntitiesSwitchResponse &value){
    ESP_LOGD(TAG, "switch:");
    ESP_LOGD(TAG, "  - platform: api_relay");
    ESP_LOGD(TAG, "    id: %s", value.object_id.c_str());
    ESP_LOGD(TAG, "    key: %u", value.key);
    ESP_LOGD(TAG, "    name: %s", value.name.c_str());
    ESP_LOGD(TAG, "    icon: %s", value.icon.c_str());
}
void APIClientConnection::on_switch_state_response(const SwitchStateResponse &value){
    Nameable *entity = key_map_[value.key];
    if (entity != nullptr) {
        uint32_t key = entity->get_object_id_hash();
        auto *obj = App.get_switch_by_key(key);
        if (obj != nullptr)
            obj->publish_state(value.state);
    }
}
void APIClientConnection::register_switch(uint32_t key, switch_::Switch *obj){
    this->register_mapping_(key, obj);
}
#endif
#ifdef USE_TEXT_SENSOR
void APIClientConnection::on_list_entities_text_sensor_response(const ListEntitiesTextSensorResponse &value){
    ESP_LOGD(TAG, "text_sensor:");
    ESP_LOGD(TAG, "  - platform: api_relay");
    ESP_LOGD(TAG, "    id: %s", value.object_id.c_str());
    ESP_LOGD(TAG, "    key: %u", value.key);
    ESP_LOGD(TAG, "    name: %s", value.name.c_str());
    ESP_LOGD(TAG, "    icon: %s", value.icon.c_str());
}
void APIClientConnection::on_text_sensor_state_response(const TextSensorStateResponse &value){
    Nameable *entity = key_map_[value.key];
    if (entity != nullptr) {
        uint32_t key = entity->get_object_id_hash();
        auto *obj = App.get_text_sensor_by_key(key);
        if (obj != nullptr) {
            obj->publish_state(value.state);
        }
    }
}
void APIClientConnection::register_text_sensor(uint32_t key, text_sensor::TextSensor *obj){
    this->register_mapping_(key, obj);
}
#endif
void APIClientConnection::on_subscribe_logs_response(const SubscribeLogsResponse &value){
    if (logger::global_logger != nullptr && value.level <= ESPHOME_LOG_LEVEL) {
        size_t i1 = value.message.find_first_of('[', 7);
        size_t i2 = -1, i3 = -1;
        std::string tag;
        if (i1 >= 0)
            i1 = value.message.find_first_of('[', i1+1);
        if (i1 > 0)
            i2 = value.message.find_first_of(']', i1+1);
        if (i2 > i1)
            tag = value.message.substr(i1+1, i2-i1-1);
        if (!tag.empty()) {
            i3 = tag.find_first_of(':');
            int line = atoi(tag.substr(i3+1).c_str());
            tag = tag.substr(0, i3);
            std::string message = value.message.substr(i2+2);
            logger::global_logger->log_vprintf_(value.level, tag.c_str(), line, message.c_str(), va_list());
        }
    }
}
void APIClientConnection::on_homeassistant_service_response(const HomeassistantServiceResponse &value){
}
void APIClientConnection::on_subscribe_home_assistant_state_response(const SubscribeHomeAssistantStateResponse &value){
}
void APIClientConnection::on_get_time_request(const GetTimeRequest &value){
}
void APIClientConnection::on_get_time_response(const GetTimeResponse &value){
}
void APIClientConnection::on_list_entities_services_response(const ListEntitiesServicesResponse &value){
}
void APIClientConnection::on_hello_response(const HelloResponse &value){
    this->server_info_ = value.server_info;
    if (value.api_version_major != 1) {
        this->on_fatal_error();
        return;
    }
    if (value.api_version_minor != 4) {
        ESP_LOGW(TAG, "API minor version mismatch, sent %d but got %d", 4, value.api_version_minor);
    }
    DeviceInfoRequest req;
    if (!this->send_device_info_request(req)) {
        ESP_LOGE(TAG, "Failed to send device_info_request");
        this->on_fatal_error();
    }
}
void APIClientConnection::on_device_info_response(const DeviceInfoResponse &value){
    ConnectRequest req;
    if (value.uses_password)
        req.password = password_;
    if (!this->send_connect_request(req)) {
        ESP_LOGE(TAG, "Failed to send connect_request");
        this->on_fatal_error();
    }
}
void APIClientConnection::on_connect_response(const ConnectResponse &value){
    if (value.invalid_password) {
        ESP_LOGE(TAG, "Incorrect password");
        this->on_fatal_error();
        return;
    }
    // SubscribeLogsRequest req;
    // req.level = static_cast<enums::LogLevel>(4 /*ESPHOME_LOG_LEVEL*/);
    // req.dump_config = true;
    // if (!this->send_subscribe_logs_request(req)) {
    //     ESP_LOGE(TAG, "Failed to send subscribe_logs_request");
    //     this->on_fatal_error();
    //     return;
    // }
    ListEntitiesRequest req;
    if (!this->send_list_entities_request(req)) {
        ESP_LOGE(TAG, "Failed to send list_entities_request");
        this->on_fatal_error();
        return;
    }
}
void APIClientConnection::on_disconnect_request(const DisconnectRequest &value){
    DisconnectResponse resp;
    if (!this->send_disconnect_response(resp)) {
        ESP_LOGE(TAG, "Error sending disconnect response");
        this->on_fatal_error();
        return;
    }
    this->disconnect_client();
}
void APIClientConnection::on_disconnect_response(const DisconnectResponse &value){
    this->disconnect_client();
}
void APIClientConnection::on_ping_request(const PingRequest &value){
    PingResponse resp;
    if (!this->send_ping_response(resp)) {
        ESP_LOGE(TAG, "Error sending ping response");
        this->on_fatal_error();
    }
}
void APIClientConnection::on_ping_response(const PingResponse &value){
    this->sent_ping_ = false;
    this->last_traffic_ = millis();
}
void APIClientConnection::on_list_entities_done_response(const ListEntitiesDoneResponse &value){
    SubscribeStatesRequest req;
    if (!this->send_subscribe_states_request(req)) {
        ESP_LOGE(TAG, "Error sending subscribe states request");
        this->on_fatal_error();
    }
}
#ifdef USE_ESP32_CAMERA
void APIClientConnection::on_list_entities_camera_response(const ListEntitiesCameraResponse &value){
}
void APIClientConnection::on_camera_image_response(const CameraImageResponse &value){
}
#endif
#ifdef USE_CLIMATE
void APIClientConnection::on_list_entities_climate_response(const ListEntitiesClimateResponse &value){
}
void APIClientConnection::on_climate_state_response(const ClimateStateResponse &value){
}
#endif

StreamAPIClientConnection::StreamAPIClientConnection() {}
StreamAPIClientConnection::~StreamAPIClientConnection() {}
void StreamAPIClientConnection::loop() {
  size_t available = this->client_->available();
  uint8_t buf[32];
  if (available) {
    while (available) {
      if (available > sizeof(buf))
        available = sizeof(buf);
      size_t read = this->client_->readBytes(buf, available);
      if (read > 0)
        this->recv_buffer_.insert(this->recv_buffer_.end(), buf, buf+read);
      available = this->client_->available();
    }
    this->last_traffic_ = millis();
  }
  APIClientConnection::loop();
}
bool StreamAPIClientConnection::send_buffer(std::vector<uint8_t> header, ProtoWriteBuffer buffer){
  size_t wrote, expected;

  expected = header.size();
  wrote = this->client_->write(reinterpret_cast<uint8_t *>(header.data()), header.size());
  if (wrote != expected) {
    ESP_LOGE(TAG, "Wrote %d, expected %d", wrote, expected);
    return false;
  }
  expected = buffer.get_buffer()->size();
  wrote = this->client_->write(reinterpret_cast<uint8_t *>(buffer.get_buffer()->data()), buffer.get_buffer()->size());
  if (wrote != expected) {
    ESP_LOGE(TAG, "Wrote %d, expected %d", wrote, expected);
    return false;
  }
  return true;
}
void StreamAPIClientConnection::on_fatal_error(){
  ESP_LOGE(TAG, "Error: Disconnecting %s", this->server_info_.c_str());
  this->connection_state_ = ConnectionState::WAITING_FOR_HELLO;

  this->last_traffic_ = millis();
  this->sent_ping_ = false;
  this->recv_buffer_.clear();
  this->set_timeout("retry", 15000, [this]() {
    HelloRequest req;
    req.client_info = App.get_name();
    this->send_hello_request(req);
  });
}
void StreamAPIClientConnection::disconnect_client(){
    ESP_LOGE(TAG, "Disconnecting client, will retry");
    this->connection_state_ = ConnectionState::WAITING_FOR_HELLO;

    this->last_traffic_ = millis();
    this->sent_ping_ = false;
    this->recv_buffer_.clear();
    this->set_timeout("retry", 5000, [this]() {
        HelloRequest req;
        req.client_info = App.get_name();
        this->send_hello_request(req);
    });
}

}  // namespace api
}  // namespace esphome
