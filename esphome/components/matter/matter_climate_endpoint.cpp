#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_CLIMATE

#include "matter_climate_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/climate/climate_mode.h"
#include "esphome/components/climate/climate_traits.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>

#include <algorithm>
#include <cmath>

namespace esphome::matter {

static const char *const TAG = "matter.climate";

namespace {

// Conversions between ESPHome degrees-Celsius floats and the int16
// hundredths-of-Celsius Matter uses on the wire. Kept in one place so a
// future Fahrenheit-native ESPHome climate does not silently double-scale.
int16_t celsius_to_hundredths(float c) {
  if (std::isnan(c)) {
    // Setpoint attributes are non-nullable — return a safe midpoint so a
    // fabric read never sees an int16 min/max sentinel. LocalTemperature is
    // nullable and handled with a dedicated code path in report_state_.
    return 2000;  // 20.00 °C
  }
  // Clamp before the int16 cast — a bogus reading (disconnected sensor
  // returning e.g. 1e6 °C) would otherwise wrap around silently instead of
  // saturating at the spec range. Mirrors the sensor wrapper's clamp on
  // every measurement path.
  const long v = std::lround(std::clamp(c * 100.0f, -32768.0f, 32767.0f));
  return static_cast<int16_t>(v);
}

float hundredths_to_celsius(int16_t h) { return static_cast<float>(h) / 100.0f; }

}  // namespace

MatterClimateEndpoint::MatterClimateEndpoint(climate::Climate *climate) : climate_(climate) {}

bool MatterClimateEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for climate '%s'", this->climate_->get_name().c_str());
    return false;
  }

  auto traits = this->climate_->get_traits();
  this->supports_heating_ = traits.supports_mode(climate::CLIMATE_MODE_HEAT);
  this->supports_cooling_ = traits.supports_mode(climate::CLIMATE_MODE_COOL);
  this->supports_two_point_target_ = traits.has_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);
  // AutoMode requires both Heating and Cooling per Matter spec AND enforces
  // MinSetpointDeadBand between the two setpoints. Without a real two-point
  // target, publishing target ± spread would drift the ESPHome target on
  // every fabric round-trip. So we only advertise Auto when the climate
  // actually exposes target_temperature_low / _high. Without Auto the
  // MinSetpointDeadBand check does not apply (spec §4.3.7.5), so we can
  // still expose both Heating and Cooling with a shared single setpoint.
  const bool declares_auto =
      traits.supports_mode(climate::CLIMATE_MODE_HEAT_COOL) || traits.supports_mode(climate::CLIMATE_MODE_AUTO);
  this->supports_auto_ = declares_auto && this->supports_two_point_target_;
  if (declares_auto) {
    // ESPHome advertised HEAT_COOL, so the climate can physically do both.
    this->supports_heating_ = true;
    this->supports_cooling_ = true;
    if (!this->supports_two_point_target_) {
      ESP_LOGD(TAG,
               "climate '%s' supports HEAT_COOL but not two-point target — "
               "not advertising Matter AutoMode; heating/cooling setpoints "
               "share the single ESPHome target",
               this->climate_->get_name().c_str());
    }
  }

  // esp-matter's thermostat cluster runs VALIDATE_FEATURES_AT_LEAST_ONE at
  // boot; an off-only climate would trip that check and abort the whole
  // node. Skip with a warning — the user probably forgot to add
  // supported_modes to the template climate.
  if (!this->supports_heating_ && !this->supports_cooling_) {
    ESP_LOGW(TAG, "climate '%s' supports neither HEAT nor COOL — skipping (Matter Thermostat requires at least one)",
             this->climate_->get_name().c_str());
    return false;
  }

  ::esp_matter::endpoint::thermostat::config_t config;

  uint32_t feature_flags = 0;
  if (this->supports_heating_) {
    feature_flags |= static_cast<uint32_t>(chip::app::Clusters::Thermostat::Feature::kHeating);
    // Seed the initial heating setpoint from the current target if we're in a
    // heating-capable mode; otherwise use the default (20.00°C).
    if (!std::isnan(this->climate_->target_temperature)) {
      config.thermostat.features.heating.occupied_heating_setpoint =
          celsius_to_hundredths(this->climate_->target_temperature);
    }
  }
  if (this->supports_cooling_) {
    feature_flags |= static_cast<uint32_t>(chip::app::Clusters::Thermostat::Feature::kCooling);
    if (!std::isnan(this->climate_->target_temperature)) {
      config.thermostat.features.cooling.occupied_cooling_setpoint =
          celsius_to_hundredths(this->climate_->target_temperature);
    }
  }
  if (this->supports_auto_) {
    feature_flags |= static_cast<uint32_t>(chip::app::Clusters::Thermostat::Feature::kAutoMode);
  }
  config.thermostat.feature_flags = feature_flags;

  // ControlSequenceOfOperation summarizes what heating/cooling this thermostat
  // can do. Matches the feature bits we chose above; controllers use it to
  // decide which SystemMode values to offer in the UI.
  if (this->supports_heating_ && this->supports_cooling_) {
    config.thermostat.control_sequence_of_operation =
        static_cast<uint8_t>(chip::app::Clusters::Thermostat::ControlSequenceOfOperationEnum::kCoolingAndHeating);
  } else if (this->supports_heating_) {
    config.thermostat.control_sequence_of_operation =
        static_cast<uint8_t>(chip::app::Clusters::Thermostat::ControlSequenceOfOperationEnum::kHeatingOnly);
  } else {
    config.thermostat.control_sequence_of_operation =
        static_cast<uint8_t>(chip::app::Clusters::Thermostat::ControlSequenceOfOperationEnum::kCoolingOnly);
  }

  // SystemMode seed — pick from the current ESPHome mode so a re-boot lands
  // the fabric on the state the device was actually in.
  switch (this->climate_->mode) {
    case climate::CLIMATE_MODE_OFF:
      config.thermostat.system_mode = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kOff);
      break;
    case climate::CLIMATE_MODE_HEAT:
      config.thermostat.system_mode = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kHeat);
      break;
    case climate::CLIMATE_MODE_COOL:
      config.thermostat.system_mode = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kCool);
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_AUTO:
      config.thermostat.system_mode = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kAuto);
      break;
    default:
      // FAN_ONLY / DRY have Matter equivalents (7 / 8) but they need extra
      // clusters that aren't wired up in this MVP — fall back to Off so the
      // fabric doesn't get a mode it can't drive back to.
      config.thermostat.system_mode = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kOff);
      break;
  }

  ::esp_matter::endpoint_t *endpoint =
      ::esp_matter::endpoint::thermostat::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create thermostat endpoint for '%s'", this->climate_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->climate_->get_name());

  // MinHeat/MinCool/MaxHeat/MaxCoolSetpointLimit are OPTIONAL Thermostat
  // attributes — feature::heating::add / feature::cooling::add only create
  // the OccupiedHeating/CoolingSetpoint attribute, so trying to update the
  // limits here would silently fail. The fabric falls back to the
  // AbsMin/Max defaults from the CHIP thermostat server (700-3000 heat,
  // 1600-3200 cool). For the MVP that range is fine; a future iteration
  // could add the limit attributes via cluster::attribute::create_* to
  // narrow the fabric UI slider to the ClimateTraits visual range.

  // Probe which optional setpoint attributes actually got added by the
  // feature helpers so report_state_to_fabric_ can skip the ones missing.
  // Without this we'd log a warning on every publish for setpoints the CHIP
  // stack legitimately doesn't have — noisy and misleading.
  ::esp_matter::cluster_t *thermo_cluster = ::esp_matter::cluster::get(endpoint, chip::app::Clusters::Thermostat::Id);
  this->has_heating_setpoint_attr_ =
      thermo_cluster != nullptr &&
      ::esp_matter::attribute::get(thermo_cluster,
                                   chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Id) != nullptr;
  this->has_cooling_setpoint_attr_ =
      thermo_cluster != nullptr &&
      ::esp_matter::attribute::get(thermo_cluster,
                                   chip::app::Clusters::Thermostat::Attributes::OccupiedCoolingSetpoint::Id) != nullptr;
  ESP_LOGI(TAG, "endpoint %u thermostat attrs: heat_setpoint=%d cool_setpoint=%d", this->endpoint_id_,
           static_cast<int>(this->has_heating_setpoint_attr_), static_cast<int>(this->has_cooling_setpoint_attr_));

  this->climate_->add_on_state_callback([this](climate::Climate & /*unused*/) {
    if (this->applying_matter_write_) {
      return;
    }
    ESP_LOGD(TAG, "device state change → fabric: endpoint=%u mode=%u current=%.2f target=%.2f climate='%s'",
             this->endpoint_id_, static_cast<unsigned>(this->climate_->mode), this->climate_->current_temperature,
             this->climate_->target_temperature, this->climate_->get_name().c_str());
    this->report_state_to_fabric_();
  });

  ESP_LOGI(TAG, "registered climate '%s' as Matter thermostat endpoint %u (heat=%d cool=%d auto=%d)",
           this->climate_->get_name().c_str(), this->endpoint_id_, static_cast<int>(this->supports_heating_),
           static_cast<int>(this->supports_cooling_), static_cast<int>(this->supports_auto_));
  return true;
}

