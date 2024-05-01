#include "generic_humidifier.h"
#include "esphome/core/log.h"

namespace esphome {
namespace generic_humidifier {

static const char *const TAG = "generic_humdifier.humidifier";

void GenericHumidifier::setup() {
  this->sensor_->add_on_state_callback([this](float state) {
    this->current_humidity = state;
    // control may have changed, recompute
    this->compute_state_();
    // current humidity changed, publish state
    this->publish_state();
  });
  this->current_humidity = this->sensor_->state;

  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    // restore from defaults,
    if (supports_normal_) {
      this->mode = humidifier::HUMIDIFIER_MODE_NORMAL;
    } else if (supports_eco_) {
      this->mode = humidifier::HUMIDIFIER_MODE_ECO;
    } else if (supports_boost_) {
      this->mode = humidifier::HUMIDIFIER_MODE_BOOST;
    } else if (supports_comfort_) {
      this->mode = humidifier::HUMIDIFIER_MODE_COMFORT;
    } else if (supports_sleep_) {
      this->mode = humidifier::HUMIDIFIER_MODE_SLEEP;
    } else if (supports_auto_) {
      this->mode = humidifier::HUMIDIFIER_MODE_AUTO;
    } else if (supports_baby_) {
      this->mode = humidifier::HUMIDIFIER_MODE_BABY;
    }
  }
}
void GenericHumidifier::control(const humidifier::HumidifierCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_humidity().has_value())
    this->target_humidity = *call.get_target_humidity();

  this->compute_state_();
  this->publish_state();
}
humidifier::HumidifierTraits GenericHumidifier::traits() {
  auto traits = humidifier::HumidifierTraits();
  traits.set_supports_current_humidity(true);
  traits.set_supported_modes({
      humidifier::HUMIDIFIER_MODE_OFF,
  });
  if (supports_normal_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_NORMAL);
  if (supports_eco_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_ECO);
  if (supports_boost_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_BOOST);
  if (supports_comfort_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_COMFORT);
  if (supports_sleep_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_SLEEP);
  if (supports_auto_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_AUTO);
  if (supports_baby_)
    traits.add_supported_mode(humidifier::HUMIDIFIER_MODE_BABY);
  traits.set_supports_action(true);
  return traits;
}
void GenericHumidifier::compute_state_() {
  if (this->mode == humidifier::HUMIDIFIER_MODE_OFF) {
    this->switch_to_action_(humidifier::HUMIDIFIER_ACTION_OFF);
    return;
  }
  if (std::isnan(this->current_humidity) || std::isnan(this->target_humidity)) {
    this->switch_to_action_(humidifier::HUMIDIFIER_ACTION_OFF);
    return;
  }
}

void GenericHumidifier::switch_to_action_(humidifier::HumidifierAction action) {
  if (action == this->action) {
    // already in target mode
    return;
  }

  if (this->prev_trigger_ != nullptr) {
    this->prev_trigger_->stop_action();
    this->prev_trigger_ = nullptr;
  }
  Trigger<> *trig;
  switch (action) {
    case humidifier::HUMIDIFIER_ACTION_OFF:
    case humidifier::HUMIDIFIER_ACTION_NORMAL:
      trig = this->normal_trigger_;
      break;
    case humidifier::HUMIDIFIER_ACTION_ECO:
      trig = this->eco_trigger_;
      break;
    case humidifier::HUMIDIFIER_ACTION_BOOST:
      trig = this->boost_trigger_;
      break;
    case humidifier::HUMIDIFIER_ACTION_COMFORT:
      trig = this->comfort_trigger_;
      break;
    case humidifier::HUMIDIFIER_ACTION_SLEEP:
      trig = this->sleep_trigger_;
      break;
    case humidifier::HUMIDIFIER_ACTION_AUTO:
      trig = this->auto_trigger_;
      break;
    case humidifier::HUMIDIFIER_ACTION_BABY:
      trig = this->baby_trigger_;
      break;
    default:
      trig = nullptr;
  }
  assert(trig != nullptr);
  trig->trigger();
  this->action = action;
  this->prev_trigger_ = trig;
  this->publish_state();
}

void GenericHumidifier::set_normal_config(const GenericHumidifierTargetHumidityConfig &normal_config) {
  this->normal_config_ = normal_config;
}

GenericHumidifier::GenericHumidifier()
    : normal_trigger_(new Trigger<>()),
      eco_trigger_(new Trigger<>()),
      boost_trigger_(new Trigger<>()),
      comfort_trigger_(new Trigger<>()),
      sleep_trigger_(new Trigger<>()),
      auto_trigger_(new Trigger<>()),
      baby_trigger_(new Trigger<>()) {}
void GenericHumidifier::set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
Trigger<> *GenericHumidifier::get_normal_trigger() const { return this->normal_trigger_; }
void GenericHumidifier::set_supports_normal(bool supports_normal) { this->supports_normal_ = supports_normal; }
Trigger<> *GenericHumidifier::get_eco_trigger() const { return this->eco_trigger_; }
void GenericHumidifier::set_supports_eco(bool supports_eco) { this->supports_eco_ = supports_eco; }
Trigger<> *GenericHumidifier::get_boost_trigger() const { return this->boost_trigger_; }
void GenericHumidifier::set_supports_boost(bool supports_boost) { this->supports_boost_ = supports_boost; }
Trigger<> *GenericHumidifier::get_comfort_trigger() const { return this->comfort_trigger_; }
void GenericHumidifier::set_supports_comfort(bool supports_comfort) { this->supports_comfort_ = supports_comfort; }
Trigger<> *GenericHumidifier::get_sleep_trigger() const { return this->sleep_trigger_; }
void GenericHumidifier::set_supports_sleep(bool supports_sleep) { this->supports_sleep_ = supports_sleep; }
Trigger<> *GenericHumidifier::get_auto_trigger() const { return this->auto_trigger_; }
void GenericHumidifier::set_supports_auto(bool supports_auto) { this->supports_auto_ = supports_auto; }
Trigger<> *GenericHumidifier::get_baby_trigger() const { return this->baby_trigger_; }
void GenericHumidifier::set_supports_baby(bool supports_baby) { this->supports_baby_ = supports_baby; }
void GenericHumidifier::dump_config() {
  LOG_HUMIDIFIER("", "Generic Humidifier", this);
  ESP_LOGCONFIG(TAG, "  Supports Normal Mode: %s", YESNO(this->supports_normal_));
  ESP_LOGCONFIG(TAG, "  Supports Eco Mode: %s", YESNO(this->supports_eco_));
  ESP_LOGCONFIG(TAG, "  Supports Boost Mode: %s", YESNO(this->supports_boost_));
  ESP_LOGCONFIG(TAG, "  Supports Comfort Mode: %s", YESNO(this->supports_comfort_));
  ESP_LOGCONFIG(TAG, "  Supports Sleep Mode: %s", YESNO(this->supports_sleep_));
  ESP_LOGCONFIG(TAG, "  Supports Auto Mode: %s", YESNO(this->supports_auto_));
  ESP_LOGCONFIG(TAG, "  Supports Baby Mode: %s", YESNO(this->supports_baby_));
  ESP_LOGCONFIG(TAG, "  Default Target Humidity: %.2f%%", this->normal_config_.default_humidity);
}

GenericHumidifierTargetHumidityConfig::GenericHumidifierTargetHumidityConfig() = default;
GenericHumidifierTargetHumidityConfig::GenericHumidifierTargetHumidityConfig(float default_humidity)
    : default_humidity(default_humidity) {}

}  // namespace generic_humidifier
}  // namespace esphome
