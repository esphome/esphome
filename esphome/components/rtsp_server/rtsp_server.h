#pragma once
#ifdef USE_RTSP_SERVER

#include "esphome/components/socket/socket.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/static_task.h"
#include "rtsp_track.h"

#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>
#include <string>

namespace esphome::rtsp_server {

static constexpr size_t RTSP_MAX_CLIENTS = 2;
static constexpr size_t RTSP_RX_BUFFER_BYTES = 1024;
static constexpr uint16_t RTSP_DEFAULT_PORT = 554;
static constexpr uint16_t RTP_PAYLOAD_MIN = 256;
static constexpr uint16_t RTP_PAYLOAD_MAX = 1400;
static constexpr uint16_t RTP_PAYLOAD_DEFAULT = RTP_PAYLOAD_MAX;

enum class RtspState : uint8_t { RTSP_STATE_INIT, RTSP_STATE_READY, RTSP_STATE_PLAYING };

struct RtspClient {
  std::unique_ptr<socket::Socket> connection;
  RtspState state{RtspState::RTSP_STATE_INIT};
  uint32_t session_id{0};
  uint16_t rtp_seq{0};
  uint32_t rtp_timestamp_offset{0};
  bool needs_keyframe{false};
  char rx[RTSP_RX_BUFFER_BYTES]{};
  size_t rx_len{0};

  bool is_connected() const { return this->connection != nullptr; }
  int fd() const { return this->connection == nullptr ? -1 : this->connection->get_fd(); }
};

class RtspServer final : public Component, public RtspTrackListener, public RtpPacketSink {
 public:
  explicit RtspServer(RtspTrack *track) : track_(track) {}
  ~RtspServer() override;

  void set_port(uint16_t port) { this->port_ = port; }
  void set_auth_username(const std::string &username) { this->auth_username_ = username; }
  void set_auth_password(const std::string &password) { this->auth_password_ = password; }
  void set_rtp_payload_size(uint16_t payload_size) {
    this->rtp_payload_size_ = clamp_at_most(clamp_at_least(payload_size, RTP_PAYLOAD_MIN), RTP_PAYLOAD_MAX);
  }

  void setup() override;
  void dump_config() override;
  void on_shutdown() override;
  bool teardown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI - 2; }

  void on_rtsp_track_frame_available() override;

  size_t get_maximum_payload_size() const override { return this->rtp_payload_size_; }
  bool begin_access_unit(uint32_t timestamp, bool keyframe, size_t data_length) override;
  bool send_packet(const RtpPacketView &packet) override;
  bool end_access_unit() override;

 protected:
  static void server_task_entry(void *arg);
  void server_task_();
  void notify_server_task_();
  void request_server_task_stop_();
  bool finish_server_task_stop_(bool force);

  std::unique_ptr<socket::ListenSocket> open_listen_socket_();
  void accept_client_(socket::ListenSocket &listen_socket);
  void read_client_(RtspClient &client);
  void close_client_(RtspClient &client);

  void handle_request_(RtspClient &client);
  bool check_auth_(const char *request) const;
  void send_response_(socket::Socket *connection, const char *format, ...);
  void send_unauthorized_(RtspClient &client, int cseq);
  void handle_options_(RtspClient &client, int cseq);
  void handle_describe_(RtspClient &client, int cseq);
  void handle_setup_(RtspClient &client, int cseq, const char *request);
  void handle_play_(RtspClient &client, int cseq);
  void handle_teardown_(RtspClient &client, int cseq);

  bool send_rtp_packet_(RtspClient &client, const RtpPacketView &packet, uint32_t timestamp);

  RtspTrack *track_{nullptr};
  uint16_t port_{RTSP_DEFAULT_PORT};
  uint16_t rtp_payload_size_{RTP_PAYLOAD_DEFAULT};
  std::string auth_username_;
  std::string auth_password_;

  RtspClient clients_[RTSP_MAX_CLIENTS];
  StaticTask server_task_runner_;
  std::atomic<bool> running_{false};
  std::atomic<bool> server_task_exited_{false};
  std::atomic<bool> streaming_{false};
  std::atomic<bool> priming_subscription_{false};
  uint32_t ssrc_{0};
  uint32_t sent_frames_{0};

  uint8_t current_client_mask_{0};
  uint8_t current_bootstrap_mask_{0};
  uint32_t current_timestamp_{0};
  size_t current_data_length_{0};
  bool current_keyframe_{false};
  bool current_media_sent_{false};
};

}  // namespace esphome::rtsp_server
#endif
