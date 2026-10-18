#pragma once

#include "esphome/core/defines.h"
#ifdef USE_OTA
#include "esphome/components/ota/ota_backend_factory.h"
#include "esphome/components/socket/socket.h"
#ifdef USE_OTA_ENCRYPTION
#include "esphome/components/noise/noise_handshake.h"
#endif
#ifdef USE_OTA_DEFLATE
#include "ota_esphome_inflate.h"
#endif
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hash_base.h"

// UDP data phase: raw lwIP needs a main loop lock (or a single thread); the host polls a socket instead
#ifdef USE_ESP32
#include <sdkconfig.h>
#endif
#if defined(USE_ESP8266) || defined(USE_RP2) || defined(USE_LIBRETINY) || \
    (defined(USE_ESP32) && defined(CONFIG_LWIP_TCPIP_CORE_LOCKING))
#define ESPHOME_OTA_UDP
#define ESPHOME_OTA_UDP_LWIP
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#if defined(USE_LIBRETINY) && !LWIP_TCPIP_CORE_LOCKING
#undef ESPHOME_OTA_UDP
#undef ESPHOME_OTA_UDP_LWIP
#endif
#elif defined(USE_HOST)
#define ESPHOME_OTA_UDP
#endif

namespace esphome {

/// ESPHomeOTAComponent provides a simple way to integrate Over-the-Air updates into your app using ArduinoOTA.
class ESPHomeOTAComponent final : public ota::OTAComponent {
 public:
  enum class OTAState : uint8_t {
    IDLE,
    MAGIC_READ,    // Reading magic bytes
    MAGIC_ACK,     // Sending OK and version after magic bytes
    FEATURE_READ,  // Reading feature flags from client
    FEATURE_ACK,   // Sending feature acknowledgment
#ifdef USE_OTA_PASSWORD
    AUTH_SEND,  // Sending authentication request
    AUTH_READ,  // Reading authentication data
#endif          // USE_OTA_PASSWORD
#ifdef USE_OTA_ENCRYPTION
    NOISE_HANDSHAKE,  // Exchanging Noise handshake frames
#endif
    DATA,  // BLOCKING! Processing OTA data (update, etc.)
  };
#ifdef USE_OTA_PASSWORD
  void set_auth_password(const std::string &password) { password_ = password; }
#else
  // Stub so lambdas referencing set_auth_password() produce a clear error instead of
  // a cryptic "no member" diagnostic. Only fires if the stub is actually instantiated.
  template<bool B = false> void set_auth_password(const std::string &) {
    static_assert(B, "set_auth_password() requires the OTA auth path to be compiled. "
                     "Add 'password: \"\"' (empty string) to your 'ota: - platform: esphome' "
                     "config to enable runtime password rotation.");
  }
#endif  // USE_OTA_PASSWORD

#ifdef USE_OTA_ENCRYPTION
  /// psk points at 32 bytes that live in flash for the life of the program
  void set_noise_psk(const uint8_t *psk) { this->noise_ctx_.set_psk(psk); }
#endif

