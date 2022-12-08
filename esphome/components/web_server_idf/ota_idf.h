#pragma once
#ifdef USE_ESP_IDF

#include <esp_ota_ops.h>

namespace esphome {
namespace web_server_idf {

class OTA {
 public:
  bool begin(int image_size);
  bool is_running() const { return this->handle_; }
  void abort();
  // NOLINTNEXTLINE(readability-identifier-naming)
  bool hasError() const { return this->error_ != ESP_OK; }
  bool write(const uint8_t *data, int size);
  bool end(bool unused);

  esp_err_t get_error() const { return this->error_; }

 protected:
  const esp_partition_t *partition_{};
  esp_ota_handle_t handle_{};
  esp_err_t error_{ESP_OK};
};

}  // namespace web_server_idf
}  // namespace esphome

extern esphome::web_server_idf::OTA Update;  // NOLINT

#endif  // !defined(USE_ESP_IDF)
