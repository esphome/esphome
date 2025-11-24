#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/coap_client/coap_client_component.h"
#include "esphome/components/coap_client/ota/ota_coap_client_component.h"
#include "esphome/components/update/update_entity.h"

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#endif

namespace esphome::coap_client {

class CoapClientUpdate : public update::UpdateEntity, public PollingComponent {
 public:
  void setup() override;
  void update() override;

  void perform(bool force) override;
  void check() override { this->update(); }

  void set_source_url(const std::string &source_url) { this->source_url_ = source_url; }

  void set_request_parent(CoapClientComponent *request_parent) { this->request_parent_ = request_parent; }
  void set_ota_parent(OtaCoapClientComponent *ota_parent) { this->ota_parent_ = ota_parent; }

  void append_update_response(const unsigned char *data, size_t len) {
    this->update_response_.append(reinterpret_cast<const char *>(data), len);
  }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void set_update_response_ready(bool ready) { this->update_response_ready_ = ready; }
  bool is_update_response_ready() { return this->update_response_ready_; }
  static void update_callback(uint16_t response_code, const unsigned char *data, size_t len, size_t offset,
                              size_t total, void *context);

 protected:
  void get_(std::string &url, std::function<void(uint16_t response_code, const unsigned char *data, size_t data_len,
                                                 size_t offset, size_t total, void *context)>
                                  callback);

  CoapClientComponent *request_parent_;
  OtaCoapClientComponent *ota_parent_;
  std::string source_url_;

  static void update_task(void *params);
#ifdef USE_ESP32
  TaskHandle_t update_task_handle_{nullptr};
  bool update_response_ready_{false};
  std::string update_response_{};
#endif
};

}  // namespace esphome::coap_client
