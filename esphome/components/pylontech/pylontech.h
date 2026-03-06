#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"
#include <vector>
#include <algorithm>

namespace esphome::pylontech {

static const uint8_t NUM_BUFFERS = 32;
static const uint8_t TEXT_SENSOR_MAX_LEN = 14;

class PylontechListener {
 public:
  struct LineContents {
    int bat_num = 0, volt, curr, tempr, tlow, thigh, vlow, vhigh, coulomb, mostempr;
    char base_st[TEXT_SENSOR_MAX_LEN] = {0}, volt_st[TEXT_SENSOR_MAX_LEN] = {0}, curr_st[TEXT_SENSOR_MAX_LEN] = {0},
         temp_st[TEXT_SENSOR_MAX_LEN] = {0};
  };

  struct CellLineContents {
    int bat_num = 0;
    int cell_num = 0;
    int volt = 0;
    int curr = 0;
    int tempr = 0;
    int coulomb = 0;
    int soc = 0;
    bool balancing = false;
  };

  virtual void on_line_read(LineContents *line);
  virtual void on_cell_line_read(CellLineContents *line);
  virtual void dump_config();
};

class PylontechComponent : public PollingComponent, public uart::UARTDevice {
 public:
  PylontechComponent();

  /// Schedule data readings.
  void update() override;
  /// Read data once available
  void loop() override;
  /// Setup the sensor and test for a connection.
  void setup() override;
  void dump_config() override;

  void register_listener(PylontechListener *listener) { this->listeners_.push_back(listener); }

  void set_cell_polling_enabled(bool enabled) { this->cell_polling_enabled_ = enabled; }
  bool is_cell_polling_enabled() const { return this->cell_polling_enabled_; }

  /// Register a battery number for cell-level data retrieval via the "bat N" command.
  void request_cell_data(int bat_num) {
    if (std::find(this->bat_batteries_.begin(), this->bat_batteries_.end(), bat_num) == this->bat_batteries_.end()) {
      this->bat_batteries_.push_back(bat_num);
      std::sort(this->bat_batteries_.begin(), this->bat_batteries_.end());
    }
  }

 protected:
  enum class State : uint8_t { IDLE, PWR_SENT, BAT_SENT };

  void process_line_(std::string &buffer);
  void parse_pwr_line_(std::string &buffer);
  void parse_cell_line_(std::string &buffer);
  void send_bat_command_();

  // ring buffer
  std::string buffer_[NUM_BUFFERS];
  int buffer_index_write_ = 0;
  int buffer_index_read_ = 0;
  bool has_tlow_id_ = false;

  std::vector<PylontechListener *> listeners_{};

  // state machine for sequential pwr + bat N commands
  State state_ = State::IDLE;
  std::vector<int> bat_batteries_{};
  int current_bat_index_ = 0;
  bool send_next_bat_ = false;
  bool cell_polling_enabled_ = false;
};

template<typename... Ts> class SetCellPollingAction : public Action<Ts...>, public Parented<PylontechComponent> {
 public:
  TEMPLATABLE_VALUE(bool, enable);

  void play(const Ts &...x) {
    bool enable = this->enable_.value(x...);
    this->parent_->set_cell_polling_enabled(enable);
  }
};

}  // namespace esphome::pylontech