  /// Manually set the port OTA should listen on
  void set_port(uint16_t port) { this->port_ = port; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

  uint16_t get_port() const { return this->port_; }

 protected:
  void handle_handshake_();
  void handle_data_();

#ifdef ESPHOME_OTA_UDP
  // UDP transport for the data phase; wire format in ota_esphome_udp.cpp
  static constexpr size_t UDP_OUT_SIZE = 64;  // our unacked responses: at most three encrypted ones
  // Messages the device keeps unread: like TCP's 4 segment window, so no more memory than TCP holds
  static constexpr uint8_t UDP_WINDOW = 4;
#ifdef ESPHOME_OTA_UDP_LWIP
  using UdpSlot = struct pbuf *;
#else
  using UdpSlot = struct OtaUdpDatagram *;
#endif
  // Lives on handle_data_'s stack; the receive callback shares it under the lwIP lock
  struct UdpLink {
    ~UdpLink();
    void release();  // closes the socket and frees what is queued
#ifdef ESPHOME_OTA_UDP_LWIP
    struct udp_pcb *pcb{nullptr};
    ip_addr_t peer_ip;
#else
    std::unique_ptr<socket::Socket> sock;
    struct sockaddr_storage peer;  // the TCP peer; its port is replaced by peer_port
    socklen_t peer_len{0};
#endif
    uint16_t peer_port{0};  // learned from the client's datagrams
    // The session token, our answer to the client: top bit clear, so never an error code; all zero declines
    uint8_t reply[4]{};
    UdpSlot queue[UDP_WINDOW]{};  // slot seq % UDP_WINDOW for seq in [consumed, consumed + UDP_WINDOW)
    uint16_t received{0};         // every seq before it is here
    uint16_t consumed{0};         // the message being read
    uint16_t read_offset{0};      // payload bytes of it already read
    uint16_t out_base{0};         // stream offset of out[0]
    uint8_t out_len{0};
    bool committed{false};  // the client heard us over UDP
    uint8_t out[UDP_OUT_SIZE];
  };
  // Fills link.reply on success; leaves it all zero (declined) otherwise
  void udp_open_(UdpLink &link);
  // Waits for the client to commit to UDP or fall back to TCP; false on failure
  bool udp_start_();
  // Like a blocking socket read with SO_RCVTIMEO: up to len bytes, or -1 with EWOULDBLOCK
  ssize_t udp_read_(uint8_t *buf, size_t len);
  bool udp_send_(const uint8_t *data, size_t len);
  // In a UDP session, keeps answering until the client has our responses (an error code), briefly
  void udp_linger_();
  // A datagram from the client's IP; caller holds the lwIP lock (or runs in the receive callback)
  static void udp_on_datagram(UdpLink &link, UdpSlot p, uint16_t port);
  // Caller holds the lwIP lock (or runs in the receive callback)
  static void udp_send_ack(UdpLink &link, uint16_t prompted_by);
#ifdef ESPHOME_OTA_UDP_LWIP
  static void udp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port);
  void udp_poll_() {}
#else
  void udp_poll_();  // hands what the socket received to udp_on_datagram
#endif
#endif
  // Data phase only (udp_ shares storage with the handshake's connect time): TCP or, once committed, the UDP stream
  inline ssize_t transport_read_(uint8_t *buf, size_t len) {
#ifdef ESPHOME_OTA_UDP
    if (this->udp_ != nullptr)
      return this->udp_read_(buf, len);
#endif
    return this->client_->read(buf, len);
  }
#ifdef USE_OTA_PASSWORD
  static constexpr size_t SHA256_HEX_SIZE = 64;  // SHA256 hash as hex string (32 bytes * 2)
  bool handle_auth_send_();
  bool handle_auth_read_();
  bool select_auth_type_();
  void cleanup_auth_();
  void log_auth_warning_(const LogString *msg);
#endif  // USE_OTA_PASSWORD
  bool readall_(uint8_t *buf, size_t len);
  bool writeall_(const uint8_t *buf, size_t len);

#ifdef USE_OTA_ENCRYPTION
  // Heap-allocated only while an encrypted OTA session is active.
  struct NoiseSession {
    ~NoiseSession();
    noise::NoiseResponderHandshake handshake;
    NoiseCipherState *send_cipher{nullptr};
    NoiseCipherState *recv_cipher{nullptr};
    uint16_t frame_len{0};  // total frame size once the header is parsed, 0 until then
    uint16_t frame_pos{0};  // bytes read or written so far
    bool writing{false};    // a produced handshake frame is still being flushed
    uint8_t frame_buf[noise::FRAME_HEADER_SIZE + 1 + noise::MAX_HANDSHAKE_SIZE];
  };
  // The api server's live context when it exists, otherwise our own (a build
  // time key, or the saved key loaded in safe mode)
  const noise::NoiseContext &noise_context_() const;
  // True once the feature ack offers noise and the client asked for it
  bool noise_offered_() const;
  void noise_reserve_session_();
  bool noise_start_session_(uint8_t server_feature_flags);
  bool handle_noise_handshake_();
  bool noise_try_read_frame_();
  size_t noise_frame_payload_len_(const uint8_t *header, size_t min_len, size_t max_len);
  bool noise_try_write_frame_();
  void noise_send_reject_(const LogString *reason);
  ssize_t noise_decrypt_(uint8_t *buf, size_t len);
  ssize_t noise_read_frame_blocking_(uint8_t *buf, size_t min_ciphertext, size_t max_ciphertext);
  bool noise_readall_(uint8_t *buf, size_t len);
  ssize_t noise_read_data_(uint8_t *buf, size_t capacity);
  static constexpr size_t NOISE_MAX_RESPONSE = 4;  // the UDP token
  bool noise_write_(const uint8_t *data, size_t len);
#endif  // USE_OTA_ENCRYPTION

