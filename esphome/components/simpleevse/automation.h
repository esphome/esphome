#pragma once

#include "esphome/core/automation.h"
#include "simplesevse.h"

namespace esphome {
namespace simpleevse {

template<typename... Ts> class PluggedCondition : public Condition<Ts...> {
 public:
  PluggedCondition(SimpleEvseComponent *parent) : parent_(parent) {}
  bool check(Ts... x) override {
    uint16_t vehicle_state = this->parent_->get_register()[REGISTER_VEHICLE_STATE];
    return vehicle_state == VehicleState::VEHICLE_EV_PRESENT || vehicle_state == VehicleState::VEHICLE_CHARGING ||
           vehicle_state == VehicleState::VEHICLE_CHARGING_WITH_VENT;
  }

 protected:
  SimpleEvseComponent *parent_;
};

template<typename... Ts> class UnpluggedCondition : public Condition<Ts...> {
 public:
  UnpluggedCondition(SimpleEvseComponent *parent) : parent_(parent) {}
  bool check(Ts... x) override {
    uint16_t vehicle_state = this->parent_->get_register()[REGISTER_VEHICLE_STATE];
    return vehicle_state == VehicleState::VEHICLE_READY;
  }

 protected:
  SimpleEvseComponent *parent_;
};

/** Action for setting the charging current.
 *
 * This action expects a uint8_t value 'current' with the current to set.
 */
template<typename... Ts> class SetChargingCurrent : public Action<Ts...> {
 public:
  explicit SetChargingCurrent(SimpleEvseComponent *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint8_t, current)

  virtual void play_complex(Ts... x) {
    this->num_running_++;
    this->var_ = std::make_tuple(x...);

    uint8_t current = this->current_.value(x...);
    auto trans = make_unique<ModbusWriteHoldingRegistersTransaction>(
        1000, current, std::bind(&SetChargingCurrent::on_update_, this, std::placeholders::_1));
    this->parent_->add_transaction(std::move(trans));
  }

  void play(Ts... x) override { /* ignore - see play complex */
  }

 protected:
  SimpleEvseComponent *parent_;
  std::tuple<Ts...> var_{};

  void on_update_(ModbusTransactionResult) {
    if (this->num_running_ > 0) {
      this->play_next_tuple_(this->var_);
    }
  }
};

/** Action for enable/disable the charging.
 *
 * This action expects a bool value 'enable' with true or false.
 */
template<typename... Ts> class SetChargingEnabled : public Action<Ts...> {
 public:
  explicit SetChargingEnabled(SimpleEvseComponent *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(bool, enabled)

  virtual void play_complex(Ts... x) {
    this->num_running_++;
    this->var_ = std::make_tuple(x...);

    bool enable = this->enabled_.value(x...);
    auto trans = make_unique<ModbusWriteHoldingRegistersTransaction>(
        1004, enable ? 0x0 : 0x1, std::bind(&SetChargingEnabled::on_update_, this, std::placeholders::_1));
    this->parent_->add_transaction(std::move(trans));
  }

  void play(Ts... x) override { /* ignore - see play complex */
  }

 protected:
  SimpleEvseComponent *parent_;
  std::tuple<Ts...> var_{};

  void on_update_(ModbusTransactionResult) {
    if (this->num_running_ > 0) {
      this->play_next_tuple_(this->var_);
    }
  }
};

}  // namespace simpleevse
}  // namespace esphome
