#pragma once

#include "esphome/core/automation.h"
#include "haier_base.h"
#include "hon_climate.h"

namespace esphome {
namespace haier {

template<typename... Ts> class DisplayOnAction : public Action<Ts...> {
 public:
  DisplayOnAction(HaierClimateBase *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->set_display_state(true); }

 protected:
  HaierClimateBase *parent_;
};

template<typename... Ts> class DisplayOffAction : public Action<Ts...> {
 public:
  DisplayOffAction(HaierClimateBase *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->set_display_state(false); }

 protected:
  HaierClimateBase *parent_;
};

template<typename... Ts> class BeeperOnAction : public Action<Ts...> {
 public:
  BeeperOnAction(HonClimate *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->set_beeper_state(true); }

 protected:
  HonClimate *parent_;
};

template<typename... Ts> class BeeperOffAction : public Action<Ts...> {
 public:
  BeeperOffAction(HonClimate *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->set_beeper_state(false); }

 protected:
  HonClimate *parent_;
};

template<typename... Ts> class VerticalAirflowAction : public Action<Ts...> {
 public:
  VerticalAirflowAction(HonClimate *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(AirflowVerticalDirection, direction)
  void play(Ts... x) { this->parent_->set_vertical_airflow(this->direction_.value(x...)); }

 protected:
  HonClimate *parent_;
};

template<typename... Ts> class HorizontalAirflowAction : public Action<Ts...> {
 public:
  HorizontalAirflowAction(HonClimate *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(AirflowHorizontalDirection, direction)
  void play(Ts... x) { this->parent_->set_horizontal_airflow(this->direction_.value(x...)); }

 protected:
  HonClimate *parent_;
};

template<typename... Ts> class HealthOnAction : public Action<Ts...> {
 public:
  HealthOnAction(HaierClimateBase *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->set_health_mode(true); }

 protected:
  HaierClimateBase *parent_;
};

template<typename... Ts> class HealthOffAction : public Action<Ts...> {
 public:
  HealthOffAction(HaierClimateBase *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->set_health_mode(false); }

 protected:
  HaierClimateBase *parent_;
};

template<typename... Ts> class StartSelfCleaningAction : public Action<Ts...> {
 public:
  StartSelfCleaningAction(HonClimate *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->start_self_cleaning(); }

 protected:
  HonClimate *parent_;
};

template<typename... Ts> class StartSteriCleaningAction : public Action<Ts...> {
 public:
  StartSteriCleaningAction(HonClimate *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->start_steri_cleaning(); }

 protected:
  HonClimate *parent_;
};

template<typename... Ts> class PowerOnAction : public Action<Ts...> {
 public:
  PowerOnAction(HaierClimateBase *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->send_power_on_command(); }

 protected:
  HaierClimateBase *parent_;
};

template<typename... Ts> class PowerOffAction : public Action<Ts...> {
 public:
  PowerOffAction(HaierClimateBase *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->send_power_off_command(); }

 protected:
  HaierClimateBase *parent_;
};

template<typename... Ts> class PowerToggleAction : public Action<Ts...> {
 public:
  PowerToggleAction(HaierClimateBase *parent) : parent_(parent) {}
  void play(Ts... x) { this->parent_->toggle_power(); }

 protected:
  HaierClimateBase *parent_;
};

}  // namespace haier
}  // namespace esphome
