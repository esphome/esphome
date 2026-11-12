#include "improv_serial_component.h"
#ifdef USE_IMPROV_SERIAL
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"

#include "esphome/components/logger/logger.h"
#include "esphome/components/network/util.h"
#ifdef USE_WIFI
#include "esphome/components/wifi/scan_list.h"
#endif

#include <array>

namespace esphome::improv_serial {

ESPHOME_LOG_TAG(TAG, "improv_serial");

static constexpr uint32_t WIFI_CONNECT_TIMEOUT_ID = 0;

static constexpr uint8_t FRAME_PREFIX[] PROGMEM = {'I', 'M', 'P', 'R', 'O', 'V', improv::IMPROV_SERIAL_VERSION};
static_assert(sizeof(FRAME_PREFIX) + 2 == FRAME_HEADER_SIZE, "type and length follow the prefix");

#if defined(USE_NETWORK_PRIMARY_INTERFACE_WIFI) && defined(USE_WIFI)
static constexpr bool WIFI_URL_FIRST = true;
#else
static constexpr bool WIFI_URL_FIRST = false;
#endif

/// Fill the frame header and return its checksum.
static uint8_t write_header(uint8_t *frame, improv::ImprovSerialType type, uint8_t length) {
  progmem_memcpy(frame, FRAME_PREFIX, sizeof(FRAME_PREFIX));
  frame[FRAME_HEADER_SIZE - 2] = type;
  frame[FRAME_HEADER_SIZE - 1] = length;
  return sum_bytes(frame, FRAME_HEADER_SIZE);
}

void ImprovSerialComponent::setup() {
  global_improv_serial_component = this;

  // The Improv state machine tracks Wi-Fi provisioning only. General device
  // connectivity (e.g. Ethernet) is reported separately via GET_NETWORK_STATE.
#ifdef USE_WIFI
  auto *wifi = wifi::global_wifi_component;
  if (wifi != nullptr && wifi->has_sta()) {
    this->state_ = improv::STATE_PROVISIONED;
  } else if (wifi != nullptr && !wifi->is_disabled()) {
    // Respect Wi-Fi's disabled state; forcing a scan while disabled throws
    // the wifi component into an invalid state from which it cannot recover.
    wifi->start_scanning();
  }
#endif
}

void ImprovSerialComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->last_read_byte_ && (now - this->last_read_byte_ > IMPROV_SERIAL_TIMEOUT)) {
    this->last_read_byte_ = 0;
    this->reset_rx_buffer_();
    ESP_LOGV(TAG, "Timeout");
  }

  uint8_t byte;
  while (this->read_byte_(byte)) {
    if (this->parse_improv_serial_byte_(byte)) {
      this->last_read_byte_ = now;
    } else {
      this->last_read_byte_ = 0;
      this->reset_rx_buffer_();
    }
  }

#ifdef USE_WIFI
  auto *wifi = wifi::global_wifi_component;
  if (this->state_ == improv::STATE_PROVISIONING && wifi != nullptr && wifi->is_connected()) {
    // Being connected is not enough: re-provisioning a device that is already online leaves the
    // prior network up until it drops, so check that the joined network is the requested one
    // before reporting success. Same test as the wifi.connect action.
    char ssid_buf[wifi::SSID_BUFFER_SIZE];
    if (strcmp(wifi->wifi_ssid_to(ssid_buf), this->connecting_sta_->get_ssid().c_str()) == 0) {
      wifi->save_wifi_sta(this->connecting_sta_->get_ssid(), this->connecting_sta_->get_password());
      this->connecting_sta_.reset();
      this->cancel_timeout(WIFI_CONNECT_TIMEOUT_ID);
      this->set_state_(improv::STATE_PROVISIONED);

      this->send_settings_response_(improv::WIFI_SETTINGS);
    }
  }
#endif
}

void ImprovSerialComponent::dump_config() { ESP_LOGCONFIG(TAG, "Improv Serial:"); }

