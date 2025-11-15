#pragma once
#include "esphome/core/defines.h"
#ifdef USE_COAP_SERVER
#include <netdb.h>
#include <string>
#include "coap3/coap.h"
#ifdef USE_ESP_IDF
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif
#include "esphome/core/component.h"

namespace esphome::coap {

class CoapServerComponent : public Component {
 public:
  CoapServerComponent();
  void setup() override;
  bool teardown() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void set_listen_port(uint16_t port) { this->listen_port_ = port; }
  void set_secure_listen_port(uint16_t port) { this->secure_listen_port_ = port; }
  void set_websocket_port(uint16_t port) { this->websocket_port_ = port; }
  void set_secure_websocket_port(uint16_t port) { this->secure_websocket_port_ = port; }
  void set_mcast_ip_mode_v4_addr(std::string ip) {
    this->mcast_ip_mode_v4_ = true;
    this->mcast_ip_mode_v4_addr_ = ip;
  }
  void set_mcast_ip_mode_v6_addr(std::string ip) {
    this->mcast_ip_mode_v6_ = true;
    this->mcast_ip_mode_v6_addr_ = ip;
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
  void set_max_idle_sessions(uint16_t sessions) { this->max_idle_sessions_ = sessions; }
  void set_keep_alive(uint16_t alive) { this->keep_alive_ = alive; }

  static void esphome_get_handler(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                                  const coap_string_t *query, coap_pdu_t *response);

 protected:
  void main_();

#ifdef CONFIG_COAP_MBEDTLS_PKI
  void provision_pki_(coap_dtls_pki_t *dtls_pki);
#endif

  bool main_looping_{true};
  uint8_t server_failure_cntdown_{100};
  bool torndown_{false};
  uint16_t listen_port_{5683};
  uint16_t secure_listen_port_{5684};
  uint16_t websocket_port_{80};
  uint16_t secure_websocket_port_{443};
  bool mcast_ip_mode_v4_{true};
  bool mcast_ip_mode_v6_{true};
  std::string mcast_ip_mode_v4_addr_{"224.0.1.187"};
  std::string mcast_ip_mode_v6_addr_{"FF02::FD"};

  TaskHandle_t main_task_handle_{nullptr};
#ifdef CONFIG_COAP_OSCORE_SUPPORT
  std::string oscore_conf_str_{};
#endif
#ifdef CONFIG_COAP_MBEDTLS_PSK
  std::string psk_identity_{};
  std::string psk_key_{};
#endif
#ifdef CONFIG_COAP_MBEDTLS_PKI
  std::string ca_pem_str_{};
  std::string server_crt_str_{};
  std::string server_key_str_{};
#endif
  uint16_t max_idle_sessions_{20};
  uint16_t keep_alive_{30};
  uint32_t request_timeout_{COAP_RESOURCE_CHECK_TIME * 1000};
};

extern CoapServerComponent *global_coap_server;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::coap
#endif