  // Data-phase I/O dispatch: through the noise transport when a session is
  // active, straight to the socket otherwise.
  // len is at most NOISE_MAX_RESPONSE
  inline bool data_write_(const uint8_t *data, size_t len) {
#ifdef USE_OTA_ENCRYPTION
    if (this->noise_ != nullptr)
      return this->noise_write_(data, len);
#endif
    return this->writeall_(data, len);
  }
  inline bool data_write_byte_(uint8_t byte) { return this->data_write_(&byte, 1); }
  // When encrypted, buf must have room for len + noise::MAC_SIZE bytes.
  inline bool data_readall_(uint8_t *buf, size_t len) {
#ifdef USE_OTA_ENCRYPTION
    if (this->noise_ != nullptr)
      return this->noise_readall_(buf, len);
#endif
    return this->readall_(buf, len);
  }

  // Upload accounting shared by the data loop and the inflate read callback
  struct DataTransfer {
    size_t ota_size{0};  // bytes the client sends
    size_t total{0};     // bytes received so far
#if USE_OTA_VERSION == 2
    size_t acknowledged{0};
#endif
    uint32_t last_data_ms{0};
    uint32_t last_progress{0};
  };
  // Up to OTA_BUFFER_SIZE bytes into buf; returns bytes read, -1 on failure (logged)
  ssize_t receive_data_(uint8_t *buf, DataTransfer &xfer);
  // Raw lwIP cannot service the radio during a sector write, so the ack waits
  // for the write there; a socket task lets the next block arrive meanwhile
#ifdef USE_SOCKET_IMPL_LWIP_TCP
  static constexpr bool ACK_AFTER_WRITE = true;
#else
  static constexpr bool ACK_AFTER_WRITE = false;
#endif
  void send_chunk_acks_(DataTransfer &xfer);
  inline void ack_received_(DataTransfer &xfer) {
    if (!ACK_AFTER_WRITE)
      this->send_chunk_acks_(xfer);
  }
  inline void ack_written_(DataTransfer &xfer) {
    if (ACK_AFTER_WRITE)
      this->send_chunk_acks_(xfer);
  }
  inline bool read_size_(uint8_t *buf, size_t &size, const LogString *desc);
  // Writes to the backend and logs a failure
  ota::OTAResponseTypes write_flash_(uint8_t *data, size_t len);

  bool try_read_(size_t to_read, const LogString *desc);
  bool try_write_(size_t to_write, const LogString *desc);

  inline bool would_block_(int error_code) const { return error_code == EAGAIN || error_code == EWOULDBLOCK; }
  bool handle_read_error_(ssize_t read, const LogString *desc);
  bool handle_write_error_(ssize_t written, const LogString *desc);
  inline void transition_ota_state_(OTAState next_state) {
    this->ota_state_ = next_state;
    this->handshake_buf_pos_ = 0;  // Reset buffer position for next state
  }

  void server_failed_(const LogString *msg);
  void log_socket_error_(const LogString *msg);
  void log_read_error_(const LogString *what);
  bool client_left_before_start_();
  void log_start_(const LogString *phase);
  void log_remote_closed_(const LogString *during);
  void cleanup_connection_();
  inline void send_error_and_cleanup_(ota::OTAResponseTypes error) {
    uint8_t error_byte = static_cast<uint8_t>(error);
    this->client_->write(&error_byte, 1);  // Best effort, non-blocking
    this->cleanup_connection_();
  }
  void yield_and_feed_watchdog_();

#ifdef USE_OTA_PASSWORD
  std::string password_;
  RAMUniquePtr<uint8_t[]> auth_buf_;
#endif  // USE_OTA_PASSWORD
#ifdef USE_OTA_ENCRYPTION
  noise::NoiseContext noise_ctx_;
#ifdef USE_OTA_ENCRYPTION_PROVISIONED
  // Backs noise_ctx_ in safe mode, where no api server holds the saved key
  RAMUniquePtr<noise::psk_t> saved_psk_;
#endif
  RAMUniquePtr<NoiseSession> noise_;
#endif  // USE_OTA_ENCRYPTION

