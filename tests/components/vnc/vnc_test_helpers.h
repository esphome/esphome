#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <initializer_list>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "esphome/components/display/display.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/vnc/vnc_client.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#ifdef USE_HOST

namespace esphome::vnc::testing {

using Bytes = std::vector<uint8_t>;

inline void set_now(uint32_t ms) { LoopBlockingGuard guard(nullptr, nullptr, ms); }
inline uint32_t now_ms() { return App.get_loop_component_start_time(); }

inline std::string hex(const Bytes &bytes) {
  std::string out;
  char buf[4];
  for (size_t i = 0; i < bytes.size(); i++) {
    snprintf(buf, sizeof(buf), i == 0 ? "%02X" : " %02X", bytes[i]);
    out += buf;
  }
  return out;
}

inline Bytes cat(std::initializer_list<Bytes> parts) {
  Bytes out;
  for (const auto &part : parts) {
    out.insert(out.end(), part.begin(), part.end());
  }
  return out;
}

inline Bytes text(const std::string &s) { return Bytes(s.begin(), s.end()); }

// ---------------------------------------------------------------------------
// Displays

inline constexpr uint32_t UNSET = 0xFF000000;

inline uint32_t pack(Color c) { return (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b; }

/// The colour a wire pixel must show as, decoded the way the display API decodes it.
inline uint32_t expect_px(uint16_t wire, uint8_t bpp) {
  return pack(display::ColorUtil::to_color(wire, bpp == 2 ? display::COLOR_BITNESS_565 : display::COLOR_BITNESS_332));
}

class FakeDisplay : public display::Display {
 public:
  FakeDisplay(int width = 64, int height = 48, display::DisplayType type = display::DISPLAY_TYPE_COLOR)
      : width_(width), height_(height), type_(type), pixels_(static_cast<size_t>(width) * height, UNSET) {}

  using display::Display::draw_pixels_at;

  void update() override { this->updates_++; }
  display::DisplayType get_display_type() override { return this->type_; }
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }

  void draw_pixel_at(int x, int y, Color color) override {
    if (x < 0 || y < 0 || x >= this->width_ || y >= this->height_) {
      this->out_of_bounds_++;
      ADD_FAILURE() << "pixel drawn outside the display at " << x << "," << y;
      return;
    }
    this->pixels_[static_cast<size_t>(y) * this->width_ + x] = pack(color);
  }

  void draw_pixels_internal(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorBitness bitness,
                            bool big_endian, int x_offset, int y_offset, int x_pad) override {
    this->draw_calls_++;
    if (w <= 0 || h <= 0 || x_start < 0 || y_start < 0 || x_start + w > this->width_ || y_start + h > this->height_) {
      this->out_of_bounds_++;
      ADD_FAILURE() << "block " << w << "x" << h << " at " << x_start << "," << y_start << " does not fit "
                    << this->width_ << "x" << this->height_;
    }
    if (this->busy()) {
      this->draws_while_busy_++;
      ADD_FAILURE() << "drawn while the display is busy";
    }
    if (this->draw_delay_us_ != 0) {
      usleep(this->draw_delay_us_);
    }
    display::Display::draw_pixels_internal(x_start, y_start, w, h, ptr, bitness, big_endian, x_offset, y_offset, x_pad);
  }

  virtual bool busy() const { return false; }

  uint32_t pixel(int x, int y) const { return this->pixels_[static_cast<size_t>(y) * this->width_ + x]; }
  const std::vector<uint32_t> &pixels() const { return this->pixels_; }
  size_t drawn_count() const {
    return static_cast<size_t>(
        std::count_if(this->pixels_.begin(), this->pixels_.end(), [](uint32_t p) { return p != UNSET; }));
  }
  int updates() const { return this->updates_; }
  int draw_calls() const { return this->draw_calls_; }
  int out_of_bounds() const { return this->out_of_bounds_; }
  int draws_while_busy() const { return this->draws_while_busy_; }
  void set_draw_delay_us(uint32_t us) { this->draw_delay_us_ = us; }

 protected:
  int width_;
  int height_;
  display::DisplayType type_;
  std::vector<uint32_t> pixels_;
  int updates_{0};
  int draw_calls_{0};
  int out_of_bounds_{0};
  int draws_while_busy_{0};
  uint32_t draw_delay_us_{0};
};

/// Behaves like it8951 and epaper_spi: parked (loop disabled) while idle,
/// update() starts a refresh that runs in its loop until finish().
class FakeParkingDisplay : public FakeDisplay {
 public:
  FakeParkingDisplay(int width = 64, int height = 48, display::DisplayType type = display::DISPLAY_TYPE_GRAYSCALE)
      : FakeDisplay(width, height, type) {
    this->disable_loop();
  }
  void update() override {
    this->updates_++;
    this->enable_loop();
  }
  /// The refresh is over and the driver parks its loop again.
  void finish() { this->disable_loop(); }
  /// Something other than the VNC client started a refresh.
  void external_update() { this->enable_loop(); }
  bool busy() const override { return !this->is_idle(); }
};

/// Starts with its loop running and parks once its start-up is done, like it8951 at boot.
class FakeStartingDisplay : public FakeParkingDisplay {
 public:
  FakeStartingDisplay() {
    this->component_state_ |= COMPONENT_HAS_LOOP;
    this->enable_loop();
  }
  void finish_start() {
    this->started_ = true;
    this->finish();
  }
  bool busy() const override { return this->started_ && FakeParkingDisplay::busy(); }

