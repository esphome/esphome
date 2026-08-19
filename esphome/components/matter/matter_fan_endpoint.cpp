#include "esphome/core/defines.h"

// esp-matter 1.6.0 only supports these ESP32 variants. Strip the whole
// TU on any other target (P4, S2, C2, C5, C61, H4, H21, S31) so clang-tidy
// jobs for those variants — which grep this file in via USE_WIFI /
// USE_ETHERNET — don't try to compile against an esp_matter.h that upstream
// never ships for those chips. Runtime builds are already rejected by the
// only_on_variant config validator in matter/__init__.py; this guard is the
// static-analysis mirror of the same restriction.
#ifdef USE_ESP_IDF
#if defined(USE_ESP32_VARIANT_ESP32) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32C3) || \
    defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32H2)
#ifdef USE_FAN

#include "matter_fan_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/fan/fan_traits.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>

#include <algorithm>
#include <cmath>

namespace esphome::matter {

static const char *const TAG = "matter.fan";

MatterFanEndpoint::MatterFanEndpoint(fan::Fan *fan) : fan_(fan) {}

bool MatterFanEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for fan '%s'", this->fan_->get_name().c_str());
    return false;
  }

  auto traits = this->fan_->get_traits();
  this->supports_speed_ = traits.supports_speed();
  this->supported_speed_count_ = std::max(1, traits.supported_speed_count());

  ::esp_matter::endpoint::fan::config_t config;
  // FanModeSequence default (2 = Off/L/M/H/Auto) is fine. Feature flags
  // stay 0 — no VALIDATE_FEATURES_AT_LEAST_ONE on FanControl in 1.5.1, so
  // no MultiSpeed/etc. required for a basic on/off + percent fan.
  config.fan_control.percent_setting = this->esphome_to_percent_();
  config.fan_control.percent_current = this->esphome_to_percent_();
  config.fan_control.fan_mode = this->fan_->state ? 4 /*On*/ : 0 /*Off*/;

  ::esp_matter::endpoint_t *endpoint =
      ::esp_matter::endpoint::fan::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create fan endpoint for '%s'", this->fan_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->fan_->get_name());

  // esp-matter 1.5.1's fan_control::create() hardcodes feature_map=0 and
  // does not accept features via config. Without at least one FanControl
  // feature bit, HA (and other commissioners) treat the endpoint as read-only
  // "mudo" — no toggles or slider rendered. Add the MultiSpeed feature
  // post-create so SpeedMax/SpeedSetting/SpeedCurrent attributes appear and
  // controllers know the fan is speed-controllable.
  if (this->supports_speed_) {
    ::esp_matter::cluster_t *fc_cluster = ::esp_matter::cluster::get(endpoint, chip::app::Clusters::FanControl::Id);
    if (fc_cluster != nullptr) {
      ::esp_matter::cluster::fan_control::feature::multi_speed::config_t ms_config;
      ms_config.speed_max = static_cast<uint8_t>(this->supported_speed_count_);
      ms_config.speed_setting = static_cast<uint8_t>(this->fan_->speed);
      ms_config.speed_current = static_cast<uint8_t>(this->fan_->speed);
      esp_err_t ferr = ::esp_matter::cluster::fan_control::feature::multi_speed::add(fc_cluster, &ms_config);
      if (ferr != ESP_OK) {
        ESP_LOGW(TAG, "fan MultiSpeed feature add failed for '%s': %s", this->fan_->get_name().c_str(),
                 esp_err_to_name(ferr));
      }
    } else {
      // Without MultiSpeed the fabric renders a read-only fan (no slider) —
      // the exact symptom the block above exists to prevent. If the cluster
      // lookup ever returns null, surface it instead of silently degrading.
      ESP_LOGW(TAG, "fan '%s' endpoint=%u: FanControl cluster lookup returned null — MultiSpeed feature not added",
               this->fan_->get_name().c_str(), this->endpoint_id_);
    }
  }

  // ESPHome fan callback takes no args — re-read state on each fire.
  this->fan_->add_on_state_callback([this]() {
    if (this->applying_matter_write_) {
      return;
    }
    ESP_LOGD(TAG, "device state change → fabric: endpoint=%u state=%d speed=%d fan='%s'", this->endpoint_id_,
             static_cast<int>(this->fan_->state), this->fan_->speed, this->fan_->get_name().c_str());
    this->report_state_to_fabric_();
  });

  ESP_LOGI(TAG, "registered fan '%s' as Matter fan endpoint %u (speed_count=%d, supports_speed=%d)",
           this->fan_->get_name().c_str(), this->endpoint_id_, this->supported_speed_count_,
           static_cast<int>(this->supports_speed_));
  return true;
}

uint8_t MatterFanEndpoint::esphome_to_percent_() const {
  if (!this->fan_->state) {
    return 0;
  }
  if (!this->supports_speed_ || this->supported_speed_count_ <= 0) {
    return 100;
  }
  int speed = std::clamp(this->fan_->speed, 1, this->supported_speed_count_);
  return static_cast<uint8_t>(std::lround(100.0f * speed / this->supported_speed_count_));
}

