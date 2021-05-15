#pragma once

#include "modbus_utils.h"
#include "esphome/core/automation.h"
#include <queue>

namespace esphome {
namespace simpleevse {

extern const char *TAG;

const uint32_t STATUS_POLL_INTERVAL{5000};

const uint16_t FIRST_STATUS_REGISTER{1000};
const uint16_t COUNT_STATUS_REGISTER{7};

enum StatusRegister : uint16_t {
  REGISTER_CHARGE_CURRENT = 0,
  REGISTER_ACTUAL_CURRENT = 1,
  REGISTER_VEHICLE_STATE = 2,
  REGISTER_MAX_CURRENT = 3,
  REGISTER_CTRL_BITS = 4,
  REGISTER_FIRMWARE = 5,
  REGISTER_EVSE_STATE = 6,
};

enum VehicleState : uint16_t {
  VEHICLE_UNKNOWN = 0,
  VEHICLE_READY = 1,
  VEHICLE_EV_PRESENT = 2,
  VEHICLE_CHARGING = 3,
  VEHICLE_CHARGING_WITH_VENT = 4,
};

const uint16_t CHARGING_ENABLED_ON{0x0000};
const uint16_t CHARGING_ENABLED_OFF{0x0001};
const uint16_t CHARGING_ENABLED_MASK{0x0001};

const uint16_t FIRST_CONFIG_REGISTER{2000};
const uint16_t COUNT_CONFIG_REGISTER{18};

/** Interface for classes which will be notified about status updates. */
class UpdateListener {
 public:
  virtual void update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register) = 0;
};

/** Implements the SimpleEVSE communication on top of the Modbus
 * class. */
class SimpleEvseComponent : public ModbusDeviceComponent {
 public:
  void dump_config() override;
  /// Adds a transaction to the execution queue.
  void add_transaction(std::unique_ptr<ModbusTransaction> &&transaction);

  void add_observer(UpdateListener *observer) { this->observer_.push_back(observer); }

  std::array<uint16_t, COUNT_STATUS_REGISTER> get_register() const { return this->status_register_; }

  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }
  uint32_t get_update_interval() const { return this->update_interval_; }

  void set_unplugged_trigger(Trigger<> *trigger) { this->unplugged_trigger_ = trigger; }
  void set_plugged_trigger(Trigger<> *trigger) { this->plugged_trigger_ = trigger; }

 protected:
  void idle() override;

  void on_status_received_(ModbusTransactionResult result, const std::vector<uint16_t> &reg);
  void process_triggers_();

  // status of SimpleEVSE - will be updated regularly
  bool running_{false};
  std::array<uint16_t, COUNT_STATUS_REGISTER> status_register_;
  uint32_t update_interval_{STATUS_POLL_INTERVAL};

  /// Time of last status update.
  uint32_t last_state_udpate_{0};
  bool force_update_{true};

  std::vector<UpdateListener *> observer_;

  /// Queued transactions which should be executed.
  std::queue<std::unique_ptr<ModbusTransaction>> transactions_;

  // Triggers
  Trigger<> *unplugged_trigger_{nullptr};
  Trigger<> *plugged_trigger_{nullptr};
  bool was_plugged_{false};
};

}  // namespace simpleevse
}  // namespace esphome
