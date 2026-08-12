#include "radon_eye_rd200.h"

#ifdef USE_BLE_CLIENT_GATT_NODES

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::radon_eye_rd200 {

static const char *const TAG = "radon_eye_rd200";

using ble_device_base::ESPBTUUID;

// V1 (RD200 firmware < 2.0) exposes a vendor service; V2 (>= 2.0) moved to
// Bluetooth-base (16-bit) UUIDs with a different command byte and payload
// layout.
static const char *const SERVICE_UUID_V1 = "00001523-1212-efde-1523-785feabcd123";
static const char *const WRITE_CHARACTERISTIC_UUID_V1 = "00001524-1212-efde-1523-785feabcd123";
static const char *const READ_CHARACTERISTIC_UUID_V1 = "00001525-1212-efde-1523-785feabcd123";
static const uint8_t WRITE_COMMAND_V1 = 0x50;

static const uint16_t SERVICE_UUID_V2 = 0x1523;
static const uint16_t WRITE_CHARACTERISTIC_UUID_V2 = 0x1524;
static const uint16_t READ_CHARACTERISTIC_UUID_V2 = 0x1525;
static const uint8_t WRITE_COMMAND_V2 = 0x40;

// Minimum notification payload carrying all three measurements.
static const uint16_t MESSAGE_MIN_LEN_V1 = 20;
static const uint16_t MESSAGE_MIN_LEN_V2 = 68;

RadonEyeRD200::RadonEyeRD200() : PollingComponent(10000) {}

void RadonEyeRD200::update() {
  if (this->parent()->connected())
    return;
  if (!this->parent()->enabled) {
    ESP_LOGW(TAG, "Reconnecting to device");
    this->parent()->set_enabled(true);
  } else {
    ESP_LOGW(TAG, "Connection in progress");
  }
}

void RadonEyeRD200::on_connected(const ble_device_base::GattServiceTable &table) {
  if (!this->resolve_handles_(table)) {
    // Retried on the next poll (update() re-enables the client).
    this->parent()->set_enabled(false);
    return;
  }
  // Local notification registration; the CCCD write follows in
  // on_notify_state (the contract leaves the CCCD to the node).
  if (this->parent()->notify_characteristic(this->read_handle_, true) != 0) {
    this->parent()->set_enabled(false);
  }
}

bool RadonEyeRD200::resolve_handles_(const ble_device_base::GattServiceTable &table) {
  struct Variant {
    ESPBTUUID service;
    ESPBTUUID write_chr;
    ESPBTUUID read_chr;
    uint8_t command;
  };
  // Built on the stack per (cold) discovery so the UUID objects stay out of
  // static RAM; the V1 strings live in flash.
  const Variant variants[] = {
      {ESPBTUUID::from_raw(SERVICE_UUID_V1), ESPBTUUID::from_raw(WRITE_CHARACTERISTIC_UUID_V1),
       ESPBTUUID::from_raw(READ_CHARACTERISTIC_UUID_V1), WRITE_COMMAND_V1},
      {ESPBTUUID::from_uint16(SERVICE_UUID_V2), ESPBTUUID::from_uint16(WRITE_CHARACTERISTIC_UUID_V2),
       ESPBTUUID::from_uint16(READ_CHARACTERISTIC_UUID_V2), WRITE_COMMAND_V2},
  };
  for (const auto &variant : variants) {
    const auto *service = ble_device_base::find_service(table, variant.service);
    if (service == nullptr) {
      continue;
    }
    const auto *read_chr = ble_device_base::find_characteristic(table, *service, variant.read_chr);
    const auto *write_chr = ble_device_base::find_characteristic(table, *service, variant.write_chr);
    if (read_chr == nullptr || write_chr == nullptr) {
      ESP_LOGW(TAG, "Service found but a sensor characteristic is missing");
      return false;
    }
    this->cccd_handle_ = ble_device_base::find_cccd(table, *read_chr);
    if (this->cccd_handle_ == 0) {
      ESP_LOGW(TAG, "Sensor read characteristic has no CCCD");
      return false;
    }
    this->read_handle_ = read_chr->value_handle;
    this->write_handle_ = write_chr->value_handle;
    this->write_command_ = variant.command;
    return true;
  }
  ESP_LOGW(TAG, "No supported device has been found, disconnecting");
  return false;
}

void RadonEyeRD200::on_notify_state(uint16_t handle, bool enabled, int error) {
  if (handle != this->read_handle_) {
    return;
  }
  if (error != 0) {
    ESP_LOGW(TAG, "Error registering for sensor notify, status=%d", error);
    this->parent()->set_enabled(false);
    return;
  }
  if (!enabled) {
    return;
  }
  static const uint8_t ENABLE_NOTIFY[2] = {0x01, 0x00};
  if (this->parent()->write_descriptor(this->cccd_handle_, ENABLE_NOTIFY, sizeof(ENABLE_NOTIFY)) != 0) {
    this->parent()->set_enabled(false);
  }
}

void RadonEyeRD200::on_write_result(uint16_t handle, int error) {
  // The command write (no response) also lands here; only the CCCD
  // completion advances the sequence.
  if (handle != this->cccd_handle_) {
    return;
  }
  if (error != 0) {
    ESP_LOGE(TAG, "write descr failed, error status = %x", error);
    this->parent()->set_enabled(false);
    return;
  }
  ESP_LOGV(TAG, "Write descr success, writing 0x%02X at write_handle=%d", this->write_command_, this->write_handle_);
  if (this->parent()->write_characteristic(this->write_handle_, &this->write_command_, sizeof(this->write_command_),
                                           false) != 0) {
    ESP_LOGW(TAG, "Error writing 0x%02x command", this->write_command_);
    this->parent()->set_enabled(false);
  }
}

void RadonEyeRD200::on_notify(uint16_t handle, const uint8_t *data, uint16_t len) {
  if (handle != this->read_handle_) {
    return;
  }
  ESP_LOGV(TAG, "Received notify value, %d bytes", len);
  this->read_sensors_(data, len);
  // This instance must not stay connected so other clients can connect to it
  // (e.g. the mobile app).
  this->parent()->set_enabled(false);
}

void RadonEyeRD200::read_sensors_(const uint8_t *value, uint16_t value_len) {
  if (value_len < 1) {
    ESP_LOGW(TAG, "Unexpected empty message");
    return;
  }

  uint8_t command = value[0];

  if ((command == WRITE_COMMAND_V1 && value_len < MESSAGE_MIN_LEN_V1) ||
      (command == WRITE_COMMAND_V2 && value_len < MESSAGE_MIN_LEN_V2)) {
    ESP_LOGW(TAG, "Unexpected command 0x%02X message length %d", command, value_len);
    return;
  }

  // Example data V1:
  // 501085EBB9400000000000000000220025000000
  // Example data V2:
  // 4042323230313033525532303338330652443230304e56322e302e3200014a00060a00080000000300010079300000e01108001c00020000003822005c8f423fa4709d3f
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  // Sized for the longest supported message; format_hex_to truncates longer.
  char hex_buf[format_hex_size(MESSAGE_MIN_LEN_V2)];
  ESP_LOGV(TAG, "radon sensors raw bytes: %s", format_hex_to(hex_buf, value, value_len));
#endif

  // Convert from pCi/L to Bq/m³
  constexpr float convert_to_bwpm3 = 37.0;

  float radon_now;    // in Bq/m³
  float radon_day;    // in Bq/m³
  float radon_month;  // in Bq/m³
  if (command == WRITE_COMMAND_V1) {
    // Use memcpy to avoid unaligned memory access
    float temp;
    memcpy(&temp, value + 2, sizeof(float));
    radon_now = temp * convert_to_bwpm3;
    memcpy(&temp, value + 6, sizeof(float));
    radon_day = temp * convert_to_bwpm3;
    memcpy(&temp, value + 10, sizeof(float));
    radon_month = temp * convert_to_bwpm3;
  } else if (command == WRITE_COMMAND_V2) {
    // Use memcpy to avoid unaligned memory access
    uint16_t temp;
    memcpy(&temp, value + 33, sizeof(uint16_t));
    radon_now = temp;
    memcpy(&temp, value + 35, sizeof(uint16_t));
    radon_day = temp;
    memcpy(&temp, value + 37, sizeof(uint16_t));
    radon_month = temp;
  } else {
    ESP_LOGW(TAG, "Unexpected command value: 0x%02X", command);
    return;
  }

  if (this->radon_sensor_ != nullptr) {
    this->radon_sensor_->publish_state(radon_now);
  }

  if (this->radon_long_term_sensor_ != nullptr) {
    if (radon_month > 0) {
      ESP_LOGV(TAG, "Radon Long Term based on month");
      this->radon_long_term_sensor_->publish_state(radon_month);
    } else {
      ESP_LOGV(TAG, "Radon Long Term based on day");
      this->radon_long_term_sensor_->publish_state(radon_day);
    }
  }

  ESP_LOGV(TAG,
           "  Measurements (Bq/m³) now: %0.03f, day: %0.03f, month: %0.03f\n"
           "  Measurements (pCi/L) now: %0.03f, day: %0.03f, month: %0.03f",
           radon_now, radon_day, radon_month, radon_now / convert_to_bwpm3, radon_day / convert_to_bwpm3,
           radon_month / convert_to_bwpm3);
}

void RadonEyeRD200::dump_config() {
  LOG_SENSOR("  ", "Radon", this->radon_sensor_);
  LOG_SENSOR("  ", "Radon Long Term", this->radon_long_term_sensor_);
}

}  // namespace esphome::radon_eye_rd200

#endif  // USE_BLE_CLIENT_GATT_NODES