#ifdef USE_WEBSERVER
void ImprovSerialComponent::add_webserver_urls_(improv::RpcResponseBuilder &builder, [[maybe_unused]] bool wifi_first) {
  // The webserver listens on every interface, so advertise each one that has a usable IPv4.
  // network::get_ip_addresses() can't be used here: it returns only the highest-priority
  // interface's addresses, which are all-unset (0.0.0.0) when e.g. Ethernet has no link while
  // the device is online via Wi-Fi, and 0.0.0.0 must not become the advertised URL. OpenThread
  // is omitted: it only ever has IPv6 addresses, which cannot form an IPv4 http:// URL.
  const auto append_urls = [this, &builder](const network::IPAddresses &addresses) {
    for (const auto &ip : addresses) {
      if (ip.is_ip4() && ip.is_set())
        this->add_webserver_url_(builder, ip);
    }
  };
#ifdef USE_WIFI
  // Clients redirect to the first URL, so the interface the client just configured has to lead:
  // another interface's address can be on a subnet that client cannot reach.
  const auto append_wifi_urls = [&append_urls]() {
    if (wifi::global_wifi_component != nullptr)
      append_urls(wifi::global_wifi_component->get_ip_addresses());
  };
  if (wifi_first)
    append_wifi_urls();
#endif
#ifdef USE_ETHERNET
  if (ethernet::global_eth_component != nullptr)
    append_urls(ethernet::global_eth_component->get_ip_addresses());
#endif
#ifdef USE_MODEM
  if (modem::global_modem_component != nullptr)
    append_urls(modem::global_modem_component->get_ip_addresses());
#endif
#ifdef USE_WIFI
  if (!wifi_first)
    append_wifi_urls();
#endif
}
#endif  // USE_WEBSERVER

void ImprovSerialComponent::send_settings_response_(improv::Command command) {
  std::array<uint8_t, improv::RPC_RESPONSE_MAX_SIZE> buf;
  improv::RpcResponseBuilder builder(buf, command);
#ifdef USE_IMPROV_NEXT_URL
  this->add_next_url_(builder, MAX_NEXT_URL_LEN);
#endif
#ifdef USE_WEBSERVER
  // This response only ever answers Wi-Fi provisioning, so lead with the Wi-Fi URL as it did
  // before other interfaces were reported.
  this->add_webserver_urls_(builder, /*wifi_first=*/true);
#endif
  this->send_response_(builder.finish(false));
}

void ImprovSerialComponent::send_version_info_() {
// Entry cost per field is sizeof(lit): a length byte plus the string
#ifdef ESPHOME_PROJECT_NAME
  static constexpr size_t INFO_ENTRIES_LEN =
      sizeof(ESPHOME_PROJECT_NAME) + sizeof(ESPHOME_PROJECT_VERSION) + sizeof(ESPHOME_VARIANT);
#else
  static constexpr size_t INFO_ENTRIES_LEN = sizeof("ESPHome") + sizeof(ESPHOME_VERSION) + sizeof(ESPHOME_VARIANT);
#endif
  static_assert(INFO_ENTRIES_LEN < MAX_SERIAL_PAYLOAD,
                "esphome project name and version too long for the improv_serial device info frame");
  std::array<uint8_t, improv::RPC_RESPONSE_MAX_SIZE> buf;
  improv::RpcResponseBuilder builder(buf, improv::GET_DEVICE_INFO);
#ifdef USE_ESP8266
  // Keep each literal in flash and copy it through an exact size stack buffer,
  // so a long project name or version can never be truncated
#define IMPROV_ADD_INFO(lit) \
  do { \
    static const char progmem_str[] PROGMEM = lit; \
    char tmp[sizeof(lit)]; \
    progmem_memcpy(tmp, progmem_str, sizeof(lit)); \
    builder.add_string(tmp, sizeof(lit) - 1); \
  } while (0)
#else
  // Literals are directly flash mapped on all other platforms
#define IMPROV_ADD_INFO(lit) builder.add_string(lit, sizeof(lit) - 1)
#endif
#ifdef ESPHOME_PROJECT_NAME
  IMPROV_ADD_INFO(ESPHOME_PROJECT_NAME);
  IMPROV_ADD_INFO(ESPHOME_PROJECT_VERSION);
#else
  IMPROV_ADD_INFO("ESPHome");
  IMPROV_ADD_INFO(ESPHOME_VERSION);
#endif
  IMPROV_ADD_INFO(ESPHOME_VARIANT);
#undef IMPROV_ADD_INFO
  // Only the device name length is unknown at compile time
  const auto &name = App.get_name();
  if (INFO_ENTRIES_LEN + 1 + name.size() <= MAX_SERIAL_PAYLOAD) {
    builder.add_string(name.c_str(), name.size());
  } else {
    ESP_LOGW(TAG, "Response full; device name dropped");
  }
  this->send_response_(builder.finish(false));
}