 protected:
  bool started_{false};
};

/// A display whose loop always runs (never reports idle).
class FakeFreeRunDisplay : public FakeDisplay {
 public:
  FakeFreeRunDisplay(int width = 64, int height = 48) : FakeDisplay(width, height) {
    this->disable_loop();
    this->enable_loop();
  }
};

/// A display with its own loop that never parks.
class FakeLoopingDisplay : public FakeFreeRunDisplay {
 public:
  FakeLoopingDisplay() { this->component_state_ |= COMPONENT_HAS_LOOP; }
};

/// Parked from setup and refreshes synchronously inside update(), like headless SDL.
class FakeSyncParkedDisplay : public FakeDisplay {
 public:
  FakeSyncParkedDisplay(int width = 64, int height = 48) : FakeDisplay(width, height) { this->disable_loop(); }
};

// ---------------------------------------------------------------------------
// Client under test

class TestVncClient : public VncClient {
 public:
  using VncClient::VncClient;
  ~TestVncClient() {
    this->high_freq_.stop();
    this->link_.close();
  }

  /// The state a connect still in flight has: a socket, but no session yet.
  void forget_session() { this->state_ = VncState::VNC_STATE_IDLE; }

  /// Hand the client a connected stream, as connect_step_() does after a successful connect.
  void adopt(int fd) {
    // connect_step_() has seen the network up before any connect succeeds.
    this->network_was_up_ = true;
    this->link_.adopt(std::make_unique<socket::Socket>(fd));
    this->start_handshake_(now_ms());
  }

  /// Parse bytes straight from memory, the way service_rx_() would, without a socket read.
  VncParse feed(const uint8_t *data, size_t len) {
    VncParse result = VncParse::VNC_PARSE_MORE;
    size_t offset = 0;
    do {
      const size_t room = RX_BUFFER_SIZE - this->rx_len_;
      const size_t count = std::min(room, len - offset);
      std::memcpy(this->rx_ + this->rx_len_, data + offset, count);
      this->rx_len_ += static_cast<uint16_t>(count);
      this->rx_total_ += count;
      offset += count;
      for (int guard = 0;; guard++) {
        this->pass_start_ms_ = millis();
        result = this->parse_();
        if (result != VncParse::VNC_PARSE_PAUSE || this->display_busy_ || guard > 100000) {
          break;
        }
      }
      if (result != VncParse::VNC_PARSE_MORE) {
        return result;
      }
      if (this->rx_pos_ != 0) {
        this->rx_len_ -= this->rx_pos_;
        std::memmove(this->rx_, this->rx_ + this->rx_pos_, this->rx_len_);
        this->rx_pos_ = 0;
      } else if (count == 0) {
        ADD_FAILURE() << "parser made no progress with a full buffer";
        return result;
      }
    } while (offset < len);
    return result;
  }
  VncParse feed(const Bytes &bytes) { return this->feed(bytes.data(), bytes.size()); }

