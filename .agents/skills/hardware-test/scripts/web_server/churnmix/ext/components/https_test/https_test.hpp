#pragma once

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

// CERT and KEY as PEM strings: run https_idf/make_certs.sh and copy its ext/httpstest/certs.h next to this file.
#include "certs.h"

namespace esphome::https_test {

// Starts the shared web server with HTTPS on 443, the way a component that supplies its own certificate would.
class HttpsTest : public Component {
 public:
  // Set before web_server's own setup() runs init(), so the shared server starts as HTTPS.
  explicit HttpsTest(web_server_base::WebServerBase *base) : base_(base) {
    this->base_->set_port(443);
    this->base_->set_ssl_credentials(CERT, KEY);
  }

  void setup() override { this->base_->init(); }
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

 protected:
  web_server_base::WebServerBase *base_;
};

}  // namespace esphome::https_test
