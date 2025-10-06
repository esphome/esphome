#include "dns_server_esp32_idf.h"
#ifdef USE_ESP_IDF

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/socket/socket.h"
#include <lwip/sockets.h>
#include <lwip/inet.h>

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
  ESP_LOGV(TAG, "Starting DNS server on %s", ip.str().c_str());

  // Create loop-monitored UDP socket
  this->socket_ = socket::socket_ip_loop_monitored(SOCK_DGRAM, IPPROTO_UDP);
  if (this->socket_ == nullptr) {
    ESP_LOGE(TAG, "Socket create failed");
    return;
  }

  // Set socket options
  int enable = 1;
  this->socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

  // Bind to port 53
  struct sockaddr_storage server_addr = {};
  socklen_t addr_len = socket::set_sockaddr_any((struct sockaddr *) &server_addr, sizeof(server_addr), DNS_PORT);

  int err = this->socket_->bind((struct sockaddr *) &server_addr, addr_len);
  if (err != 0) {
    ESP_LOGE(TAG, "Bind failed: %d", errno);
    this->socket_ = nullptr;
    return;
  }
  ESP_LOGV(TAG, "Bound to port %d", DNS_PORT);
}

void DNSServer::stop() {
  if (this->socket_ != nullptr) {
    this->socket_->close();
    this->socket_ = nullptr;
  }
  ESP_LOGV(TAG, "Stopped");
}

void DNSServer::process_next_request() {
  // Process one request if socket is valid and data is available
  if (this->socket_ == nullptr || !this->socket_->ready()) {
    return;
  }
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  // Receive DNS request using raw fd for recvfrom
  int fd = this->socket_->get_fd();
  if (fd < 0) {
    return;
  }

  ssize_t len = recvfrom(fd, this->buffer_, sizeof(this->buffer_), MSG_DONTWAIT, (struct sockaddr *) &client_addr,
                         &client_addr_len);

  if (len < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
      ESP_LOGE(TAG, "recvfrom failed: %d", errno);
    }
    return;
  }

  ESP_LOGVV(TAG, "Received %d bytes from %s:%d", len, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

  if (len < static_cast<ssize_t>(sizeof(DNSHeader) + 1)) {
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
    return;  // Not a standard query
  }

  // Parse domain name (we don't actually care about it - redirect everything)
  uint8_t *ptr = this->buffer_ + sizeof(DNSHeader);
  uint8_t *end = this->buffer_ + len;

  while (ptr < end && *ptr != 0) {
    uint8_t label_len = *ptr;
    if (label_len > 63) {  // Check for invalid label length
      return;
    }
    // Check if we have room for this label plus the length byte
    if (ptr + label_len + 1 > end) {
      return;  // Would overflow
    }
    ptr += label_len + 1;
  }

  // Check if we reached a proper null terminator
  if (ptr >= end || *ptr != 0) {
    return;  // Name not terminated or truncated
  }
  ptr++;  // Skip the null terminator

  // Check we have room for the question
  if (ptr + sizeof(DNSQuestion) > end) {
    return;  // Request truncated
  }

  // Parse DNS question
  DNSQuestion *question = (DNSQuestion *) ptr;
  uint16_t qtype = ntohs(question->type);
  uint16_t qclass = ntohs(question->dns_class);

  // We only handle A queries
  if (qtype != DNS_QTYPE_A || qclass != DNS_QCLASS_IN) {
    ESP_LOGV(TAG, "Not an A query: type=0x%04X, class=0x%04X", qtype, qclass);
    return;  // Not an A query
  }

  // Build DNS response by modifying the request in-place
  header->flags = htons(DNS_QR_FLAG | 0x8000);  // Response + Authoritative
  header->an_count = htons(1);                  // One answer

  // Add answer section after the question
  size_t question_len = (ptr + sizeof(DNSQuestion)) - this->buffer_ - sizeof(DNSHeader);
  size_t answer_offset = sizeof(DNSHeader) + question_len;

  // Check if we have room for the answer
  if (answer_offset + sizeof(DNSAnswer) > sizeof(this->buffer_)) {
    ESP_LOGW(TAG, "Response too large");
    return;
  }

  DNSAnswer *answer = (DNSAnswer *) (this->buffer_ + answer_offset);

  // Pointer to name in question (offset from start of packet)
  answer->ptr_offset = htons(0xC000 | sizeof(DNSHeader));
  answer->type = htons(DNS_QTYPE_A);
  answer->dns_class = htons(DNS_QCLASS_IN);
  answer->ttl = htonl(DNS_ANSWER_TTL);
  answer->addr_len = htons(4);

  // Get the raw IP address
  ip4_addr_t addr = this->server_ip_;
  answer->ip_addr = addr.addr;

  size_t response_len = answer_offset + sizeof(DNSAnswer);

  // Send response
  ssize_t sent =
      this->socket_->sendto(this->buffer_, response_len, 0, (struct sockaddr *) &client_addr, client_addr_len);
  if (sent < 0) {
    ESP_LOGV(TAG, "Send failed: %d", errno);
  } else {
    ESP_LOGV(TAG, "Sent %d bytes", sent);
  }
}

}  // namespace esphome::captive_portal

#endif  // USE_ESP_IDF
