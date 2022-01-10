#pragma once

#ifdef USE_ESP32

#include <utility>

#include "esphome/components/json/json_util.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace nspanel {

enum WeatherIcon : uint8_t {
  SUNNY = 1,
  SUN_CLOUD = 2,
  CLOUD_BLUE = 7,
  CLOUD_FOG = 11,
  CLOUD_RAIN = 12,
  CLOUD_RAIN_LIGHTNING = 15,
  CLOUD_SNOWFLAKE = 20,
  CLOUD_SNOWFLAKES = 22,
  CLOUD_ICE_CRYSTALS = 24,
  CLOUD_RAIN_SNOW = 29,
  RED_THERMOSTAT = 30,
  BLUE_THERMOSTAT = 31,
  WIND = 32,
  RAINY_CLOUD = 40,
};

enum WidgetType : uint8_t { EMPTY = 0, DEVICE, GROUP, SCENE };

struct GroupItem {
  inline GroupItem() ALWAYS_INLINE = default;
  inline GroupItem(uint8_t id, uint8_t widget_id, std::string name) ALWAYS_INLINE : id(id),
                                                                                    widget_id(widget_id),
                                                                                    name(std::move(name)) {}

  uint8_t id;
  uint8_t widget_id;
  std::string name;
  Trigger<bool> *trigger = new Trigger<bool>();
};

struct Widget {
  inline Widget() ALWAYS_INLINE = default;
  inline Widget(uint8_t id, WidgetType type) ALWAYS_INLINE : id(id), type(type) {}
  inline Widget(uint8_t id, WidgetType type, std::string name) ALWAYS_INLINE : id(id),
                                                                               type(type),
                                                                               name(std::move(name)) {}
  inline Widget(uint8_t id, WidgetType type, std::string name, uint8_t uiid) ALWAYS_INLINE : id(id),
                                                                                             type(type),
                                                                                             name(std::move(name)),
                                                                                             uiid(uiid) {}
  inline Widget(uint8_t id, WidgetType type, std::string name, uint8_t uiid, std::vector<GroupItem> items) ALWAYS_INLINE
      : id(id),
        type(type),
        name(std::move(name)),
        uiid(uiid),
        items(std::move(items)) {}

  uint8_t id;
  WidgetType type;

  std::string name;
  uint8_t uiid{0};

  std::vector<GroupItem> items;

  Trigger<> *trigger = new Trigger<>();
};

class NSPanel : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  void dump_config() override;

  void set_time(time::RealTimeClock *time) {
    this->time_ = time;
    this->time_->add_on_time_sync_callback([this]() { this->send_time_(); });
  }

  void set_relays(switch_::Switch *relay_1, switch_::Switch *relay_2) {
    this->relay_1_ = relay_1;
    this->relay_2_ = relay_2;

    this->relay_1_->add_on_state_callback([this](bool state) { this->send_relay_states_(); });
    this->relay_2_->add_on_state_callback([this](bool state) { this->send_relay_states_(); });
  }

  void set_eco_mode_switch(switch_::Switch *eco_mode) {
    this->eco_mode_switch_ = eco_mode;
    this->eco_mode_switch_->add_on_state_callback([this](bool state) { this->send_eco_mode_(state); });
  }

  void set_screen_power_switch(switch_::Switch *screen_power) { this->screen_power_switch_ = screen_power; }

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) {
    temperature_sensor->add_on_state_callback([this](float state) { this->send_temperature_(state); });
  }

  void set_temperature_unit_celsius(bool celsius) { this->temperature_celsius_ = celsius; }

  void initialize();

  void set_widget(uint8_t index, Widget widget) { this->widgets_[index] = std::move(widget); }

  void send_weather_data(WeatherIcon icon, int8_t temperature, int8_t min, int8_t max);

  void control_switch(GroupItem &item, bool state);

  void send_json_command(uint8_t type, const std::string &command);

 protected:
  void send_nextion_command_(const std::string &command);
  static uint16_t crc16(const uint8_t *data, uint16_t len);

  bool process_data_();
  void process_command_(uint8_t type, JsonObject &root, const std::string &message);

  void send_relay_states_();
  void send_time_();
  void send_temperature_(float temperature);
  void send_eco_mode_(bool eco_mode);

  void send_wifi_state_();
  void send_all_widgets_();

  std::vector<uint8_t> buffer_;
  time::RealTimeClock *time_;
  switch_::Switch *relay_1_;
  switch_::Switch *relay_2_;

  switch_::Switch *eco_mode_switch_;
  switch_::Switch *screen_power_switch_;

  Widget widgets_[8];

  bool temperature_celsius_;

  bool last_connected_{true};
  uint32_t last_wifi_sent_at_{0};
};

}  // namespace nspanel
}  // namespace esphome

#endif  // USE_ESP32