  socket::TcpClientLink &link() { return this->link_; }
  VncState state() const { return this->state_; }
  VncStep step() const { return this->step_; }
  uint32_t hold_ms() const { return this->hold_ms_; }
  uint32_t hold_start_ms() const { return this->hold_start_ms_; }
  uint32_t backoff_ms() const { return this->backoff_ms_; }
  bool rx_pending() const { return this->rx_pending_; }
  bool in_update() const { return this->in_update_; }
  bool refresh_pending() const { return this->refresh_pending_; }
  bool display_busy() const { return this->display_busy_; }
  uint16_t rx_buffered() const { return this->rx_len_ - this->rx_pos_; }
  uint16_t vis_w() const { return this->vis_w_; }
  uint16_t vis_h() const { return this->vis_h_; }
  uint8_t bpp() const { return this->bpp_; }
};

// ---------------------------------------------------------------------------
// Server-side byte builder (RFB 3.x, server to client)

class Rfb {
 public:
  Bytes bytes;

  Rfb &u8(uint8_t v) {
    this->bytes.push_back(v);
    return *this;
  }
  Rfb &u16(uint16_t v) { return this->u8(v >> 8).u8(v & 0xFF); }
  Rfb &u32(uint32_t v) { return this->u16(v >> 16).u16(v & 0xFFFF); }
  Rfb &s32(int32_t v) { return this->u32(static_cast<uint32_t>(v)); }
  Rfb &raw(const Bytes &b) {
    this->bytes.insert(this->bytes.end(), b.begin(), b.end());
    return *this;
  }
  Rfb &str(const std::string &s) { return this->raw(text(s)); }
  Rfb &fill(size_t count, uint8_t value) {
    this->bytes.insert(this->bytes.end(), count, value);
    return *this;
  }
  /// One pixel in the client's format: little-endian RGB565 (bpp 2) or RGB332 (bpp 1).
  Rfb &px(uint16_t v, uint8_t bpp = 2) {
    this->u8(v & 0xFF);
    if (bpp == 2) {
      this->u8(v >> 8);
    }
    return *this;
  }

  Rfb &version(const std::string &v = "RFB 003.008\n") { return this->str(v); }
  Rfb &security_list(std::initializer_list<uint8_t> types) {
    this->u8(static_cast<uint8_t>(types.size()));
    for (uint8_t t : types) {
      this->u8(t);
    }
    return *this;
  }
  Rfb &security_33(uint32_t type) { return this->u32(type); }
  Rfb &security_result(uint32_t result) { return this->u32(result); }
  Rfb &reason(const std::string &s) { return this->u32(s.size()).str(s); }
  Rfb &challenge() {
    for (uint8_t i = 0; i < 16; i++) {
      this->u8(i);
    }
    return *this;
  }
  Rfb &server_init(uint16_t w, uint16_t h, const std::string &name = "esphome-test") {
    // The server's own pixel format; the client replaces it with SetPixelFormat.
    this->u16(w).u16(h).u8(32).u8(24).u8(0).u8(1).u16(255).u16(255).u16(255).u8(16).u8(8).u8(0).fill(3, 0);
    return this->u32(name.size()).str(name);
  }

  Rfb &fbu(uint16_t rects) { return this->u8(0).u8(0).u16(rects); }
  Rfb &rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, int32_t encoding) {
    return this->u16(x).u16(y).u16(w).u16(h).s32(encoding);
  }
  Rfb &raw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const std::vector<uint16_t> &pixels, uint8_t bpp = 2) {
    this->rect(x, y, w, h, 0);
    for (uint16_t p : pixels) {
      this->px(p, bpp);
    }
    return *this;
  }
  Rfb &cursor(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t bpp = 2) {
    this->rect(x, y, w, h, -239);
    for (uint32_t i = 0; i < static_cast<uint32_t>(w) * h; i++) {
      this->px(0xA5A5, bpp);
    }
    return this->fill(((w + 7u) / 8u) * h, 0xFF);
  }
  Rfb &bell() { return this->u8(2); }
  Rfb &cut_text(uint32_t len, uint8_t value = 'x') { return this->u8(3).fill(3, 0).u32(len).fill(len, value); }
  Rfb &colour_map(uint16_t first, uint16_t count) {
    this->u8(1).u8(0).u16(first).u16(count);
    for (uint16_t i = 0; i < count; i++) {
      this->u16(i).u16(i).u16(i);
    }
    return *this;
  }
};

