#include "util.h"
#include "api_server.h"
#include "user_services.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace api {

APIBuffer::APIBuffer(std::vector<uint8_t> *buffer) : buffer_(buffer) {}
size_t APIBuffer::get_length() const { return this->buffer_->size(); }
void APIBuffer::write(uint8_t value) { this->buffer_->push_back(value); }
void APIBuffer::encode_uint32(uint32_t field, uint32_t value, bool force) {
  if (value == 0 && !force)
    return;

  this->encode_field_raw(field, 0);
  this->encode_varint_raw(value);
}
void APIBuffer::encode_int32(uint32_t field, int32_t value, bool force) {
  this->encode_uint32(field, static_cast<uint32_t>(value), force);
}
void APIBuffer::encode_bool(uint32_t field, bool value, bool force) {
  if (!value && !force)
    return;

  this->encode_field_raw(field, 0);
  this->write(0x01);
}
void APIBuffer::encode_string(uint32_t field, const std::string &value) {
  this->encode_string(field, value.data(), value.size());
}
void APIBuffer::encode_bytes(uint32_t field, const uint8_t *data, size_t len) {
  this->encode_string(field, reinterpret_cast<const char *>(data), len);
}
void APIBuffer::encode_string(uint32_t field, const char *string, size_t len) {
  if (len == 0)
    return;

  this->encode_field_raw(field, 2);
  this->encode_varint_raw(len);
  const uint8_t *data = reinterpret_cast<const uint8_t *>(string);
  for (size_t i = 0; i < len; i++) {
    this->write(data[i]);
  }
}
void APIBuffer::encode_fixed32(uint32_t field, uint32_t value, bool force) {
  if (value == 0 && !force)
    return;

  this->encode_field_raw(field, 5);
  this->write((value >> 0) & 0xFF);
  this->write((value >> 8) & 0xFF);
  this->write((value >> 16) & 0xFF);
  this->write((value >> 24) & 0xFF);
}
void APIBuffer::encode_float(uint32_t field, float value, bool force) {
  if (value == 0.0f && !force)
    return;

  union {
    float value_f;
    uint32_t value_raw;
  } val;
  val.value_f = value;
  this->encode_fixed32(field, val.value_raw);
}
void APIBuffer::encode_field_raw(uint32_t field, uint32_t type) {
  uint32_t val = (field << 3) | (type & 0b111);
  this->encode_varint_raw(val);
}
void APIBuffer::encode_varint_raw(uint32_t value) {
  if (value <= 0x7F) {
    this->write(value);
    return;
  }

  while (value) {
    uint8_t temp = value & 0x7F;
    value >>= 7;
    if (value) {
      this->write(temp | 0x80);
    } else {
      this->write(temp);
    }
  }
}
void APIBuffer::encode_sint32(uint32_t field, int32_t value, bool force) {
  if (value < 0)
    this->encode_uint32(field, ~(uint32_t(value) << 1), force);
  else
    this->encode_uint32(field, uint32_t(value) << 1, force);
}
void APIBuffer::encode_nameable(Nameable *nameable) {
  // string object_id = 1;
  this->encode_string(1, nameable->get_object_id());
  // fixed32 key = 2;
  this->encode_fixed32(2, nameable->get_object_id_hash());
  // string name = 3;
  this->encode_string(3, nameable->get_name());
}
size_t APIBuffer::begin_nested(uint32_t field) {
  this->encode_field_raw(field, 2);
  return this->buffer_->size();
}
void APIBuffer::end_nested(size_t begin_index) {
  const uint32_t nested_length = this->buffer_->size() - begin_index;
  // add varint
  std::vector<uint8_t> var;
  uint32_t val = nested_length;
  if (val <= 0x7F) {
    var.push_back(val);
  } else {
    while (val) {
      uint8_t temp = val & 0x7F;
      val >>= 7;
      if (val) {
        var.push_back(temp | 0x80);
      } else {
        var.push_back(temp);
      }
    }
  }
  this->buffer_->insert(this->buffer_->begin() + begin_index, var.begin(), var.end());
}

optional<uint32_t> proto_decode_varuint32(const uint8_t *buf, size_t len, uint32_t *consumed) {
  if (len == 0)
    return {};

  uint32_t result = 0;
  uint8_t bitpos = 0;

  for (uint32_t i = 0; i < len; i++) {
    uint8_t val = buf[i];
    result |= uint32_t(val & 0x7F) << bitpos;
    bitpos += 7;
    if ((val & 0x80) == 0) {
      if (consumed != nullptr) {
        *consumed = i + 1;
      }
      return result;
    }
  }

  return {};
}

std::string as_string(const uint8_t *value, size_t len) {
  return std::string(reinterpret_cast<const char *>(value), len);
}

