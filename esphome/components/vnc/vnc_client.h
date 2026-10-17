#pragma once

#include "esphome/components/display/display.h"
#include "esphome/components/socket/tcp_client_link.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#ifdef USE_VNC_TOUCHSCREEN
#include "esphome/components/touchscreen/touchscreen.h"
#endif

#include <cstddef>
#include <cstdint>
#include <string>

namespace esphome::vnc {

enum class VncPixelFormat : uint8_t {
  VNC_PIXEL_FORMAT_AUTO,
  VNC_PIXEL_FORMAT_RGB565,
  VNC_PIXEL_FORMAT_RGB332,
};

enum class VncKeyAction : uint8_t {
  VNC_KEY_ACTION_PRESS,
  VNC_KEY_ACTION_DOWN,
  VNC_KEY_ACTION_UP,
};

enum class VncState : uint8_t {
  VNC_STATE_IDLE,
  VNC_STATE_HANDSHAKE,
  VNC_STATE_RUNNING,
  VNC_STATE_STOPPED,
  VNC_STATE_AUTH_FAILED,
};

enum class VncStep : uint8_t {
  VNC_STEP_VERSION,
  VNC_STEP_SECURITY_COUNT,
  VNC_STEP_SECURITY_LIST,
  VNC_STEP_SECURITY_TYPE_33,
  VNC_STEP_CHALLENGE,
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
  VNC_FAILURE_AUTH,
  VNC_FAILURE_AUTH_AMBIGUOUS,
};

class VncClient;

#ifdef USE_VNC_TOUCHSCREEN
class VncTouchListener final : public touchscreen::TouchListener, public Parented<VncClient> {
 public:
  void update(const touchscreen::TouchPoints_t &tpoints) override;
  void release() override;
};
#endif

/// RFB (VNC) client that draws the remote framebuffer into a display.
class VncClient : public Component {
 public:
  explicit VncClient(display::Display *display) : display_(display) {}

  void set_host(const char *host) { this->link_.set_host(host); }
  void set_port(uint16_t port) { this->link_.set_port(port); }
  void set_reconnect_interval(uint32_t ms) { this->link_.set_reconnect_interval(ms); }
  void set_update_delay(uint32_t ms) { this->update_delay_ms_ = ms; }
  void set_max_update_delay(uint32_t ms) { this->max_update_delay_ms_ = ms; }
  void set_pixel_format(VncPixelFormat format) { this->pixel_format_ = format; }
#ifdef USE_VNC_AUTH
  /// 8 bytes, already bit-reversed for VNC, may live in PROGMEM.
  void set_auth_key(const uint8_t *key) { this->auth_key_ = key; }
#endif
#ifdef USE_VNC_TOUCHSCREEN
  void register_touchscreen(touchscreen::Touchscreen *touchscreen) {
    this->touch_listener_.set_parent(this);
    touchscreen->register_listener(&this->touch_listener_);
  }
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  bool is_connected() const { return this->state_ == VncState::VNC_STATE_RUNNING; }
  /// Retry now, also after an authentication failure or disconnect().
  void connect();
  /// Close the session and stop retrying until connect().
  void disconnect();
  /// Ask the server for the whole screen again.
  void refresh();

  void send_pointer(uint16_t x, uint16_t y, uint8_t buttons);
  void send_key(uint32_t keysym, VncKeyAction action);
  void type_text(const char *text, size_t len);
  void type_text(const std::string &text) { this->type_text(text.data(), text.size()); }
  /// Touch input in display coordinates.
  void handle_touch(uint16_t x, uint16_t y);
  void handle_release();

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
  void start_session_(uint32_t now);
  void send_client_init_();
  void frame_done_();
  void rect_done_();
  void next_tile_();
  void draw_tile_();
  uint32_t tile_hash_() const;
  void clear_tiles_();
  void forget_tiles_(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void draw_row_(uint16_t x, uint16_t y, uint16_t count, const uint8_t *pixels);
  void fill_tile_(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t pixel);
  uint16_t read_pixel_(const uint8_t *data) const {
    return this->bpp_ == 1 ? data[0] : static_cast<uint16_t>(data[0] | (data[1] << 8));
  }
  void skip_(uint32_t count, VncStep next);
  bool budget_exhausted_() const;
  bool send_(const uint8_t *data, size_t len);
  bool send_update_request_(bool incremental);
  bool send_key_event_(uint32_t keysym, bool down);
  void note_input_();
  VncParse fail_(VncFailure failure, const LogString *reason);
  VncFailure lost_failure_() const;
  void session_lost_(uint32_t now);
  void session_end_(uint32_t now, VncFailure failure, const LogString *reason);
  void reset_session_();

  display::Display *display_;
  socket::TcpClientLink link_;
#ifdef USE_VNC_AUTH
  const uint8_t *auth_key_{nullptr};
#endif
#ifdef USE_VNC_TOUCHSCREEN
  VncTouchListener touch_listener_;
#endif
  LazyCallbackManager<void()> connect_callback_;
  LazyCallbackManager<void()> disconnect_callback_;
  HighFrequencyLoopRequester high_freq_;
  // Fingerprint of the tile last drawn at each 16x16 cell, so tiles the server resends unchanged are skipped.
  uint32_t *tile_hashes_{nullptr};
  uint32_t hash_capacity_{0};

  uint32_t update_delay_ms_{200};
  uint32_t max_update_delay_ms_{1000};
  uint32_t backoff_ms_{0};
  uint32_t hold_ms_{0};
  uint32_t hold_start_ms_{0};
  uint32_t session_start_ms_{0};
  uint32_t update_start_ms_{0};
  // The first and the latest frame drawn since the display last refreshed.
  uint32_t first_change_ms_{0};
  uint32_t last_input_ms_{0};
  uint32_t last_change_ms_{0};
  uint32_t rx_total_{0};
  uint32_t update_start_bytes_{0};
  uint16_t drawn_tiles_{0};
  uint32_t last_request_ms_{0};
  uint32_t last_rx_ms_{0};
  uint32_t pass_start_ms_{0};
  uint32_t skip_left_{0};
  uint32_t text_len_{0};

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
  uint16_t hex_bg_{0};
  uint16_t hex_fg_{0};
  uint16_t hash_cols_{0};
  uint16_t hash_rows_{0};
  uint16_t hash_rotation_{0};
  uint16_t ptr_x_{0};
  uint16_t ptr_y_{0};
  uint16_t press_x_{0};
  uint16_t press_y_{0};

  uint8_t tile_w_{0};
  uint8_t tile_h_{0};
  uint8_t subrects_left_{0};
  uint8_t bpp_{2};
  uint8_t minor_{8};
  uint8_t sec_type_{0};
  uint8_t sec_count_{0};
  uint8_t ptr_buttons_{0};
  uint8_t auth_ambiguous_{0};
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
  bool display_parks_{false};
  bool display_busy_{false};
  bool network_was_up_{false};
  bool dragging_{false};
  // Input was sent recently, so frames are painted without waiting for the screen to settle.
  bool input_boost_{false};

  uint8_t rx_[RX_BUFFER_SIZE];
  alignas(4) uint8_t tile_[TILE_BUFFER_SIZE];
};

}  // namespace esphome::vnc
