#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/http_request/http_request.h"
#include "esphome/components/http_request/ota/ota_http_request.h"
#include "esphome/components/update/update_entity.h"

namespace esphome {
namespace http_request {

class HttpRequestUpdate : public update::UpdateEntity, public PollingComponent {
 public:
  void setup() override;
  void update() override;

  void perform() override;

  void set_source_url(const std::string &source_url) { this->source_url_ = source_url; }

  void set_request_parent(HttpRequestComponent *request_parent) { this->request_parent_ = request_parent; }
  void set_ota_parent(OtaHttpRequestComponent *ota_parent) { this->ota_parent_ = ota_parent; }

  void set_current_version(const std::string &current_version) { this->current_version_ = current_version; }

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  HttpRequestComponent *request_parent_;
  OtaHttpRequestComponent *ota_parent_;
  std::string source_url_;
  std::string current_version_{""};
};

}  // namespace http_request
}  // namespace esphome