bool ImprovSerialComponent::parse_improv_serial_byte_(uint8_t byte) {
  const size_t at = this->rx_len_;
  ESP_LOGV(TAG, "Byte: 0x%02X", byte);
  if (at == 0) {
    // The parser's own first test; a stray byte then costs no heap block
    if (byte != 'I')
      return false;
    this->rx_buffer_ = std::make_unique<uint8_t[]>(MAX_FRAME_SIZE);
  } else if (at > FRAME_HEADER_SIZE && at > FRAME_HEADER_SIZE + this->rx_buffer_[FRAME_HEADER_SIZE - 1]) {
    // Past the checksum, where the parser rejects the byte without reading it
    return false;
  }
  this->rx_buffer_[at] = byte;
  this->rx_len_ = at + 1;

  return improv::parse_improv_serial_byte(
      at, byte, this->rx_buffer_.get(),
      [this](improv::ImprovCommand command) -> bool { return this->parse_improv_payload_(command); },
      [this](improv::Error error) -> void {
        ESP_LOGW(TAG, "Error decoding payload");
        this->send_state_byte_(improv::TYPE_ERROR_STATE, error);
      });
}

bool ImprovSerialComponent::parse_improv_payload_(improv::ImprovCommand &command) {
#ifdef USE_WIFI
  auto *wifi = wifi::global_wifi_component;
#endif
  switch (command.command) {
    case improv::WIFI_SETTINGS: {
#ifdef USE_WIFI
      if (wifi == nullptr || wifi->is_disabled()) {
        // Wi-Fi is disabled, so we can't provision. Respond immediately
        // instead of letting the client wait out its provisioning timeout.
        ESP_LOGW(TAG, "Wi-Fi is disabled; cannot provision");
        this->send_state_byte_(improv::TYPE_ERROR_STATE, improv::ERROR_UNABLE_TO_CONNECT);
        return true;
      }
      this->connecting_sta_ = std::make_unique<wifi::WiFiAP>();
      auto &sta = *this->connecting_sta_;
      sta.set_ssid(command.ssid.c_str());
      sta.set_password(command.password.c_str());

      // Sampled before start_connecting(): the old connection drops asynchronously after it.
      const bool switching = wifi->is_connected();
      wifi->set_sta(sta);
      wifi->start_connecting(sta);
      this->set_state_(improv::STATE_PROVISIONING);
      ESP_LOGD(TAG, "Received settings: SSID=%s, password=" LOG_SECRET("%s"), command.ssid.c_str(),
               command.password.c_str());

      this->set_timeout(WIFI_CONNECT_TIMEOUT_ID, switching ? WIFI_SWITCH_TIMEOUT_MS : WIFI_CONNECT_TIMEOUT_MS,
                        [this]() { this->on_wifi_connect_timeout_(); });
#else
      // No Wi-Fi support compiled in; there is nothing to provision.
      ESP_LOGW(TAG, "Wi-Fi not supported; cannot provision");
      this->send_state_byte_(improv::TYPE_ERROR_STATE, improv::ERROR_UNABLE_TO_CONNECT);
#endif
      return true;
    }
    case improv::GET_CURRENT_STATE: {
      // This state machine tracks Wi-Fi provisioning only. When Wi-Fi is disabled or not
      // compiled in, provisioning is unavailable -> report STOPPED so the client doesn't
      // offer a Wi-Fi form. General connectivity (e.g. Ethernet) is reported separately
      // via GET_NETWORK_STATE.
#ifdef USE_WIFI
      if (wifi == nullptr || wifi->is_disabled()) {
        // Reported transiently without disturbing our internal provisioning state machine,
        // so a later `wifi.enable` still reports the correct state.
        this->send_state_byte_(improv::TYPE_CURRENT_STATE, improv::STATE_STOPPED);
        return true;
      }
      this->set_state_(this->state_);
      if (this->state_ == improv::STATE_PROVISIONED) {
        this->send_settings_response_(improv::GET_CURRENT_STATE);
      }
#else
      this->send_state_byte_(improv::TYPE_CURRENT_STATE, improv::STATE_STOPPED);
#endif
      return true;
    }
    case improv::GET_DEVICE_INFO: {
      this->send_version_info_();
      return true;
    }
    case improv::GET_WIFI_NETWORKS: {
      // Declared out here because the terminating empty response is sent with or without Wi-Fi
      std::array<uint8_t, improv::RPC_RESPONSE_MAX_SIZE> buf;
#ifdef USE_WIFI
      const auto &results = wifi->get_scan_result();
      for (const auto &scan : results) {
        bool with_auth = false;
        if (!wifi::should_show_scan_entry(results, scan, with_auth))
          continue;
        // Send each ssid separately to avoid overflowing the buffer
        char rssi_buf[5];  // int8_t: -128 to 127, max 4 chars + null
        char *rssi_end = int8_to_str(rssi_buf, scan.get_rssi());
        *rssi_end = '\0';
        improv::RpcResponseBuilder builder(buf, improv::GET_WIFI_NETWORKS);
        // SSID(32) + RSSI(4) + YESNO(3) entries always fit the payload
        const auto &ssid = scan.get_ssid();
        builder.add_string(ssid.c_str(), ssid.size());
        builder.add_string(rssi_buf, rssi_end - rssi_buf);
        builder.add_string(YESNO(with_auth));
        this->send_response_(builder.finish(false));
      }
#endif  // USE_WIFI
      // Send empty response to signify the end of the list.
      improv::RpcResponseBuilder builder(buf, improv::GET_WIFI_NETWORKS);
      this->send_response_(builder.finish(false));
      return true;
    }
    case improv::GET_NETWORK_STATE: {
      // Reports general device connectivity and which network interfaces are present, decoupled
      // from the Wi-Fi-only provisioning state machine. data[0] is a decimal flags byte;
      // when online, the reachable device URL(s) follow.
      uint8_t flags = 0;
      if (network::is_connected())
        flags |= improv::NETWORK_IS_ONLINE;
#ifdef USE_WIFI
      flags |= improv::NETWORK_SUPPORTS_WIFI;
#endif
#ifdef USE_ETHERNET
      flags |= improv::NETWORK_SUPPORTS_ETHERNET;
#endif
#ifdef USE_OPENTHREAD
      flags |= improv::NETWORK_SUPPORTS_THREAD;
#endif
#ifdef USE_MODEM
      flags |= improv::NETWORK_SUPPORTS_MODEM;
#endif
      std::array<uint8_t, improv::RPC_RESPONSE_MAX_SIZE> buf;
      improv::RpcResponseBuilder builder(buf, improv::GET_NETWORK_STATE);
      // Every flag bit fits int8_t's positive range, so int8_to_str renders the byte
      static_assert(improv::NETWORK_SUPPORTS_MODEM <= 0x7F, "network flags no longer fit int8_to_str");
      char flags_buf[4];  // uint8_t: max "255" + null
      char *flags_end = int8_to_str(flags_buf, static_cast<int8_t>(flags));
      builder.add_string(flags_buf, flags_end - flags_buf);
#ifdef USE_WEBSERVER
      // Not tied to one interface, so follow the configured priority the way
      // network::get_ip_addresses() does: a wifi-first network priority list leads with Wi-Fi.
      if (flags & improv::NETWORK_IS_ONLINE)
        this->add_webserver_urls_(builder, WIFI_URL_FIRST);
#endif
      this->send_response_(builder.finish(false));
      return true;
    }
    default: {
      ESP_LOGW(TAG, "Unknown payload");
      this->send_state_byte_(improv::TYPE_ERROR_STATE, improv::ERROR_UNKNOWN_RPC);
      return false;
    }
  }
}

