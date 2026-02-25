#include "bthome_decoder.h"

#include "esphome/core/log.h"

namespace esphome {
namespace bthome {

static const char *const TAG = "bthome";

static size_t get_bthome_value_length(BTHomeObjectType obj_type) {
  switch (obj_type) {
    // 1 Byte (uint8 / sint8)
    case BTHomeObjectType::PACKET_ID:
    case BTHomeObjectType::BATTERY_PCT:
    case BTHomeObjectType::COUNT_U8:
    case BTHomeObjectType::HUMIDITY_PCT_U8:
    case BTHomeObjectType::MOISTURE_PCT_U8:
    case BTHomeObjectType::UV_INDEX_E1:
    case BTHomeObjectType::TEMPERATURE_C_I8:
    case BTHomeObjectType::TEMPERATURE_C_I8_0_35:
    case BTHomeObjectType::COUNT_I8:
    case BTHomeObjectType::CHANNEL:

    // Binary sensors:
    case BTHomeObjectType::GENERIC_BOOLEAN:
    case BTHomeObjectType::POWER_ON:
    case BTHomeObjectType::OPENING_OPEN:
    case BTHomeObjectType::BATTERY_LOW:
    case BTHomeObjectType::BATTERY_CHARGING:
    case BTHomeObjectType::CO_DETECTED:
    case BTHomeObjectType::COLD_DETECTED:
    case BTHomeObjectType::CONNECTIVITY_CONNECTED:
    case BTHomeObjectType::DOOR_OPEN:
    case BTHomeObjectType::GARAGE_DOOR_OPEN:
    case BTHomeObjectType::GAS_DETECTED:
    case BTHomeObjectType::HEAT_DETECTED:
    case BTHomeObjectType::LIGHT_DETECTED:
    case BTHomeObjectType::LOCK_UNLOCKED:
    case BTHomeObjectType::MOISTURE_WET:
    case BTHomeObjectType::MOTION_DETECTED:
    case BTHomeObjectType::MOVING_ACTIVE:
    case BTHomeObjectType::OCCUPANCY_DETECTED:
    case BTHomeObjectType::PLUG_PLUGGED_IN:
    case BTHomeObjectType::PRESENCE_HOME:
    case BTHomeObjectType::PROBLEM_DETECTED:
    case BTHomeObjectType::RUNNING_ACTIVE:
    case BTHomeObjectType::SAFETY_SAFE:
    case BTHomeObjectType::SMOKE_DETECTED:
    case BTHomeObjectType::SOUND_DETECTED:
    case BTHomeObjectType::TAMPER_ACTIVE:
    case BTHomeObjectType::VIBRATION_DETECTED:
    case BTHomeObjectType::WINDOW_OPEN:
      return 1;

    // 2 Bytes (uint16 / sint16)
    case BTHomeObjectType::TEMPERATURE_C_E2:
    case BTHomeObjectType::HUMIDITY_PCT_E2:
    case BTHomeObjectType::MASS_KG_E2:
    case BTHomeObjectType::MASS_LB_E2:
    case BTHomeObjectType::DEWPOINT_C_E2:
    case BTHomeObjectType::VOLTAGE_V_E3:
    case BTHomeObjectType::PM25_UGM3:
    case BTHomeObjectType::PM10_UGM3:
    case BTHomeObjectType::CO2_PPM:
    case BTHomeObjectType::TVOC_UGM3:
    case BTHomeObjectType::MOISTURE_PCT_E2:
    case BTHomeObjectType::COUNT_U16:
    case BTHomeObjectType::ROTATION_DEG_E1:
    case BTHomeObjectType::DISTANCE_MM:
    case BTHomeObjectType::DISTANCE_M_E1:
    case BTHomeObjectType::CURRENT_A_E3:
    case BTHomeObjectType::SPEED_MS_E2:
    case BTHomeObjectType::TEMPERATURE_C_E1:
    case BTHomeObjectType::VOLUME_L_E1:
    case BTHomeObjectType::VOLUME_ML:
    case BTHomeObjectType::VOLUME_FLOW_M3HR_E3:
    case BTHomeObjectType::VOLTAGE_V_E1:
    case BTHomeObjectType::ACCELERATION_MSS_E3:
    case BTHomeObjectType::GYROSCOPE_DEGS_E3:
    case BTHomeObjectType::CONDUCTIVITY_USCM:
    case BTHomeObjectType::COUNT_I16:
    case BTHomeObjectType::CURRENT_A_I16_E3:
    case BTHomeObjectType::DIRECTION_DEG_E2:
    case BTHomeObjectType::PRECIPITATION_MM_E1:
    case BTHomeObjectType::ROTATIONAL_SPEED_RPM:
      return 2;

    // 3 Bytes (uint24)
    case BTHomeObjectType::PRESSURE_HPA_E2:
    case BTHomeObjectType::ILLUMINANCE_LX_E2:
    case BTHomeObjectType::ENERGY_KWH_E3:
    case BTHomeObjectType::POWER_W_E2:
    case BTHomeObjectType::DURATION_S_E3:
    case BTHomeObjectType::GAS_M3_U24_E3:
      return 3;

    // 4 Bytes (uint32 / sint32)
    case BTHomeObjectType::COUNT_U32:
    case BTHomeObjectType::GAS_M3_U32_E3:
    case BTHomeObjectType::ENERGY_KWH_U32_E3:
    case BTHomeObjectType::VOLUME_L_U32_E3:
    case BTHomeObjectType::WATER_L_E3:
    case BTHomeObjectType::TIMESTAMP:
    case BTHomeObjectType::VOLUME_STORAGE_L_E3:
    case BTHomeObjectType::COUNT_I32:
    case BTHomeObjectType::POWER_W_I32_E2:
    case BTHomeObjectType::SPEED_MS_I32_E6:
    case BTHomeObjectType::ACCELERATION_MSS_I32_E6:
      return 4;

    // Variable length or Unknown
    case BTHomeObjectType::TEXT:
    case BTHomeObjectType::RAW:
    default:
      return 0;
  }
}

static uint16_t read_uint16_le(const uint8_t *data) { return (uint16_t) data[0] | ((uint16_t) data[1] << 8); }

static uint32_t read_uint24_le(const uint8_t *data) {
  return (uint32_t) data[0] | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16);
}

static uint32_t read_uint32_le(const uint8_t *data) {
  return (uint32_t) data[0] | ((uint32_t) data[1] << 8) | ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

static int16_t read_sint16_le(const uint8_t *data) { return (int16_t) read_uint16_le(data); }

static int32_t read_sint32_le(const uint8_t *data) { return (int32_t) read_uint32_le(data); }

float BTHomeObject::scaling_factor() const {
  switch (type) {
    // 0.000001f scaling
    case BTHomeObjectType::SPEED_MS_I32_E6:
    case BTHomeObjectType::ACCELERATION_MSS_I32_E6:
      return 0.000001f;

    // 0.001f scaling
    case BTHomeObjectType::ENERGY_KWH_E3:
    case BTHomeObjectType::VOLTAGE_V_E3:
    case BTHomeObjectType::DURATION_S_E3:
    case BTHomeObjectType::CURRENT_A_E3:
    case BTHomeObjectType::VOLUME_FLOW_M3HR_E3:
    case BTHomeObjectType::GAS_M3_U24_E3:
    case BTHomeObjectType::GAS_M3_U32_E3:
    case BTHomeObjectType::ENERGY_KWH_U32_E3:
    case BTHomeObjectType::VOLUME_L_U32_E3:
    case BTHomeObjectType::WATER_L_E3:
    case BTHomeObjectType::ACCELERATION_MSS_E3:
    case BTHomeObjectType::GYROSCOPE_DEGS_E3:
    case BTHomeObjectType::VOLUME_STORAGE_L_E3:
    case BTHomeObjectType::CURRENT_A_I16_E3:
      return 0.001f;

    // 0.01f scaling
    case BTHomeObjectType::TEMPERATURE_C_E2:
    case BTHomeObjectType::HUMIDITY_PCT_E2:
    case BTHomeObjectType::PRESSURE_HPA_E2:
    case BTHomeObjectType::ILLUMINANCE_LX_E2:
    case BTHomeObjectType::MASS_KG_E2:
    case BTHomeObjectType::MASS_LB_E2:
    case BTHomeObjectType::DEWPOINT_C_E2:
    case BTHomeObjectType::POWER_W_E2:
    case BTHomeObjectType::MOISTURE_PCT_E2:
    case BTHomeObjectType::SPEED_MS_E2:
    case BTHomeObjectType::POWER_W_I32_E2:
    case BTHomeObjectType::DIRECTION_DEG_E2:
      return 0.01f;

    // 0.1f scaling
    case BTHomeObjectType::ROTATION_DEG_E1:
    case BTHomeObjectType::TEMPERATURE_C_E1:
    case BTHomeObjectType::UV_INDEX_E1:
    case BTHomeObjectType::VOLUME_L_E1:
    case BTHomeObjectType::VOLTAGE_V_E1:
    case BTHomeObjectType::PRECIPITATION_MM_E1:
    case BTHomeObjectType::DISTANCE_M_E1:
      return 0.1f;

    // Unique scaling
    case BTHomeObjectType::TEMPERATURE_C_I8_0_35:
      return 0.35f;

    default:
      return 1.0f;
  }
}

bool BTHomeObject::is_signed() const {
  switch (type) {
    case BTHomeObjectType::ACCELERATION_MSS_I32_E6:
    case BTHomeObjectType::SPEED_MS_I32_E6:
    case BTHomeObjectType::TEMPERATURE_C_E2:
    case BTHomeObjectType::DEWPOINT_C_E2:
    case BTHomeObjectType::ROTATION_DEG_E1:
    case BTHomeObjectType::TEMPERATURE_C_E1:
    case BTHomeObjectType::TEMPERATURE_C_I8:
    case BTHomeObjectType::TEMPERATURE_C_I8_0_35:
    case BTHomeObjectType::COUNT_I8:
    case BTHomeObjectType::COUNT_I16:
    case BTHomeObjectType::COUNT_I32:
    case BTHomeObjectType::POWER_W_I32_E2:
    case BTHomeObjectType::CURRENT_A_I16_E3:
      return true;
    default:
      return false;
  }
}

uint32_t BTHomeObject::as_uint() const {
  switch (length) {
    case 1:
      return data[0];
    case 2:
      return read_uint16_le(data);
    case 3:
      return read_uint24_le(data);
    case 4:
      return read_uint32_le(data);
    default:
      return 0.0f;
  }
}

int32_t BTHomeObject::as_int() const {
  switch (length) {
    case 1:
      return (int8_t) data[0];
    case 2:
      return read_sint16_le(data);
    case 3:
      return read_uint24_le(data);
    case 4:
      return read_uint32_le(data);
    default:
      return 0.0f;
  }
}

float BTHomeObject::as_float() const { return scaling_factor() * (is_signed() ? float(as_int()) : float(as_uint())); }

bool BTHomeObject::as_bool() const { return as_uint() != 0; }

std::string_view BTHomeObject::as_string() const { return std::string_view((const char *) data, length); }

BTHomePayloadDecoder::Iterator::Iterator(const uint8_t *ptr, size_t remaining) : ptr_(ptr), remaining_(remaining) {
  this->parse_next_();
}

BTHomeObject BTHomePayloadDecoder::Iterator::operator*() const { return current_obj_; }

BTHomePayloadDecoder::Iterator &BTHomePayloadDecoder::Iterator::operator++() {
  this->parse_next_();
  return *this;
}

bool BTHomePayloadDecoder::Iterator::operator!=(const Iterator &other) const { return ptr_ != other.ptr_; }

void BTHomePayloadDecoder::Iterator::parse_next_() {
  if (remaining_ == 0) {
    ptr_ = nullptr;
    return;
  }

  const uint8_t *start = ptr_;
  BTHomeObjectType obj_type = static_cast<BTHomeObjectType>(*ptr_++);
  remaining_--;

  size_t value_length = 0;
  if (obj_type == BTHomeObjectType::TEXT || obj_type == BTHomeObjectType::RAW) {  // variable-size objects
    if (remaining_ == 0) {
      ptr_ = nullptr;
      remaining_ = 0;
      return;
    }
    value_length = *ptr_++;
    remaining_--;
  } else {
    value_length = get_bthome_value_length(obj_type);
    if (value_length == 0) {
      ptr_ = nullptr;  // Invalid type, stop iteration
      remaining_ = 0;
      return;
    }
  }

  if (remaining_ < value_length || value_length == 0) {
    ptr_ = nullptr;
    remaining_ = 0;
    return;
  }

  if (obj_type < current_obj_.type) {
    ESP_LOGVV(TAG, "BTHome objects not in ascending order");
  }

  current_obj_ = {obj_type, ptr_, value_length};
  ptr_ += value_length;
  remaining_ -= value_length;
}

BTHomePayloadDecoder::BTHomePayloadDecoder(const uint8_t *payload, size_t size) : payload_(payload), size_(size) {}

BTHomePayloadDecoder::Iterator BTHomePayloadDecoder::begin() const { return Iterator(payload_, size_); }
BTHomePayloadDecoder::Iterator BTHomePayloadDecoder::end() const { return Iterator(nullptr, 0); }

}  // namespace bthome
}  // namespace esphome
