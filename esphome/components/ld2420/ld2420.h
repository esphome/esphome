#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#include <map>
#include <functional>

namespace esphome {
namespace ld2420 {

// Local const's
static const uint16_t REFRESH_RATE_MS = 1000;

// Command sets
static const uint8_t CMD_ABD_DATA_REPLY_SIZE = 0x04;
static const uint8_t CMD_ABD_DATA_REPLY_START = 0x0A;
static const uint16_t CMD_DISABLE_CONF = 0x00FE;
static const uint16_t CMD_ENABLE_CONF = 0x00FF;
static const uint8_t CMD_MAX_BYTES = 0x64;
static const uint16_t CMD_PARM_HIGH_TRESH = 0x0012;
static const uint16_t CMD_PARM_LOW_TRESH = 0x0021;
static const uint16_t CMD_PROTOCOL_VER = 0x0002;
static const uint16_t CMD_READ_ABD_PARAM = 0x0008;
static const uint16_t CMD_READ_REG_ADDR = 0x0020;
static const uint16_t CMD_READ_REGISTER = 0x0002;
static const uint16_t CMD_READ_SERIAL_NUM = 0x0011;
static const uint16_t CMD_READ_SYS_PARAM = 0x0013;
static const uint16_t CMD_READ_VERSION = 0x0000;
static const uint8_t CMD_REG_DATA_REPLY_SIZE = 0x02;
static const uint16_t CMD_RESTART = 0x0068;
static const uint16_t CMD_SYSTEM_MODE = 0x0000;
static const uint16_t CMD_SYSTEM_MODE_GR = 0x0003;
static const uint16_t CMD_SYSTEM_MODE_MTT = 0x0001;
static const uint16_t CMD_SYSTEM_MODE_SIMPLE = 0x0064;
static const uint16_t CMD_SYSTEM_MODE_DEBUG = 0x0000;
static const uint16_t CMD_SYSTEM_MODE_ENERGY = 0x0004;
static const uint16_t CMD_SYSTEM_MODE_VS = 0x0002;
static const uint16_t CMD_WRITE_ABD_PARAM = 0x0007;
static const uint16_t CMD_WRITE_REGISTER = 0x0001;
static const uint16_t CMD_WRITE_SYS_PARAM = 0x0012;

static const uint8_t LD2420_ERROR_NONE = 0x00;
static const uint8_t LD2420_ERROR_TIMEOUT = 0x02;
static const uint8_t LD2420_ERROR_UNKNOWN = 0x01;
static const uint8_t LD2420_TOTAL_GATES = 16;
static const uint8_t CALIBRATE_SAMPLES = 64;

// Register address values
static const uint16_t CMD_MIN_GATE_REG = 0x0000;
static const uint16_t CMD_MAX_GATE_REG = 0x0001;
static const uint16_t CMD_TIMEOUT_REG = 0x0004;
static const uint16_t CMD_GATE_MOVE_THRESH[LD2420_TOTAL_GATES] = {0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015,
                                                                  0x0016, 0x0017, 0x0018, 0x0019, 0x001A, 0x001B,
                                                                  0x001C, 0x001D, 0x001E, 0x001F};
static const uint16_t CMD_GATE_STILL_THRESH[LD2420_TOTAL_GATES] = {0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025,
                                                                   0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B,
                                                                   0x002C, 0x002D, 0x002E, 0x002F};
static const uint32_t FACTORY_MOVE_THRESH[LD2420_TOTAL_GATES] = {60000, 30000, 400, 250, 250, 250, 250, 250,
                                                                 250,   250,   250, 250, 250, 250, 250, 250};
static const uint32_t FACTORY_STILL_THRESH[LD2420_TOTAL_GATES] = {40000, 20000, 200, 200, 200, 200, 200, 150,
                                                                  150,   100,   100, 100, 100, 100, 100, 100};
static const uint16_t FACTORY_TIMEOUT = 120;
static const uint16_t FACTORY_MIN_GATE = 1;
static const uint16_t FACTORY_MAX_GATE = 12;

// COMMAND_BYTE Header & Footer
static const uint8_t CMD_FRAME_COMMAND = 6;
static const uint8_t CMD_FRAME_DATA_LENGTH = 4;
static const uint32_t CMD_FRAME_FOOTER = 0x01020304;
static const uint32_t CMD_FRAME_HEADER = 0xFAFBFCFD;
static const uint32_t DEBUG_FRAME_FOOTER = 0xFAFBFCFD;
static const uint32_t DEBUG_FRAME_HEADER = 0x1410BFAA;
static const uint32_t ENERGY_FRAME_FOOTER = 0xF5F6F7F8;
static const uint32_t ENERGY_FRAME_HEADER = 0xF1F2F3F4;
static const uint8_t CMD_FRAME_STATUS = 7;
static const uint8_t CMD_ERROR_WORD = 8;
static const uint8_t ENERGY_SENSOR_START = 9;
static const uint8_t CALIBRATE_REPORT_INTERVAL = 4;
static const int CALIBRATE_VERSION_MIN = 154;
static const std::string OP_NORMAL_MODE_STRING = "Normal";
static const std::string OP_SIMPLE_MODE_STRING = "Simple";

enum OpModeStruct : uint8_t { OP_NORMAL_MODE = 1, OP_CALIBRATE_MODE = 2, OP_SIMPLE_MODE = 3 };
static const std::map<std::string, uint8_t> OP_MODE_TO_UINT{
    {"Normal", OP_NORMAL_MODE}, {"Calibrate", OP_CALIBRATE_MODE}, {"Simple", OP_SIMPLE_MODE}};
static constexpr const char *ERR_MESSAGE[] = {"None", "Unknown", "Timeout"};

class LD2420Listener {
 public:
  virtual void on_presence(bool presence){};
  virtual void on_distance(uint16_t distance){};
  virtual void on_energy(uint16_t *sensor_energy, size_t size){};
  virtual void on_fw_version(std::string &fw){};
};

class LD2420Component : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
#ifdef USE_SELECT
  void set_operating_mode_select(select::Select *selector) { this->operating_selector_ = selector; };
#endif
#ifdef USE_NUMBER
  void set_gate_timeout_number(number::Number *number) { this->gate_timeout_number_ = number; };
  void set_gate_select_number(number::Number *number) { this->gate_select_number_ = number; };
  void set_min_gate_distance_number(number::Number *number) { this->min_gate_distance_number_ = number; };
  void set_max_gate_distance_number(number::Number *number) { this->max_gate_distance_number_ = number; };
  void set_gate_move_sensitivity_factor_number(number::Number *number) {
    this->gate_move_sensitivity_factor_number_ = number;
  };
  void set_gate_still_sensitivity_factor_number(number::Number *number) {
    this->gate_still_sensitivity_factor_number_ = number;
  };
  void set_gate_still_threshold_numbers(int gate, number::Number *n) { this->gate_still_threshold_numbers_[gate] = n; };
  void set_gate_move_threshold_numbers(int gate, number::Number *n) { this->gate_move_threshold_numbers_[gate] = n; };
  bool is_gate_select() { return gate_select_number_ != nullptr; };
  uint8_t get_gate_select_value() { return static_cast<uint8_t>(this->gate_select_number_->state); };
  float get_min_gate_distance_value() { return min_gate_distance_number_->state; };
  float get_max_gate_distance_value() { return max_gate_distance_number_->state; };
  void publish_gate_move_threshold(uint8_t gate) {
    // With gate_select we only use 1 number pointer, thus we hard code [0]
    this->gate_move_threshold_numbers_[0]->publish_state(this->new_config.move_thresh[gate]);
  };
  void publish_gate_still_threshold(uint8_t gate) {
    this->gate_still_threshold_numbers_[0]->publish_state(this->new_config.still_thresh[gate]);
  };
  void init_gate_config_numbers();
  void refresh_gate_config_numbers();
#endif
#ifdef USE_BUTTON
  void set_apply_config_button(button::Button *button) { this->apply_config_button_ = button; };
  void set_revert_config_button(button::Button *button) { this->revert_config_button_ = button; };
  void set_restart_module_button(button::Button *button) { this->restart_module_button_ = button; };
  void set_factory_reset_button(button::Button *button) { this->factory_reset_button_ = button; };
#endif
  void register_listener(LD2420Listener *listener) { this->listeners_.push_back(listener); }

