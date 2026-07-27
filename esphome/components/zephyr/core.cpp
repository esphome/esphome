#ifdef USE_ZEPHYR

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"

#ifdef USE_ZEPHYR_VARIANT_FAMILY_ESP32
#include <zephyr/drivers/hwinfo.h>
#endif

// Probe rather than require: this header only exists once another component (sha256, md5,
// api encryption) has turned on the MBEDTLS Kconfig.
#if __has_include(<mbedtls/build_info.h>)
#include <mbedtls/build_info.h>
#if MBEDTLS_VERSION_MAJOR >= 4
#define ESPHOME_ZEPHYR_HAS_PSA_CRYPTO
#include <psa/crypto.h>
#endif
#endif

namespace esphome {

// HAL functions live in hal.cpp.

Mutex::Mutex() {
  auto *mutex = new k_mutex();
  this->handle_ = mutex;
  k_mutex_init(mutex);
}
Mutex::~Mutex() { delete static_cast<k_mutex *>(this->handle_); }
void Mutex::lock() { k_mutex_lock(static_cast<k_mutex *>(this->handle_), K_FOREVER); }
bool Mutex::try_lock() { return k_mutex_lock(static_cast<k_mutex *>(this->handle_), K_NO_WAIT) == 0; }
void Mutex::unlock() { k_mutex_unlock(static_cast<k_mutex *>(this->handle_)); }

IRAM_ATTR InterruptLock::InterruptLock() { state_ = irq_lock(); }
IRAM_ATTR InterruptLock::~InterruptLock() { irq_unlock(state_); }

// Zephyr LwIPLock is defined inline as a no-op in helpers.h

bool random_bytes(uint8_t *data, size_t len) {
  sys_rand_get(data, len);
  return true;
}

#ifdef USE_NRF52
void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
  mac[0] = ((NRF_FICR->DEVICEADDR[1] & 0xFFFF) >> 8) | 0xC0;
  mac[1] = NRF_FICR->DEVICEADDR[1] & 0xFFFF;
  mac[2] = NRF_FICR->DEVICEADDR[0] >> 24;
  mac[3] = NRF_FICR->DEVICEADDR[0] >> 16;
  mac[4] = NRF_FICR->DEVICEADDR[0] >> 8;
  mac[5] = NRF_FICR->DEVICEADDR[0];
}
#elif defined(USE_ZEPHYR_VARIANT_NATIVE_SIM)
void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
  static const uint8_t addr[6] = USE_ESPHOME_HOST_MAC_ADDRESS;
  memcpy(mac, addr, sizeof(addr));
}
#elif defined(USE_ZEPHYR_VARIANT_FAMILY_ESP32)
void get_mac_address_raw(uint8_t *mac) {  // NOLINT(readability-non-const-parameter)
  // hwinfo_esp32.c is generic, so this covers every esp32-family chip.
  if (hwinfo_get_device_id(mac, 6) != 6) {
    memset(mac, 0, 6);
  }
}
#endif

}  // namespace esphome

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM
#include <climits>
#include <unistd.h>

namespace esphome::zephyr {

static std::string s_exe_path;     // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::string s_reexec_path;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const std::string &get_exe_path() {
  if (s_exe_path.empty()) {
    char buf[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
      buf[len] = '\0';
      s_exe_path = buf;
    }
  }
  return s_exe_path;
}

void arm_reexec(const std::string &path) { s_reexec_path = path; }

const char *get_reexec_path() { return s_reexec_path.empty() ? nullptr : s_reexec_path.c_str(); }

}  // namespace esphome::zephyr
#endif  // USE_ZEPHYR_VARIANT_NATIVE_SIM

void setup();
void loop();

#ifdef CONFIG_SMP
// ESP-IDF (and every other multi-core ESPHome platform) runs the main loop pinned to the
// second core, not the boot core. On Zephyr, main() itself can't pin itself to a core while
// it's running (k_thread_cpu_pin() only accepts threads that aren't currently scheduled), so
// a dedicated worker thread is spawned here and pinned before it ever runs.
static K_THREAD_STACK_DEFINE(esphome_main_stack, CONFIG_MAIN_STACK_SIZE);
static struct k_thread esphome_main_thread_data;

static void esphome_main_thread(void *, void *, void *) {
#ifdef ESPHOME_ZEPHYR_HAS_PSA_CRYPTO
  psa_crypto_init();
#endif
  setup();
  while (true) {
    loop();
  }
}
#endif

int main() {
#ifdef CONFIG_SMP
  // k_thread_cpu_pin() only takes effect on a thread that isn't currently schedulable, so the
  // thread is created suspended (K_FOREVER) and started explicitly after it's pinned.
  k_tid_t tid =
      k_thread_create(&esphome_main_thread_data, esphome_main_stack, K_THREAD_STACK_SIZEOF(esphome_main_stack),
                      esphome_main_thread, nullptr, nullptr, nullptr, CONFIG_MAIN_THREAD_PRIORITY, 0, K_FOREVER);
  k_thread_cpu_pin(tid, 1);
  k_thread_name_set(tid, "esphome_main");
  k_thread_start(tid);
#else
#ifdef ESPHOME_ZEPHYR_HAS_PSA_CRYPTO
  psa_crypto_init();
#endif
  setup();
  while (true) {
    loop();
  }
#endif
  return 0;
}

#endif
