#include "tcp_client_link_test_component.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::tcp_client_link_test_component {

static const char *const TAG = "tcp_link_test";

void TcpClientLinkTestComponent::setup() { this->link_.begin(TAG); }

void TcpClientLinkTestComponent::loop() {
  this->link_.poll();
  bool up = this->link_.connected();
  if (up != this->was_up_) {
    this->was_up_ = up;
    ESP_LOGI(TAG, "Link %s", up ? LOG_STR_LITERAL("up") : LOG_STR_LITERAL("down"));
  }
  if (!up) {
    return;
  }
  // Retries a partial echo; inline no-op when nothing is queued.
  this->link_.flush_tx();
  if (!this->link_.ready()) {
    return;
  }
  uint8_t buf[64];
  // Read only what the outgoing buffer can take, so the echo never drops bytes.
  size_t want = std::min(sizeof(buf), this->link_.tx_free());
  if (want == 0) {
    return;
  }
  ssize_t count = this->link_.read(buf, want);
  if (count > 0) {
    ESP_LOGI(TAG, "Echoing %d bytes", static_cast<int>(count));
    this->link_.queue(buf, static_cast<size_t>(count));
    this->link_.flush_tx();
  }
}

}  // namespace esphome::tcp_client_link_test_component
