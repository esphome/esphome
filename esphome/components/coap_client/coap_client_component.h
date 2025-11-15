#pragma once
#include "esphome/core/defines.h"
#ifdef USE_COAP_CLIENT
#include <netdb.h>
#include <string>
#include "coap3/coap.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "esphome/core/component.h"

namespace esphome::coap {

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
const char *coap_method_to_string(CoapMethod method);

enum CoapMediaType : uint8_t {
  TEXT_PLAIN = COAP_MEDIATYPE_TEXT_PLAIN,
  APPLICATION_JSON = COAP_MEDIATYPE_APPLICATION_JSON,
  APPLICATION_LINK_FORMAT = COAP_MEDIATYPE_APPLICATION_LINK_FORMAT,
  APPLICATION_XML = COAP_MEDIATYPE_APPLICATION_XML,
  APPLICATION_OCTET_STREAM = COAP_MEDIATYPE_APPLICATION_OCTET_STREAM,
  APPLICATION_RDF_XML = COAP_MEDIATYPE_APPLICATION_RDF_XML,
  APPLICATION_EXI = COAP_MEDIATYPE_APPLICATION_EXI,
  APPLICATION_CBOR = COAP_MEDIATYPE_APPLICATION_CBOR,
  APPLICATION_CWT = COAP_MEDIATYPE_APPLICATION_CWT,
};
const char *coap_media_type_to_string(CoapMediaType media_type);

struct CoapClientRequestData {
  CoapMethod method = CoapMethod::EMPTY;
  std::string uri{};
  size_t max_block_size{0};
  std::function<void(uint16_t response_code, const unsigned char *data, size_t data_len, size_t offset, size_t total,
                     void *context)>
      callback;
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
           std::function<void(uint16_t response_code, const unsigned char *data, size_t data_len, size_t offset,
                              size_t total, void *context)>
               callback,
           void *callback_context, size_t max_block_size = 0);

  void post(std::string uri,
            std::function<void(uint16_t response_code, const unsigned char *data, size_t data_len, size_t offset,
                               size_t total, void *context)>
                callback,
            void *callback_context, std::string payload, CoapMediaType media_type = CoapMediaType::TEXT_PLAIN,
            size_t max_block_size = 0);

  void process_request(CoapClientRequestData *request);
  static coap_response_t response_handler(coap_session_t *session, const coap_pdu_t *sent, const coap_pdu_t *received,
                                          const coap_mid_t mid);

  coap_response_t process_response(coap_session_t *session, const coap_pdu_t *sent, const coap_pdu_t *received,
                                   const coap_mid_t mid);

#ifdef CONFIG_COAP_MBEDTLS_PKI
  static int verify_cn_callback(const char *cn, const uint8_t *asn1_public_cert, size_t asn1_length,
                                coap_session_t *session, unsigned depth, int validated, void *arg);
#endif
  void set_max_block_size(size_t block_size) {
    if (block_size > 16) {
      this->max_block_size_ = block_size;
    }
  }
  void set_request_timeout(uint32_t timeout) {
    if (timeout > COAP_RESOURCE_CHECK_TIME * 1000) {
      this->request_timeout_ = timeout;
    }
  }
  void set_ack_timeout(uint32_t timeout) {
    if (timeout > 2000) {
      this->ack_timeout_ = timeout;
    }
  }
  void set_max_retransmit(uint8_t max) {
    if (max >= 0) {
      this->max_retransmit_ = max;
    }
  }
#ifdef CONFIG_COAP_OSCORE_SUPPORT
  void set_oscore_conf(std::string str) { this->oscore_conf_str_ = str; }
#endif
#ifdef CONFIG_COAP_MBEDTLS_PSK
  void set_psk_identity(std::string str) { this->psk_identity_ = str; }
  void set_psk_key(std::string str) { this->psk_key_ = str; }
#endif
#ifdef CONFIG_COAP_MBEDTLS_PKI
  void set_ca_pem(std::string str) { this->ca_pem_str_ = str; }
  void set_client_crt(std::string str) { this->client_crt_str_ = str; }
  void set_client_key(std::string str) { this->client_key_str_ = str; }
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
  bool torndown_{false};
  QueueHandle_t request_queue_{nullptr};
  TaskHandle_t main_task_handle_{nullptr};
  std::function<void(uint16_t response_code, const unsigned char *data, size_t len, size_t offset, size_t total,
                     void *context)>
      response_callback_{nullptr};
  void *response_callback_context_{nullptr};
  size_t max_block_size_{512};
  uint8_t uri_path_buffer_size_{100};
  uint32_t request_timeout_{COAP_RESOURCE_CHECK_TIME * 1000};
  uint32_t ack_timeout_{2000};
  uint8_t max_retransmit_{4};
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

}  // namespace esphome::coap
#endif