  struct CmdFrameT {
    uint32_t header{0};
    uint16_t length{0};
    uint16_t command{0};
    uint8_t data[18];
    uint16_t data_length{0};
    uint32_t footer{0};
  };

  struct RegConfigT {
    uint16_t min_gate{0};
    uint16_t max_gate{0};
    uint16_t timeout{0};
    uint32_t move_thresh[LD2420_TOTAL_GATES];
    uint32_t still_thresh[LD2420_TOTAL_GATES];
  };

  void send_module_restart();
  void restart_module_action();
  void apply_config_action();
  void factory_reset_action();
  void revert_config_action();
  float get_setup_priority() const override;
  int send_cmd_from_array(CmdFrameT cmd_frame);
  void report_gate_data();
  void handle_cmd_error(uint8_t error);
  void set_operating_mode(const std::string &state);
  void auto_calibrate_sensitivity();
  void update_radar_data(uint16_t const *gate_energy, uint8_t sample_number);
  uint8_t calc_checksum(void *data, size_t size);

  RegConfigT current_config;
  RegConfigT new_config;
  int32_t last_periodic_millis = millis();
  int32_t report_periodic_millis = millis();
  int32_t monitor_periodic_millis = millis();
  int32_t last_normal_periodic_millis = millis();
  bool output_energy_state{false};
  uint8_t current_operating_mode{OP_NORMAL_MODE};
  uint16_t radar_data[LD2420_TOTAL_GATES][CALIBRATE_SAMPLES];
  uint16_t gate_avg[LD2420_TOTAL_GATES];
  uint16_t gate_peak[LD2420_TOTAL_GATES];
  uint8_t sample_number_counter{0};
  uint16_t total_sample_number_counter{0};
  float gate_move_sensitivity_factor{0.5};
  float gate_still_sensitivity_factor{0.5};
#ifdef USE_SELECT
  select::Select *operating_selector_{nullptr};
#endif
#ifdef USE_BUTTON
  button::Button *apply_config_button_{nullptr};
  button::Button *revert_config_button_{nullptr};
  button::Button *restart_module_button_{nullptr};
  button::Button *factory_reset_button_{nullptr};
#endif
  void set_min_max_distances_timeout(uint32_t max_gate_distance, uint32_t min_gate_distance, uint32_t timeout);
  void set_gate_threshold(uint8_t gate);
  void set_reg_value(uint16_t reg, uint16_t value);
  uint8_t set_config_mode(bool enable);
  void set_system_mode(uint16_t mode);
  void ld2420_restart();

