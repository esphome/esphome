#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "esphome/components/genibus/genibus.h"
#include "esphome/core/defines.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::grundfos_alpha {

enum class Reading : uint8_t {
  READING_FLOW,
  READING_HEAD,
  READING_MEDIUM_TEMPERATURE,
  READING_VOLTAGE,
  READING_CURRENT,
  READING_POWER,
  READING_SPEED,
};

/// A big-endian float at offset in the body of an object, multiplied by factor before it is published.
struct Field {
  Reading reading;
  uint8_t offset;
  float factor;
};

/// A class 10 object as one model sends it. A reply of another type, version or size is not decoded.
struct ObjectLayout {
  uint8_t object;
  uint16_t sub_id;
  uint16_t type;
  uint8_t version;
  uint8_t size;
  std::span<const Field> fields;
};

/// The objects read from a pump of one unit family, type and version.
struct Model {
  uint8_t family;
  uint8_t type;
  uint8_t version;
  uint8_t name;  // index into ModelNames
  std::span<const ObjectLayout> objects;
};

// Values of current_ besides an object index
static constexpr uint8_t REQUEST_IDENTITY = 0xFE;
static constexpr uint8_t REQUEST_NONE = 0xFF;

/// A Grundfos ALPHA circulator pump. It reads the pump's unit family, type and version once, then polls the
/// objects of that model. It only reads; it never writes to the pump.
class GrundfosAlpha final : public genibus::GenibusDevice {
#ifdef USE_SENSOR
  SUB_SENSOR(flow)
  SUB_SENSOR(head)
  SUB_SENSOR(medium_temperature)
  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(power)
  SUB_SENSOR(speed)
#endif

 public:
  void update() override;
  void dump_config() override;
  void on_response(const genibus::GenibusResponse &response) override;
  void on_connected() override {
    this->missing_ = 0;
    this->update();
  }
  void on_disconnected() override { this->publish_unknown_(); }

 protected:
  bool identify_(const genibus::GenibusResponse &response);
  void decode_(uint8_t index, const genibus::GenibusResponse &response);
  void request_from_(size_t index);
  bool wants_(const ObjectLayout &layout) const;
  void publish_(Reading reading, float value);
  void publish_unknown_();
  bool first_warning_(uint8_t bit);
#ifdef USE_SENSOR
  sensor::Sensor *sensor_(Reading reading) const;
#endif

  const Model *model_{nullptr};
  uint8_t current_{REQUEST_NONE};  // index of the object on the wire, or REQUEST_IDENTITY
  uint8_t warned_{0};
  uint8_t missing_{0};  // a bit per object index the pump does not have
  bool unsupported_{false};
  bool has_values_{false};
};

}  // namespace esphome::grundfos_alpha