void MatterClimateEndpoint::on_matter_system_mode_write(uint8_t system_mode) {
  ESP_LOGD(TAG, "matter SystemMode write endpoint=%u mode=%u climate='%s'", this->endpoint_id_,
           static_cast<unsigned>(system_mode), this->climate_->get_name().c_str());
  climate::ClimateMode target;
  switch (static_cast<chip::app::Clusters::Thermostat::SystemModeEnum>(system_mode)) {
    case chip::app::Clusters::Thermostat::SystemModeEnum::kOff:
      target = climate::CLIMATE_MODE_OFF;
      break;
    case chip::app::Clusters::Thermostat::SystemModeEnum::kAuto:
      target = climate::CLIMATE_MODE_HEAT_COOL;
      break;
    case chip::app::Clusters::Thermostat::SystemModeEnum::kCool:
    case chip::app::Clusters::Thermostat::SystemModeEnum::kPrecooling:
      target = climate::CLIMATE_MODE_COOL;
      break;
    case chip::app::Clusters::Thermostat::SystemModeEnum::kHeat:
    case chip::app::Clusters::Thermostat::SystemModeEnum::kEmergencyHeat:
      target = climate::CLIMATE_MODE_HEAT;
      break;
    case chip::app::Clusters::Thermostat::SystemModeEnum::kFanOnly:
      target = climate::CLIMATE_MODE_FAN_ONLY;
      break;
    case chip::app::Clusters::Thermostat::SystemModeEnum::kDry:
      target = climate::CLIMATE_MODE_DRY;
      break;
    default:
      ESP_LOGW(TAG, "unsupported SystemMode value %u — ignoring", static_cast<unsigned>(system_mode));
      return;
  }
  // Runs on the CHIP task — defer the Climate call and the guard onto the
  // main loop so the mode transition and its listeners don't race the
  // ESPHome loop.
  MatterComponent::instance()->defer_on_main_loop([this, target]() {
    this->applying_matter_write_ = true;
    this->climate_->make_call().set_mode(target).perform();
    this->applying_matter_write_ = false;
  });
}

