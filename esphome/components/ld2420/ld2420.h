#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ld2420 {

// Local const's
static const uint16_t REFRESH_RATE_MS = 1000;

// Command sets
static const uint8_t CMD_ABD_DATA_REPLY_SIZE = 0x04;
static const uint8_t CMD_ABD_DATA_REPLY_START = 0x0A;
static const uint8_t CMD_REG_DATA_REPLY_SIZE = 0x02;
static const uint16_t CMD_PROTOCOL_VER = 0x0002;
static const uint16_t CMD_ENABLE_CONF = 0x00FF;
static const uint16_t CMD_DISABLE_CONF = 0x00FE;
static const uint16_t CMD_READ_VERSION = 0x0000;
static const uint16_t CMD_WRITE_REGISTER = 0x0001;
static const uint16_t CMD_READ_REGISTER = 0x0002;
static const uint16_t CMD_READ_REG_ADDR = 0x0020;
static const uint16_t CMD_PARM_HIGH_TRESH = 0x0012;
static const uint16_t CMD_PARM_LOW_TRESH = 0x0021;
static const uint16_t CMD_WRITE_ABD_PARAM = 0x0007;
static const uint16_t CMD_READ_ABD_PARAM = 0x0008;
static const uint16_t CMD_WRITE_SYS_PARAM = 0x0012;
static const uint16_t CMD_READ_SYS_PARAM = 0x0013;
static const uint16_t CMD_READ_SERIAL_NUM = 0x0011;
static const uint16_t CMD_RESTART = 0x0068;
static const uint16_t CMD_SYSTEM_MODE = 0x0000;
static const uint16_t CMD_SYSTEM_MODE_TRANSPARENT = 0x0000;
static const uint16_t CMD_SYSTEM_MODE_MTT = 0x0001;
static const uint16_t CMD_SYSTEM_MODE_VS = 0x0002;
static const uint16_t CMD_SYSTEM_MODE_GR = 0x0003;
static const uint16_t CMD_SYSTEM_MODE_NORMAL = 0x0064;

static const uint8_t LD2420_ERROR_NONE = 0x00;
static const uint8_t LD2420_ERROR_UNKNOWN = 0x01;
static const uint8_t LD2420_ERROR_TIMEOUT = 0x02;

// Register address values
static const uint16_t CMD_MIN_GATE_REG = 0x0000;
static const uint16_t CMD_MAX_GATE_REG = 0x0001;
static const uint16_t CMD_TIMEOUT_REG = 0x0004;
static const uint16_t CMD_GATE_HIGH_THRESH[16] = {0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
                                                  0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F};
static const uint16_t CMD_GATE_LOW_THRESH[16] = {0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
                                                 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F};

// COMMAND_BYTE Header & Footer
static const uint32_t CMD_FRAME_HEADER = 0xFAFBFCFD;
static const uint32_t CMD_FRAME_FOOTER = 0x01020304;
static const uint8_t CMD_FRAME_DATA_LENGTH = 4;
static const uint8_t CMD_FRAME_COMMAND = 6;
static const uint8_t CMD_FRAME_STATUS = 7;
static const uint8_t CMD_ERROR_WORD = 8;

static constexpr const char *ERR_MESSAGE[] = {"None", "Unknown", "Timeout"};

class LD2420Listener {
 public:
  virtual void on_presence(bool presence){};
  virtual void on_distance(uint16_t distance){};
};

