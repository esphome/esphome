#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#ifdef USE_ESP32_CAMERA
#include "esphome/components/esp32_camera/esp32_camera.h"
#endif

namespace esphome {
namespace api {

class APIBuffer {
 public:
  APIBuffer(std::vector<uint8_t> *buffer);

  size_t get_length() const;
  void write(uint8_t value);

  void encode_int32(uint32_t field, int32_t value, bool force = false);
  void encode_uint32(uint32_t field, uint32_t value, bool force = false);
  void encode_sint32(uint32_t field, int32_t value, bool force = false);
  void encode_bool(uint32_t field, bool value, bool force = false);
  void encode_string(uint32_t field, const std::string &value);
  void encode_string(uint32_t field, const char *string, size_t len);
  void encode_bytes(uint32_t field, const uint8_t *data, size_t len);
  void encode_fixed32(uint32_t field, uint32_t value, bool force = false);
  void encode_float(uint32_t field, float value, bool force = false);
  void encode_nameable(Nameable *nameable);

  size_t begin_nested(uint32_t field);
  void end_nested(size_t begin_index);

  void encode_field_raw(uint32_t field, uint32_t type);
  void encode_varint_raw(uint32_t value);

 protected:
  std::vector<uint8_t> *buffer_;
};

optional<uint32_t> proto_decode_varuint32(const uint8_t *buf, size_t len, uint32_t *consumed = nullptr);

std::string as_string(const uint8_t *value, size_t len);
int32_t as_sint32(uint32_t val);
float as_float(uint32_t val);

class APIServer;
class UserServiceDescriptor;

class ComponentIterator {
 public:
  ComponentIterator(APIServer *server);

  void begin();
  void advance();
  virtual bool on_begin();
#ifdef USE_BINARY_SENSOR
  virtual bool on_binary_sensor(binary_sensor::BinarySensor *binary_sensor) = 0;
#endif
#ifdef USE_COVER
  virtual bool on_cover(cover::Cover *cover) = 0;
#endif
#ifdef USE_FAN
  virtual bool on_fan(fan::FanState *fan) = 0;
#endif
#ifdef USE_LIGHT
  virtual bool on_light(light::LightState *light) = 0;
#endif
#ifdef USE_SENSOR
  virtual bool on_sensor(sensor::Sensor *sensor) = 0;
#endif
#ifdef USE_SWITCH
  virtual bool on_switch(switch_::Switch *a_switch) = 0;
#endif
#ifdef USE_TEXT_SENSOR
  virtual bool on_text_sensor(text_sensor::TextSensor *text_sensor) = 0;
#endif
  virtual bool on_service(UserServiceDescriptor *service);
#ifdef USE_ESP32_CAMERA
  virtual bool on_camera(esp32_camera::ESP32Camera *camera);
#endif
#ifdef USE_CLIMATE
  virtual bool on_climate(climate::Climate *climate) = 0;
#endif
  virtual bool on_end();

 protected:
  enum class IteratorState {
    NONE = 0,
    BEGIN,
#ifdef USE_BINARY_SENSOR
    BINARY_SENSOR,
#endif
#ifdef USE_COVER
    COVER,
#endif
#ifdef USE_FAN
    FAN,
#endif
#ifdef USE_LIGHT
    LIGHT,
#endif
#ifdef USE_SENSOR
    SENSOR,
#endif
#ifdef USE_SWITCH
    SWITCH,
#endif
#ifdef USE_TEXT_SENSOR
    TEXT_SENSOR,
#endif
    SERVICE,
#ifdef USE_ESP32_CAMERA
    CAMERA,
#endif
#ifdef USE_CLIMATE
    CLIMATE,
#endif
    MAX,
  } state_{IteratorState::NONE};
  size_t at_{0};

  APIServer *server_;
};

}  // namespace api
}  // namespace esphome
