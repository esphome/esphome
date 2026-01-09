#include "dns_server_arduino.h"
#if defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY)

#include "dns_server_common.h"
#include "esphome/core/log.h"
#include <lwip/def.h>

namespace esphome::captive_portal {

static const char *const TAG = "captive_portal.dns";

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

  // Parse domain name
  uint8_t *ptr = this->buffer_ + sizeof(DNSHeader);
  uint8_t *end = this->buffer_ + len;
  char domain[128];

  ptr = parse_dns_domain(ptr, end, domain, sizeof(domain));
  if (ptr == nullptr) {
    return;  // Invalid domain name
  }

  // Check whitelist and send REFUSED if needed
  if (is_whitelisted_domain(domain)) {
    ESP_LOGD(TAG, "Whitelisted domain, sending REFUSED: %s", domain);
    build_dns_refused_header(header);
    this->udp_->beginPacket(this->udp_->remoteIP(), this->udp_->remotePort());
    this->udp_->write(this->buffer_, len);
    this->udp_->endPacket();
    return;
  }

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
  build_dns_response_header(header);

  // Add answer section after the question
  size_t question_end_offset = (ptr + sizeof(DNSQuestion)) - this->buffer_;
  if (build_dns_answer(this->buffer_, sizeof(this->buffer_), question_end_offset,
                       static_cast<uint32_t>(this->server_ip_)) == nullptr) {
    ESP_LOGW(TAG, "Response too large");
    return;
  }

  size_t response_len = question_end_offset + sizeof(DNSAnswer);

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

#endif  // USE_ESP8266 || USE_RP2040 || USE_LIBRETINY
