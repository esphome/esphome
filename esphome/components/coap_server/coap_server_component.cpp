/* Component Built from CoAP server Example

   The source example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#ifdef USE_COAP_SERVER
#include "esphome/core/log.h"
#include "coap_server_component.h"

namespace esphome::coap {

static const char *TAG = "coap_server";

// CoapServerComponent
CoapServerComponent *global_coap_server = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

CoapServerComponent::CoapServerComponent() { global_coap_server = this; }

void CoapServerComponent::setup() {
  // Initialize CoAP task
  coap_startup();
// Set up the CoAP logging
#if CONFIG_LOG_DYNAMIC_LEVEL_CONTROL
  ESP_LOGI(TAG, "Set CoAP Log Level to %d", CONFIG_LOG_DEFAULT_LEVEL);
  coap_set_log_level((coap_log_t) CONFIG_LOG_DEFAULT_LEVEL);
#endif

  xTaskCreate(
      [](void *arg) {
        CoapServerComponent *obj = (CoapServerComponent *) arg;
        obj->main_();
        coap_cleanup();
        vTaskDelete(nullptr);
        obj->torndown_ = true;
      },
      "CoapServerMain", 8 * 1024, this, 5, nullptr);
}

bool CoapServerComponent::teardown() {
  this->main_looping_ = false;
  xTaskAbortDelay(this->main_task_handle_);
  return this->torndown_;
}

void CoapServerComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CoAP Server:");
#ifdef CONFIG_COAP_OSCORE_SUPPORT
  ESP_LOGCONFIG(TAG, "  OSCore Conf Provided");
#endif
#ifdef CONFIG_COAP_MBEDTLS_PSK
  ESP_LOGCONFIG(TAG, "  MBDEDTLS PSK Provided");
#endif
#ifdef CONFIG_COAP_MBEDTLS_PKI
  ESP_LOGCONFIG(TAG, "  MBDEDTLS PKI Provided");
#endif
}

#ifdef CONFIG_COAP_MBEDTLS_PKI
int CoapServerComponent::verify_cn_callback(const char *cn, const uint8_t *asn1_public_cert, size_t asn1_length,
                                            coap_session_t *session, unsigned depth, int validated, void *arg) {
  ESP_LOGD(TAG, "CN '%s' presented by server (%s)", cn, depth ? "CA" : "Certificate");
  return 1;
}
#endif

void CoapServerComponent::main_() {
  this->main_task_handle_ = xTaskGetCurrentTaskHandle();
  coap_context_t *context = nullptr;
  coap_resource_t *resource = nullptr;
  coap_addr_info_t *info = nullptr;
  coap_addr_info_t *info_list = nullptr;
  int have_end_points = 0;
  uint32_t scheme_hint_bits;
#ifdef CONFIG_COAP_OSCORE_SUPPORT
  coap_oscore_conf_t *oscore_conf = nullptr;
  coap_str_const_t osc_conf = {
      .s = this->oscore_conf_str_.c_str(),
      .length = this->oscore_conf_str_.length(),
  } oscore_conf = coap_new_oscore_conf(osc_conf, NULL, NULL, 0);
#endif

  auto cleanup = [](void *self, coap_context_t *&context) {
    if (context) {
      coap_free_context(context);
    }
    CoapServerComponent *obj = (CoapServerComponent *) self;
    if (!obj->server_failure_cntdown_ > 0) {
      ESP_LOGE(TAG, "Coap Server Failure, stopping");
    }
  };

  // ESP_LOGV(TAG, "Begin coap main loop");
  for (; this->main_looping_ && this->server_failure_cntdown_ > 0; cleanup(this, context)) {
    unsigned wait_ms;
    context = coap_new_context(NULL);
    if (!context) {
      ESP_LOGE(TAG, "coap_new_context() failed");
      server_failure_cntdown_--;
      continue;
    }
    coap_context_set_block_mode(context, COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);
    coap_context_set_max_idle_sessions(context, this->max_idle_sessions_);
    coap_context_set_keepalive(context, this->keep_alive_);

#ifdef CONFIG_COAP_MBEDTLS_PSK
    coap_context_set_psk(context, "CoAP", (const uint8_t *) this->psk_key_.c_str(), this->psk_key_.length());
#endif
#ifdef CONFIG_COAP_MBEDTLS_PKI
    coap_dtls_pki_t dtls_pki;
    this->provision_pki_(&dtls_pki);
    coap_context_set_pki(context, &dtls_pki);
#endif
#ifdef CONFIG_COAP_OSCORE_SUPPORT
    coap_context_oscore_server(context, oscore_conf);
#endif

    // set up the CoAP server socket(s)
    scheme_hint_bits = coap_get_available_scheme_hint_bits(
#if defined(CONFIG_COAP_MBEDTLS_PSK) || defined(CONFIG_COAP_MBEDTLS_PKI)
        1,
#else
        0,
#endif
#ifdef CONFIG_COAP_WEBSOCKETS
        1,
#else
        0,
#endif
        COAP_PROTO_NONE);

#if LWIP_IPV6
    // if os can, it will also accept ipv4 to this bind
    std::string host = "::";
#else
    std::string host = "" 0.0.0.0 "";
#endif
    info_list = coap_resolve_address_info(coap_make_str_const(host.c_str()), this->listen_port_,
                                          this->secure_listen_port_, this->websocket_port_,
                                          this->secure_websocket_port_, 0, scheme_hint_bits, COAP_RESOLVE_TYPE_LOCAL);
    if (info_list == NULL) {
      ESP_LOGE(TAG, "coap_resolve_address_info() failed");
      server_failure_cntdown_--;
      continue;
    }
    for (info = info_list; info != NULL; info = info->next) {
      coap_endpoint_t *end_point;
      end_point = coap_new_endpoint(context, &info->addr, info->proto);
      if (!end_point) {
        ESP_LOGW(TAG, "cannot create endpoint for proto %u", info->proto);
      } else {
        have_end_points = 1;
      }
    }
    coap_free_address_info(info_list);
    if (!have_end_points) {
      ESP_LOGE(TAG, "No endpoints available");
      server_failure_cntdown_--;
      continue;
    }

#ifdef CONFIG_COAP_OSCORE_SUPPORT
    int flags = COAP_RESOURCE_FLAGS_OSCORE_ONLY
#else
    int flags = 0;
#endif
        // TBD - how to drive from cvs?
        resource = coap_resource_init(coap_make_str_const("test"), flags);
    if (!resource) {
      ESP_LOGE(TAG, "coap_resource_init() failed");
      server_failure_cntdown_--;
      continue;
    }
    coap_register_handler(resource, COAP_REQUEST_GET, CoapServerComponent::esphome_get_handler);
    coap_add_resource(context, resource);
    // END TBD

    if (mcast_ip_mode_v4_ || mcast_ip_mode_v6_) {
      /*
      esp_netif_t *netif = NULL;
      for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) {
        char buf[8];
        netif = esp_netif_next(netif);
        esp_netif_get_netif_impl_name(netif, buf);
        */
      if (mcast_ip_mode_v4_) {
        coap_join_mcast_group_intf(context, this->mcast_ip_mode_v4_addr_.c_str(), NULL);
      }
      if (mcast_ip_mode_v6_) {
        // When adding IPV6 esp-idf requires ifname param to be filled in
        coap_join_mcast_group_intf(context, this->mcast_ip_mode_v6_addr_.c_str(), NULL);
      }
      /*}*/
    }

    // Process incoming requests
    int result = 0;
    while (1) {
      // Wait for up to request_timeout_ milliseconds for I/O to happen.
      // The return value is the time spent in the function in milliseconds.
      result = coap_io_process(context, this->request_timeout_);
      if (result < 0) {
        ESP_LOGE(TAG, "CoAP I/O error! Exiting loop.");
        break;
      }
    }
    ESP_LOGV(TAG, "CoAP Server Resetting");
  }
}

