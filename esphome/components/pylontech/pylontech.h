#pragma once

#include <array>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/line_reader.h"
#include "esphome/components/uart/uart.h"

namespace esphome::pylontech {

static constexpr uint8_t TEXT_SENSOR_MAX_LEN = 14;
// Battery numbers run 1 to MAX_BATTERIES; bit n - 1 of a battery_mask_t stands for battery n
static constexpr uint8_t MAX_BATTERIES = 16;
using battery_mask_t = uint16_t;
static_assert(MAX_BATTERIES <= sizeof(battery_mask_t) * 8, "battery mask too narrow");
// A battery reports cells 0 to MAX_CELLS - 1; bit c of a cell_mask_t stands for cell c
static constexpr uint8_t MAX_CELLS = 15;
using cell_mask_t = uint16_t;
static_assert(MAX_CELLS <= sizeof(cell_mask_t) * 8, "cell mask too narrow");

class PylontechListener {
 public:
  // No default initializers: the hub's slots stay plain zeroed storage; a parse writes every field
  struct LineContents {
    int32_t curr, tempr, tlow, thigh, mostempr;
    uint16_t volt, vlow, vhigh;  // mV
    uint8_t bat_num, coulomb;
    char base_st[TEXT_SENSOR_MAX_LEN], volt_st[TEXT_SENSOR_MAX_LEN], curr_st[TEXT_SENSOR_MAX_LEN],
        temp_st[TEXT_SENSOR_MAX_LEN];
  };
  struct CellContents {
    int16_t tempr;  // 0.01 °C
    uint16_t volt;  // mV
  };
  using CellArray = std::array<CellContents, MAX_CELLS>;

  explicit PylontechListener(uint8_t bat_num) : bat_num_(bat_num) {}
  uint8_t get_battery() const { return this->bat_num_; }

  /// Called only with records for this listener's battery.
  virtual void on_line_read(LineContents *line) = 0;
#ifdef PYLONTECH_CELL_BATTERY_COUNT
  /// Called with the cells of this listener's battery after a "bat N" reply; bit c of read is set when cell c
  /// was in the reply.
  virtual void on_cells_read(const CellArray &cells, cell_mask_t read) {}
#endif
  virtual void dump_config() = 0;

 protected:
  uint8_t bat_num_;
};

class PylontechComponent final : public PollingComponent, public uart::UARTDevice {
 public:
  PylontechComponent();

  /// Schedule data readings.
  void update() override;
  /// Read data once available
  void loop() override;
  /// Setup the sensor and test for a connection.
  void setup() override;
  void dump_config() override;

  /// Turn the "bat N" cell readings after each "pwr" reply off or on; nothing to turn off without cell sensors.
  void set_cell_polling_enabled(bool enabled) {
#ifdef PYLONTECH_CELL_BATTERY_COUNT
    this->cell_polling_enabled_ = enabled;
#endif
  }

#ifdef PYLONTECH_LISTENER_COUNT
  void register_listener(PylontechListener *listener) {
    this->listeners_.push_back(listener);
    this->batteries_ |= 1u << (listener->get_battery() - 1);
  }
#ifdef PYLONTECH_CELL_BATTERY_COUNT
  /// Read the cells of this battery with "bat N" after every "pwr" reply.
  void add_cell_battery(uint8_t bat_num) { this->cell_batteries_ |= 1u << (bat_num - 1); }
#endif

 protected:
  void process_line_(const char *buffer);
  void parse_header_(const char *buffer);
  void dispatch_one_();
  void reject_line_() {
    if (this->rejected_ < UINT8_MAX) {
      this->rejected_++;
    }
  }

  // The longest line, the newer firmware header, is 182 characters
  LineReader<200> line_reader_;
  StaticVector<PylontechListener *, PYLONTECH_LISTENER_COUNT> listeners_;
  // One slot per listened battery, in battery order; parsed while the UART drains, published one per
  // loop once it is idle. A newer line for a battery replaces the unpublished one.
  static_assert(PYLONTECH_BATTERY_COUNT <= MAX_BATTERIES, "more slots than batteries");
  std::array<PylontechListener::LineContents, PYLONTECH_BATTERY_COUNT> slots_;
  battery_mask_t batteries_{0};  // batteries with a listener
  battery_mask_t pending_{0};    // bit i set: slots_[i] is waiting to be published
  bool has_tlow_id_{false};
  uint8_t rejected_{0};  // malformed battery lines since the last idle pass

#ifdef PYLONTECH_CELL_BATTERY_COUNT
  void parse_cell_line_(const char *buffer);
  void send_next_bat_();

  // One cell slot per battery with cell sensors, in battery order, filled by its "bat N" reply and published
  // to that battery's listeners once the reply has ended and the UART is idle
  std::array<PylontechListener::CellArray, PYLONTECH_CELL_BATTERY_COUNT> cell_slots_;
  std::array<cell_mask_t, PYLONTECH_CELL_BATTERY_COUNT> cells_read_;  // cells the current reply has filled
  battery_mask_t cell_batteries_{0};                                  // batteries whose cells are read
  battery_mask_t cells_pending_{0};  // batteries whose cell slot is waiting to be published
  uint8_t bat_reply_{0};             // battery whose "bat N" reply is arriving; 0 while the "pwr" reply arrives
  bool cell_polling_enabled_{true};
#endif
#endif
};

}  // namespace esphome::pylontech
