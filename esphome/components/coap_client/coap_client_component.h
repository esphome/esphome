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

enum CoapMethod : uint8_t {
  EMPTY = COAP_EMPTY_CODE,
  GET = COAP_REQUEST_CODE_GET,
  POST = COAP_REQUEST_CODE_POST,
  PUT = COAP_REQUEST_CODE_PUT,
  DELETE = COAP_REQUEST_CODE_DELETE,
  FETCH = COAP_REQUEST_CODE_FETCH,
  PATCH = COAP_REQUEST_CODE_PATCH,
  IPATCH = COAP_REQUEST_CODE_IPATCH,
};

enum CoapMediaType : uint8_t {
  TEXT_PLAIN = COAP_MEDIATYPE_TEXT_PLAIN,
  APPLICATION_LINK_FORMAT = COAP_MEDIATYPE_APPLICATION_LINK_FORMAT,
  APPLICATION_XML = COAP_MEDIATYPE_APPLICATION_XML,
  APPLICATION_OCTET_STREAM = COAP_MEDIATYPE_APPLICATION_OCTET_STREAM,
  APPLICATION_RDF_XML = COAP_MEDIATYPE_APPLICATION_RDF_XML,
  APPLICATION_EXI = COAP_MEDIATYPE_APPLICATION_EXI,
  APPLICATION_CBOR = COAP_MEDIATYPE_APPLICATION_CBOR,
  APPLICATION_CWT = COAP_MEDIATYPE_APPLICATION_CWT,
};

struct CoapClientRequestData {
  CoapMethod method = CoapMethod::EMPTY;
  std::string uri{};
  size_t max_block_size{1024};
  std::function<void(const unsigned char *data, size_t data_len, size_t offset, size_t total, void *context)> callback;
  void *callback_context;
  CoapMediaType media_type = CoapMediaType::TEXT_PLAIN;
  std::string payload{};
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
  void process_request(CoapClientRequestData *request);
  static coap_response_t response_handler(coap_session_t *session, const coap_pdu_t *sent, const coap_pdu_t *received,
                                          const coap_mid_t mid);
  coap_response_t process_response(coap_session_t *session, const coap_pdu_t *sent, const coap_pdu_t *received,
                                   const coap_mid_t mid);
#ifdef CONFIG_COAP_MBEDTLS_PKI
  static int verify_cn_callback(const char *cn, const uint8_t *asn1_public_cert, size_t asn1_length,
                                coap_session_t *session, unsigned depth, int validated, void *arg);
#endif

 protected:
  void main_();

#ifdef CONFIG_COAP_MBEDTLS_PSK
  coap_session_t *coap_start_psk_session_(coap_context_t *ctx, coap_address_t *dst_addr, coap_uri_t *uri,
                                          coap_proto_t proto);
#endif
#ifdef CONFIG_COAP_MBEDTLS_PKI
  coap_session_t *coap_start_pki_session_(coap_context_t *ctx, coap_address_t *dst_addr, coap_uri_t *uri,
                                          coap_proto_t proto);
#endif
  coap_session_t *coap_start_anon_pki_session_(coap_context_t *ctx, coap_address_t *dst_addr, coap_uri_t *uri,
                                               coap_proto_t proto);

  bool main_looping_{true};
  bool toredown_{false};
  QueueHandle_t request_queue_{nullptr};
  TaskHandle_t main_task_handle_{nullptr};
  std::function<void(const unsigned char *data, size_t len, size_t offset, size_t total, void *context)>
      response_callback_{nullptr};
  void *response_callback_context_{nullptr};
  const uint8_t uri_path_buffer_size_{40};
  const uint32_t request_timeout_{60000};
#ifdef CONFIG_COAP_OSCORE_SUPPORT
  std::string oscore_conf_str_{};
#endif
#ifdef CONFIG_COAP_MBEDTLS_PSK
  std::string psk_identity_{};
  std::string psk_key_{};
#endif
#ifdef CONFIG_COAP_MBEDTLS_PKI
  std::string ca_pem_str_{};
  std::string client_crt_str_{};
  std::string client_key_str_{};
#endif
};

extern CoapClientComponent *global_coap_client;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::coap_client_component
#endif
