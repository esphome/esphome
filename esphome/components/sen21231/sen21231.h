#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

// ref:
// https://github.com/usefulsensors/person_sensor_pico_c/blob/main/person_sensor.h

namespace esphome {
namespace sen21231_sensor {
// The I2C address of the person sensor board.
static const uint8_t PERSON_SENSOR_I2C_ADDRESS = 0x62;
static const uint8_t PERSON_SENSOR_REG_MODE = 0x01;
static const uint8_t PERSON_SENSOR_REG_ENABLE_ID = 0x02;
static const uint8_t PERSON_SENSOR_REG_SINGLE_SHOT = 0x03;
static const uint8_t PERSON_SENSOR_REG_CALIBRATE_ID = 0x04;
static const uint8_t PERSON_SENSOR_REG_PERSIST_IDS = 0x05;
static const uint8_t PERSON_SENSOR_REG_ERASE_IDS = 0x06;
static const uint8_t PERSON_SENSOR_REG_DEBUG_MODE = 0x07;

static const uint8_t PERSON_SENSOR_MAX_FACES_COUNT = 4;
static const uint8_t PERSON_SENSOR_MAX_IDS_COUNT = 7;

// The results returned from the sensor have a short header providing
// information about the length of the data packet:
//   reserved: Currently unused bytes.
//   data_size: Length of the entire packet, excluding the header and
//   checksum.
//     For version 1.0 of the sensor, this should be 40.
using person_sensor_results_header_t = struct {
  uint8_t reserved[2];  // Bytes 0-1.
  uint16_t data_size;   // Bytes 2-3.
};

// Each face found has a set of information associated with it:
//   box_confidence: How certain we are we have found a face, from 0 to 255.
//   box_left: X coordinate of the left side of the box, from 0 to 255.
//   box_top: Y coordinate of the top edge of the box, from 0 to 255.
//   box_width: Width of the box, where 255 is the full view port size.
//   box_height: Height of the box, where 255 is the full view port size.
//   id_confidence: How sure the sensor is about the recognition result.
//   id: Numerical ID assigned to this face.
//   is_looking_at: Whether the person is facing the camera, 0 or 1.
using person_sensor_face_t = struct __attribute__((__packed__)) {
  uint8_t box_confidence;  // Byte 1.
  uint8_t box_left;        // Byte 2.
  uint8_t box_top;         // Byte 3.
  uint8_t box_right;       // Byte 4.
  uint8_t box_bottom;      // Byte 5.
  int8_t id_confidence;    // Byte 6.
  int8_t id;               // Byte 7
  uint8_t is_facing;       // Byte 8.
};

// This is the full structure of the packet returned over the wire from the
// sensor when we do an I2C read from the peripheral address.
// The checksum should be the CRC16 of bytes 0 to 38. You shouldn't need to
// verify this in practice, but we found it useful during our own debugging.
using person_sensor_results_t = struct __attribute__((__packed__)) {
  person_sensor_results_header_t header;                      // Bytes 0-4.
  int8_t num_faces;                                           // Byte 5.
  person_sensor_face_t faces[PERSON_SENSOR_MAX_FACES_COUNT];  // Bytes 6-37.
  uint16_t checksum;                                          // Bytes 38-39.
};

class Sen21231Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void update() override;
  void dump_config() override;

 protected:
  void read_data_();
};

}  // namespace sen21231_sensor
}  // namespace esphome
