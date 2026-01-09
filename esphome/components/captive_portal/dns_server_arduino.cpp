#include "dns_server_arduino.h"
#if defined(USE_ARDUINO) && (defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY))

#include <cstring>
#include "esphome/core/log.h"
#include <lwip/def.h>

namespace esphome::captive_portal {

static const char *const TAG = "captive_portal.dns";

// DNS constants
static constexpr uint16_t DNS_PORT = 53;
static constexpr uint16_t DNS_QR_FLAG = 1 << 15;
static constexpr uint16_t DNS_OPCODE_MASK = 0x7800;
static constexpr uint16_t DNS_QTYPE_A = 0x0001;
static constexpr uint16_t DNS_QCLASS_IN = 0x0001;
static constexpr uint16_t DNS_ANSWER_TTL = 300;

// DNS Header structure
struct DNSHeader {
  uint16_t id;
  uint16_t flags;
  uint16_t qd_count;
  uint16_t an_count;
  uint16_t ns_count;
  uint16_t ar_count;
} __attribute__((packed));

// DNS Question structure
struct DNSQuestion {
  uint16_t type;
  uint16_t dns_class;
} __attribute__((packed));

// DNS Answer structure
struct DNSAnswer {
  uint16_t ptr_offset;
  uint16_t type;
  uint16_t dns_class;
  uint32_t ttl;
  uint16_t addr_len;
  uint32_t ip_addr;
} __attribute__((packed));

void DNSServer::start(const network::IPAddress &ip) {
  this->server_ip_ = ip;
  ESP_LOGV(TAG, "Starting DNS server");

  this->udp_ = make_unique<WiFiUDP>();
  if (!this->udp_->begin(DNS_PORT)) {
    ESP_LOGE(TAG, "Failed to start UDP on port %d", DNS_PORT);
    this->udp_ = nullptr;
    return;
  }
  ESP_LOGV(TAG, "Bound to port %d", DNS_PORT);
}

void DNSServer::stop() {
  if (this->udp_ != nullptr) {
    this->udp_->stop();
    this->udp_ = nullptr;
  }
  ESP_LOGV(TAG, "Stopped");
}

void DNSServer::process_next_request() {
  if (this->udp_ == nullptr) {
    return;
  }

  int packet_size = this->udp_->parsePacket();
  if (packet_size == 0) {
    return;
  }

  if (packet_size > static_cast<int>(sizeof(this->buffer_))) {
    // Packet too large, skip it
    while (this->udp_->available()) {
      this->udp_->read();
    }
    return;
  }

  int len = this->udp_->read(this->buffer_, sizeof(this->buffer_));
  if (len < static_cast<int>(sizeof(DNSHeader) + 1)) {
    ESP_LOGV(TAG, "Request too short: %d", len);
    return;
  }

  // Parse DNS header
  DNSHeader *header = (DNSHeader *) this->buffer_;
  uint16_t flags = ntohs(header->flags);
  uint16_t qd_count = ntohs(header->qd_count);

  // Check if it's a standard query
  if ((flags & DNS_QR_FLAG) || (flags & DNS_OPCODE_MASK) || qd_count != 1) {
    ESP_LOGV(TAG, "Not a standard query: flags=0x%04X, qd_count=%d", flags, qd_count);
    return;
  }

  // Parse domain name and check for whitelisted domains
  uint8_t *ptr = this->buffer_ + sizeof(DNSHeader);
  uint8_t *end = this->buffer_ + len;

  // Build domain name string for whitelist checking
  char domain[128];
  size_t domain_len = 0;
  uint8_t *name_ptr = ptr;

  while (name_ptr < end && *name_ptr != 0) {
    uint8_t label_len = *name_ptr;
    if (label_len > 63) {
      return;
    }
    if (name_ptr + label_len + 1 > end) {
      return;
    }
    if (domain_len > 0 && domain_len < sizeof(domain) - 1) {
      domain[domain_len++] = '.';
    }
    for (uint8_t i = 0; i < label_len && domain_len < sizeof(domain) - 1; i++) {
      domain[domain_len++] = name_ptr[1 + i];
    }
    name_ptr += label_len + 1;
  }
  domain[domain_len] = '\0';

  if (name_ptr >= end || *name_ptr != 0) {
    return;
  }
  ptr = name_ptr + 1;

#ifdef USE_WEBSERVER
  // Whitelist: don't redirect web_server's CDN domain
  // Respond with REFUSED to tell client to try another DNS server (e.g., cellular)
  if (domain_len >= 11) {
    const char *suffix = domain + domain_len - 11;
    if (strcmp(suffix, ".esphome.io") == 0 || strcmp(domain, "esphome.io") == 0) {
      ESP_LOGD(TAG, "Whitelisted domain, sending REFUSED: %s", domain);
      // Send REFUSED response (RCODE=5) to trigger fallback to other DNS
      header->flags = htons(DNS_QR_FLAG | 0x8005);  // Response + REFUSED
      header->an_count = 0;
      header->ns_count = 0;
      header->ar_count = 0;
      this->udp_->beginPacket(this->udp_->remoteIP(), this->udp_->remotePort());
      this->udp_->write(this->buffer_, len);
      this->udp_->endPacket();
      return;
    }
  }
#endif

  ESP_LOGV(TAG, "Redirecting DNS query for: %s", domain);

  // Check we have room for the question
  if (ptr + sizeof(DNSQuestion) > end) {
    return;
  }

  // Parse DNS question
  DNSQuestion *question = (DNSQuestion *) ptr;
  uint16_t qtype = ntohs(question->type);
  uint16_t qclass = ntohs(question->dns_class);

  if (qtype != DNS_QTYPE_A || qclass != DNS_QCLASS_IN) {
    ESP_LOGV(TAG, "Not an A query: type=0x%04X, class=0x%04X", qtype, qclass);
    return;
  }

  // Build DNS response
  header->flags = htons(DNS_QR_FLAG | 0x8000);
  header->an_count = htons(1);

  size_t question_len = (ptr + sizeof(DNSQuestion)) - this->buffer_ - sizeof(DNSHeader);
  size_t answer_offset = sizeof(DNSHeader) + question_len;

  if (answer_offset + sizeof(DNSAnswer) > sizeof(this->buffer_)) {
    ESP_LOGW(TAG, "Response too large");
    return;
  }

  DNSAnswer *answer = (DNSAnswer *) (this->buffer_ + answer_offset);
  answer->ptr_offset = htons(0xC000 | sizeof(DNSHeader));
  answer->type = htons(DNS_QTYPE_A);
  answer->dns_class = htons(DNS_QCLASS_IN);
  answer->ttl = htonl(DNS_ANSWER_TTL);
  answer->addr_len = htons(4);
  answer->ip_addr = static_cast<uint32_t>(this->server_ip_);

  size_t response_len = answer_offset + sizeof(DNSAnswer);

  // Send response
  this->udp_->beginPacket(this->udp_->remoteIP(), this->udp_->remotePort());
  this->udp_->write(this->buffer_, response_len);
  if (!this->udp_->endPacket()) {
    ESP_LOGV(TAG, "Send failed");
  } else {
    ESP_LOGV(TAG, "Sent %d bytes", response_len);
  }
}

}  // namespace esphome::captive_portal

#endif  // USE_ARDUINO
