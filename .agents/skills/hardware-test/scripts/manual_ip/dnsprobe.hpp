#pragma once
#include "esphome/core/log.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#ifdef USE_ESP8266
#include <ESP8266WiFi.h>
#else
#include "lwip/netdb.h"
#include "lwip/inet.h"
#endif
inline void dns_probe() {
  char b0[IPADDR_STRLEN_MAX], b1[IPADDR_STRLEN_MAX], res[16] = "-";
  ipaddr_ntoa_r(dns_getserver(0), b0, sizeof(b0));
  ipaddr_ntoa_r(dns_getserver(1), b1, sizeof(b1));
  int ok;
#ifdef USE_ESP8266
  IPAddress ip;
  ok = WiFi.hostByName("pool.ntp.org", ip);
  if (ok)
    strncpy(res, ip.toString().c_str(), sizeof(res) - 1);
#else
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  struct addrinfo *r = nullptr;
  ok = lwip_getaddrinfo("pool.ntp.org", nullptr, &hints, &r) == 0 && r != nullptr;
  if (ok) {
    inet_ntoa_r(((struct sockaddr_in *) r->ai_addr)->sin_addr, res, sizeof(res));
    lwip_freeaddrinfo(r);
  }
#endif
  ESP_LOGI("dnsprobe", "dns0=%s dns1=%s lookup=%s %s", b0, b1, ok ? "ok" : "FAIL", res);
}
