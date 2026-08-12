// The ble_client.ble_write action: a node on the platform-neutral interface,
// so one implementation serves both engines (the esp32 bridge and the
// neutral engine).

#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_CLIENT_GATT_NODES

#include <tuple>
#include <vector>

// One of the two engine headers resolves per build.
#include "ble_client.h"
#include "ble_client_gatt.h"
#include "ble_client_node.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ble_client {

static const char *const BLE_WRITE_TAG = "ble_client.automation";

// Maximum bytes to log in hex format for BLE writes (many logging buffers are 256 chars)
static constexpr size_t BLE_WRITE_MAX_LOG_BYTES = 64;

template<typename... Ts> class BLEClientWriteAction final : public Action<Ts...>, public BLEClientNode {
 public:
  BLEClientWriteAction(BLEClient *ble_client) {
    ble_client->register_gatt_node(this);
    ble_client_ = ble_client;
  }

  void set_service_uuid16(uint16_t uuid) { this->service_uuid_ = ble_device_base::ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { this->service_uuid_ = ble_device_base::ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = ble_device_base::ESPBTUUID::from_raw(uuid); }

  void set_char_uuid16(uint16_t uuid) { this->char_uuid_ = ble_device_base::ESPBTUUID::from_uint16(uuid); }
  void set_char_uuid32(uint32_t uuid) { this->char_uuid_ = ble_device_base::ESPBTUUID::from_uint32(uuid); }
  void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = ble_device_base::ESPBTUUID::from_raw(uuid); }

  void set_value_template(std::vector<uint8_t> (*func)(Ts...)) {
    this->value_.func = func;
    this->len_ = -1;  // Sentinel value indicates template mode
  }

  // Store pointer to static data in flash (no RAM copy)
  void set_value_simple(const uint8_t *data, size_t len) {
    this->value_.data = data;
    this->len_ = len;  // Length >= 0 indicates static mode
  }

  void play(const Ts &...x) override {}

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    this->var_ = std::make_tuple(x...);

    bool result;
    if (this->len_ >= 0) {
      result = this->write(this->value_.data, this->len_);
    } else {
      std::vector<uint8_t> value = this->value_.func(x...);
      result = this->write(value.data(), value.size());
    }

    // on write failure, continue the automation chain rather than stopping so
    // that e.g. disconnect can work.
    if (!result)
      this->play_next_(x...);
  }

  // Initiate the write; the completion arrives in on_write_result. The
  // response-less path can complete synchronously inside the call, so the
  // handle is armed before the backend is touched.
  bool write(const uint8_t *data, size_t len) {
    if (!this->ble_client_->connected()) {
      esph_log_w(BLE_WRITE_TAG, "Cannot write to BLE characteristic - not connected");
      return false;
    }
    if (!this->resolved_) {
      esph_log_w(BLE_WRITE_TAG, "Cannot write to BLE characteristic - characteristic was not resolved");
      return false;
    }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
    char hex_buf[format_hex_pretty_size(BLE_WRITE_MAX_LOG_BYTES)];
    esph_log_vv(BLE_WRITE_TAG, "Will write %d bytes: %s", len, format_hex_pretty_to(hex_buf, data, len));
#endif
    int err = this->ble_client_->write_characteristic(this->char_handle_, data, len, this->write_response_);
    if (err != 0) {
      esph_log_e(BLE_WRITE_TAG, "Error writing to characteristic: %d!", err);
      return false;
    }
    return true;
  }

  void on_connected(const ble_device_base::GattServiceTable &table) override {
    const auto *service = ble_device_base::find_service(table, this->service_uuid_);
    const auto *chr =
        service == nullptr ? nullptr : ble_device_base::find_characteristic(table, *service, this->char_uuid_);
    if (chr == nullptr) {
      char char_buf[ble_device_base::UUID_STR_LEN];
      char service_buf[ble_device_base::UUID_STR_LEN];
      esph_log_w(BLE_WRITE_TAG, "Characteristic %s was not found in service %s", this->char_uuid_.to_str(char_buf),
                 this->service_uuid_.to_str(service_buf));
      return;
    }
    if (chr->properties & ble_device_base::GATT_CHAR_PROP_WRITE) {
      this->write_response_ = true;
    } else if (chr->properties & ble_device_base::GATT_CHAR_PROP_WRITE_NO_RSP) {
      this->write_response_ = false;
    } else {
      char char_buf[ble_device_base::UUID_STR_LEN];
      esph_log_e(BLE_WRITE_TAG, "Characteristic %s does not allow writing", this->char_uuid_.to_str(char_buf));
      return;
    }
    this->char_handle_ = chr->value_handle;
    this->resolved_ = true;
    char char_buf[ble_device_base::UUID_STR_LEN];
    esph_log_d(BLE_WRITE_TAG, "Found characteristic %s on device %s", this->char_uuid_.to_str(char_buf),
               this->ble_client_->address_str());
  }

  void on_disconnected() override {
    this->resolved_ = false;
    this->char_handle_ = 0;
    if (this->num_running_ != 0)
      this->stop_complex();
  }

  void on_write_result(uint16_t handle, int error) override {
    if (this->num_running_ == 0) {
      return;
    }
    if (!this->resolved_ || handle != this->char_handle_) {
      // A parked chain waiting on a completion that never matches would
      // otherwise stall silently until disconnect.
      esph_log_d(BLE_WRITE_TAG, "Write result for handle 0x%04x ignored, waiting on 0x%04x", handle,
                 this->char_handle_);
      return;
    }
    if (error != 0) {
      // Continue the chain (legacy parity) but leave a breadcrumb.
      esph_log_w(BLE_WRITE_TAG, "Write completed with status %d", error);
    }
    this->ble_client_->run_later([this]() { this->play_next_tuple_(this->var_); });
  }

 private:
  BLEClient *ble_client_;
  ssize_t len_{-1};  // -1 = template mode, >=0 = static mode with length
  union Value {
    std::vector<uint8_t> (*func)(Ts...);  // Function pointer (stateless lambdas)
    const uint8_t *data;                  // Pointer to static data in flash
  } value_;
  ble_device_base::ESPBTUUID service_uuid_;
  ble_device_base::ESPBTUUID char_uuid_;
  std::tuple<Ts...> var_{};
  uint16_t char_handle_{};
  bool write_response_{false};
  bool resolved_{false};
};

}  // namespace esphome::ble_client

#endif  // USE_BLE_CLIENT_GATT_NODES
