#pragma once
#include <cstdint>
#include <cstring>
#include <lwip/def.h>
#include "esphome/core/defines.h"
#include "esphome/core/progmem.h"

namespace esphome::captive_portal {

// DNS constants
static constexpr uint16_t DNS_PORT = 53;
static constexpr uint16_t DNS_QR_FLAG = 1 << 15;
static constexpr uint16_t DNS_OPCODE_MASK = 0x7800;
static constexpr uint16_t DNS_QTYPE_A = 0x0001;
static constexpr uint16_t DNS_QCLASS_IN = 0x0001;
static constexpr uint16_t DNS_ANSWER_TTL = 300;
static constexpr size_t DNS_BUFFER_SIZE = 192;

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

/// Check if domain is allowlisted (web_server CDN domains)
/// Returns true if the domain should NOT be redirected
inline bool is_allowlisted_domain([[maybe_unused]] const char *domain) {
#ifdef USE_WEBSERVER
#ifdef WEBSERVER_CDN_DOMAIN_0
  if (ESPHOME_strcasecmp_P(domain, ESPHOME_F(WEBSERVER_CDN_DOMAIN_0)) == 0)
    return true;
#endif
#ifdef WEBSERVER_CDN_DOMAIN_1
  if (ESPHOME_strcasecmp_P(domain, ESPHOME_F(WEBSERVER_CDN_DOMAIN_1)) == 0)
    return true;
#endif
#endif
  return false;
}

/// Parse DNS domain name from packet into string buffer
/// Returns pointer past the domain name (after null terminator), or nullptr on error
inline uint8_t *parse_dns_domain(uint8_t *ptr, const uint8_t *end, char *domain, size_t domain_size) {
  size_t domain_len = 0;

  while (ptr < end && *ptr != 0) {
    uint8_t label_len = *ptr;
    if (label_len > 63) {  // Check for invalid label length
      return nullptr;
    }
    // Check if we have room for this label plus the length byte
    if (ptr + label_len + 1 > end) {
      return nullptr;  // Would overflow
    }
    // Add dot separator if not first label
    if (domain_len > 0 && domain_len < domain_size - 1) {
      domain[domain_len++] = '.';
    }
    // Copy label to domain string
    for (uint8_t i = 0; i < label_len && domain_len < domain_size - 1; i++) {
      domain[domain_len++] = ptr[1 + i];
    }
    ptr += label_len + 1;
  }
  domain[domain_len] = '\0';

  // Check if we reached a proper null terminator
  if (ptr >= end || *ptr != 0) {
    return nullptr;  // Name not terminated or truncated
  }
  return ptr + 1;  // Skip the null terminator
}

/// Build DNS response header for A record query
/// Modifies header in place, returns true if successful
inline void build_dns_response_header(DNSHeader *header) {
  header->flags = htons(DNS_QR_FLAG | 0x8000);  // Response + Authoritative
  header->an_count = htons(1);                  // One answer
}

/// Build DNS REFUSED response header
/// Used when domain is allowlisted to trigger fallback to other DNS
inline void build_dns_refused_header(DNSHeader *header) {
  header->flags = htons(DNS_QR_FLAG | 0x8005);  // Response + REFUSED
  header->an_count = 0;
  header->ns_count = 0;
  header->ar_count = 0;
}

/// Build DNS answer section
/// Returns the answer pointer, or nullptr if buffer too small
inline DNSAnswer *build_dns_answer(uint8_t *buffer, size_t buffer_size, size_t question_end_offset, uint32_t ip_addr) {
  size_t answer_offset = question_end_offset;

  // Check if we have room for the answer
  if (answer_offset + sizeof(DNSAnswer) > buffer_size) {
    return nullptr;
  }

  DNSAnswer *answer = reinterpret_cast<DNSAnswer *>(buffer + answer_offset);

  // Pointer to name in question (offset from start of packet)
  answer->ptr_offset = htons(0xC000 | sizeof(DNSHeader));
  answer->type = htons(DNS_QTYPE_A);
  answer->dns_class = htons(DNS_QCLASS_IN);
  answer->ttl = htonl(DNS_ANSWER_TTL);
  answer->addr_len = htons(4);
  answer->ip_addr = ip_addr;

  return answer;
}

}  // namespace esphome::captive_portal
