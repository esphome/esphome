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
#ifdef USE_SENSOR

#include "matter_sensor_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>

namespace esphome::matter {

static const char *const TAG = "matter.sensor";

MatterSensorEndpoint::MatterSensorEndpoint(sensor::Sensor *s) : sensor_(s) {}

bool MatterSensorEndpoint::detect_kind() {
  // StringRef supports operator==(const char *) via esphome/core/string_ref.h,
  // so we compare the unit directly against literals — no intermediate copy or
  // std allocation needed.
  const StringRef &unit = this->sensor_->get_unit_of_measurement_ref();

  // Preferred: unit_of_measurement — narrow set of well-known unit strings.
  // "°C" in source is already the UTF-8 sequence \xc2\xb0"C", so we drop the
  // explicit-bytes duplicate that used to sit next to it (clang-tidy's
  // misc-redundant-expression flags the pair as equivalent operands).
  if (unit == "°C" || unit == "C") {
    this->kind_ = Kind::KIND_TEMPERATURE;
  } else if (unit == "%") {
    // Ambiguous — could be humidity or battery. Fall through to device_class
    // check below; only assume humidity if device_class matches too, else
    // skip so we don't accidentally publish battery % as humidity.
  } else if (unit == "hPa") {
    this->kind_ = Kind::KIND_PRESSURE_HPA;
  } else if (unit == "kPa") {
    this->kind_ = Kind::KIND_PRESSURE_KPA;
  } else if (unit == "lx") {
    this->kind_ = Kind::KIND_ILLUMINANCE;
  } else if (unit == "m³/h" || unit == "m^3/h" || unit == "m3/h") {
    this->kind_ = Kind::KIND_FLOW;
  }

  if (this->kind_ != Kind::KIND_UNKNOWN) {
    return true;
  }

  // Fallback: device_class. Uses the buffer-based API (the pre-2026.3 direct
  // accessor is deprecated and vanishes in 2026.9).
  char buf[esphome::MAX_DEVICE_CLASS_LENGTH]{};
  const char *dc = this->sensor_->get_device_class_to(std::span<char, esphome::MAX_DEVICE_CLASS_LENGTH>(buf));
  if (dc == nullptr || dc[0] == '\0') {
    return false;
  }
  if (std::strcmp(dc, "temperature") == 0) {
    this->kind_ = Kind::KIND_TEMPERATURE;
  } else if (std::strcmp(dc, "humidity") == 0) {
    this->kind_ = Kind::KIND_HUMIDITY;
  } else if (std::strcmp(dc, "pressure") == 0 || std::strcmp(dc, "atmospheric_pressure") == 0) {
    // Assume hPa when the user gave a device_class but no clear unit —
    // matches ESPHome's typical sensor: platform: bmp280 defaults.
    this->kind_ = Kind::KIND_PRESSURE_HPA;
  } else if (std::strcmp(dc, "illuminance") == 0) {
    this->kind_ = Kind::KIND_ILLUMINANCE;
  }
  return this->kind_ != Kind::KIND_UNKNOWN;
}

bool MatterSensorEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for sensor '%s'", this->sensor_->get_name().c_str());
    return false;
  }
  if (this->kind_ == Kind::KIND_UNKNOWN) {
    ESP_LOGW(TAG, "skipping sensor '%s' — no unit/device_class mapping to Matter cluster",
             this->sensor_->get_name().c_str());
    return false;
  }

  ::esp_matter::endpoint_t *endpoint = nullptr;
  const char *device_type_name = "";
  switch (this->kind_) {
    case Kind::KIND_TEMPERATURE: {
      ::esp_matter::endpoint::temperature_sensor::config_t config;
      endpoint =
          ::esp_matter::endpoint::temperature_sensor::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
      device_type_name = "temperature_sensor";
      break;
    }
    case Kind::KIND_HUMIDITY: {
      ::esp_matter::endpoint::humidity_sensor::config_t config;
      endpoint = ::esp_matter::endpoint::humidity_sensor::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
      device_type_name = "humidity_sensor";
      break;
    }
    case Kind::KIND_PRESSURE_HPA:
    case Kind::KIND_PRESSURE_KPA: {
      ::esp_matter::endpoint::pressure_sensor::config_t config;
      endpoint = ::esp_matter::endpoint::pressure_sensor::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
      device_type_name = "pressure_sensor";
      break;
    }
    case Kind::KIND_ILLUMINANCE: {
      ::esp_matter::endpoint::light_sensor::config_t config;
      endpoint = ::esp_matter::endpoint::light_sensor::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
      device_type_name = "light_sensor";
      break;
    }
    case Kind::KIND_FLOW: {
      ::esp_matter::endpoint::flow_sensor::config_t config;
      endpoint = ::esp_matter::endpoint::flow_sensor::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
      device_type_name = "flow_sensor";
      break;
    }
    case Kind::KIND_UNKNOWN:
      return false;
  }
  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create sensor endpoint for '%s'", this->sensor_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->sensor_->get_name());

  this->sensor_->add_on_state_callback([this](float state) {
    if (std::isnan(state)) {
      // Sensor became unavailable — every MeasuredValue attribute we use is
      // nullable per Matter spec, so publish null instead of leaving the last
      // good reading in the fabric cache indefinitely.
      ESP_LOGD(TAG, "device state change → fabric: endpoint=%u state=NaN → null sensor='%s'", this->endpoint_id_,
               this->sensor_->get_name().c_str());
      this->report_null_to_fabric_();
      return;
    }
    ESP_LOGD(TAG, "device state change → fabric: endpoint=%u state=%.3f sensor='%s'", this->endpoint_id_, state,
             this->sensor_->get_name().c_str());
    this->report_state_to_fabric_(state);
  });

  ESP_LOGI(TAG, "registered sensor '%s' as Matter %s endpoint %u", this->sensor_->get_name().c_str(), device_type_name,
           this->endpoint_id_);
  return true;
}

