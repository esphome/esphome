#pragma once

#include "esphome/components/display/display.h"
#include "esphome/components/socket/tcp_client_link.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cstddef>
#include <cstdint>

namespace esphome::vnc {

enum class VncPixelFormat : uint8_t {
  VNC_PIXEL_FORMAT_AUTO,
  VNC_PIXEL_FORMAT_RGB565,
  VNC_PIXEL_FORMAT_RGB332,
};

enum class VncState : uint8_t {
  VNC_STATE_IDLE,
  VNC_STATE_HANDSHAKE,
  VNC_STATE_RUNNING,
};

enum class VncStep : uint8_t {
  VNC_STEP_VERSION,
  VNC_STEP_SECURITY_COUNT,
  VNC_STEP_SECURITY_LIST,
  VNC_STEP_SECURITY_TYPE_33,
  VNC_STEP_SECURITY_RESULT,
  VNC_STEP_REASON_LENGTH,
  VNC_STEP_REASON,
  VNC_STEP_SERVER_INIT,
  VNC_STEP_SERVER_NAME,
  VNC_STEP_SESSION_START,
  VNC_STEP_MESSAGE,
  VNC_STEP_RECT,
  VNC_STEP_RECT_DONE,
  VNC_STEP_RAW,
  VNC_STEP_TILE,
  VNC_STEP_TILE_RAW,
  VNC_STEP_SUBRECTS,
  VNC_STEP_SKIP,
};

enum class VncParse : uint8_t {
  VNC_PARSE_MORE,   // the next step needs bytes that have not arrived
  VNC_PARSE_PAUSE,  // out of time, or the display is busy; resume without new data
  VNC_PARSE_STOP,   // the session ended
};

enum class VncFailure : uint8_t {
  VNC_FAILURE_TRANSIENT,
  VNC_FAILURE_CONFIG,
};

/// RFB (VNC) client that draws the remote framebuffer into a display.
class VncClient : public Component {
 public:
  explicit VncClient(display::Display *display, bool big_endian = false) : display_(display), big_endian_(big_endian) {}