uint8_t MatterFanEndpoint::percent_to_speed_(uint8_t percent) const {
  if (!this->supports_speed_ || this->supported_speed_count_ <= 0) {
    return percent > 0 ? 1 : 0;
  }
  if (percent == 0) {
    return 0;
  }
  int speed = static_cast<int>(std::lround(percent * this->supported_speed_count_ / 100.0f));
  return static_cast<uint8_t>(std::clamp(speed, 1, this->supported_speed_count_));
}

uint8_t MatterFanEndpoint::percent_to_fan_mode_(uint8_t percent) const {
  // Rough mapping — HA fabric mostly ignores FanMode when PercentCurrent is
  // present, but we still keep the two attributes coherent for controllers
  // that read FanMode first (Apple Home, some others).
  if (percent == 0)
    return 0;  // Off
  if (percent <= 33)
    return 1;  // Low
  if (percent <= 66)
    return 2;  // Medium
  if (percent < 100)
    return 3;  // High
  return 4;    // On
}

void MatterFanEndpoint::on_matter_percent_write(uint8_t percent) {
  ESP_LOGD(TAG, "matter PercentSetting write endpoint=%u percent=%u fan='%s'", this->endpoint_id_,
           static_cast<unsigned>(percent), this->fan_->get_name().c_str());
  // Runs on the CHIP task — defer the Fan::make_call sequence and the guard
  // flag onto the main loop to avoid racing the fan's own state machine.
  MatterComponent::instance()->defer_on_main_loop([this, percent]() {
    this->applying_matter_write_ = true;
    auto call = this->fan_->make_call();
    call.set_state(percent > 0);
    if (this->supports_speed_) {
      uint8_t speed = this->percent_to_speed_(percent);
      if (speed > 0) {
        call.set_speed(speed);
      }
    }
    call.perform();
    this->applying_matter_write_ = false;
  });
}

void MatterFanEndpoint::on_matter_fan_mode_write(uint8_t fan_mode) {
  ESP_LOGD(TAG, "matter FanMode write endpoint=%u mode=%u fan='%s'", this->endpoint_id_,
           static_cast<unsigned>(fan_mode), this->fan_->get_name().c_str());
  MatterComponent::instance()->defer_on_main_loop([this, fan_mode]() {
    this->applying_matter_write_ = true;
    auto call = this->fan_->make_call();
    if (fan_mode == 0) {
      call.set_state(false);
    } else {
      call.set_state(true);
      // Map the coarse mode back to a speed roughly matching the last
      // percent_to_fan_mode_ mapping — keeps a fabric-side FanMode-only write
      // from resetting the speed to whatever restore_mode saved.
      if (this->supports_speed_) {
        uint8_t speed = 1;
        if (fan_mode == 2) {  // Medium
          speed = static_cast<uint8_t>(std::max(1, this->supported_speed_count_ / 2));
        } else if (fan_mode == 3 || fan_mode == 4) {  // High / On
          speed = static_cast<uint8_t>(this->supported_speed_count_);
        }
        call.set_speed(speed);
      }
    }
    call.perform();
    this->applying_matter_write_ = false;
  });
}

void MatterFanEndpoint::push_initial_state() { this->report_state_to_fabric_(); }

void MatterFanEndpoint::report_state_to_fabric_() {
  uint8_t percent = this->esphome_to_percent_();
  uint8_t fan_mode = this->percent_to_fan_mode_(percent);

  ApplyingReportGuard applying_report_guard(this->applying_report_);

  // PercentCurrent — read-only device→fabric.
  ::esp_matter_attr_val_t v_current = ::esp_matter_uint8(percent);
  esp_err_t err =
      ::esp_matter::attribute::update(this->endpoint_id_, chip::app::Clusters::FanControl::Id,
                                      chip::app::Clusters::FanControl::Attributes::PercentCurrent::Id, &v_current);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "attribute::update PercentCurrent endpoint=%u failed: %s", this->endpoint_id_, esp_err_to_name(err));
  }

  // PercentSetting — mirror the current setting to what the device is
  // actually doing so the fabric UI stays coherent.
  ::esp_matter_attr_val_t v_setting = ::esp_matter_nullable_uint8(percent);
  err = ::esp_matter::attribute::update(this->endpoint_id_, chip::app::Clusters::FanControl::Id,
                                        chip::app::Clusters::FanControl::Attributes::PercentSetting::Id, &v_setting);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "attribute::update PercentSetting endpoint=%u failed: %s", this->endpoint_id_, esp_err_to_name(err));
  }

  // FanMode — keep coherent with the derived mode above.
  ::esp_matter_attr_val_t v_mode = ::esp_matter_enum8(fan_mode);
  err = ::esp_matter::attribute::update(this->endpoint_id_, chip::app::Clusters::FanControl::Id,
                                        chip::app::Clusters::FanControl::Attributes::FanMode::Id, &v_mode);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "attribute::update FanMode endpoint=%u failed: %s", this->endpoint_id_, esp_err_to_name(err));
  }
}

}  // namespace esphome::matter

#endif  // USE_FAN
#endif  // matter supported variant
#endif  // USE_ESP_IDF