void MatterClimateEndpoint::on_matter_heating_setpoint_write(int16_t hundredths) {
  ESP_LOGD(TAG, "matter OccupiedHeatingSetpoint write endpoint=%u value=%d (%.2f °C) climate='%s'", this->endpoint_id_,
           static_cast<int>(hundredths), hundredths_to_celsius(hundredths), this->climate_->get_name().c_str());
  // Route to the plain single-point setter — HVAC UIs that write both
  // setpoints separately during HEAT_COOL still end up with the "most recent
  // wins" behavior, which matches ESPHome's single-value target semantics for
  // the MVP. Deferred to the main loop like the other CHIP-task-driven writes.
  MatterComponent::instance()->defer_on_main_loop([this, hundredths]() {
    this->applying_matter_write_ = true;
    this->climate_->make_call().set_target_temperature(hundredths_to_celsius(hundredths)).perform();
    this->applying_matter_write_ = false;
  });
}

void MatterClimateEndpoint::on_matter_cooling_setpoint_write(int16_t hundredths) {
  ESP_LOGD(TAG, "matter OccupiedCoolingSetpoint write endpoint=%u value=%d (%.2f °C) climate='%s'", this->endpoint_id_,
           static_cast<int>(hundredths), hundredths_to_celsius(hundredths), this->climate_->get_name().c_str());
  MatterComponent::instance()->defer_on_main_loop([this, hundredths]() {
    this->applying_matter_write_ = true;
    this->climate_->make_call().set_target_temperature(hundredths_to_celsius(hundredths)).perform();
    this->applying_matter_write_ = false;
  });
}

void MatterClimateEndpoint::push_initial_state() { this->report_state_to_fabric_(); }