#ifdef CONFIG_COAP_MBEDTLS_PKI
void CoapServerComponent::provision_pki_(coap_dtls_pki_t *dtls_pki) {
  memset(dtls_pki, 0, sizeof(coap_dtls_pki_t));
  dtls_pki->version = COAP_DTLS_PKI_SETUP_VERSION;
  dtls_pki->verify_peer_cert = 1;
  dtls_pki->check_common_ca = 1;
  dtls_pki->allow_self_signed = 1;
  dtls_pki->allow_expired_certs = 1;
  dtls_pki->cert_chain_validation = 1;
  dtls_pki->cert_chain_verify_depth = 2;
  dtls_pki->check_cert_revocation = 1;
  dtls_pki->allow_no_crl = 1;
  dtls_pki->allow_expired_crl = 1;
  dtls_pki->allow_bad_md_hash = 1;
  dtls_pki->allow_short_rsa_length = 1;
  dtls_pki->validate_cn_call_back = this->validate_cn_callback;
  dtls_pki->cn_call_back_arg = NULL;
  dtls_pki->validate_sni_call_back = NULL;
  dtls_pki->sni_call_back_arg = NULL;
  dtls_pki->pki_key.key_type = COAP_PKI_KEY_PEM_BUF;
  dtls_pki->pki_key.key.pem_buf.public_cert = this->client_crt_str_.c_str();
  dtls_pki->pki_key.key.pem_buf.public_cert_len = this->client_crt_str_.length();
  dtls_pki->pki_key.key.pem_buf.private_key = this->client_key_str_.c_str();
  dtls_pki->pki_key.key.pem_buf.private_key_len = this->client_key_str_.length();
  dtls_pki->pki_key.key.pem_buf.ca_cert = this->ca_pem_str_.c_str();
  dtls_pki->pki_key.key.pem_buf.ca_cert_len = this->ca_pem_str_.length();
}
#endif

void CoapServerComponent::esphome_get_handler(coap_resource_t *resource, coap_session_t *session,
                                              const coap_pdu_t *request, const coap_string_t *query,
                                              coap_pdu_t *response) {
  coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
  coap_add_data_large_response(resource, session, request, response, query, COAP_MEDIATYPE_TEXT_PLAIN, 60, 0,
                               (size_t) 4, (const u_char *) "test", NULL, NULL);
}

}  // namespace esphome::coap
#endif