  void set_host(const char *host) { this->link_.set_host(host); }
  void set_port(uint16_t port) { this->link_.set_port(port); }
  void set_reconnect_interval(uint32_t ms) { this->link_.set_reconnect_interval(ms); }
  void set_update_delay(uint32_t ms) { this->update_delay_ms_ = ms; }
  void set_max_update_delay(uint32_t ms) { this->max_update_delay_ms_ = ms; }
  void set_pixel_format(VncPixelFormat format) { this->pixel_format_ = format; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  bool is_connected() const { return this->state_ == VncState::VNC_STATE_RUNNING; }
  /// Ask the server for the whole screen again.
  void refresh();

  template<typename F> void add_on_connect_callback(F &&callback) {
    this->connect_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_disconnect_callback(F &&callback) {
    this->disconnect_callback_.add(std::forward<F>(callback));
  }

 protected:
  static constexpr size_t RX_BUFFER_SIZE = 512;
  static constexpr size_t TILE_BUFFER_SIZE = 16 * 16 * 2;

  void connect_step_(uint32_t now);
  void start_handshake_(uint32_t now);
  void service_rx_(uint32_t now);
  VncParse parse_();
  void pace_(uint32_t now);
  bool start_session_(uint32_t now);
  bool send_client_init_();
  void frame_done_();
  void rect_done_();
  void next_tile_();
  void draw_tile_();
  uint32_t tile_hash_() const;
  void clear_tiles_();
  void forget_tiles_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void draw_row_(uint16_t x, uint16_t y, uint16_t count, const uint8_t *pixels);
  void fill_tile_(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *pixel);
  void skip_(uint32_t count, VncStep next);
  bool budget_exhausted_() const;
  bool send_(const uint8_t *data, size_t len);
  bool send_update_request_(bool incremental, uint32_t now);
  VncParse fail_(VncFailure failure, const LogString *reason);
  VncParse fail_send_() { return this->fail_(VncFailure::VNC_FAILURE_TRANSIENT, LOG_STR("Send failed")); }
  VncFailure lost_failure_() const;
  void session_lost_(uint32_t now);
  void session_end_(uint32_t now, VncFailure failure, const LogString *reason);
  void reset_session_();

  display::Display *display_;
  socket::TcpClientLink link_;
  LazyCallbackManager<void()> connect_callback_;
  LazyCallbackManager<void()> disconnect_callback_;
  HighFrequencyLoopRequester high_freq_;
  // Fingerprint of the tile last drawn at each 16x16 cell, so tiles the server resends unchanged are skipped.
  uint32_t *tile_hashes_{nullptr};

  uint32_t update_delay_ms_{200};
  uint32_t max_update_delay_ms_{1000};
  uint32_t backoff_ms_{0};
  uint32_t hold_ms_{0};
  uint32_t hold_start_ms_{0};
  uint32_t setup_ms_{0};
  uint32_t busy_since_ms_{0};
  uint32_t session_start_ms_{0};
  uint32_t update_start_ms_{0};
  // The first and the latest frame drawn since the display last refreshed.
  uint32_t first_change_ms_{0};
  uint32_t last_change_ms_{0};
  uint32_t rx_total_{0};
  uint32_t update_start_bytes_{0};
  uint32_t last_request_ms_{0};
  uint32_t last_rx_ms_{0};
  uint32_t pass_start_ms_{0};
  uint32_t skip_left_{0};

  uint16_t rx_pos_{0};
  uint16_t rx_len_{0};
  uint16_t fb_w_{0};
  uint16_t fb_h_{0};
  uint16_t vis_w_{0};
  uint16_t vis_h_{0};
  uint16_t rects_left_{0};
  uint16_t rect_x_{0};
  uint16_t rect_y_{0};
  uint16_t rect_w_{0};
  uint16_t rect_h_{0};
  // Raw: column and row inside the rect. Hextile: absolute origin of the current tile.
  uint16_t pos_x_{0};
  uint16_t pos_y_{0};
  uint16_t tile_fill_{0};
  uint16_t drawn_tiles_{0};
  uint16_t hash_capacity_{0};
  uint16_t hash_cols_{0};
  uint16_t hash_rows_{0};

  uint8_t tile_w_{0};
  uint8_t tile_h_{0};
  uint8_t subrects_left_{0};
  uint8_t bpp_{2};
  uint8_t minor_{8};
  uint8_t sec_count_{0};
  uint8_t text_len_{0};  // clamped to MAX_TEXT_LEN
  // The display rotation the cached tiles were drawn at, in quarter turns.
  uint8_t hash_rotation_{0};
  uint8_t hex_bg_[2]{};
  uint8_t hex_fg_[2]{};
  display::ColorBitness bitness_{display::COLOR_BITNESS_565};
  VncPixelFormat pixel_format_{VncPixelFormat::VNC_PIXEL_FORMAT_AUTO};
  VncState state_{VncState::VNC_STATE_IDLE};
  VncStep step_{VncStep::VNC_STEP_VERSION};
  VncStep after_skip_{VncStep::VNC_STEP_MESSAGE};
  VncFailure reason_failure_{VncFailure::VNC_FAILURE_TRANSIENT};

  bool rx_pending_{false};
  bool in_update_{false};
  bool request_outstanding_{false};
  bool refresh_pending_{false};
  bool need_full_{false};
  bool frame_drawn_{false};
  bool subrects_coloured_{false};
  bool tile_hidden_{false};
  bool display_parks_{false};
  bool display_busy_{false};
  bool busy_warned_{false};
  bool refresh_retried_{false};  // one retry per refresh the driver abandoned
  bool network_was_up_{false};
  bool big_endian_;  // the pixel order the display takes

  uint8_t rx_[RX_BUFFER_SIZE];
  alignas(4) uint8_t tile_[TILE_BUFFER_SIZE];
};

}  // namespace esphome::vnc
