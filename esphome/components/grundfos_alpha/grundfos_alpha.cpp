#include "grundfos_alpha.h"

#include <cmath>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::grundfos_alpha {

ESPHOME_LOG_TAG(TAG, "grundfos_alpha");

using genibus::GenibusError;

static constexpr uint8_t CLASS_MEASURED_DATA = 2;
// Class 2 GET of unit_family (148), unit_type (149) and unit_version (150)
static constexpr uint8_t IDENTITY_REQUEST[] = {CLASS_MEASURED_DATA, 0x03, 148, 149, 150};
// warned_ holds a bit per object index below these flags
static constexpr uint8_t WARNED_IDENTITY = 0x80;
static constexpr uint8_t WARNED_FLAGS = WARNED_IDENTITY;

// The data model of the Grundfos GO app converts with 1 m = 9804.14 Pa. A real ALPHA3 reports the default and limits
// of its constant pressure set point (86/15) as 29412 (default), 14706 (minimum) and 44118 Pa (maximum): 3, 1.5 and
// 4.5 m at 9804 Pa/m. The alpha3 component uses 10000.
static constexpr float PASCAL_PER_METER = 9804.0f;
static constexpr float SECONDS_PER_HOUR = 3600.0f;

// Object layouts and field names from the data model of the Grundfos GO app for the ALPHA3 Model B. The alpha3
// component reads the same requests, reply headers and the offsets 0, 4, 0, 8, 12 and 20 from a real ALPHA3.
// 93/289, type 0x0130 v1: pump_flow (m³/s), pump_differential_pressure (Pa), inlet_pressure, outlet_pressure,
// media_temperature, differential_temperature
static constexpr Field ALPHA3_PROCESS_FIELDS[] = {
    {Reading::READING_FLOW, 0, SECONDS_PER_HOUR},
    {Reading::READING_HEAD, 4, 1.0f / PASCAL_PER_METER},
    {Reading::READING_MEDIUM_TEMPERATURE, 16, 1.0f},  // °C, unlike the SI units of the other fields
};
// 87/69, type 0x0100 v1: grid_voltage, dc_link_voltage, current, power_consumption, dc_power_consumption,
// absolute_speed (rpm), three temperatures, pwm_type
static constexpr Field ALPHA3_MOTOR_FIELDS[] = {
    {Reading::READING_VOLTAGE, 0, 1.0f},
    {Reading::READING_CURRENT, 8, 1.0f},
    // dc_power_consumption: the value the Grundfos GO app shows as power; alpha3 reads offset 12
    {Reading::READING_POWER, 16, 1.0f},
    {Reading::READING_SPEED, 20, 1.0f},
};
static constexpr ObjectLayout ALPHA3_OBJECTS[] = {
    {93, 289, 0x0130, 1, 24, ALPHA3_PROCESS_FIELDS},
    {87, 69, 0x0100, 1, 37, ALPHA3_MOTOR_FIELDS},
};

PROGMEM_STRING_TABLE(ModelNames, "ALPHA3 Model B");
// Add a model only after it was tested on a real pump
static constexpr Model MODELS[] = {
    {52, 1, 0, 0, ALPHA3_OBJECTS},
};

// missing_ and warned_ keep a bit per object index
static constexpr bool objects_fit_bits() {
  for (const auto &model : MODELS) {
    if (((1u << model.objects.size()) - 1u) & (WARNED_FLAGS | ~0xFFu))
      return false;
  }
  return true;
}
static_assert(objects_fit_bits(), "A model has more objects than missing_ and warned_ have bits");

