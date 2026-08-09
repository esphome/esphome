#include "radon_eye_rd200.h"

#ifdef USE_BLE_GATT_CLIENT

#include "esphome/core/log.h"

#include <cstring>

namespace esphome::radon_eye_rd200 {

static const char *const TAG = "radon_eye_rd200";

using ble_device_base::ESPBTUUID;

// V1 (RD200 firmware < 2.0) exposes a vendor service; V2 (>= 2.0) moved to a
// 16-bit-based service with a different command byte and payload layout.
static const char *const SERVICE_UUID_V1 = "00001523-1212-efde-1523-785feabcd123";
static const char *const WRITE_CHARACTERISTIC_UUID_V1 = "00001524-1212-efde-1523-785feabcd123";
static const char *const READ_CHARACTERISTIC_UUID_V1 = "00001525-1212-efde-1523-785feabcd123";
static const uint8_t WRITE_COMMAND_V1 = 0x50;

static const char *const SERVICE_UUID_V2 = "00001523-0000-1000-8000-00805f9b34fb";
static const char *const WRITE_CHARACTERISTIC_UUID_V2 = "00001524-0000-1000-8000-00805f9b34fb";
static const char *const READ_CHARACTERISTIC_UUID_V2 = "00001525-0000-1000-8000-00805f9b34fb";
static const uint8_t WRITE_COMMAND_V2 = 0x40;

// BLE public address type (shared code space with the API/backends).
static const uint8_t BLE_ADDR_TYPE_PUBLIC = 0;

void RadonEyeRD200::update() {
  if (this->busy_) {
    ESP_LOGW(TAG, "Connection in progress");
    return;
  }
  ESP_LOGD(TAG, "Connecting");
  if (this->backend_->connect(this->address_, BLE_ADDR_TYPE_PUBLIC) == 0) {
    this->busy_ = true;
  } else {
    ESP_LOGW(TAG, "Connect request rejected, will retry");
  }
}

void RadonEyeRD200::on_connection_state(bool connected, uint16_t mtu, int error) {
  if (!connected) {
    if (error != 0) {
      ESP_LOGW(TAG, "Disconnected, status=%d", error);
    }
    this->busy_ = false;
    return;
  }
  ESP_LOGI(TAG, "Connected successfully!");
  if (this->backend_->discover_services() != 0) {
    this->abort_connection_();
  }
}

void RadonEyeRD200::on_service_discovery_done(int error) {
  if (error != 0) {
    ESP_LOGW(TAG, "Service discovery failed, status=%d", error);
    this->abort_connection_();
    return;
  }
  bool resolved = this->resolve_handles_();
  // The table is backend-owned transient storage; release before continuing.
  this->backend_->release_services();
  if (!resolved) {
    this->abort_connection_();
    return;
  }
  // Local notification registration; the CCCD write follows in
  // on_notify_state (the contract leaves the CCCD to the client).
  if (this->backend_->notify_characteristic(this->read_handle_, true) != 0) {
    this->abort_connection_();
  }
}

bool RadonEyeRD200::resolve_handles_() {
  auto table = this->backend_->get_service_table();
  const char *write_uuid;
  const char *read_uuid;
  const ble_device_base::GattService *service;
  if ((service = ble_device_base::find_service(table, ESPBTUUID::from_raw(SERVICE_UUID_V1))) != nullptr) {
    write_uuid = WRITE_CHARACTERISTIC_UUID_V1;
    read_uuid = READ_CHARACTERISTIC_UUID_V1;
    this->write_command_ = WRITE_COMMAND_V1;
  } else if ((service = ble_device_base::find_service(table, ESPBTUUID::from_raw(SERVICE_UUID_V2))) != nullptr) {
    write_uuid = WRITE_CHARACTERISTIC_UUID_V2;
    read_uuid = READ_CHARACTERISTIC_UUID_V2;
    this->write_command_ = WRITE_COMMAND_V2;
  } else {
    ESP_LOGW(TAG, "No supported device has been found, disconnecting");
    return false;
  }

  const auto *read_chr = ble_device_base::find_characteristic(table, *service, ESPBTUUID::from_raw(read_uuid));
  if (read_chr == nullptr) {
    ESP_LOGW(TAG, "No sensor read characteristic found at char %s", read_uuid);
    return false;
  }
  const auto *write_chr = ble_device_base::find_characteristic(table, *service, ESPBTUUID::from_raw(write_uuid));
  if (write_chr == nullptr) {
    ESP_LOGW(TAG, "No sensor write characteristic found at char %s", write_uuid);
    return false;
  }
  this->cccd_handle_ = ble_device_base::find_cccd(table, *read_chr);
  if (this->cccd_handle_ == 0) {
    ESP_LOGW(TAG, "Sensor read characteristic has no CCCD");
    return false;
  }
  this->read_handle_ = read_chr->value_handle;
  this->write_handle_ = write_chr->value_handle;
  return true;
}

void RadonEyeRD200::on_notify_state(uint16_t handle, bool enabled, int error) {
  if (error != 0) {
    ESP_LOGW(TAG, "Error registering for sensor notify, status=%d", error);
    this->abort_connection_();
    return;
  }
  static const uint8_t enable_notify[2] = {0x01, 0x00};
  if (this->backend_->write_descriptor(this->cccd_handle_, enable_notify, sizeof(enable_notify)) != 0) {
    this->abort_connection_();
  }
}

void RadonEyeRD200::on_write_result(uint16_t handle, int error) {
  // The command write (no response on some backends) also lands here; only
  // the CCCD completion advances the sequence.
  if (handle != this->cccd_handle_) {
    return;
  }
  if (error != 0) {
    ESP_LOGE(TAG, "write descr failed, error status = %x", error);
    this->abort_connection_();
    return;
  }
  ESP_LOGV(TAG, "Write descr success, writing 0x%02X at write_handle=%d", this->write_command_, this->write_handle_);
  if (this->backend_->write_characteristic(this->write_handle_, &this->write_command_, sizeof(this->write_command_),
                                           false) != 0) {
    ESP_LOGW(TAG, "Error writing 0x%02x command", this->write_command_);
    this->abort_connection_();
  }
}

void RadonEyeRD200::on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) {
  ESP_LOGV(TAG, "Received notify value, %d bytes", len);
  this->read_sensors_(data, len);
  // This instance must not stay connected so other clients can connect to it
  // (e.g. the mobile app).
  this->backend_->disconnect();
}

void RadonEyeRD200::abort_connection_() { this->backend_->disconnect(); }

void RadonEyeRD200::read_sensors_(const uint8_t *value, uint16_t value_len) {
  if (value_len < 1) {
    ESP_LOGW(TAG, "Unexpected empty message");
    return;
  }

  uint8_t command = value[0];

  if ((command == WRITE_COMMAND_V1 && value_len < 20) || (command == WRITE_COMMAND_V2 && value_len < 68)) {
    ESP_LOGW(TAG, "Unexpected command 0x%02X message length %d", command, value_len);
    return;
  }

  // Example data V1:
  // 501085EBB9400000000000000000220025000000
  // Example data V2:
  // 4042323230313033525532303338330652443230304e56322e302e3200014a00060a00080000000300010079300000e01108001c00020000003822005c8f423fa4709d3f
  ESP_LOGV(TAG, "radon sensors raw bytes");
  ESP_LOG_BUFFER_HEX_LEVEL(TAG, value, value_len, ESP_LOG_VERBOSE);

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

#endif  // USE_BLE_GATT_CLIENT
