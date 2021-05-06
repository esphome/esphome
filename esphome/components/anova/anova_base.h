#pragma once

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace anova {

enum CurrentQuery {
  None,
  ReadDeviceStatus,
  ReadTargetTemperature,
  ReadCurrentTemperature,
  ReadData,
  ReadUnit,
  SetTargetTemperature,
  SetUnit,
  Start,
  Stop,
};

struct anova_packet {
  uint16_t length;
  uint8_t data[24];
};

#define READ_DEVICE_STATUS "status\r"
#define READ_TARGET_TEMP "read set temp\r"
#define READ_CURRENT_TEMP "read temp\r"
#define READ_UNIT "read unit\r"
#define READ_DATA "read data\r"
#define SET_TARGET_TEMP "set temp %.1f\r"
#define SET_TEMP_UNIT "set unit %c\r"

#define CMD_START "start\r"
#define CMD_STOP "stop\r"

class AnovaCodec {
 public:
  anova_packet* get_read_device_status_request();
  anova_packet* get_read_target_temp_request();
  anova_packet* get_read_current_temp_request();
  anova_packet* get_read_data_request();
  anova_packet* get_read_unit_request();

  anova_packet* get_set_target_temp_request(float temperature);
  anova_packet* get_set_unit_request(char unit);

  anova_packet* get_start_request();
  anova_packet* get_stop_request();

  void decode(const uint8_t* data, uint16_t length);
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
  anova_packet* clean_packet();
  anova_packet packet_;

  bool has_target_temp_;
  bool has_current_temp_;
  bool has_unit_;
  bool has_running_;
  char buf_[32];

  CurrentQuery current_query_;
};

}  // namespace anova
}  // namespace esphome
