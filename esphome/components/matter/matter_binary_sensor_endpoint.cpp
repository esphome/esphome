#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
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

#include <cstring>
#include <span>

namespace esphome {
namespace matter {

static const char *const TAG = "matter.binary_sensor";

MatterBinarySensorEndpoint::MatterBinarySensorEndpoint(binary_sensor::BinarySensor *bs) : bs_(bs) {}

MatterBinarySensorEndpoint::DeviceKind MatterBinarySensorEndpoint::pick_device_kind_() const {
  char buf[esphome::MAX_DEVICE_CLASS_LENGTH]{};
  const char *dc = this->bs_->get_device_class_to(std::span<char, esphome::MAX_DEVICE_CLASS_LENGTH>(buf));
  if (dc == nullptr || dc[0] == '\0') {
    return DeviceKind::CONTACT;
  }
  // Matter OccupancySensing covers motion detectors, presence detectors and
  // occupancy sensors. Everything else defaults to a contact sensor (spec-safe
  // fallback — a door sensor mislabelled as contact still works).
  if (std::strcmp(dc, "motion") == 0 || std::strcmp(dc, "occupancy") == 0 || std::strcmp(dc, "presence") == 0) {
    return DeviceKind::OCCUPANCY;
  }
  return DeviceKind::CONTACT;
}

bool MatterBinarySensorEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for binary_sensor '%s'", this->bs_->get_name().c_str());
    return false;
  }

  this->kind_ = this->pick_device_kind_();

  ::esp_matter::endpoint_t *endpoint = nullptr;
  if (this->kind_ == DeviceKind::OCCUPANCY) {
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
           this->kind_ == DeviceKind::OCCUPANCY ? "occupancy_sensor" : "contact_sensor", this->endpoint_id_);
  return true;
}

void MatterBinarySensorEndpoint::push_initial_state() { this->report_state_to_fabric_(this->bs_->state); }

void MatterBinarySensorEndpoint::report_state_to_fabric_(bool state) {
  ::esp_matter_attr_val_t val;
  uint32_t cluster_id;
  uint32_t attribute_id;
  bool internally_managed;
  if (this->kind_ == DeviceKind::OCCUPANCY) {
    // OccupancySensing.Occupancy bit0 = occupied.
    val = ::esp_matter_bitmap8(state ? 0x01 : 0x00);
    cluster_id = chip::app::Clusters::OccupancySensing::Id;
    attribute_id = chip::app::Clusters::OccupancySensing::Attributes::Occupancy::Id;
    internally_managed = false;
  } else {
    val = ::esp_matter_bool(state);
    cluster_id = chip::app::Clusters::BooleanState::Id;
    attribute_id = chip::app::Clusters::BooleanState::Attributes::StateValue::Id;
    // In esp-matter 1.5.1 BooleanState.StateValue is created with
    // ATTRIBUTE_FLAG_MANAGED_INTERNALLY. attribute::update() rejects writes
    // with ESP_ERR_NOT_SUPPORTED — we must use attribute::report() which
    // marks the value dirty without going through the writeable path.
    internally_managed = true;
  }
  if (!internally_managed) {
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
  // MarkDirty), which asserts the CHIP stack lock is held by the current
  // thread. We are on the ESPHome scheduler thread here, so grab the lock
  // for the duration of the write.
  ::esp_matter::lock::ScopedChipStackLock lock(portMAX_DELAY);
  cluster->SetStateValue(state);
}

}  // namespace matter
}  // namespace esphome

#endif  // USE_BINARY_SENSOR
#endif  // USE_ESP_IDF
