#pragma once

#include "esphome/core/automation.h"
#include "haier_climate.h"

namespace esphome {
namespace haier {

template<typename... Ts> 
class DisplayOnAction : public Action<Ts...> 
{
public:
    DisplayOnAction(HaierClimate* parent) : parent_(parent) {}
    void play(Ts... x) 
    {
        this->parent_->set_display_state(true);
    }

protected:
    HaierClimate* parent_;
};

template<typename... Ts> 
class DisplayOffAction : public Action<Ts...> 
{
public:
    DisplayOffAction(HaierClimate* parent) : parent_(parent) {}
    void play(Ts... x) 
    {
        this->parent_->set_display_state(false);
    }

protected:
    HaierClimate* parent_;
};

template<typename... Ts> 
class BeeperOnAction : public Action<Ts...> 
{
public:
    BeeperOnAction(HaierClimate* parent) : parent_(parent) {}
    void play(Ts... x) 
    {
        this->parent_->set_beeper_state(true);
    }

protected:
    HaierClimate* parent_;
};

template<typename... Ts> 
class BeeperOffAction : public Action<Ts...> 
{
public:
    BeeperOffAction(HaierClimate* parent) : parent_(parent) {}
    void play(Ts... x) 
    {
        this->parent_->set_beeper_state(false);
    }

protected:
    HaierClimate* parent_;
};

template<typename... Ts> 
class VerticalAirflowAction : public Action<Ts...> 
{
public:
    VerticalAirflowAction(HaierClimate* parent) : parent_(parent) {}
    TEMPLATABLE_VALUE(AirflowVerticalDirection, direction)
    void play(Ts... x) 
    {
        this->parent_->set_vertical_airflow(this->direction_.value(x...));
    }

protected:
    HaierClimate* parent_;
};

template<typename... Ts> 
class HorizontalAirflowAction : public Action<Ts...> 
{
public:
    HorizontalAirflowAction(HaierClimate* parent) : parent_(parent) {}
    TEMPLATABLE_VALUE(AirflowHorizontalDirection, direction)
    void play(Ts... x) 
    {
        this->parent_->set_horizontal_airflow(this->direction_.value(x...));
    }

protected:
    HaierClimate* parent_;
};

}
}