/// A full RFB 3.8 None handshake up to and including ServerInit.
inline Bytes handshake_38(uint16_t w, uint16_t h) {
  return Rfb().version().security_list({1}).security_result(0).server_init(w, h).bytes;
}

// ---------------------------------------------------------------------------
// Client-side messages the tests expect

inline Bytes fbur(bool incremental, uint16_t w, uint16_t h) {
  return Rfb().u8(3).u8(incremental ? 1 : 0).u16(0).u16(0).u16(w).u16(h).bytes;
}
inline Bytes set_pixel_format_565() { return {0, 0, 0, 0, 16, 16, 0, 1, 0, 31, 0, 63, 0, 31, 11, 5, 0, 0, 0, 0}; }
inline Bytes set_pixel_format_332() { return {0, 0, 0, 0, 8, 8, 0, 1, 0, 7, 0, 7, 0, 3, 5, 2, 0, 0, 0, 0}; }
inline Bytes set_encodings() { return {2, 0, 0, 2, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0x11}; }
inline Bytes version_reply(char minor) { return text(std::string("RFB 003.00") + minor + "\n"); }

/// Splits a stream of client messages that are all FramebufferUpdateRequests.
inline int count_fburs(const Bytes &tx, bool incremental) {
  int count = 0;
  for (size_t i = 0; i + 10 <= tx.size(); i += 10) {
    if (tx[i] == 3 && (tx[i + 1] != 0) == incremental) {
      count++;
    }
  }
  return count;
}

// ---------------------------------------------------------------------------
// Ports

/// A loopback port that was free a moment ago, so a connect to it is refused.
inline uint16_t refused_port() {
  int probe = ::socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  socklen_t len = sizeof(addr);
  EXPECT_EQ(::bind(probe, reinterpret_cast<struct sockaddr *>(&addr), len), 0);
  EXPECT_EQ(::getsockname(probe, reinterpret_cast<struct sockaddr *>(&addr), &len), 0);
  ::close(probe);
  return ntohs(addr.sin_port);
}

/// A loopback listener that counts the connects that reach it.
class Listener {
 public:
  Listener() {
    this->fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof(addr);
    EXPECT_EQ(::bind(this->fd_, reinterpret_cast<struct sockaddr *>(&addr), len), 0);
    EXPECT_EQ(::listen(this->fd_, 16), 0);
    EXPECT_EQ(::getsockname(this->fd_, reinterpret_cast<struct sockaddr *>(&addr), &len), 0);
    fcntl(this->fd_, F_SETFL, fcntl(this->fd_, F_GETFL, 0) | O_NONBLOCK);
    this->port_ = ntohs(addr.sin_port);
  }
  ~Listener() {
    for (int fd : this->accepted_) {
      if (fd >= 0) {
        ::close(fd);
      }
    }
    ::close(this->fd_);
  }
  uint16_t port() const { return this->port_; }
  /// Connections accepted so far, including any waiting now.
  int accepted() {
    for (;;) {
      int fd = ::accept(this->fd_, nullptr, nullptr);
      if (fd >= 0) {
        this->accepted_.push_back(fd);
      } else if (errno == ECONNABORTED) {
        // The client gave up on it already; it still was an attempt.
        this->accepted_.push_back(-1);
      } else if (errno != EINTR) {
        break;
      }
    }
    return static_cast<int>(this->accepted_.size());
  }

 protected:
  int fd_{-1};
  uint16_t port_{0};
  std::vector<int> accepted_;
};

// ---------------------------------------------------------------------------
// Test harness: the test plays the server on the far end of a socketpair.

class Session {
 public:
  explicit Session(display::Display *display, uint32_t start_ms = 1000) : client(display) {
    signal(SIGPIPE, SIG_IGN);
    set_now(start_ms);
    this->client.set_host("127.0.0.1");
    this->client.set_port(refused_port());
    this->client.setup();
  }
  ~Session() { this->close_peer(); }