void GrundfosAlpha::dump_config() {
  ESP_LOGCONFIG(TAG, "Grundfos ALPHA:\n  Address: 0x%02X", this->address_);
  LOG_UPDATE_INTERVAL(this);
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Flow", this->flow_sensor_);
  LOG_SENSOR("  ", "Head", this->head_sensor_);
  LOG_SENSOR("  ", "Medium temperature", this->medium_temperature_sensor_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  LOG_SENSOR("  ", "Speed", this->speed_sensor_);
#endif
}

void GrundfosAlpha::update() {
  // A pump without power is normal: skip quietly and show the values as unknown
  if (!this->parent_->is_connected()) {
    this->publish_unknown_();
    return;
  }
  if (this->unsupported_ || this->parent_->is_busy(this))
    return;
  if (this->model_ == nullptr) {
    if (this->parent_->send(this, IDENTITY_REQUEST))
      this->current_ = REQUEST_IDENTITY;
    return;
  }
  this->request_from_(0);
}

void GrundfosAlpha::on_response(const genibus::GenibusResponse &response) {
  const uint8_t current = this->current_;
  this->current_ = REQUEST_NONE;
  // on_disconnected() follows and marks the values unknown
  if (response.error == GenibusError::GENIBUS_ERROR_DISCONNECTED)
    return;
  if (current == REQUEST_IDENTITY) {
    if (this->identify_(response))
      this->request_from_(0);
    return;
  }
  if (this->model_ == nullptr || current >= this->model_->objects.size())
    return;
  this->decode_(current, response);
  this->request_from_(current + 1);
}

bool GrundfosAlpha::identify_(const genibus::GenibusResponse &response) {
  // The bus has logged a timeout or a broken frame; try again at the next update
  if (response.error != GenibusError::GENIBUS_ERROR_NONE)
    return false;
  genibus::ApduReader reader(response.apdus);
  genibus::Apdu apdu{};
  const bool has_apdu = reader.next(apdu);
  if (!has_apdu || apdu.data_class != CLASS_MEASURED_DATA || apdu.ack != 0 || apdu.data.size() != 3) {
    if (this->first_warning_(WARNED_IDENTITY)) {
      if (!has_apdu) {
        ESP_LOGW(TAG, "No APDU in the identity reply");
      } else {
        ESP_LOGW(TAG, "No unit family, type and version in the reply (class %u, ACK %u, %u bytes)", apdu.data_class,
                 apdu.ack, static_cast<unsigned>(apdu.data.size()));
      }
    }
    this->status_set_warning(LOG_STR("No pump identity"));
    return false;
  }
  const uint8_t family = apdu.data[0];
  const uint8_t type = apdu.data[1];
  const uint8_t version = apdu.data[2];
  for (const auto &model : MODELS) {
    if (model.family == family && model.type == type && model.version == version) {
      this->model_ = &model;
      this->status_clear_warning();
      ESP_LOGI(TAG, "Found %s", LOG_STR_ARG(ModelNames::get_log_str(model.name, 0)));
      return true;
    }
  }
  this->unsupported_ = true;
  this->status_set_warning(LOG_STR("Model not supported"));
  ESP_LOGW(TAG, "Model %u/%u/%u not supported", family, type, version);
  return false;
}

void GrundfosAlpha::decode_(uint8_t index, const genibus::GenibusResponse &response) {
  const ObjectLayout &layout = this->model_->objects[index];
  genibus::ApduReader reader(response.apdus);
  genibus::Apdu apdu{};
  genibus::DataObject object{};
  const bool replied = response.error == GenibusError::GENIBUS_ERROR_NONE && reader.next(apdu);
  const bool parsed = replied && genibus::parse_data_object(apdu, object);
  const bool ok =
      parsed && object.type == layout.type && object.version == layout.version && object.body.size() == layout.size;
  if (replied && genibus::is_object_missing(apdu)) {
    // A firmware without the object: skip it until the next connection instead of asking at every update
    this->missing_ |= 1u << index;
    if (this->first_warning_(1u << index)) {
      ESP_LOGW(TAG, "The pump has no object %u/%u; its sensors stay unknown", layout.object, layout.sub_id);
    }
  } else if (!ok && response.error == GenibusError::GENIBUS_ERROR_NONE && this->first_warning_(1u << index)) {
    // Never read offsets from another version of the object; log only the fields the reply really had
    if (!replied) {
      ESP_LOGW(TAG, "No APDU in the reply to object %u/%u", layout.object, layout.sub_id);
    } else if (!parsed) {
      ESP_LOGW(TAG, "Unexpected reply to object %u/%u: class %u, ACK %u, %u bytes", layout.object, layout.sub_id,
               apdu.data_class, apdu.ack, static_cast<unsigned>(apdu.data.size()));
    } else {
      ESP_LOGW(TAG, "Unexpected reply to object %u/%u: type 0x%04X v%u, %u bytes", layout.object, layout.sub_id,
               object.type, object.version, static_cast<unsigned>(object.body.size()));
    }
  }
  for (const auto &field : layout.fields) {
    float value = NAN;
    if (ok && field.offset + 4u <= object.body.size()) {
      const auto *at = &object.body[field.offset];
      const uint32_t raw = encode_uint32(at[0], at[1], at[2], at[3]);
      // A real ALPHA3 sends exactly 0 as the medium temperature for about a minute after power-up: no value yet
      if (raw != 0 || field.reading != Reading::READING_MEDIUM_TEMPERATURE)
        value = bit_cast<float>(raw) * field.factor;
    }
    this->publish_(field.reading, value);
  }
  if (ok)
    this->has_values_ = true;
}

void GrundfosAlpha::request_from_(size_t index) {
  const auto objects = this->model_->objects;
  for (; index < objects.size(); index++) {
    const ObjectLayout &layout = objects[index];
    if ((this->missing_ & (1u << index)) != 0 || !this->wants_(layout))
      continue;
    if (this->request_object_(layout.object, layout.sub_id))
      this->current_ = static_cast<uint8_t>(index);
    return;
  }
}

bool GrundfosAlpha::wants_(const ObjectLayout &layout) const {
#ifdef USE_SENSOR
  for (const auto &field : layout.fields) {
    if (this->sensor_(field.reading) != nullptr)
      return true;
  }
#endif
  return false;
}

void GrundfosAlpha::publish_(Reading reading, float value) {
#ifdef USE_SENSOR
  PUBLISH_STATE(this->sensor_(reading), value);
#endif
}

#ifdef USE_SENSOR
sensor::Sensor *GrundfosAlpha::sensor_(Reading reading) const {
  switch (reading) {
    case Reading::READING_FLOW:
      return this->flow_sensor_;
    case Reading::READING_HEAD:
      return this->head_sensor_;
    case Reading::READING_MEDIUM_TEMPERATURE:
      return this->medium_temperature_sensor_;
    case Reading::READING_VOLTAGE:
      return this->voltage_sensor_;
    case Reading::READING_CURRENT:
      return this->current_sensor_;
    case Reading::READING_POWER:
      return this->power_sensor_;
    case Reading::READING_SPEED:
      return this->speed_sensor_;
  }
  return nullptr;
}
#endif

void GrundfosAlpha::publish_unknown_() {
  if (!this->has_values_)
    return;
  this->has_values_ = false;
  if (this->model_ == nullptr)
    return;
  for (const auto &layout : this->model_->objects) {
    for (const auto &field : layout.fields)
      this->publish_(field.reading, NAN);
  }
}

bool GrundfosAlpha::first_warning_(uint8_t bit) {
  if ((this->warned_ & bit) != 0)
    return false;
  this->warned_ |= bit;
  return true;
}

}  // namespace esphome::grundfos_alpha