void MatterSensorEndpoint::push_initial_state() {
  if (this->sensor_->has_state()) {
    this->report_state_to_fabric_(this->sensor_->get_state());
  }
}

void MatterSensorEndpoint::report_state_to_fabric_(float state) {
  ::esp_matter_attr_val_t val;
  uint32_t cluster_id = 0;
  uint32_t attribute_id = 0;
  switch (this->kind_) {
    case Kind::KIND_TEMPERATURE: {
      // °C × 100, int16, clamped to spec range.
      int32_t v = static_cast<int32_t>(std::lround(state * 100.0f));
      v = std::clamp(v, static_cast<int32_t>(-27315), static_cast<int32_t>(32767));
      val = ::esp_matter_nullable_int16(static_cast<int16_t>(v));
      cluster_id = chip::app::Clusters::TemperatureMeasurement::Id;
      attribute_id = chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Id;
      break;
    }
    case Kind::KIND_HUMIDITY: {
      // %RH × 100, uint16, 0..10000.
      int32_t v = static_cast<int32_t>(std::lround(state * 100.0f));
      v = std::clamp(v, static_cast<int32_t>(0), static_cast<int32_t>(10000));
      val = ::esp_matter_nullable_uint16(static_cast<uint16_t>(v));
      cluster_id = chip::app::Clusters::RelativeHumidityMeasurement::Id;
      attribute_id = chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Id;
      break;
    }
    case Kind::KIND_PRESSURE_HPA:
    case Kind::KIND_PRESSURE_KPA: {
      // Matter attribute unit = kPa × 10, int16.
      float kpa = (this->kind_ == Kind::KIND_PRESSURE_HPA) ? state * 0.1f : state;
      int32_t v = static_cast<int32_t>(std::lround(kpa * 10.0f));
      v = std::clamp(v, static_cast<int32_t>(-32768), static_cast<int32_t>(32767));
      val = ::esp_matter_nullable_int16(static_cast<int16_t>(v));
      cluster_id = chip::app::Clusters::PressureMeasurement::Id;
      attribute_id = chip::app::Clusters::PressureMeasurement::Attributes::MeasuredValue::Id;
      break;
    }
    case Kind::KIND_ILLUMINANCE: {
      // Matter formula: 10000 * log10(lux) + 1. lux <= 1 clamps to 1.
      float lux = std::max(state, 1.0f);
      int32_t v = static_cast<int32_t>(std::lround(10000.0f * std::log10(lux) + 1.0f));
      v = std::clamp(v, static_cast<int32_t>(1), static_cast<int32_t>(0xFFFE));
      val = ::esp_matter_nullable_uint16(static_cast<uint16_t>(v));
      cluster_id = chip::app::Clusters::IlluminanceMeasurement::Id;
      attribute_id = chip::app::Clusters::IlluminanceMeasurement::Attributes::MeasuredValue::Id;
      break;
    }
    case Kind::KIND_FLOW: {
      // Matter attribute unit = m³/h × 10, uint16.
      int32_t v = static_cast<int32_t>(std::lround(state * 10.0f));
      v = std::clamp(v, static_cast<int32_t>(0), static_cast<int32_t>(0xFFFE));
      val = ::esp_matter_nullable_uint16(static_cast<uint16_t>(v));
      cluster_id = chip::app::Clusters::FlowMeasurement::Id;
      attribute_id = chip::app::Clusters::FlowMeasurement::Attributes::MeasuredValue::Id;
      break;
    }
    case Kind::KIND_UNKNOWN:
      return;
  }
  MatterComponent::instance()->defer_attribute_update(this->endpoint_id_, cluster_id, attribute_id, val);
}

