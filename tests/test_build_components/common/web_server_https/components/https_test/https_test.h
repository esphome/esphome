#pragma once

#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

namespace esphome::https_test {

// Self-signed test certificate and key, for this build test only.
static const char *const CERTIFICATE = R"(-----BEGIN CERTIFICATE-----
MIIBkDCCATegAwIBAgIUMMwcTkVDiMSsim4eX/Cdul/ZnsIwCgYIKoZIzj0EAwIw
HTEbMBkGA1UEAwwSZXNwaG9tZS10ZXN0LmxvY2FsMCAXDTI2MTEwMjIzMTYzNFoY
DzIxMjYxMDA5MjMxNjM0WjAdMRswGQYDVQQDDBJlc3Bob21lLXRlc3QubG9jYWww
WTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAASUqX7QDmtpdjZAP4DgMjaN4JIO7mYj
6FGx7tz5MZCKrz/lkTJY9wMYFfQ0+fp6X69lP76nEkbBmgmc2isz4Uo2o1MwUTAd
BgNVHQ4EFgQUzEmb4w1pKH4RGSm+lUH3dJgCHm4wHwYDVR0jBBgwFoAUzEmb4w1p
KH4RGSm+lUH3dJgCHm4wDwYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNHADBE
AiBn6XgNZA8M9FCLsheo80/VePtC6mci/MmzKrsUVHddmwIgUgouxiAVVswudya9
kedDMCS2/klleJS8/Imr9J05eJ4=
-----END CERTIFICATE-----
)";
static const char *const PRIVATE_KEY = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgOWIIXfeG3RJdg0CW
IRGNs0g7EbJUIzpQtCIUZbJ+jXGhRANCAASUqX7QDmtpdjZAP4DgMjaN4JIO7mYj
6FGx7tz5MZCKrz/lkTJY9wMYFfQ0+fp6X69lP76nEkbBmgmc2isz4Uo2
-----END PRIVATE KEY-----
)";

// Starts the shared web server with HTTPS on 443, the way a component that supplies its own certificate would.
class HttpsTest : public Component {
 public:
  explicit HttpsTest(web_server_base::WebServerBase *base) : base_(base) {}

  void setup() override {
    this->base_->set_port(443);
    this->base_->set_ssl_credentials(CERTIFICATE, PRIVATE_KEY);
    this->base_->init();
  }
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

 protected:
  web_server_base::WebServerBase *base_;
};

}  // namespace esphome::https_test
