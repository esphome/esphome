#include "radon_eye_rd200.h"
#include "esphome/components/esp32_ble/ble_uuid.h"

#include <cstring>

#ifdef USE_ESP32

namespace esphome {
namespace radon_eye_rd200 {

static const char *const TAG = "radon_eye_rd200";

static const esp32_ble_tracker::ESPBTUUID SERVICE_UUID_V1 =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001523-1212-efde-1523-785feabcd123");
static const esp32_ble_tracker::ESPBTUUID WRITE_CHARACTERISTIC_UUID_V1 =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001524-1212-efde-1523-785feabcd123");
static const esp32_ble_tracker::ESPBTUUID READ_CHARACTERISTIC_UUID_V1 =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001525-1212-efde-1523-785feabcd123");
static const uint8_t WRITE_COMMAND_V1 = 0x50;

static const esp32_ble_tracker::ESPBTUUID SERVICE_UUID_V2 =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001523-0000-1000-8000-00805f9b34fb");
static const esp32_ble_tracker::ESPBTUUID WRITE_CHARACTERISTIC_UUID_V2 =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001524-0000-1000-8000-00805f9b34fb");
static const esp32_ble_tracker::ESPBTUUID READ_CHARACTERISTIC_UUID_V2 =
    esp32_ble_tracker::ESPBTUUID::from_raw("00001525-0000-1000-8000-00805f9b34fb");
static const uint8_t WRITE_COMMAND_V2 = 0x40;

void RadonEyeRD200::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                        esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        ESP_LOGI(TAG, "Connected successfully!");
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "Disconnected!");
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      if (this->parent()->get_service(SERVICE_UUID_V1) != nullptr) {
        service_uuid_ = SERVICE_UUID_V1;
        sensors_write_characteristic_uuid_ = WRITE_CHARACTERISTIC_UUID_V1;
        sensors_read_characteristic_uuid_ = READ_CHARACTERISTIC_UUID_V1;
        write_command_ = WRITE_COMMAND_V1;
      } else if (this->parent()->get_service(SERVICE_UUID_V2) != nullptr) {
        service_uuid_ = SERVICE_UUID_V2;
        sensors_write_characteristic_uuid_ = WRITE_CHARACTERISTIC_UUID_V2;
        sensors_read_characteristic_uuid_ = READ_CHARACTERISTIC_UUID_V2;
        write_command_ = WRITE_COMMAND_V2;
      } else {
        ESP_LOGW(TAG, "No supported device has been found, disconnecting");
        parent()->set_enabled(false);
        break;
      }

      this->read_handle_ = 0;
      auto *chr = this->parent()->get_characteristic(service_uuid_, sensors_read_characteristic_uuid_);
      if (chr == nullptr) {
        char service_buf[esp32_ble::UUID_STR_LEN];
        char char_buf[esp32_ble::UUID_STR_LEN];
        ESP_LOGW(TAG, "No sensor read characteristic found at service %s char %s", service_uuid_.to_str(service_buf),
                 sensors_read_characteristic_uuid_.to_str(char_buf));
        break;
      }
      this->read_handle_ = chr->handle;

      auto *write_chr = this->parent()->get_characteristic(service_uuid_, sensors_write_characteristic_uuid_);
      if (write_chr == nullptr) {
        char service_buf[esp32_ble::UUID_STR_LEN];
        char char_buf[esp32_ble::UUID_STR_LEN];
        ESP_LOGW(TAG, "No sensor write characteristic found at service %s char %s", service_uuid_.to_str(service_buf),
                 sensors_write_characteristic_uuid_.to_str(char_buf));
        break;
      }
      this->write_handle_ = write_chr->handle;

      esp_err_t status =
          esp_ble_gattc_register_for_notify(gattc_if, this->parent()->get_remote_bda(), this->read_handle_);
      if (status) {
        ESP_LOGW(TAG, "Error registering for sensor notify, status=%d", status);
      }
      break;
    }

    case ESP_GATTC_WRITE_DESCR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "write descr failed, error status = %x", param->write.status);
        break;
      }
      ESP_LOGV(TAG, "Write descr success, writing 0x%02X at write_handle=%d", this->write_command_,
               this->write_handle_);
      esp_err_t status =
          esp_ble_gattc_write_char(gattc_if, this->parent()->get_conn_id(), this->write_handle_, sizeof(write_command_),
                                   (uint8_t *) &write_command_, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
      if (status) {
        ESP_LOGW(TAG, "Error writing 0x%02x command, status=%d", write_command_, status);
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.is_notify) {
        ESP_LOGV(TAG, "ESP_GATTC_NOTIFY_EVT, receive notify value, %d bytes", param->notify.value_len);
      } else {
        ESP_LOGV(TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value, %d bytes", param->notify.value_len);
      }
      read_sensors_(param->notify.value, param->notify.value_len);
      break;
    }

    default:
      break;
  }
}

void RadonEyeRD200::read_sensors_(uint8_t *value, uint16_t value_len) {
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

  // This instance must not stay connected
  // so other clients can connect to it (e.g. the
  // mobile app).
  parent()->set_enabled(false);
}

void RadonEyeRD200::update() {
  if (this->node_state != esp32_ble_tracker::ClientState::ESTABLISHED) {
    if (!parent()->enabled) {
      ESP_LOGW(TAG, "Reconnecting to device");
      parent()->set_enabled(true);
    } else {
      ESP_LOGW(TAG, "Connection in progress");
    }
  }
}

void RadonEyeRD200::dump_config() {
  LOG_SENSOR("  ", "Radon", this->radon_sensor_);
  LOG_SENSOR("  ", "Radon Long Term", this->radon_long_term_sensor_);
}

RadonEyeRD200::RadonEyeRD200() : PollingComponent(10000) {}

}  // namespace radon_eye_rd200
}  // namespace esphome

#endif  // USE_ESP32
