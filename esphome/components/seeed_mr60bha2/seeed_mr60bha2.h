#pragma once
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/helpers.h"

#include <map>

namespace esphome::seeed_mr60bha2 {
static constexpr uint8_t FRAME_HEADER_BUFFER = 0x01;
// Header, id, length and type bytes, header checksum, then the data and its checksum
static constexpr uint8_t FRAME_OVERHEAD = 9;
// Point cloud frames carry 4 bytes plus 16 per target; this holds 15 targets, the others at most 8 data bytes
static constexpr uint8_t FRAME_MAX_SIZE = 255;
static const uint16_t BREATH_RATE_TYPE_BUFFER = 0x0A14;
static const uint16_t PEOPLE_EXIST_TYPE_BUFFER = 0x0F09;
static const uint16_t HEART_RATE_TYPE_BUFFER = 0x0A15;
static const uint16_t DISTANCE_TYPE_BUFFER = 0x0A16;
static const uint16_t PRINT_CLOUD_BUFFER = 0x0A04;

class MR60BHA2Component final : public Component,
                                public uart::UARTDevice {  // The class name must be the name defined by text_sensor.py
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(has_target);
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(breath_rate);
  SUB_SENSOR(heart_rate);
  SUB_SENSOR(distance);
  SUB_SENSOR(num_targets);
#endif

 public:
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void dump_config() override;
  void loop() override;

 protected:
  void handle_frame_(const uint8_t *frame, size_t len);
  void process_frame_(uint16_t frame_id, uint16_t frame_type, const uint8_t *data, size_t length);

  // 01, id (2), data length (2, high byte first), type (2), header checksum, data, data checksum
  LengthFrameReader<FRAME_MAX_SIZE, FrameFormat<FrameLengthU16BE<3, FRAME_OVERHEAD>, FRAME_HEADER_BUFFER>> reader_;
};

}  // namespace esphome::seeed_mr60bha2
