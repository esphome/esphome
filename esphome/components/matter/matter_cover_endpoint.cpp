#include "esphome/core/defines.h"

// USE_MATTER_VARIANT_SUPPORTED is set by matter's Python to_code() via
// cg.add_define() on the 5 esp-matter-supported ESP32 variants (ESP32,
// S3, C3, C6, H2). It is deliberately NOT declared in
// esphome/core/defines.h — that path is only exercised by clang-tidy and
// static-analysis tools, which do not have esp_matter.h available (the
// SDK is a third-party managed component fetched at build time). Keeping
// the symbol out of defines.h means matter files strip on lint (no
// missing-header errors) but compile normally on real builds where
// Python-side codegen has run. Runtime variant enforcement lives in the
// only_on_variant validator in matter/__init__.py.
#if defined(USE_ESP_IDF) && defined(USE_MATTER_VARIANT_SUPPORTED)
#ifdef USE_COVER

#include "matter_cover_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/components/cover/cover.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>

#include <algorithm>
#include <cmath>

namespace esphome::matter {

static const char *const TAG = "matter.cover";

namespace {

// Convert ESPHome position (0.0=closed .. 1.0=open) to Matter
// percent100ths (0=open .. 10000=closed). Inverts direction and clamps
// to the valid range. Callers must handle NaN themselves (setup and
// report_state_to_fabric_ both seed the nullable attribute with a null
// sentinel rather than passing NaN through).
uint16_t esphome_to_matter_100ths(float esphome_position) {
  float inverted = 1.0f - std::clamp(esphome_position, 0.0f, 1.0f);
  int32_t v = static_cast<int32_t>(std::lround(inverted * 10000.0f));
  // Bounds typed to int32_t explicitly — on the xtensa toolchain int32_t
  // is long int, so bare 0 / 10000 literals fail template deduction.
  return static_cast<uint16_t>(std::clamp(v, static_cast<int32_t>(0), static_cast<int32_t>(10000)));
}

// Convert Matter percent100ths (0=open .. 10000=closed) to ESPHome
// position (0.0=closed .. 1.0=open). Inverts direction and clamps.
float matter_100ths_to_esphome(uint16_t percent100ths) {
  uint16_t clamped = std::min<uint16_t>(percent100ths, 10000);
  return 1.0f - (static_cast<float>(clamped) / 10000.0f);
}

}  // namespace

MatterCoverEndpoint::MatterCoverEndpoint(cover::Cover *cover) : cover_(cover) {}

bool MatterCoverEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for cover '%s'", this->cover_->get_name().c_str());
    return false;
  }

  auto traits = this->cover_->get_traits();
  this->supports_position_ = traits.get_supports_position();

  // Seed the endpoint config with the cover's current position so the
  // fabric view is coherent from the first attribute read (before
  // push_initial_state() runs after esp_matter::start). CurrentPosition /
  // TargetPositionLiftPercent100ths are nullable — seed them null when
  // the ESPHome cover has no known position (NaN) instead of substituting
  // "fully open" (percent100ths 0). Controllers reading the attributes
  // before the first state callback then get "unavailable" rather than a
  // confident wrong value.
  const bool initial_position_known = !std::isnan(this->cover_->position);
  ::nullable<uint16_t> initial_100ths_nullable =
      initial_position_known ? ::nullable<uint16_t>(esphome_to_matter_100ths(this->cover_->position))
                             : ::nullable<uint16_t>();

  ::esp_matter::endpoint::window_covering::config_t config(
      static_cast<uint8_t>(chip::app::Clusters::WindowCovering::EndProductType::kRollerShade));
  // "Type" attribute — top-level WindowCovering.Type. kRollerShade is a
  // safe generic default and matches EndProductType above.
  config.window_covering.type = static_cast<uint8_t>(chip::app::Clusters::WindowCovering::Type::kRollerShade);
  // ConfigStatus bits: Operational (device is running) + LiftPositionAware
  // (we support percent-100ths, which is what the fabric will drive).
  config.window_covering.config_status =
      static_cast<uint8_t>(chip::app::Clusters::WindowCovering::ConfigStatus::kOperational) |
      static_cast<uint8_t>(chip::app::Clusters::WindowCovering::ConfigStatus::kLiftPositionAware);
  // esp-matter 1.5.1 asserts VALIDATE_FEATURES_AT_LEAST_ONE("Lift,Tilt") on
  // WindowCovering — without a feature bit the cluster::create() calls
  // ABORT_CLUSTER_CREATE and the app resets on boot. Lift covers
  // open/close; PositionAwareLift enables percent-based target writes.
  config.window_covering.feature_flags =
      static_cast<uint32_t>(chip::app::Clusters::WindowCovering::Feature::kLift) |
      static_cast<uint32_t>(chip::app::Clusters::WindowCovering::Feature::kPositionAwareLift);
  // Seed both current and target so the fabric doesn't see a mismatch
  // (target != current → OperationalStatus says "moving"). The config uses
  // esp-matter's own ::nullable<T> template — not chip::DataModel::Nullable —
  // and implicit-converts a raw uint16 through nullable<T>::nullable(T).
  config.window_covering.features.position_aware_lift.current_position_lift_percent_100ths = initial_100ths_nullable;
  config.window_covering.features.position_aware_lift.target_position_lift_percent_100ths = initial_100ths_nullable;

  ::esp_matter::endpoint_t *endpoint =
      ::esp_matter::endpoint::window_covering::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create window_covering endpoint for '%s'", this->cover_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->cover_->get_name());

  this->cover_->add_on_state_callback([this]() {
    if (this->applying_matter_write_) {
      ESP_LOGV(TAG, "device state callback suppressed (matter-driven change) endpoint=%u", this->endpoint_id_);
      return;
    }
    ESP_LOGD(TAG, "device state change → fabric: endpoint=%u position=%.3f op=%d cover='%s'", this->endpoint_id_,
             this->cover_->position, static_cast<int>(this->cover_->current_operation),
             this->cover_->get_name().c_str());
    this->report_state_to_fabric_();
  });

  ESP_LOGI(TAG, "registered cover '%s' as Matter window_covering endpoint %u (position-aware=%d)",
           this->cover_->get_name().c_str(), this->endpoint_id_, static_cast<int>(this->supports_position_));
  return true;
}

