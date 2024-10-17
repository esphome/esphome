#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include <map>

namespace esphome {
namespace seeed_mr60bha2 {

static const uint8_t DATA_BUF_MAX_SIZE = 12;
static const uint8_t FRAME_BUF_MAX_SIZE = 21;
static const uint8_t LEN_TO_HEAD_CKSUM = 8;
static const uint8_t LEN_TO_DATA_FRAME = 9;

static const uint8_t FRAME_HEADER_BUFFER = 0x01;
static const uint16_t BREATH_RATE_TYPE_BUFFER = 0x0A14;
static const uint16_t HEART_RATE_TYPE_BUFFER = 0x0A15;
static const uint16_t DISTANCE_TYPE_BUFFER = 0x0A16;

enum FrameLocation {
  LOCATE_FRAME_HEADER,
  LOCATE_ID_FRAME1,
  LOCATE_ID_FRAME2,
  LOCATE_LENGTH_FRAME_H,
  LOCATE_LENGTH_FRAME_L,
  LOCATE_TYPE_FRAME1,
  LOCATE_TYPE_FRAME2,
  LOCATE_HEAD_CKSUM_FRAME,  // Header checksum: [from the first byte to the previous byte of the HEAD_CKSUM bit]
  LOCATE_DATA_FRAME,
  LOCATE_DATA_CKSUM_FRAME,  // Data checksum: [from the first to the previous byte of the DATA_CKSUM bit]
  LOCATE_PROCESS_FRAME,
};

class MR60BHA2Component : public Component,
                          public uart::UARTDevice {  // The class name must be the name defined by text_sensor.py
#ifdef USE_SENSOR
  SUB_SENSOR(breath_rate);
  SUB_SENSOR(heart_rate);
  SUB_SENSOR(distance);
#endif

 protected:
  uint8_t current_frame_locate_;
  uint8_t current_frame_buf_[FRAME_BUF_MAX_SIZE];
  uint8_t current_data_buf_[DATA_BUF_MAX_SIZE];
  uint16_t current_frame_id_;
  size_t current_frame_len_;
  size_t current_data_frame_len_;
  uint16_t current_frame_type_;
  uint32_t current_breath_rate_int_;
  uint32_t current_heart_rate_int_;
  uint32_t current_distance_int_;

  bool validate_checksum_(const uint8_t *data, size_t len, uint8_t expected_checksum);
  uint8_t calculate_checksum_(const uint8_t *data, size_t len);
  void split_frame_(uint8_t buffer);
  void process_frame_();

 public:
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void setup() override;
  void dump_config() override;
  void loop() override;
};

}  // namespace seeed_mr60bha2
}  // namespace esphome
