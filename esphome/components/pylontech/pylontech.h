#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace pylontech {

// --- PYLONTECH CONSTANTS ---
static constexpr uint8_t PYLONTECH_MAX_CELLS = 15;
static constexpr uint8_t PYLONTECH_STATE_STR_LEN = 16;
static constexpr uint8_t PYLONTECH_TOKEN_MAX_LEN = 128;
static constexpr uint16_t PYLONTECH_MAX_RX_BUFFER = 512;
static constexpr uint8_t PYLONTECH_CMD_DELAY_MS = 20;

class PylontechListener {
 public:
  virtual void dump_config() {}

  // Structure for global battery data (pwr command)
  struct LineContents {
    int bat_num;
    int volt;
    int curr;
    int tempr;
    int tlow;
    int thigh;
    int vlow;
    int vhigh;
    char base_st[PYLONTECH_STATE_STR_LEN];
    char volt_st[PYLONTECH_STATE_STR_LEN];
    char curr_st[PYLONTECH_STATE_STR_LEN];
    char temp_st[PYLONTECH_STATE_STR_LEN];
    int coulomb;
    int mostempr;
  };
  virtual void on_line_read(LineContents *line) {}

  // Structure for individual cell data (bat command)
  struct CellContents {
    int battery_id;
    int cell_id;
    float voltage;
    float current;
    float temperature;
    int soc;
    int coulomb;
    char balance;
  };
  virtual void on_cell_data(const CellContents *c) {}
};

class PylontechComponent : public uart::UARTDevice, public PollingComponent {
 public:
  PylontechComponent();

  /// Setup the pylontech component, clear buffers etc.
  void setup() override;
  /// Read data from the serial port and process it.
  void loop() override;

  void update() override;
  void dump_config() override;
  void register_listener(PylontechListener *listener) { this->listeners_.push_back(listener); }

  void set_max_battery(int num) {
    if (num > this->max_batteries_) {
      this->max_batteries_ = num;
    }
  }

 protected:
  void process_pwr_line_(std::string &buffer);
  void process_bat_line_(std::string &line);
  // Shared helper function to extract text tokens
  bool get_token_(const char *&cursor, char *token_buf, size_t max_len);

  std::vector<PylontechListener *> listeners_{};

  enum PylonState {
    PYLON_IDLE,
    PYLON_SEARCH,
    PYLON_WAIT_WAKEUP,
    PYLON_DELAY,
    PYLON_REQUEST_PWR,
    PYLON_READ_PWR,
    PYLON_REQUEST_BAT,
    PYLON_READ_BAT
  };

  PylonState pylon_state_{PYLON_IDLE};
  int current_bat_num_{1};
  int max_batteries_{1};
  std::string rx_buffer_;

  char buffer_index_write_{0};
  char buffer_index_read_{0};
  bool has_tlow_id_{false};
  std::string buffer_[4];
};

}  // namespace pylontech
}  // namespace esphome
