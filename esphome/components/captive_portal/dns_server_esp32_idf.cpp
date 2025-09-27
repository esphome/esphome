#include "dns_server_esp32_idf.h"
#ifdef USE_ESP_IDF

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>

namespace esphome::captive_portal {

static const char *const TAG = "captive_portal.dns";

// DNS constants
static constexpr uint16_t DNS_PORT = 53;
static constexpr uint16_t DNS_MAX_LEN = 256;
static constexpr uint16_t DNS_QR_FLAG = 1 << 15;
static constexpr uint16_t DNS_OPCODE_MASK = 0x7800;
static constexpr uint16_t DNS_QTYPE_A = 0x0001;
static constexpr uint16_t DNS_QCLASS_IN = 0x0001;
static constexpr uint16_t DNS_ANSWER_TTL = 300;
static constexpr size_t DNS_TASK_STACK_SIZE = 3072;

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

DNSServer::~DNSServer() { this->stop(); }

void DNSServer::start(const network::IPAddress &ip) {
  this->server_ip_ = ip;
  ESP_LOGI(TAG, "Starting DNS server on %s", ip.str().c_str());

  // Create socket
  this->dns_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (this->dns_socket_ < 0) {
    ESP_LOGE(TAG, "Socket create failed: %d", errno);
    return;
  }
  ESP_LOGD(TAG, "Socket created: %d", this->dns_socket_);

  // Set socket options
  int enable = 1;
  if (setsockopt(this->dns_socket_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
    ESP_LOGW(TAG, "SO_REUSEADDR failed: %d", errno);
  }

  // Bind to port 53
  struct sockaddr_in server_addr = {};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(DNS_PORT);

  if (bind(this->dns_socket_, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
    ESP_LOGE(TAG, "Bind failed: %d", errno);
    close(this->dns_socket_);
    this->dns_socket_ = -1;
    return;
  }
  ESP_LOGD(TAG, "Bound to port %d", DNS_PORT);

  // Create task
  BaseType_t task_result =
      xTaskCreate(&DNSServer::dns_server_task, "dns_server", DNS_TASK_STACK_SIZE, this, 1, &this->dns_task_handle_);
  if (task_result != pdPASS) {
    ESP_LOGE(TAG, "Task create failed");
    close(this->dns_socket_);
    this->dns_socket_ = -1;
    return;
  }
}

void DNSServer::stop() {
  if (this->dns_task_handle_) {
    vTaskDelete(this->dns_task_handle_);
    this->dns_task_handle_ = nullptr;
  }

  if (this->dns_socket_ >= 0) {
    close(this->dns_socket_);
    this->dns_socket_ = -1;
  }

  ESP_LOGI(TAG, "Stopped");
}

void DNSServer::dns_server_task(void *pvParameters) {
  DNSServer *server = static_cast<DNSServer *>(pvParameters);
  ESP_LOGV(TAG, "Task started, socket: %d", server->dns_socket_);

  // Set socket timeout to prevent blocking forever
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  if (setsockopt(server->dns_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    ESP_LOGW(TAG, "SO_RCVTIMEO failed: %d", errno);
  }

  while (true) {
    server->process_dns_request(server->dns_socket_);
  }
}

void DNSServer::process_dns_request(int sock) {
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  uint8_t rx_buffer[DNS_MAX_LEN];
  uint8_t tx_buffer[DNS_MAX_LEN];

  // Receive DNS request
  int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *) &client_addr, &client_addr_len);

  if (len < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
      ESP_LOGE(TAG, "recvfrom failed: %d", errno);
    }
    return;
  }

  ESP_LOGVV(TAG, "Received %d bytes from %s:%d", len, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

  if (len < sizeof(DNSHeader) + 1) {
    ESP_LOGW(TAG, "Request too short: %d", len);
    return;
  }

  // Parse DNS header
  DNSHeader *header = (DNSHeader *) rx_buffer;
  uint16_t flags = ntohs(header->flags);
  uint16_t qd_count = ntohs(header->qd_count);

  // Check if it's a standard query
  if ((flags & DNS_QR_FLAG) || (flags & DNS_OPCODE_MASK) || qd_count != 1) {
    ESP_LOGV(TAG, "Not a standard query: flags=0x%04X, qd_count=%d", flags, qd_count);
    return;  // Not a standard query
  }

  // Parse domain name (we don't actually care about it - redirect everything)
  uint8_t *ptr = rx_buffer + sizeof(DNSHeader);
  uint8_t *name_start = ptr;
  while (*ptr != 0 && ptr < (rx_buffer + len)) {
    if (*ptr > 63) {  // Check for invalid label length
      return;
    }
    ptr += *ptr + 1;
  }

  if (*ptr != 0) {
    return;  // Name not terminated
  }
  ptr++;  // Skip the null terminator

  // Check we have room for the question
  if (ptr + sizeof(DNSQuestion) > rx_buffer + len) {
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
  memset(tx_buffer, 0, sizeof(tx_buffer));

  // Copy request header and modify flags
  memcpy(tx_buffer, rx_buffer, sizeof(DNSHeader));
  DNSHeader *response_header = (DNSHeader *) tx_buffer;
  response_header->flags = htons(DNS_QR_FLAG | 0x8000);  // Response + Authoritative
  response_header->an_count = htons(1);                  // One answer

  // Copy the question section
  size_t question_len = (ptr + sizeof(DNSQuestion)) - rx_buffer - sizeof(DNSHeader);
  memcpy(tx_buffer + sizeof(DNSHeader), rx_buffer + sizeof(DNSHeader), question_len);

  // Add answer section
  size_t answer_offset = sizeof(DNSHeader) + question_len;
  DNSAnswer *answer = (DNSAnswer *) (tx_buffer + answer_offset);

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
  int sent = sendto(sock, tx_buffer, response_len, 0, (struct sockaddr *) &client_addr, client_addr_len);
  if (sent < 0) {
    ESP_LOGV(TAG, "Send failed: %d", errno);
  } else {
    ESP_LOGV(TAG, "Sent %d bytes", sent);
  }
}

}  // namespace esphome::captive_portal

#endif  // USE_ESP_IDF
