#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR) && defined(USE_MDNS)

#include "mdns_component.h"
#include "esphome/core/application.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <zephyr/net/dns_sd.h>
#include <zephyr/net/mdns_responder.h>
#include <zephyr/net/net_ip.h>

namespace esphome::mdns {

static const char *const TAG = "mdns";

constexpr size_t TXT_BUFFER_SIZE = 256;

// mdns_responder_set_ext_records() stores these pointers, not copies -- must live forever.
std::array<dns_sd_rec, MDNS_SERVICE_COUNT> g_records{};                             // NOLINT
std::array<uint16_t, MDNS_SERVICE_COUNT> g_ports{};                                 // NOLINT
std::array<std::array<char, TXT_BUFFER_SIZE>, MDNS_SERVICE_COUNT> g_txt_buffers{};  // NOLINT

size_t encode_txt_records(const FixedVector<MDNSTXTRecord> &txt_records, char *buf, size_t buf_size) {
  size_t pos = 0;
  for (const auto &record : txt_records) {
    char kv[TXT_BUFFER_SIZE];
    int len = snprintf(kv, sizeof(kv), "%s=%s", MDNS_STR_ARG(record.key), MDNS_STR_ARG(record.value));
    if (len <= 0) {
      continue;
    }
    size_t entry_len = std::min(static_cast<size_t>(len), static_cast<size_t>(255));
    if (pos + 1 + entry_len > buf_size) {
      break;
    }
    buf[pos++] = static_cast<char>(entry_len);
    memcpy(buf + pos, kv, entry_len);
    pos += entry_len;
  }
  return pos;
}

static void register_zephyr(MDNSComponent *comp, StaticVector<MDNSService, MDNS_SERVICE_COUNT> &services) {
#if defined(USE_MDNS_RESPONDER)
  size_t count = 0;
  for (const auto &service : services) {
    g_ports[count] = net_htons(service.port.value());  // dns_sd_rec::port must be network byte order
    char *txt_buf = g_txt_buffers[count].data();
    size_t txt_size = encode_txt_records(service.txt_records, txt_buf, TXT_BUFFER_SIZE);

    dns_sd_rec &rec = g_records[count];
    rec.instance = App.get_name().c_str();
    rec.service = MDNS_STR_ARG(service.service_type);
    rec.proto = MDNS_STR_ARG(service.proto);
    rec.domain = "local";
    rec.text = txt_buf;
    rec.text_size = txt_size;
    rec.port = &g_ports[count];
    count++;
  }
  int err = mdns_responder_set_ext_records(g_records.data(), count);
  if (err != 0) {
    ESP_LOGW(TAG, "Failed to register ext records with mdns_responder %d", err);
    comp->mark_failed();
  }
#endif
}

void MDNSComponent::setup() { this->setup_buffers_and_register_(register_zephyr); }

void MDNSComponent::on_shutdown() {
#if defined(USE_MDNS_RESPONDER)
  int err = mdns_responder_set_ext_records(nullptr, 0);
  if (err != 0) {
    ESP_LOGW(TAG, "Failed to unregister ext records with mdns_responder %d", err);
  }
#endif
}

}  // namespace esphome::mdns

#endif  // USE_ZEPHYR && USE_MDNS
