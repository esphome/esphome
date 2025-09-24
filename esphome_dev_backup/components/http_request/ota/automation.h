#pragma once
#include "ota_http_request.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace http_request {

template<typename... Ts> class OtaHttpRequestComponentFlashAction : public Action<Ts...> {
 public:
  OtaHttpRequestComponentFlashAction(OtaHttpRequestComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, md5_url)
  TEMPLATABLE_VALUE(std::string, md5)
  TEMPLATABLE_VALUE(std::string, password)
  TEMPLATABLE_VALUE(std::string, url)
  TEMPLATABLE_VALUE(std::string, username)

  void play(Ts... x) override {
    if (this->md5_url_.has_value()) {
      this->parent_->set_md5_url(this->md5_url_.value(x...));
    }
    if (this->md5_.has_value()) {
      this->parent_->set_md5(this->md5_.value(x...));
    }
    if (this->password_.has_value()) {
      this->parent_->set_password(this->password_.value(x...));
    }
    if (this->username_.has_value()) {
      this->parent_->set_username(this->username_.value(x...));
    }
    this->parent_->set_url(this->url_.value(x...));

    this->parent_->flash();
    // Normally never reached due to reboot
  }

 protected:
  OtaHttpRequestComponent *parent_;
};

}  // namespace http_request
}  // namespace esphome