  /// A new TCP session: the client gets one end, the test keeps the other.
  void attach() {
    this->close_peer();
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL, 0) | O_NONBLOCK);
    this->peer_fd = fds[1];
    this->client_fd = fds[0];
    this->out_.clear();
    this->out_pos_ = 0;
    this->client.adopt(fds[0]);
  }
  void close_peer() {
    if (this->peer_fd >= 0) {
      ::close(this->peer_fd);
      this->peer_fd = -1;
    }
  }

  /// Queue server bytes and write as many as the socket takes now.
  void send(const Bytes &bytes) {
    this->out_.insert(this->out_.end(), bytes.begin(), bytes.end());
    this->pump();
  }
  void send(const Rfb &rfb) { this->send(rfb.bytes); }
  void pump() {
    while (this->out_pos_ < this->out_.size() && this->peer_fd >= 0) {
      ssize_t n = ::write(this->peer_fd, this->out_.data() + this->out_pos_, this->out_.size() - this->out_pos_);
      if (n <= 0) {
        break;
      }
      this->out_pos_ += n;
    }
    if (this->out_pos_ == this->out_.size()) {
      this->out_.clear();
      this->out_pos_ = 0;
    }
  }
  bool server_bytes_waiting() const { return this->out_pos_ < this->out_.size(); }
  /// Bytes the client has not read from its socket yet.
  int unread() {
    int count = 0;
    if (this->client.link().connected()) {
      ioctl(this->client_fd, FIONREAD, &count);
    }
    return count;
  }

  /// Everything the client sent since the last call.
  Bytes take() {
    this->client.link().flush_tx();
    Bytes out;
    if (this->peer_fd < 0) {
      return out;
    }
    uint8_t buf[4096];
    for (;;) {
      ssize_t n = ::read(this->peer_fd, buf, sizeof(buf));
      if (n <= 0) {
        break;
      }
      out.insert(out.end(), buf, buf + n);
    }
    return out;
  }

  void loop(int count = 1) {
    for (int i = 0; i < count; i++) {
      this->pump();
      this->client.loop();
    }
  }
  /// Move the virtual clock to an absolute time and run one pass.
  void at(uint32_t ms) {
    set_now(ms);
    this->loop();
  }
  /// Advance the virtual clock and run one pass.
  void tick(uint32_t ms) { this->at(now_ms() + ms); }
  /// Advance in steps, running one pass per step.
  void run_for(uint32_t ms, uint32_t step = 10) {
    const uint32_t end = now_ms() + ms;
    while (static_cast<int32_t>(end - now_ms()) > 0) {
      this->tick(std::min(step, end - now_ms()));
    }
  }
  bool loop_until(const std::function<bool()> &done, int max_loops = 1000) {
    for (int i = 0; i < max_loops && !done(); i++) {
      this->loop();
    }
    return done();
  }
  /// Runs passes at the current virtual time until done, sleeping briefly so the kernel can finish a connect.
  bool wait_until(const std::function<bool()> &done, int max_ms = 2000) {
    for (int i = 0; i < max_ms * 5 && !done(); i++) {
      this->loop();
      if (!done()) {
        usleep(200);
      }
    }
    return done();
  }
  /// Send and keep running passes at the current time until the client has read and parsed it all.
  void deliver(const Bytes &bytes, int max_loops = 100000) {
    this->send(bytes);
    this->loop_until(
        [this]() { return !this->server_bytes_waiting() && this->unread() == 0 && !this->client.rx_pending(); },
        max_loops);
  }
  void deliver(const Rfb &rfb) { this->deliver(rfb.bytes); }

  /// Full RFB 3.8 None handshake up to RUNNING; the client's handshake bytes are discarded.
  void start(uint16_t fb_w, uint16_t fb_h) {
    this->attach();
    this->deliver(handshake_38(fb_w, fb_h));
    ASSERT_EQ(this->client.state(), VncState::VNC_STATE_RUNNING);
    this->take();
  }

  TestVncClient client;
  int peer_fd{-1};
  int client_fd{-1};

 protected:
  Bytes out_;
  size_t out_pos_{0};
};

}  // namespace esphome::vnc::testing

#endif  // USE_HOST
