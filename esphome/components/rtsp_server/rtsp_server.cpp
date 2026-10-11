#include "esphome/core/defines.h"
#ifdef USE_RTSP_SERVER

#include "rtsp_server.h"
#include "rtsp_protocol.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <esp_random.h>
#include <esp_tls_crypto.h>
#include <netinet/tcp.h>
#include <utility>

namespace esphome::rtsp_server {

static const char *const TAG = "rtsp_server";
static constexpr uint8_t RTP_HEADER_BYTES = 12;
static constexpr uint8_t INTERLEAVED_HEADER_BYTES = 4;
static constexpr uint8_t TCP_RTP_HEADER_BYTES = RTP_HEADER_BYTES + INTERLEAVED_HEADER_BYTES;
static constexpr uint8_t RTP_VERSION_2 = 0x80;
static constexpr uint8_t RTP_MARKER_BIT = 0x80;
static constexpr uint8_t RTP_CHANNEL = 0;
static constexpr uint8_t LISTEN_BACKLOG = RTSP_MAX_CLIENTS;
static constexpr size_t RESPONSE_BYTES = 768;
static constexpr size_t SDP_BYTES = 768;
static constexpr const char *BASIC_AUTH_HEADER = "Authorization: Basic ";
static constexpr suseconds_t SOCKET_SEND_TIMEOUT_US = 100000;
static constexpr uint32_t PERIODIC_FRAME_LOG_INTERVAL = 100;
static constexpr uint32_t TASK_STACK_WORDS = (12288U + sizeof(StackType_t) - 1U) / sizeof(StackType_t);
static constexpr UBaseType_t TASK_PRIORITY = 4;

static size_t encode_base64_to(char *output, size_t output_size, const uint8_t *input, size_t input_size) {
  if (output == nullptr || output_size == 0 || input == nullptr || input_size == 0) {
    return 0;
  }
  size_t encoded_length = 0;
  if (esp_crypto_base64_encode(reinterpret_cast<uint8_t *>(output), output_size, &encoded_length, input, input_size) !=
          0 ||
      encoded_length >= output_size) {
    output[0] = '\0';
    return 0;
  }
  output[encoded_length] = '\0';
  return encoded_length;
}

static bool send_iov_all(socket::Socket &connection, iovec *segments, int count) {
  while (count > 0) {
    ssize_t written = connection.writev(segments, count);
    if (written <= 0) {
      return false;
    }
    while (written > 0 && count > 0) {
      if (static_cast<size_t>(written) >= segments[0].iov_len) {
        written -= static_cast<ssize_t>(segments[0].iov_len);
        segments++;
        count--;
      } else {
        segments[0].iov_base = static_cast<uint8_t *>(segments[0].iov_base) + written;
        segments[0].iov_len -= static_cast<size_t>(written);
        written = 0;
      }
    }
  }
  return true;
}

template<typename T> static void write_big_endian(uint8_t *output, T value) {
  const T converted = convert_big_endian(value);
  memcpy(output, &converted, sizeof(converted));
}

RtspServer::~RtspServer() {
  const bool was_streaming = this->streaming_.exchange(false, std::memory_order_acq_rel);
  const bool was_priming = this->priming_subscription_.exchange(false, std::memory_order_acq_rel);
  if ((was_streaming || was_priming) && this->track_ != nullptr) {
    this->track_->stop(this);
  }
  this->request_server_task_stop_();
  (void) this->finish_server_task_stop_(true);
}

void RtspServer::setup() {
  if (this->track_ == nullptr) {
    ESP_LOGE(TAG, "RTSP track is not configured");
    this->mark_failed();
    return;
  }
  if (!this->track_->is_ready()) {
    ESP_LOGI(TAG, "Waiting for %s track initialization", this->track_->get_codec_name());
  }

  this->ssrc_ = esp_random();
  if (this->ssrc_ == 0) {
    this->ssrc_ = 1;
  }
  // Subscribe before clients connect so codec metadata can be prepared for DESCRIBE.
  this->priming_subscription_.store(true, std::memory_order_release);
  if (!this->track_->start(this)) {
    this->priming_subscription_.store(false, std::memory_order_release);
    ESP_LOGE(TAG, "RTSP track rejected initial subscription");
    this->mark_failed();
    return;
  }
  this->running_.store(true, std::memory_order_release);
  this->server_task_exited_.store(false, std::memory_order_release);
  if (!this->server_task_runner_.create(RtspServer::server_task_entry, "rtsp_srv", TASK_STACK_WORDS, this,
                                        TASK_PRIORITY, false)) {
    this->running_.store(false, std::memory_order_release);
    if (this->priming_subscription_.exchange(false, std::memory_order_acq_rel)) {
      this->track_->stop(this);
    }
    ESP_LOGE(TAG, "Failed to create RTSP server task");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "RTSP TCP on port %u clients=%zu payload=%u codec=%s auth=%s", this->port_, RTSP_MAX_CLIENTS,
           this->rtp_payload_size_, this->track_->get_codec_name(), YESNO(!this->auth_username_.empty()));
}

void RtspServer::dump_config() {
  ESP_LOGCONFIG(TAG, "RTSP Server: rtsp://<ip>:%u/stream", this->port_);
  ESP_LOGCONFIG(TAG, "  Codec: %s", this->track_ == nullptr ? "unconfigured" : this->track_->get_codec_name());
  ESP_LOGCONFIG(TAG, "  Transport: TCP interleaved");
  ESP_LOGCONFIG(TAG, "  Maximum clients: %zu", RTSP_MAX_CLIENTS);
  ESP_LOGCONFIG(TAG, "  RTP payload size: %u", this->rtp_payload_size_);
  ESP_LOGCONFIG(TAG, "  Authentication: %s", YESNO(!this->auth_username_.empty()));
}

void RtspServer::on_shutdown() {
  this->request_server_task_stop_();
  const bool was_streaming = this->streaming_.exchange(false, std::memory_order_acq_rel);
  const bool was_priming = this->priming_subscription_.exchange(false, std::memory_order_acq_rel);
  if ((was_streaming || was_priming) && this->track_ != nullptr) {
    this->track_->stop(this);
  }
  if (this->track_ != nullptr) {
    this->track_->purge();
  }
}

bool RtspServer::teardown() { return this->finish_server_task_stop_(false); }

void RtspServer::request_server_task_stop_() {
  this->running_.store(false, std::memory_order_release);
  this->notify_server_task_();
}

bool RtspServer::finish_server_task_stop_(bool force) {
  if (!this->server_task_runner_.is_created()) {
    return true;
  }
  if (!this->server_task_exited_.load(std::memory_order_acquire)) {
    if (!force) {
      return false;
    }
    ESP_LOGW(TAG, "Forcing RTSP server task deletion");
    this->server_task_runner_.destroy();
  }
  this->server_task_runner_.deallocate();
  this->server_task_exited_.store(false, std::memory_order_release);
  return true;
}

void RtspServer::notify_server_task_() {
  const TaskHandle_t handle = this->server_task_runner_.get_handle();
  if (handle != nullptr) {
    xTaskNotifyGive(handle);
  }
}

void RtspServer::server_task_entry(void *arg) {
  auto *self = static_cast<RtspServer *>(arg);
  self->server_task_();
  self->server_task_exited_.store(true, std::memory_order_release);
  self->enable_loop_soon_any_context();
  vTaskSuspend(nullptr);
}

std::unique_ptr<socket::ListenSocket> RtspServer::open_listen_socket_() {
  auto listen_socket = socket::socket_listen(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket == nullptr) {
    ESP_LOGE(TAG, "RTSP socket() failed: errno=%d", errno);
    return {};
  }
  int reuse = 1;
  (void) listen_socket->setsockopt(SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(ESPHOME_INADDR_ANY);
  address.sin_port = htons(this->port_);
  if (listen_socket->bind(reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
      listen_socket->listen(LISTEN_BACKLOG) != 0) {
    ESP_LOGE(TAG, "RTSP bind/listen failed: errno=%d", errno);
    return {};
  }
  return listen_socket;
}

void RtspServer::accept_client_(socket::ListenSocket &listen_socket) {
  auto connection = listen_socket.accept(nullptr, nullptr);
  if (connection == nullptr) {
    return;
  }
  int no_delay = 1;
  (void) connection->setsockopt(IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
  timeval send_timeout{0, SOCKET_SEND_TIMEOUT_US};
  (void) connection->setsockopt(SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));

  for (auto &client : this->clients_) {
    if (client.is_connected()) {
      continue;
    }
    client = {};
    client.connection = std::move(connection);
    client.session_id = esp_random();
    if (client.session_id == 0) {
      client.session_id = 1;
    }
    client.rtp_seq = static_cast<uint16_t>(esp_random());
    client.rtp_timestamp_offset = esp_random();
    ESP_LOGI(TAG, "RTSP client connected");
    return;
  }
  ESP_LOGW(TAG, "RTSP client rejected: maximum clients reached");
}

void RtspServer::read_client_(RtspClient &client) {
  if (client.rx_len >= sizeof(client.rx) - 1U) {
    this->close_client_(client);
    return;
  }
  const ssize_t received = client.connection->read(client.rx + client.rx_len, sizeof(client.rx) - client.rx_len - 1U);
  if (received <= 0) {
    this->close_client_(client);
    return;
  }
  client.rx_len += static_cast<size_t>(received);
  client.rx[client.rx_len] = '\0';

  while (client.rx_len >= INTERLEAVED_HEADER_BYTES && client.rx[0] == '$') {
    const uint16_t payload_length =
        encode_uint16(static_cast<uint8_t>(client.rx[2]), static_cast<uint8_t>(client.rx[3]));
    const size_t frame_length = INTERLEAVED_HEADER_BYTES + payload_length;
    if (client.rx_len < frame_length) {
      return;
    }
    memmove(client.rx, client.rx + frame_length, client.rx_len - frame_length);
    client.rx_len -= frame_length;
    client.rx[client.rx_len] = '\0';
  }

  while (client.is_connected()) {
    const size_t request_length = rtsp_request_length(client.rx, client.rx_len);
    if (request_length == 0) {
      break;
    }
    const size_t remaining = client.rx_len - request_length;
    const char saved = client.rx[request_length];
    client.rx[request_length] = '\0';
    const size_t original_length = client.rx_len;
    client.rx_len = request_length;
    this->handle_request_(client);
    client.rx_len = original_length;
    client.rx[request_length] = saved;
    if (!client.is_connected()) {
      return;
    }
    memmove(client.rx, client.rx + request_length, remaining);
    client.rx_len = remaining;
    client.rx[remaining] = '\0';
  }
}

void RtspServer::server_task_() {
  while (this->running_.load(std::memory_order_acquire) && !network::is_connected()) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  if (!this->running_.load(std::memory_order_acquire)) {
    return;
  }

  auto listen_socket = this->open_listen_socket_();
  if (listen_socket == nullptr) {
    if (this->priming_subscription_.exchange(false, std::memory_order_acq_rel)) {
      this->track_->stop(this);
    }
    return;
  }
  ESP_LOGI(TAG, "Listening rtsp://<ip>:%u/stream", this->port_);

  while (this->running_.load(std::memory_order_acquire)) {
    fd_set fds;
    FD_ZERO(&fds);
    const int listen_fd = listen_socket->get_fd();
    FD_SET(listen_fd, &fds);
    int maximum = listen_fd;
    for (const auto &client : this->clients_) {
      const int client_fd = client.fd();
      if (client_fd >= 0) {
        FD_SET(client_fd, &fds);
        maximum = clamp_at_least(maximum, client_fd);
      }
    }
    timeval timeout = this->streaming_.load(std::memory_order_acquire) ? timeval{0, 0} : timeval{1, 0};
    const int ready = select(maximum + 1, &fds, nullptr, nullptr, &timeout);
    if (ready > 0) {
      if (FD_ISSET(listen_fd, &fds)) {
        this->accept_client_(*listen_socket);
      }
      for (auto &client : this->clients_) {
        const int client_fd = client.fd();
        if (client_fd >= 0 && FD_ISSET(client_fd, &fds)) {
          this->read_client_(client);
        }
      }
    }
    if (this->priming_subscription_.load(std::memory_order_acquire) &&
        !this->streaming_.load(std::memory_order_acquire) && this->track_->is_sdp_ready() &&
        this->priming_subscription_.exchange(false, std::memory_order_acq_rel)) {
      this->track_->stop(this);
      ESP_LOGI(TAG, "%s track metadata ready; RTSP DESCRIBE ready", this->track_->get_codec_name());
    }
    if (this->streaming_.load(std::memory_order_acquire)) {
      const uint32_t period = this->track_->get_frame_period_ms();
      if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(period == 0 ? 1U : period)) != 0) {
        (void) this->track_->send_next_access_unit(*this);
      }
    }
  }

  for (auto &client : this->clients_) {
    this->close_client_(client);
  }
}

void RtspServer::send_response_(socket::Socket *connection, const char *format, ...) {
  char response[RESPONSE_BYTES];
  va_list args;
  va_start(args, format);
  const int length = vsnprintf(response, sizeof(response), format, args);
  va_end(args);
  if (length <= 0 || static_cast<size_t>(length) >= sizeof(response)) {
    ESP_LOGE(TAG, "RTSP response exceeds %zu-byte buffer", sizeof(response));
    return;
  }
  if (connection == nullptr) {
    return;
  }
  iovec segment{response, static_cast<size_t>(length)};
  if (!send_iov_all(*connection, &segment, 1)) {
    ESP_LOGD(TAG, "RTSP response send failed: errno=%d", errno);
  }
}

bool RtspServer::check_auth_(const char *request) const {
  if (this->auth_username_.empty()) {
    return true;
  }
  if (request == nullptr) {
    return false;
  }
  const size_t request_length = strlen(request);
  const char *credentials = find_case_insensitive(request, request_length, BASIC_AUTH_HEADER);
  if (credentials == nullptr) {
    return false;
  }
  credentials += strlen(BASIC_AUTH_HEADER);
  const size_t credential_length = strcspn(credentials, " \r\n");

  // Match ESPHome's web-server convention: retain the configured username and
  // password in C++, and derive the Basic token only while authenticating.
  constexpr size_t user_info_bytes = 256;
  char user_info[user_info_bytes];
  const size_t username_length = this->auth_username_.size();
  const size_t password_length = this->auth_password_.size();
  const size_t user_info_length = username_length + 1 + password_length;
  if (user_info_length >= sizeof(user_info)) {
    return false;
  }
  memcpy(user_info, this->auth_username_.data(), username_length);
  user_info[username_length] = ':';
  memcpy(user_info + username_length + 1, this->auth_password_.data(), password_length);
  user_info[user_info_length] = '\0';

  constexpr size_t auth_token_bytes = 4U * ((user_info_bytes + 2U) / 3U) + 1U;
  char expected[auth_token_bytes];
  const size_t expected_length =
      encode_base64_to(expected, sizeof(expected), reinterpret_cast<const uint8_t *>(user_info), user_info_length);
  if (expected_length == 0) {
    memset(user_info, 0, sizeof(user_info));
    memset(expected, 0, sizeof(expected));
    return false;
  }
  const bool matches = secure_token_equal(credentials, credential_length, expected, expected_length);
  memset(user_info, 0, sizeof(user_info));
  memset(expected, 0, sizeof(expected));
  return matches;
}

void RtspServer::send_unauthorized_(RtspClient &client, int cseq) {
  this->send_response_(client.connection.get(),
                       "RTSP/1.0 401 Unauthorized\r\nCSeq: %d\r\n"
                       "WWW-Authenticate: Basic realm=\"esp32\"\r\n\r\n",
                       cseq);
}

void RtspServer::handle_request_(RtspClient &client) {
  const RtspRequestView request = parse_rtsp_request(client.rx, client.rx_len);
  switch (request.method) {
    case RtspMethod::RTSP_METHOD_OPTIONS:
      this->handle_options_(client, request.cseq);
      break;
    case RtspMethod::RTSP_METHOD_DESCRIBE:
      this->handle_describe_(client, request.cseq);
      break;
    case RtspMethod::RTSP_METHOD_SETUP:
      this->handle_setup_(client, request.cseq, client.rx);
      break;
    case RtspMethod::RTSP_METHOD_PLAY:
      this->handle_play_(client, request.cseq);
      break;
    case RtspMethod::RTSP_METHOD_TEARDOWN:
      this->handle_teardown_(client, request.cseq);
      break;
    case RtspMethod::RTSP_METHOD_UNKNOWN:
      this->send_response_(client.connection.get(), "RTSP/1.0 405 Method Not Allowed\r\nCSeq: %d\r\n\r\n",
                           request.cseq);
      break;
  }
}

void RtspServer::handle_options_(RtspClient &client, int cseq) {
  this->send_response_(client.connection.get(),
                       "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
                       "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n\r\n",
                       cseq);
}

void RtspServer::handle_describe_(RtspClient &client, int cseq) {
  if (!this->check_auth_(client.rx)) {
    this->send_unauthorized_(client, cseq);
    return;
  }
  if (!this->track_->is_sdp_ready()) {
    this->send_response_(client.connection.get(),
                         "RTSP/1.0 503 Service Unavailable\r\nCSeq: %d\r\nRetry-After: 1\r\n\r\n", cseq);
    return;
  }

  char sdp[SDP_BYTES];
  const int session_length = snprintf(sdp, sizeof(sdp),
                                      "v=0\r\n"
                                      "o=- 0 0 IN IP4 0.0.0.0\r\n"
                                      "s=ESPHome RTSP\r\n"
                                      "t=0 0\r\n"
                                      "a=control:*\r\n");
  if (session_length <= 0 || static_cast<size_t>(session_length) >= sizeof(sdp) ||
      !this->track_->write_sdp(sdp + session_length, sizeof(sdp) - static_cast<size_t>(session_length), "streamid=0")) {
    this->send_response_(client.connection.get(), "RTSP/1.0 500 Internal Server Error\r\nCSeq: %d\r\n\r\n", cseq);
    return;
  }
  const size_t sdp_length = strlen(sdp);
  this->send_response_(client.connection.get(),
                       "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
                       "Content-Type: application/sdp\r\nContent-Length: %zu\r\n\r\n%s",
                       cseq, sdp_length, sdp);
}

void RtspServer::handle_setup_(RtspClient &client, int cseq, const char *request) {
  if (!this->check_auth_(request)) {
    this->send_unauthorized_(client, cseq);
    return;
  }
  if (!rtsp_transport_requests_tcp(request, strlen(request))) {
    this->send_response_(client.connection.get(), "RTSP/1.0 461 Unsupported Transport\r\nCSeq: %d\r\n\r\n", cseq);
    return;
  }
  client.state = RtspState::RTSP_STATE_READY;
  this->send_response_(client.connection.get(),
                       "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
                       "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                       "Session: %08" PRIX32 "\r\n\r\n",
                       cseq, client.session_id);
}

void RtspServer::handle_play_(RtspClient &client, int cseq) {
  if (client.state != RtspState::RTSP_STATE_READY && client.state != RtspState::RTSP_STATE_PLAYING) {
    this->send_response_(client.connection.get(), "RTSP/1.0 455 Method Not Valid\r\nCSeq: %d\r\n\r\n", cseq);
    return;
  }

  if (!this->streaming_.load(std::memory_order_acquire)) {
    const bool already_subscribed = this->priming_subscription_.exchange(false, std::memory_order_acq_rel);
    if (!already_subscribed && !this->track_->start(this)) {
      ESP_LOGE(TAG, "RTSP track rejected PLAY subscription");
      this->send_response_(client.connection.get(), "RTSP/1.0 503 Service Unavailable\r\nCSeq: %d\r\n\r\n", cseq);
      return;
    }
    this->streaming_.store(true, std::memory_order_release);
  }

  this->track_->purge();
  client.state = RtspState::RTSP_STATE_PLAYING;
  client.needs_keyframe = true;
  this->track_->request_keyframe();
  this->send_response_(client.connection.get(),
                       "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
                       "Session: %08" PRIX32 "\r\nRange: npt=0.000-\r\n\r\n",
                       cseq, client.session_id);
  ESP_LOGI(TAG, "Client PLAYING; keyframe requested");
}

void RtspServer::handle_teardown_(RtspClient &client, int cseq) {
  this->send_response_(client.connection.get(), "RTSP/1.0 200 OK\r\nCSeq: %d\r\n\r\n", cseq);
  this->close_client_(client);
}

void RtspServer::close_client_(RtspClient &client) {
  client = {};

  bool any_playing = false;
  for (const auto &other : this->clients_) {
    any_playing |= other.is_connected() && other.state == RtspState::RTSP_STATE_PLAYING;
  }
  if (!any_playing && this->streaming_.exchange(false, std::memory_order_acq_rel)) {
    this->track_->stop(this);
    this->track_->purge();
  }
}

void RtspServer::on_rtsp_track_frame_available() { this->notify_server_task_(); }

bool RtspServer::begin_access_unit(uint32_t timestamp, bool keyframe, size_t data_length) {
  this->current_client_mask_ = 0;
  this->current_bootstrap_mask_ = 0;
  this->current_timestamp_ = timestamp;
  this->current_data_length_ = data_length;
  this->current_keyframe_ = keyframe;
  this->current_media_sent_ = false;

  for (size_t index = 0; index < RTSP_MAX_CLIENTS; index++) {
    const auto &client = this->clients_[index];
    if (!client.is_connected() || client.state != RtspState::RTSP_STATE_PLAYING ||
        (client.needs_keyframe && !keyframe)) {
      continue;
    }
    const uint8_t mask = static_cast<uint8_t>(1U << index);
    this->current_client_mask_ |= mask;
    if (keyframe && client.needs_keyframe) {
      this->current_bootstrap_mask_ |= mask;
    }
  }
  return this->current_client_mask_ != 0;
}

bool RtspServer::send_packet(const RtpPacketView &packet) {
  uint8_t target_mask = packet.startup_config ? this->current_bootstrap_mask_ : this->current_client_mask_;
  if (target_mask == 0) {
    return this->current_client_mask_ != 0;
  }

  for (size_t index = 0; index < RTSP_MAX_CLIENTS; index++) {
    const uint8_t mask = static_cast<uint8_t>(1U << index);
    if ((target_mask & mask) == 0) {
      continue;
    }
    auto &client = this->clients_[index];
    if (!this->send_rtp_packet_(client, packet, this->current_timestamp_ + client.rtp_timestamp_offset)) {
      this->current_client_mask_ &= static_cast<uint8_t>(~mask);
      this->current_bootstrap_mask_ &= static_cast<uint8_t>(~mask);
      continue;
    }
    if (!packet.startup_config) {
      this->current_media_sent_ = true;
    }
  }
  return this->current_client_mask_ != 0;
}

bool RtspServer::end_access_unit() {
  if (this->current_keyframe_) {
    for (size_t index = 0; index < RTSP_MAX_CLIENTS; index++) {
      if ((this->current_client_mask_ & static_cast<uint8_t>(1U << index)) != 0) {
        this->clients_[index].needs_keyframe = false;
      }
    }
  }

  if (this->current_media_sent_) {
    this->sent_frames_++;
    if (this->sent_frames_ == 1) {
      ESP_LOGI(TAG, "RTSP access unit sent: n=%" PRIu32 " key=%d len=%zu", this->sent_frames_, this->current_keyframe_,
               this->current_data_length_);
    } else if (this->sent_frames_ % PERIODIC_FRAME_LOG_INTERVAL == 0) {
      ESP_LOGD(TAG, "RTSP access unit sent: n=%" PRIu32 " key=%d len=%zu", this->sent_frames_, this->current_keyframe_,
               this->current_data_length_);
    }
  }
  return this->current_media_sent_;
}

bool RtspServer::send_rtp_packet_(RtspClient &client, const RtpPacketView &packet, uint32_t timestamp) {
  const size_t rtp_length = RTP_HEADER_BYTES + packet.prefix_length + packet.payload_length;
  uint8_t header[TCP_RTP_HEADER_BYTES];
  header[0] = '$';
  header[1] = RTP_CHANNEL;
  write_big_endian(&header[2], static_cast<uint16_t>(rtp_length));
  header[4] = RTP_VERSION_2;
  header[5] = packet.payload_type | (packet.marker ? RTP_MARKER_BIT : 0U);
  const uint16_t sequence = client.rtp_seq;
  client.rtp_seq = static_cast<uint16_t>(client.rtp_seq + 1U);
  write_big_endian(&header[6], sequence);
  write_big_endian(&header[8], timestamp);
  write_big_endian(&header[12], this->ssrc_);

  iovec segments[3];
  int count = 0;
  segments[count++] = {header, sizeof(header)};
  if (packet.prefix_length != 0) {
    segments[count++] = {const_cast<uint8_t *>(packet.prefix), packet.prefix_length};
  }
  if (packet.payload_length != 0) {
    segments[count++] = {const_cast<uint8_t *>(packet.payload), packet.payload_length};
  }
  if (client.connection == nullptr || !send_iov_all(*client.connection, segments, count)) {
    this->close_client_(client);
    return false;
  }
  return true;
}

}  // namespace esphome::rtsp_server
#endif