 protected:
  struct CmdReplyT {
    uint8_t command;
    uint8_t status;
    uint32_t data[4];
    uint8_t length;
    uint16_t error;
    volatile bool ack;
  };

  int get_firmware_int_(const char *version_string);
  void get_firmware_version_();
  int get_gate_threshold_(uint8_t gate);
  void get_reg_value_(uint16_t reg);
  int get_min_max_distances_timeout_();
  uint16_t get_mode_() { return this->system_mode_; };
  void set_mode_(uint16_t mode) { this->system_mode_ = mode; };
  bool get_presence_() { return this->presence_; };
  void set_presence_(bool presence) { this->presence_ = presence; };
  uint16_t get_distance_() { return this->distance_; };
  void set_distance_(uint16_t distance) { this->distance_ = distance; };
  bool get_cmd_active_() { return this->cmd_active_; };
  void set_cmd_active_(bool active) { this->cmd_active_ = active; };
  void handle_simple_mode_(const uint8_t *inbuf, int len);
  void handle_energy_mode_(uint8_t *buffer, int len);
  void handle_ack_data_(uint8_t *buffer, int len);
  void readline_(int rx_data, uint8_t *buffer, int len);
  void set_calibration_(bool state) { this->calibration_ = state; };
  bool get_calibration_() { return this->calibration_; };

#ifdef USE_NUMBER
  number::Number *gate_timeout_number_{nullptr};
  number::Number *gate_select_number_{nullptr};
  number::Number *min_gate_distance_number_{nullptr};
  number::Number *max_gate_distance_number_{nullptr};
  number::Number *gate_move_sensitivity_factor_number_{nullptr};
  number::Number *gate_still_sensitivity_factor_number_{nullptr};
  std::vector<number::Number *> gate_still_threshold_numbers_ = std::vector<number::Number *>(16);
  std::vector<number::Number *> gate_move_threshold_numbers_ = std::vector<number::Number *>(16);
#endif

  uint16_t gate_energy_[LD2420_TOTAL_GATES];
  CmdReplyT cmd_reply_;
  uint32_t max_distance_gate_;
  uint32_t min_distance_gate_;
  uint16_t system_mode_{CMD_SYSTEM_MODE_ENERGY};
  bool cmd_active_{false};
  char ld2420_firmware_ver_[8]{"v0.0.0"};
  bool presence_{false};
  bool calibration_{false};
  uint16_t distance_{0};
  uint8_t config_checksum_{0};
  std::vector<LD2420Listener *> listeners_{};
};

}  // namespace ld2420
}  // namespace esphome
