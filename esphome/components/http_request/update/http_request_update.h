#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/http_request/http_request.h"
#include "esphome/components/http_request/ota/ota_http_request.h"
#include "esphome/components/update/update_entity.h"

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#endif

namespace esphome {
namespace http_request {

class HttpRequestUpdate final : public update::UpdateEntity, public PollingComponent, public ota::OTAStateListener {
 public:
  void setup() override;
  void update() override;

  void perform(bool force) override;
  void check() override { this->update(); }

  void set_source_url(const std::string &source_url) { this->source_url_ = source_url; }

  void set_request_parent(HttpRequestComponent *request_parent) { this->request_parent_ = request_parent; }
  void set_ota_parent(OtaHttpRequestComponent *ota_parent) { this->ota_parent_ = ota_parent; }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void on_ota_state(ota::OTAState state, float progress, uint8_t error) override;

 protected:
  HttpRequestComponent *request_parent_;
  OtaHttpRequestComponent *ota_parent_;
  std::string source_url_;

  static void update_task(void *params);
#ifdef USE_ESP32
  TaskHandle_t update_task_handle_{nullptr};
#endif
};

}  // namespace http_request
}  // namespace esphome
