#include "dns_server_esp32_idf.h"
#ifdef USE_ESP32

#include "dns_server_common.h"
#include "esphome/core/log.h"
#include "esphome/components/socket/socket.h"
#include <lwip/sockets.h>
#include <lwip/inet.h>

namespace esphome::captive_portal {

static const char *const TAG = "captive_portal.dns";

void DNSServer::start(const network::IPAddress &ip) {
  this->server_ip_ = ip;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
  ESP_LOGV(TAG, "Starting DNS server on %s", ip.str_to(ip_buf));
#endif

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
    ssize_t sent = this->socket_->sendto(this->buffer_, len, 0, (struct sockaddr *) &client_addr, client_addr_len);
    if (sent < 0) {
      ESP_LOGV(TAG, "Send REFUSED failed: %d", errno);
    }
    return;
  }

  ESP_LOGV(TAG, "Redirecting DNS query for: %s", domain);

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

  // Build DNS response
  build_dns_response_header(header);

  // Add answer section after the question
  size_t question_end_offset = (ptr + sizeof(DNSQuestion)) - this->buffer_;
  ip4_addr_t addr = this->server_ip_;
  if (build_dns_answer(this->buffer_, sizeof(this->buffer_), question_end_offset, addr.addr) == nullptr) {
    ESP_LOGW(TAG, "Response too large");
    return;
  }

  size_t response_len = question_end_offset + sizeof(DNSAnswer);

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

#endif  // USE_ESP32
