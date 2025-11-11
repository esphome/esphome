#pragma once
#include "esphome/core/defines.h"
#ifdef USE_COAP_CLIENT
#include <netdb.h>
#include <string>
#include "coap3/coap.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "esphome/core/component.h"

namespace esphome::coap_client_component {

struct CoapClientRequestData {
  size_t max_block_size{1024};
  std::function<void(const unsigned char *data, size_t data_len, size_t offset, size_t total, void *context)> callback;
  void *callback_context;
  std::string uri{""};
};

class CoapClientComponent : public Component {
 public:
  CoapClientComponent();
  void setup() override;
  bool teardown() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void get(std::string uri,
           std::function<void(const unsigned char *data, size_t data_len, size_t offset, size_t total, void *context)>
               callback,
           void *callback_context, size_t max_block_size = 1024);
  static coap_response_t response_handler(coap_session_t *session, const coap_pdu_t *sent, const coap_pdu_t *received,
                                          const coap_mid_t mid);

  coap_response_t process_response(coap_session_t *session, const coap_pdu_t *sent, const coap_pdu_t *received,
                                   const coap_mid_t mid);

 protected:
  void main_();
  bool main_looping_{true};
  bool toredown_{false};
  QueueHandle_t request_queue_{nullptr};
  std::function<void(const unsigned char *data, size_t len, size_t offset, size_t total, void *context)>
      response_callback_{nullptr};
  void *response_callback_context_{nullptr};
  const uint8_t uri_path_buffer_size_{40};
  const uint32_t request_timeout_{60000};
  bool response_wait_{false};
};

extern CoapClientComponent *global_coap_client;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::coap_client_component
#endif