void MatterCoverEndpoint::on_matter_target_write(uint16_t percent100ths) {
  float target_pos = matter_100ths_to_esphome(percent100ths);
  ESP_LOGD(TAG, "matter target write endpoint=%u percent100ths=%u → esphome_pos=%.3f cover='%s'", this->endpoint_id_,
           static_cast<unsigned>(percent100ths), target_pos, this->cover_->get_name().c_str());
  // Fabric writes arrive on the CHIP PlatformManager task; Cover::make_call
  // and the trailing report_state_to_fabric_ mutate ESPHome state and
  // attribute cache concurrently with the main loop otherwise. Defer the
  // whole sequence onto the main loop and keep applying_matter_write_
  // scoped inside the lambda so the guard is main-loop-only.
  MatterComponent::instance()->defer_on_main_loop([this, target_pos]() {
    this->applying_matter_write_ = true;
    auto call = this->cover_->make_call();
    if (this->supports_position_) {
      call.set_position(target_pos);
    } else {
      // Binary cover — pick the closer of fully-open / fully-closed. The
      // 50% threshold matches how Home Assistant maps slider drags to
      // OPEN/CLOSE for assumed-state covers.
      if (target_pos >= 0.5f) {
        call.set_command_open();
      } else {
        call.set_command_close();
      }
    }
    call.perform();
    this->applying_matter_write_ = false;

    // publish_state() from the template cover fired our device→fabric callback
    // synchronously while applying_matter_write_ was set, so no CurrentPosition
    // report went out. Push it now — otherwise CHIP's PostAttributeChange sees
    // Target=X, Current=old and pegs OperationalStatus at Moving forever.
    this->report_state_to_fabric_();
  });
}

void MatterCoverEndpoint::push_initial_state() { this->report_state_to_fabric_(); }

void MatterCoverEndpoint::report_state_to_fabric_() {
  // CurrentPositionLiftPercent100ths is a nullable attribute — when the
  // ESPHome cover has no known position (NaN, e.g. an assumed-state cover
  // that has not been driven since boot) publish null so the fabric renders
  // "unavailable" rather than a fabricated fully-open. Same for TargetPosition.
  const float esphome_pos = this->cover_->position;
  const bool position_known = !std::isnan(esphome_pos);
  ::nullable<uint16_t> current_nullable =
      position_known ? ::nullable<uint16_t>(esphome_to_matter_100ths(esphome_pos)) : ::nullable<uint16_t>();
  ::esp_matter_attr_val_t current_val = ::esp_matter_nullable_uint16(current_nullable);

  ApplyingReportGuard applying_report_guard(this->applying_report_);
  MatterComponent::instance()->defer_attribute_update(
      this->endpoint_id_, chip::app::Clusters::WindowCovering::Id,
      chip::app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id, current_val);

  // When the cover reports idle, align Target with Current so the CHIP
  // window-covering-server's PostAttributeChange computes OperationalState =
  // Stall (current==target) and the fabric stops thinking we're moving.
  if (this->cover_->current_operation == cover::COVER_OPERATION_IDLE) {
    ::esp_matter_attr_val_t target_val = ::esp_matter_nullable_uint16(current_nullable);
    MatterComponent::instance()->defer_attribute_update(
        this->endpoint_id_, chip::app::Clusters::WindowCovering::Id,
        chip::app::Clusters::WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id, target_val);
  }
}

}  // namespace esphome::matter

#endif  // USE_COVER
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
