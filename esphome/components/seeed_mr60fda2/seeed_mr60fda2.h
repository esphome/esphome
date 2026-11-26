#pragma once
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/helpers.h"

#include <map>

namespace esphome::seeed_mr60fda2 {

static constexpr uint8_t DATA_BUF_MAX_SIZE = 28;
static constexpr uint8_t LEN_TO_HEAD_CKSUM = 8;
static constexpr uint8_t LEN_TO_DATA_FRAME = 9;
static constexpr uint8_t FRAME_BUF_MAX_SIZE = DATA_BUF_MAX_SIZE + LEN_TO_DATA_FRAME;

static constexpr uint8_t FRAME_HEADER_BUFFER = 0x01;

/// 01, id (2), data length (2, BE), type (2), header checksum, data, data checksum; 0 for a bad header
struct TinyFrameLength {
  static constexpr size_t KNOWN_AT = LEN_TO_HEAD_CKSUM;
  static size_t get(const uint8_t *buf);
};
static const uint16_t IS_FALL_TYPE_BUFFER = 0x0E02;
static const uint16_t PEOPLE_EXIST_TYPE_BUFFER = 0x0F09;
static const uint16_t RESULT_INSTALL_HEIGHT = 0x0E04;
static const uint16_t RESULT_PARAMETERS = 0x0E06;
static const uint16_t RESULT_HEIGHT_THRESHOLD = 0x0E08;
static const uint16_t RESULT_SENSITIVITY = 0x0E0A;

class MR60FDA2Component final : public Component,
                                public uart::UARTDevice {  // The class name must be the name defined by text_sensor.py
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(people_exist)
  SUB_BINARY_SENSOR(fall_detected)
#endif
#ifdef USE_BUTTON
  SUB_BUTTON(get_radar_parameters)
  SUB_BUTTON(factory_reset)
#endif
#ifdef USE_SELECT
  SUB_SELECT(install_height)
  SUB_SELECT(height_threshold)
  SUB_SELECT(sensitivity)
#endif

 protected:
  uint32_t last_rx_{0};
  LengthFrameReader<FRAME_BUF_MAX_SIZE, FrameFormat<FrameHeader<FRAME_HEADER_BUFFER>, TinyFrameLength,
                                                    FrameMinLength<LEN_TO_DATA_FRAME + 1>>>
      reader_;

  void process_frame_(const uint8_t *frame);

 public:
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_install_height(uint8_t index);
  void set_height_threshold(uint8_t index);
  void set_sensitivity(uint8_t index);
  void get_radar_parameters();
  void factory_reset();
};

}  // namespace esphome::seeed_mr60fda2
