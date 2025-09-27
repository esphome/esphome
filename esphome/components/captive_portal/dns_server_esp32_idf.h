#pragma once
#ifdef USE_ESP_IDF

#include "esphome/core/helpers.h"
#include "esphome/components/network/ip_address.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esphome::captive_portal {

class DNSServer {
 public:
  void start(const network::IPAddress &ip);
  void stop();

 protected:
  static void dns_server_task(void *pvParameters);
  void process_dns_request(int sock);

  TaskHandle_t dns_task_handle_{nullptr};
  int dns_socket_{-1};
  network::IPAddress server_ip_;
};

}  // namespace esphome::captive_portal

#endif  // USE_ESP_IDF
