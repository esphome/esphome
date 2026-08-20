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
#ifdef USE_BINARY_SENSOR

#include "matter_binary_sensor_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/core/entity_base.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>
#include <app/clusters/boolean-state-server/BooleanStateCluster.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <platform/CHIPDeviceLayer.h>

#include <cstring>
#include <span>

namespace esphome::matter {

static const char *const TAG = "matter.binary_sensor";

MatterBinarySensorEndpoint::MatterBinarySensorEndpoint(binary_sensor::BinarySensor *bs) : bs_(bs) {}

MatterBinarySensorEndpoint::DeviceKind MatterBinarySensorEndpoint::pick_device_kind_() const {
  char buf[esphome::MAX_DEVICE_CLASS_LENGTH]{};
  const char *dc = this->bs_->get_device_class_to(std::span<char, esphome::MAX_DEVICE_CLASS_LENGTH>(buf));
  if (dc == nullptr || dc[0] == '\0') {
    return DeviceKind::DEVICE_KIND_CONTACT;
  }
  // Matter OccupancySensing covers motion detectors, presence detectors and
  // occupancy sensors. Everything else defaults to a contact sensor (spec-safe
  // fallback — a door sensor mislabelled as contact still works).
  if (std::strcmp(dc, "motion") == 0 || std::strcmp(dc, "occupancy") == 0 || std::strcmp(dc, "presence") == 0) {
    return DeviceKind::DEVICE_KIND_OCCUPANCY;
  }
  return DeviceKind::DEVICE_KIND_CONTACT;
}

bool MatterBinarySensorEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for binary_sensor '%s'", this->bs_->get_name().c_str());
    return false;
  }

  this->kind_ = this->pick_device_kind_();

  ::esp_matter::endpoint_t *endpoint = nullptr;
  if (this->kind_ == DeviceKind::DEVICE_KIND_OCCUPANCY) {
    ::esp_matter::endpoint::occupancy_sensor::config_t config;
    // OccupancySensorType default = PIR (0). Bitmap advertises PIR.
    config.occupancy_sensing.occupancy_sensor_type = 0;
    config.occupancy_sensing.occupancy_sensor_type_bitmap = 0x01;
    config.occupancy_sensing.occupancy = this->bs_->state ? 0x01 : 0x00;
    // esp-matter 1.5.1 asserts (VALIDATE_FEATURES_AT_LEAST_ONE) that at least
    // one sensing-technology feature bit is set on OccupancySensing —
    // PassiveInfrared/Ultrasonic/PhysicalContact/etc. Without this the
    // cluster::create() fires ABORT_CLUSTER_CREATE and the app resets.
    config.occupancy_sensing.feature_flags =
        static_cast<uint32_t>(chip::app::Clusters::OccupancySensing::Feature::kPassiveInfrared);
    endpoint = ::esp_matter::endpoint::occupancy_sensor::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
  } else {
    ::esp_matter::endpoint::contact_sensor::config_t config;
    config.boolean_state.state_value = this->bs_->state;
    endpoint = ::esp_matter::endpoint::contact_sensor::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
  }
  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create binary_sensor endpoint for '%s'", this->bs_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->bs_->get_name());

  this->bs_->add_on_state_callback([this](bool state) {
    ESP_LOGD(TAG, "device state change → fabric: endpoint=%u state=%d sensor='%s'", this->endpoint_id_,
             static_cast<int>(state), this->bs_->get_name().c_str());
    this->report_state_to_fabric_(state);
  });

  ESP_LOGI(TAG, "registered binary_sensor '%s' as Matter %s endpoint %u", this->bs_->get_name().c_str(),
           this->kind_ == DeviceKind::DEVICE_KIND_OCCUPANCY ? "occupancy_sensor" : "contact_sensor",
           this->endpoint_id_);
  return true;
}

void MatterBinarySensorEndpoint::push_initial_state() { this->report_state_to_fabric_(this->bs_->state); }