void MatterClimateEndpoint::report_state_to_fabric_() {
  ApplyingReportGuard applying_report_guard(this->applying_report_);

  // LocalTemperature — nullable int16 hundredths of °C. NAN means "no reading
  // yet" from the ESPHome climate, so publish the null sentinel via the
  // default-constructed ::nullable<int16_t>{} — same idiom the sensor and
  // lock wrappers use. Constructing via a raw numeric_limits::min value
  // relied on an undocumented esp-matter nullable<T> constructor detail.
  {
    ::esp_matter_attr_val_t v;
    if (std::isnan(this->climate_->current_temperature)) {
      v = ::esp_matter_nullable_int16(::nullable<int16_t>());
    } else {
      v = ::esp_matter_nullable_int16(celsius_to_hundredths(this->climate_->current_temperature));
    }
    esp_err_t err =
        ::esp_matter::attribute::update(this->endpoint_id_, chip::app::Clusters::Thermostat::Id,
                                        chip::app::Clusters::Thermostat::Attributes::LocalTemperature::Id, &v);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "attribute::update LocalTemperature endpoint=%u failed: %s", this->endpoint_id_,
               esp_err_to_name(err));
    }
  }

  // Setpoints — publish separate heating and cooling values when the climate
  // exposes a real two-point target. Otherwise share the single setpoint
  // across both attributes: with AutoMode disabled (see setup) the CHIP
  // thermostat server no longer enforces MinSetpointDeadBand, so the
  // ±spread hack is neither needed nor safe (it drifted the ESPHome target
  // downward on every fabric round-trip).
  float heat_c;
  float cool_c;
  if (this->supports_two_point_target_) {
    heat_c = this->climate_->target_temperature_low;
    cool_c = this->climate_->target_temperature_high;
    if (this->supports_auto_) {
      constexpr float kMinSpread = 1.0f;  // °C — safely above the 0.2°C default deadband
      if (std::isnan(heat_c) || std::isnan(cool_c) || cool_c - heat_c < kMinSpread) {
        float mid = std::isnan(heat_c) || std::isnan(cool_c) ? 22.5f : (heat_c + cool_c) / 2.0f;
        heat_c = mid - kMinSpread / 2.0f;
        cool_c = mid + kMinSpread / 2.0f;
      }
    }
  } else {
    heat_c = this->climate_->target_temperature;
    cool_c = this->climate_->target_temperature;
    if (std::isnan(heat_c)) {
      // celsius_to_hundredths substitutes 20.00°C for NaN because the
      // Occupied*Setpoint attributes are non-nullable; note it so the
      // fabricated value isn't mistaken for a genuine reading during
      // early boot when the climate hasn't produced a target yet.
      ESP_LOGD(TAG, "endpoint %u: no target_temperature yet — publishing setpoint fallback 20.00 °C",
               this->endpoint_id_);
    }
  }

  if (this->has_heating_setpoint_attr_) {
    ::esp_matter_attr_val_t v = ::esp_matter_int16(celsius_to_hundredths(heat_c));
    esp_err_t err =
        ::esp_matter::attribute::update(this->endpoint_id_, chip::app::Clusters::Thermostat::Id,
                                        chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Id, &v);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "attribute::update OccupiedHeatingSetpoint endpoint=%u value=%.2f failed: %s", this->endpoint_id_,
               heat_c, esp_err_to_name(err));
    }
  }
  if (this->has_cooling_setpoint_attr_) {
    ::esp_matter_attr_val_t v = ::esp_matter_int16(celsius_to_hundredths(cool_c));
    esp_err_t err =
        ::esp_matter::attribute::update(this->endpoint_id_, chip::app::Clusters::Thermostat::Id,
                                        chip::app::Clusters::Thermostat::Attributes::OccupiedCoolingSetpoint::Id, &v);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "attribute::update OccupiedCoolingSetpoint endpoint=%u value=%.2f failed: %s", this->endpoint_id_,
               cool_c, esp_err_to_name(err));
    }
  }

  // SystemMode.
  uint8_t sys;
  switch (this->climate_->mode) {
    case climate::CLIMATE_MODE_OFF:
      sys = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kOff);
      break;
    case climate::CLIMATE_MODE_HEAT:
      sys = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kHeat);
      break;
    case climate::CLIMATE_MODE_COOL:
      sys = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kCool);
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_AUTO:
      sys = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kAuto);
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      sys = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kFanOnly);
      break;
    case climate::CLIMATE_MODE_DRY:
      sys = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kDry);
      break;
    default:
      sys = static_cast<uint8_t>(chip::app::Clusters::Thermostat::SystemModeEnum::kOff);
      break;
  }
  ::esp_matter_attr_val_t v_mode = ::esp_matter_enum8(sys);
  esp_err_t err = ::esp_matter::attribute::update(this->endpoint_id_, chip::app::Clusters::Thermostat::Id,
                                                  chip::app::Clusters::Thermostat::Attributes::SystemMode::Id, &v_mode);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "attribute::update SystemMode endpoint=%u failed: %s", this->endpoint_id_, esp_err_to_name(err));
  }
}

}  // namespace esphome::matter

#endif  // USE_CLIMATE
#endif  // USE_ESP_IDF