class LD2420Component : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void register_listener(LD2420Listener *listener) { this->listeners_.push_back(listener); }

  struct CmdFrameT {
    uint32_t header;
    uint16_t length;
    uint16_t command;
    uint8_t data[18];
    uint16_t data_length;
    uint32_t footer;
  };

  float get_setup_priority() const override;
  int send_cmd_from_array(CmdFrameT cmd_frame);
  void handle_cmd_error(uint8_t error);
  void set_timeout(uint16_t value) { this->new_config_.timeout = value; };
  void set_max_gate(uint16_t value) { this->new_config_.max_gate = value; };
  void set_min_gate(uint16_t value) { this->new_config_.min_gate = value; };
  void set_gate_sense_config(uint32_t rg0_move, uint32_t rg0_still, uint32_t rg1_move, uint32_t rg1_still,
                             uint32_t rg2_move, uint32_t rg2_still, uint32_t rg3_move, uint32_t rg3_still,
                             uint32_t rg4_move, uint32_t rg4_still, uint32_t rg5_move, uint32_t rg5_still,
                             uint32_t rg6_move, uint32_t rg6_still, uint32_t rg7_move, uint32_t rg7_still,
                             uint32_t rg8_move, uint32_t rg8_still, uint32_t rg9_move, uint32_t rg9_still,
                             uint32_t rg10_move, uint32_t rg10_still, uint32_t rg11_move, uint32_t rg11_still,
                             uint32_t rg12_move, uint32_t rg12_still, uint32_t rg13_move, uint32_t rg13_still,
                             uint32_t rg14_move, uint32_t rg14_still, uint32_t rg15_move, uint32_t rg15_still) {
    this->new_config_.high_thresh[(uint8_t) 0] = rg0_move;
    this->new_config_.low_thresh[(uint8_t) 0] = rg0_still;
    this->new_config_.high_thresh[(uint8_t) 1] = rg1_move;
    this->new_config_.low_thresh[(uint8_t) 1] = rg1_still;
    this->new_config_.high_thresh[(uint8_t) 2] = rg2_move;
    this->new_config_.low_thresh[(uint8_t) 2] = rg2_still;
    this->new_config_.high_thresh[(uint8_t) 3] = rg3_move;
    this->new_config_.low_thresh[(uint8_t) 3] = rg3_still;
    this->new_config_.high_thresh[(uint8_t) 4] = rg4_move;
    this->new_config_.low_thresh[(uint8_t) 4] = rg4_still;
    this->new_config_.high_thresh[(uint8_t) 5] = rg5_move;
    this->new_config_.low_thresh[(uint8_t) 5] = rg5_still;
    this->new_config_.high_thresh[(uint8_t) 6] = rg6_move;
    this->new_config_.low_thresh[(uint8_t) 6] = rg6_still;
    this->new_config_.high_thresh[(uint8_t) 7] = rg7_move;
    this->new_config_.low_thresh[(uint8_t) 7] = rg7_still;
    this->new_config_.high_thresh[(uint8_t) 8] = rg8_move;
    this->new_config_.low_thresh[(uint8_t) 8] = rg8_still;
    this->new_config_.high_thresh[(uint8_t) 9] = rg9_move;
    this->new_config_.low_thresh[(uint8_t) 9] = rg9_still;
    this->new_config_.high_thresh[(uint8_t) 10] = rg10_move;
    this->new_config_.low_thresh[(uint8_t) 10] = rg10_still;
    this->new_config_.high_thresh[(uint8_t) 11] = rg11_move;
    this->new_config_.low_thresh[(uint8_t) 11] = rg11_still;
    this->new_config_.high_thresh[(uint8_t) 12] = rg12_move;
    this->new_config_.low_thresh[(uint8_t) 12] = rg12_still;
    this->new_config_.high_thresh[(uint8_t) 13] = rg13_move;
    this->new_config_.low_thresh[(uint8_t) 13] = rg13_still;
    this->new_config_.high_thresh[(uint8_t) 14] = rg14_move;
    this->new_config_.low_thresh[(uint8_t) 14] = rg14_still;
    this->new_config_.high_thresh[(uint8_t) 15] = rg15_move;
    this->new_config_.low_thresh[(uint8_t) 15] = rg15_still;
  };

  int32_t last_periodic_millis = millis();
  int32_t last_normal_periodic_millis = millis();

 protected:
  struct RegConfigT {
    uint16_t min_gate;
    uint16_t max_gate;
    uint16_t timeout;
    uint32_t high_thresh[16];
    uint32_t low_thresh[16];
  };

  struct CmdReplyT {
    uint8_t command;
    uint8_t status;
    uint32_t data[4];
    uint8_t length;
    uint16_t error;
    volatile bool ack;
  };

  RegConfigT current_config_;
  RegConfigT new_config_;
  CmdReplyT cmd_reply_;

  void get_firmware_version_();
  void set_min_max_distances_timeout_(uint32_t max_gate_distance, uint32_t min_gate_distance, uint32_t timeout);
  int get_gate_threshold_(uint8_t gate);
  void set_gate_threshold_(uint8_t gate);
  void get_reg_value_(uint16_t reg);
  void set_reg_value_(uint16_t reg, uint16_t value);
  int get_min_max_distances_timeout_();
  u_int8_t set_config_mode_(bool enable);
  void set_system_mode_(uint16_t mode);
  void restart_();
  uint16_t get_mode_() { return this->system_mode_; };
  void set_mode_(uint16_t mode) { this->system_mode_ = mode; };
  bool get_presence_() { return this->presence_; };
  void set_presence_(bool presence) { this->presence_ = presence; };
  uint16_t get_distance_() { return this->distance_; };
  void set_distance_(uint16_t distance) { this->distance_ = distance; };
  bool get_cmd_active_() { return this->cmd_active_; };
  void set_cmd_active_(bool active) { this->cmd_active_ = active; };
  void handle_stream_data_(uint8_t *buffer, int len);
  void handle_normal_mode_(const uint8_t *inbuf, int len);
  void handle_ack_data_(uint8_t *buffer, int len);
  void readline_(uint8_t rx_data, uint8_t *buffer, int len);

  uint32_t timeout_;
  uint32_t max_distance_gate_;
  uint32_t min_distance_gate_;
  uint16_t system_mode_{0xFFFF};
  bool cmd_active_{false};
  char ld2420_firmware_ver_[8];
  bool presence_{false};
  uint16_t distance_{0};
  std::vector<LD2420Listener *> listeners_{};
};

}  // namespace ld2420
}  // namespace esphome