void MatterBinarySensorEndpoint::report_state_to_fabric_(bool state) {
  // BooleanState.StateValue is created with ATTRIBUTE_FLAG_MANAGED_INTERNALLY
  // in esp-matter 1.5.1; attribute::update() rejects writes with
  // ESP_ERR_NOT_SUPPORTED there and we have to go through the cluster's own
  // setter (see below). OccupancySensing.Occupancy is a regular writable
  // attribute and goes through the update path.
  const bool is_occupancy = (this->kind_ == DeviceKind::DEVICE_KIND_OCCUPANCY);
  // OccupancySensing.Occupancy is a bitmap8 (bit0 = occupied); BooleanState
  // .StateValue is a plain bool. Different value encoders, different
  // cluster/attribute IDs — build the target attribute descriptor via a
  // ternary so the two branches don't structurally clone (clang-tidy's
  // bugprone-branch-clone would otherwise flag the pair of if/else
  // blocks even though the values differ).
  const uint32_t cluster_id =
      is_occupancy ? chip::app::Clusters::OccupancySensing::Id : chip::app::Clusters::BooleanState::Id;
  const uint32_t attribute_id = is_occupancy ? chip::app::Clusters::OccupancySensing::Attributes::Occupancy::Id
                                             : chip::app::Clusters::BooleanState::Attributes::StateValue::Id;
  ::esp_matter_attr_val_t val = is_occupancy ? ::esp_matter_bitmap8(state ? 0x01 : 0x00) : ::esp_matter_bool(state);
  if (is_occupancy) {
    esp_err_t err = ::esp_matter::attribute::update(this->endpoint_id_, cluster_id, attribute_id, &val);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "attribute::update endpoint=%u failed: %s", this->endpoint_id_, esp_err_to_name(err));
    }
    return;
  }
  // BooleanState.StateValue is ATTRIBUTE_FLAG_MANAGED_INTERNALLY without
  // ATTRIBUTE_FLAG_WRITABLE. esp-matter's set_val() returns ESP_ERR_NOT_SUPPORTED
  // for that combination and leaves a TODO: "we could use the cluster-specific
  // setter API to update the value". We do that here — reach into the CHIP
  // BooleanStateCluster instance the esp-matter integration layer registered
  // in gServers[endpoint_id] and call SetStateValue() directly.
  ::chip::app::ServerClusterInterface *iface = ::esp_matter::data_model::provider::get_instance().registry().Get(
      ::chip::app::ConcreteClusterPath(this->endpoint_id_, ::chip::app::Clusters::BooleanState::Id));
  if (iface == nullptr) {
    ESP_LOGW(TAG, "no BooleanState cluster instance for endpoint=%u", this->endpoint_id_);
    return;
  }
  auto *cluster = static_cast<::chip::app::Clusters::BooleanStateCluster *>(iface);
  // SetStateValue() reaches into the CHIP reporting engine (SetDirty →
  // MarkDirty), which asserts the CHIP stack lock. We are on the ESPHome
  // scheduler thread here, so we must NOT block the loop waiting for the
  // lock — the CHIP task holds it while encoding subscription reports, and
  // on a high-endpoint bridge under multi-controller load that runs for
  // 1–3 s, well past the task watchdog. Marshal the SetStateValue call
  // onto the CHIP task via ScheduleWork so it runs after any in-progress
  // report finishes, without stalling the main loop.
  struct WorkArgs {
    ::chip::app::Clusters::BooleanStateCluster *cluster;
    bool state;
  };
  auto *args = new WorkArgs{cluster, state};
  const CHIP_ERROR err = ::chip::DeviceLayer::PlatformMgr().ScheduleWork(
      [](intptr_t arg) {
        auto *a = reinterpret_cast<WorkArgs *>(arg);
        // Runs on the CHIP task with the stack lock already held by the
        // scheduler dispatcher, so SetStateValue's MarkDirty is safe here.
        a->cluster->SetStateValue(a->state);
        delete a;
      },
      reinterpret_cast<intptr_t>(args));
  if (err != CHIP_NO_ERROR) {
    ESP_LOGW(TAG, "ScheduleWork(SetStateValue) endpoint=%u failed: %s — dropping this update", this->endpoint_id_,
             err.AsString());
    delete args;
  }
}

}  // namespace esphome::matter

#endif  // USE_BINARY_SENSOR
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
