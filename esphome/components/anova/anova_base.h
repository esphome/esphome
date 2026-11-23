#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::anova {

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
  /// Copy a fixed command, terminator included, into the packet.
  template<size_t N> AnovaPacket *set_command_(const char (&cmd)[N]) {
    static_assert(N <= sizeof(AnovaPacket::data), "Command does not fit the packet");
    memcpy(this->packet_.data, cmd, N);
    return this->finish_packet_(N - 1);
  }
  /// Build prefix + arg + "\r", truncated to the packet.
  AnovaPacket *set_command_with_arg_(const char *prefix, const char *arg);
  AnovaPacket *finish_packet_(size_t length);
  AnovaPacket packet_;

  bool has_target_temp_;
  bool has_current_temp_;
  bool has_unit_;
  bool has_running_;
  bool fahrenheit_;

  CurrentQuery current_query_;
};

}  // namespace esphome::anova
