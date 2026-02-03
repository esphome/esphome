#include "esphome/core/helpers.h"

#ifdef USE_HOST

#ifndef _WIN32
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#endif
#include <unistd.h>
#include <limits>
#include <random>

#include "esphome/core/defines.h"
#include "esphome/core/log.h"

namespace esphome {

static const char *const TAG = "helpers.host";

uint32_t random_uint32() {
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<uint32_t> dist(0, std::numeric_limits<uint32_t>::max());
  return dist(rng);
}

bool random_bytes(uint8_t *data, size_t len) {
  FILE *fp = fopen("/dev/urandom", "r");
  if (fp == nullptr) {
    ESP_LOGW(TAG, "Could not open /dev/urandom, errno=%d", errno);
    exit(1);
  }
  size_t read = fread(data, 1, len, fp);
  if (read != len) {
    ESP_LOGW(TAG, "Not enough data from /dev/urandom");
    exit(1);
  }
  fclose(fp);
  return true;
}

// Host platform uses std::mutex for proper thread synchronization
Mutex::Mutex() { handle_ = new std::mutex(); }
Mutex::~Mutex() { delete static_cast<std::mutex *>(handle_); }
void Mutex::lock() { static_cast<std::mutex *>(handle_)->lock(); }
bool Mutex::try_lock() { return static_cast<std::mutex *>(handle_)->try_lock(); }
void Mutex::unlock() { static_cast<std::mutex *>(handle_)->unlock(); }

void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
  static const uint8_t esphome_host_mac_address[6] = USE_ESPHOME_HOST_MAC_ADDRESS;
  memcpy(mac, esphome_host_mac_address, sizeof(esphome_host_mac_address));
}

}  // namespace esphome

#endif  // USE_HOST