void ImprovSerialComponent::set_state_(improv::State state) {
  this->state_ = state;
  this->send_state_byte_(improv::TYPE_CURRENT_STATE, state);
}

void ImprovSerialComponent::send_state_byte_(improv::ImprovSerialType type, uint8_t value) {
  uint8_t frame[FRAME_HEADER_SIZE + 3];
  const uint8_t checksum = write_header(frame, type, 1) + value;
  frame[FRAME_HEADER_SIZE] = value;
  frame[FRAME_HEADER_SIZE + 1] = checksum;
  frame[FRAME_HEADER_SIZE + 2] = '\n';
  this->write_(frame, sizeof(frame));
}

void ImprovSerialComponent::send_response_(std::span<const uint8_t> response) {
  // The serial frame length field is a single byte
  if (response.size() > MAX_SERIAL_RESPONSE) {
    ESP_LOGE(TAG, "Response too long");
    // Fail fast instead of leaving the client to wait out its timeout
    this->send_state_byte_(improv::TYPE_ERROR_STATE, improv::ERROR_UNKNOWN);
    return;
  }
  uint8_t header[FRAME_HEADER_SIZE];
  const uint8_t footer[2] = {
      sum_bytes(response.data(), response.size(), write_header(header, improv::TYPE_RPC_RESPONSE, response.size())),
      '\n'};
  this->write_(header, sizeof(header));
  this->write_(response.data(), response.size());
  this->write_(footer, sizeof(footer));
}

#ifdef USE_WIFI
void ImprovSerialComponent::on_wifi_connect_timeout_() {
  this->connecting_sta_.reset();
  this->send_state_byte_(improv::TYPE_ERROR_STATE, improv::ERROR_UNABLE_TO_CONNECT);
  this->set_state_(improv::STATE_AUTHORIZED);
  ESP_LOGW(TAG, "Timed out while connecting to Wi-Fi network");
  wifi::global_wifi_component->clear_sta();
}
#endif

ImprovSerialComponent *global_improv_serial_component =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    nullptr;                                             // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::improv_serial

#endif