  socket::ListenSocket *server_{nullptr};
  std::unique_ptr<socket::Socket> client_;
  ota::OTABackendPtr backend_;

#ifdef ESPHOME_OTA_UDP_LWIP
  union {
    uint32_t client_connect_time_{0};  // handshake states
    UdpLink *udp_;                     // data phase (never both), points into handle_data_'s stack
  };
  static_assert(sizeof(UdpLink *) == sizeof(uint32_t), "clearing client_connect_time_ must clear udp_");
#else
  uint32_t client_connect_time_{0};
#ifdef ESPHOME_OTA_UDP
  UdpLink *udp_{nullptr};  // data phase, points into handle_data_'s stack
#endif
#endif
#if defined(ESPHOME_OTA_UDP) && defined(USE_OTA_ENCRYPTION)
  static_assert(sizeof(UdpLink::reply) <= NOISE_MAX_RESPONSE, "the token goes out through noise_write_");
#endif
  static constexpr size_t HANDSHAKE_BUF_SIZE = 5;
  // Buffer size for OTA data transfer. The upload client derives its maximum
  // encrypted frame plaintext from this (espota2.NOISE_MAX_PLAINTEXT is this
  // minus the 16-byte MAC); both must change together.
  static constexpr size_t OTA_BUFFER_SIZE = 1040;
#ifdef USE_OTA_ENCRYPTION
  // espota2.NOISE_MAX_PLAINTEXT; shrinking the buffer would reject every
  // frame a current CLI sends
  static constexpr size_t NOISE_CLIENT_MAX_PLAINTEXT = 1024;
  static_assert(OTA_BUFFER_SIZE >= NOISE_CLIENT_MAX_PLAINTEXT + noise::MAC_SIZE,
                "OTA_BUFFER_SIZE must fit a full encrypted data frame");
#endif
#ifdef USE_OTA_DEFLATE
  // At least 1 << espota2.DEFLATE_WINDOW_BITS; also the inflate output buffer
  static constexpr size_t OTA_INFLATE_WINDOW_SIZE = 4096;
  // Heap-allocated only while a deflate upload is negotiated; the decoder
  // state is the base so the read callback can recover the session
  struct InflateSession : OtaInflateState {
    // The session outlives the upload it serves, but these three are borrowed
    // from inflate_data_'s caller and dangle once that call returns; only that
    // call, and the flush and read callback it drives, may read them
    ESPHomeOTAComponent *self;
    DataTransfer *xfer;
    uint8_t *in;  // caller's buffer for the compressed input
    size_t image_size;
    size_t written;               // inflated bytes in flash
    size_t flushed;               // bytes of the current window already in flash
    ota::OTAResponseTypes error;  // first failure inside the read callback
    uint8_t window[OTA_INFLATE_WINDOW_SIZE];
  };
#ifndef CLANG_TIDY  // static analysis sets every define at once
  static_assert(!ota::OTABackend::supports_compression(),
                "USE_OTA_DEFLATE is for backends that cannot store a gzip image");
#endif
  // Writes the decoded bytes not yet in flash without moving dest
  ota::OTAResponseTypes inflate_flush_(InflateSession &session);
  ota::OTAResponseTypes inflate_data_(uint8_t *in, size_t image_size, DataTransfer &xfer);
  RAMUniquePtr<InflateSession> inflate_;
#endif

  static constexpr uint8_t MAGIC_BYTES[5] = {0x6C, 0x26, 0xF7, 0x5C, 0x45};
  // Derived from the feature byte; storing it would pad the trailing bytes
  bool extended_proto_() const;
#ifdef USE_OTA_PARTITIONS
  uint32_t running_app_offset_{0};
  size_t running_app_size_{0};
#endif
  uint16_t port_;
  uint8_t handshake_buf_[HANDSHAKE_BUF_SIZE];
  OTAState ota_state_{OTAState::IDLE};
  uint8_t handshake_buf_pos_{0};
  uint8_t ota_features_{0};
  bool remote_closed_{false};  // the peer hung up cleanly during a blocking read
#ifdef USE_OTA_PASSWORD
  uint8_t auth_buf_pos_{0};
  uint8_t auth_type_{0};  // Store auth type to know which hasher to use
#endif                    // USE_OTA_PASSWORD
};

}  // namespace esphome
#endif
