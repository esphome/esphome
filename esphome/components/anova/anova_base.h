#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace anova {

enum CurrentQuery {
  NONE,
  READ_DEVICE_STATUS,
  READ_TARGET_TEMPERATURE,
  READ_CURRENT_TEMPERATURE,
  READ_DATA,
  READ_UNIT,
  SET_TARGET_TEMPERATURE,
  SET_UNIT,
  START,
  STOP,
};

struct AnovaPacket {
  uint16_t length;
  uint8_t data[24];
};

#define CMD_READ_DEVICE_STATUS "status\r"
#define CMD_READ_TARGET_TEMP "read set temp\r"
#define CMD_READ_CURRENT_TEMP "read temp\r"
#define CMD_READ_UNIT "read unit\r"
#define CMD_READ_DATA "read data\r"
#define CMD_SET_TARGET_TEMP "set temp %.1f\r"
#define CMD_SET_TEMP_UNIT "set unit %c\r"

#define CMD_START "start\r"
#define CMD_STOP "stop\r"

class AnovaCodec {
 public:
  AnovaPacket *get_read_device_status_request();
  AnovaPacket *get_read_target_temp_request();
  AnovaPacket *get_read_current_temp_request();
  AnovaPacket *get_read_data_request();
  AnovaPacket *get_read_unit_request();

  AnovaPacket *get_set_target_temp_request(float temperature);
  AnovaPacket *get_set_unit_request(char unit);

  AnovaPacket *get_start_request();
  AnovaPacket *get_stop_request();

  void decode(const uint8_t *data, uint16_t length);
  bool has_target_temp() { return this->has_target_temp_; }
  bool has_current_temp() { return this->has_current_temp_; }
  bool has_unit() { return this->has_unit_; }
  bool has_running() { return this->has_running_; }

  union {
    float target_temp_;
    float current_temp_;
    char unit_;
    bool running_;
  };

 protected:
  AnovaPacket *clean_packet_();
  AnovaPacket packet_;

  bool has_target_temp_;
  bool has_current_temp_;
  bool has_unit_;
  bool has_running_;
  bool fahrenheit_;

  CurrentQuery current_query_;
};

}  // namespace anova
}  // namespace esphome