int32_t as_sint32(uint32_t val) {
  if (val & 1)
    return uint32_t(~(val >> 1));
  else
    return uint32_t(val >> 1);
}

float as_float(uint32_t val) {
  static_assert(sizeof(uint32_t) == sizeof(float), "float must be 32bit long");
  union {
    uint32_t raw;
    float value;
  } x;
  x.raw = val;
  return x.value;
}

ComponentIterator::ComponentIterator(APIServer *server) : server_(server) {}
void ComponentIterator::begin() {
  this->state_ = IteratorState::BEGIN;
  this->at_ = 0;
}
void ComponentIterator::advance() {
  bool advance_platform = false;
  bool success = true;
  switch (this->state_) {
    case IteratorState::NONE:
      // not started
      return;
    case IteratorState::BEGIN:
      if (this->on_begin()) {
        advance_platform = true;
      } else {
        return;
      }
      break;
#ifdef USE_BINARY_SENSOR
    case IteratorState::BINARY_SENSOR:
      if (this->at_ >= App.get_binary_sensors().size()) {
        advance_platform = true;
      } else {
        auto *binary_sensor = App.get_binary_sensors()[this->at_];
        if (binary_sensor->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_binary_sensor(binary_sensor);
        }
      }
      break;
#endif
#ifdef USE_COVER
    case IteratorState::COVER:
      if (this->at_ >= App.get_covers().size()) {
        advance_platform = true;
      } else {
        auto *cover = App.get_covers()[this->at_];
        if (cover->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_cover(cover);
        }
      }
      break;
#endif
#ifdef USE_FAN
    case IteratorState::FAN:
      if (this->at_ >= App.get_fans().size()) {
        advance_platform = true;
      } else {
        auto *fan = App.get_fans()[this->at_];
        if (fan->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_fan(fan);
        }
      }
      break;
#endif
#ifdef USE_LIGHT
    case IteratorState::LIGHT:
      if (this->at_ >= App.get_lights().size()) {
        advance_platform = true;
      } else {
        auto *light = App.get_lights()[this->at_];
        if (light->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_light(light);
        }
      }
      break;
#endif
#ifdef USE_SENSOR
    case IteratorState::SENSOR:
      if (this->at_ >= App.get_sensors().size()) {
        advance_platform = true;
      } else {
        auto *sensor = App.get_sensors()[this->at_];
        if (sensor->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_sensor(sensor);
        }
      }
      break;
#endif
#ifdef USE_SWITCH
    case IteratorState::SWITCH:
      if (this->at_ >= App.get_switches().size()) {
        advance_platform = true;
      } else {
        auto *a_switch = App.get_switches()[this->at_];
        if (a_switch->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_switch(a_switch);
        }
      }
      break;
#endif
#ifdef USE_TEXT_SENSOR
    case IteratorState::TEXT_SENSOR:
      if (this->at_ >= App.get_text_sensors().size()) {
        advance_platform = true;
      } else {
        auto *text_sensor = App.get_text_sensors()[this->at_];
        if (text_sensor->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_text_sensor(text_sensor);
        }
      }
      break;
#endif
    case IteratorState ::SERVICE:
      if (this->at_ >= this->server_->get_user_services().size()) {
        advance_platform = true;
      } else {
        auto *service = this->server_->get_user_services()[this->at_];
        success = this->on_service(service);
      }
      break;
#ifdef USE_ESP32_CAMERA
    case IteratorState::CAMERA:
      if (esp32_camera::global_esp32_camera == nullptr) {
        advance_platform = true;
      } else {
        if (esp32_camera::global_esp32_camera->is_internal()) {
          advance_platform = success = true;
          break;
        } else {
          advance_platform = success = this->on_camera(esp32_camera::global_esp32_camera);
        }
      }
      break;
#endif
#ifdef USE_CLIMATE
    case IteratorState::CLIMATE:
      if (this->at_ >= App.get_climates().size()) {
        advance_platform = true;
      } else {
        auto *climate = App.get_climates()[this->at_];
        if (climate->is_internal()) {
          success = true;
          break;
        } else {
          success = this->on_climate(climate);
        }
      }
      break;
#endif
    case IteratorState::MAX:
      if (this->on_end()) {
        this->state_ = IteratorState::NONE;
      }
      return;
  }

  if (advance_platform) {
    this->state_ = static_cast<IteratorState>(static_cast<uint32_t>(this->state_) + 1);
    this->at_ = 0;
  } else if (success) {
    this->at_++;
  }
}
bool ComponentIterator::on_end() { return true; }
bool ComponentIterator::on_begin() { return true; }
bool ComponentIterator::on_service(UserServiceDescriptor *service) { return true; }
#ifdef USE_ESP32_CAMERA
bool ComponentIterator::on_camera(esp32_camera::ESP32Camera *camera) { return true; }
#endif

}  // namespace api
}  // namespace esphome
