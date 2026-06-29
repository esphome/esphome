#pragma once

#include "binary_storage.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace binary_storage {

//========================================================================
// Actions for Automations
//========================================================================

/**
 * @brief Read data from binary storage
 *
 * Usage in YAML:
 * ```yaml
 * - binary_storage.read:
 *     id: my_storage
 *     address: 0x0000
 *     length: 100
 * ```
 */
template<typename... Ts> class ReadAction : public Action<Ts...> {
 public:
  explicit ReadAction(BinaryStorage *storage) : storage_(storage) {}

  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(uint32_t, length)

  void play(Ts... x) override {
    uint32_t addr = this->address_.value(x...);
    uint32_t len = this->length_.value(x...);

    if (len > 0 && len <= 4096) {  // Safety limit
      std::vector<uint8_t> buffer(len);
      if (this->storage_->read(addr, buffer.data(), len)) {
        // Store result for potential use by other actions
        // Could trigger a read_complete event with the data
        ESP_LOGD("binary_storage", "Read %u bytes from 0x%04X", len, addr);
      } else {
        ESP_LOGE("binary_storage", "Read failed at 0x%04X", addr);
      }
    }
  }

 protected:
  BinaryStorage *storage_;
};

/**
 * @brief Write data to binary storage
 *
 * Usage in YAML:
 * ```yaml
 * - binary_storage.write:
 *     id: my_storage
 *     address: 0x0000
 *     data: [0x01, 0x02, 0x03]
 * ```
 */
template<typename... Ts> class WriteAction : public Action<Ts...> {
 public:
  explicit WriteAction(BinaryStorage *storage) : storage_(storage) {}

  TEMPLATABLE_VALUE(uint32_t, address)

  void set_data(const std::vector<uint8_t> &data) { this->data_ = data; }
  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) { this->data_func_ = func; }

  void play(Ts... x) override {
    uint32_t addr = this->address_.value(x...);

    std::vector<uint8_t> data;
    if (this->data_func_.has_value()) {
      data = this->data_func_.value()(x...);
    } else {
      data = this->data_;
    }

    if (!data.empty()) {
      if (this->storage_->write(addr, data.data(), data.size())) {
        ESP_LOGD("binary_storage", "Wrote %u bytes to 0x%04X", data.size(), addr);
      } else {
        ESP_LOGE("binary_storage", "Write failed at 0x%04X", addr);
      }
    }
  }

 protected:
  BinaryStorage *storage_;
  std::vector<uint8_t> data_;
  optional<std::function<std::vector<uint8_t>(Ts...)>> data_func_;
};

/**
 * @brief Fill storage with a value
 *
 * Usage in YAML:
 * ```yaml
 * - binary_storage.fill:
 *     id: my_storage
 *     value: 0xFF  # or 0x00 for clear
 * ```
 */
template<typename... Ts> class FillAction : public Action<Ts...> {
 public:
  explicit FillAction(BinaryStorage *storage) : storage_(storage) {}

  TEMPLATABLE_VALUE(uint8_t, value)

  void play(Ts... x) override {
    uint8_t val = this->value_.value(x...);
    uint32_t bytes_written = this->storage_->fill(val);
    ESP_LOGI("binary_storage", "Filled %u bytes with 0x%02X", bytes_written, val);
  }

 protected:
  BinaryStorage *storage_;
};

/**
 * @brief Write a single byte value
 *
 * Usage in YAML:
 * ```yaml
 * - binary_storage.write_byte:
 *     id: my_storage
 *     address: 0x0000
 *     value: 42
 * ```
 */
template<typename... Ts> class WriteByteAction : public Action<Ts...> {
 public:
  explicit WriteByteAction(BinaryStorage *storage) : storage_(storage) {}

  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(uint8_t, value)

  void play(Ts... x) override {
    uint32_t addr = this->address_.value(x...);
    uint8_t val = this->value_.value(x...);

    if (this->storage_->write(addr, &val, 1)) {
      ESP_LOGD("binary_storage", "Wrote byte 0x%02X to 0x%04X", val, addr);
    } else {
      ESP_LOGE("binary_storage", "Write byte failed at 0x%04X", addr);
    }
  }

 protected:
  BinaryStorage *storage_;
};

/**
 * @brief Write a string
 *
 * Usage in YAML:
 * ```yaml
 * - binary_storage.write_string:
 *     id: my_storage
 *     address: 0x0000
 *     value: "Hello FRAM!"
 * ```
 */
template<typename... Ts> class WriteStringAction : public Action<Ts...> {
 public:
  explicit WriteStringAction(BinaryStorage *storage) : storage_(storage) {}

  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(std::string, value)

  void play(Ts... x) override {
    uint32_t addr = this->address_.value(x...);
    std::string str = this->value_.value(x...);

    if (this->storage_->write(addr, reinterpret_cast<const uint8_t *>(str.c_str()), str.length() + 1)) {
      ESP_LOGD("binary_storage", "Wrote string '%s' to 0x%04X", str.c_str(), addr);
    } else {
      ESP_LOGE("binary_storage", "Write string failed at 0x%04X", addr);
    }
  }

 protected:
  BinaryStorage *storage_;
};

//========================================================================
// Conditions for Automations
//========================================================================

/**
 * @brief Check if storage is ready
 *
 * Usage in YAML:
 * ```yaml
 * if:
 *   condition:
 *     binary_storage.is_ready:
 *       id: my_storage
 * ```
 */
template<typename... Ts> class IsReadyCondition : public Condition<Ts...> {
 public:
  explicit IsReadyCondition(BinaryStorage *storage) : storage_(storage) {}

  bool check(Ts... x) override { return this->storage_->is_ready(); }

 protected:
  BinaryStorage *storage_;
};

}  // namespace binary_storage
}  // namespace esphome