void MatterSensorEndpoint::report_null_to_fabric_() {
  ::esp_matter_attr_val_t val;
  uint32_t cluster_id = 0;
  uint32_t attribute_id = 0;
  // Cases share the same shape (nullable value + cluster/attribute id assignment)
  // but each targets a distinct cluster — clang-tidy's bugprone-branch-clone
  // would flag the pattern despite the semantic differences.
  // NOLINTBEGIN(bugprone-branch-clone)
  switch (this->kind_) {
    case Kind::KIND_TEMPERATURE:
      val = ::esp_matter_nullable_int16(::nullable<int16_t>());
      cluster_id = chip::app::Clusters::TemperatureMeasurement::Id;
      attribute_id = chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Id;
      break;
    case Kind::KIND_HUMIDITY:
      val = ::esp_matter_nullable_uint16(::nullable<uint16_t>());
      cluster_id = chip::app::Clusters::RelativeHumidityMeasurement::Id;
      attribute_id = chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Id;
      break;
    case Kind::KIND_PRESSURE_HPA:
    case Kind::KIND_PRESSURE_KPA:
      val = ::esp_matter_nullable_int16(::nullable<int16_t>());
      cluster_id = chip::app::Clusters::PressureMeasurement::Id;
      attribute_id = chip::app::Clusters::PressureMeasurement::Attributes::MeasuredValue::Id;
      break;
    case Kind::KIND_ILLUMINANCE:
      val = ::esp_matter_nullable_uint16(::nullable<uint16_t>());
      cluster_id = chip::app::Clusters::IlluminanceMeasurement::Id;
      attribute_id = chip::app::Clusters::IlluminanceMeasurement::Attributes::MeasuredValue::Id;
      break;
    case Kind::KIND_FLOW:
      val = ::esp_matter_nullable_uint16(::nullable<uint16_t>());
      cluster_id = chip::app::Clusters::FlowMeasurement::Id;
      attribute_id = chip::app::Clusters::FlowMeasurement::Attributes::MeasuredValue::Id;
      break;
    case Kind::KIND_UNKNOWN:
      return;
  }
  // NOLINTEND(bugprone-branch-clone)
  MatterComponent::instance()->defer_attribute_update(this->endpoint_id_, cluster_id, attribute_id, val);
}

}  // namespace esphome::matter

#endif  // USE_SENSOR
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
