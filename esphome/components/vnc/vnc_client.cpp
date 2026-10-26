#include "vnc_client.h"

#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#ifdef USE_VNC_AUTH
#include "vnc_des.h"
#endif

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <iterator>

namespace esphome::vnc {

ESPHOME_LOG_TAG(TAG, "vnc");

// Below the 50 ms blocking warning, and the same slice it8951 uses for its transfers.
static constexpr uint32_t LOOP_BUDGET_MS = 20;
static constexpr uint32_t HANDSHAKE_TIMEOUT_MS = 15000;
static constexpr uint32_t STABLE_SESSION_MS = 60000;
static constexpr uint32_t MAX_BACKOFF_MS = 60000;
static constexpr uint32_t CONFIG_RETRY_MS = 300000;
static constexpr uint32_t AUTH_AMBIGUOUS_HOLD_MS = 60000;
static constexpr uint32_t HEARTBEAT_MS = 60000;
static constexpr uint32_t MIN_REQUEST_SPACING_MS = 50;
// The longest the first refresh waits for a display with a loop to park once after boot.
static constexpr uint32_t DISPLAY_START_MS = 5000;
// Longer than any driver's own busy timeout: a panel that stays busy this long is stuck
static constexpr uint32_t DISPLAY_BUSY_TIMEOUT_MS = 120000;
static constexpr uint16_t MAX_CURSOR_SIDE = 1024;
static constexpr uint32_t MAX_TEXT_LEN = 63;

static constexpr uint8_t SECURITY_NONE = 1;
static constexpr uint8_t SECURITY_VNC_AUTH = 2;

static constexpr int32_t ENCODING_RAW = 0;
static constexpr int32_t ENCODING_HEXTILE = 5;
static constexpr int32_t ENCODING_RICH_CURSOR = -239;
// In preference order: TightVNC uses the first real encoding it supports for every rectangle.
static constexpr int32_t ENCODINGS[] = {ENCODING_HEXTILE, ENCODING_RAW, ENCODING_RICH_CURSOR};

// Indexed by VncPixelFormat
PROGMEM_STRING_TABLE(PixelFormatStrings, "auto", "RGB565", "RGB332");

static constexpr uint8_t HEXTILE_RAW = 0x01;
static constexpr uint8_t HEXTILE_BACKGROUND = 0x02;
static constexpr uint8_t HEXTILE_FOREGROUND = 0x04;
static constexpr uint8_t HEXTILE_ANY_SUBRECTS = 0x08;
static constexpr uint8_t HEXTILE_SUBRECTS_COLOURED = 0x10;

static constexpr uint8_t MSG_FRAMEBUFFER_UPDATE = 0;
static constexpr uint8_t MSG_SET_COLOUR_MAP = 1;
static constexpr uint8_t MSG_BELL = 2;
static constexpr uint8_t MSG_SERVER_CUT_TEXT = 3;

static inline uint16_t read_be16(const uint8_t *p) { return encode_uint16(p[0], p[1]); }
static inline uint32_t read_be32(const uint8_t *p) { return encode_uint32(p[0], p[1], p[2], p[3]); }
static inline void write_be16(uint8_t *p, uint16_t v) {
  p[0] = v >> 8;
  p[1] = v;
}
static inline void write_be32(uint8_t *p, uint32_t v) {
  p[0] = v >> 24;
  p[1] = v >> 16;
  p[2] = v >> 8;
  p[3] = v;
}

static int parse_three_digits(const uint8_t *p) {
  int value = 0;
  for (int i = 0; i < 3; i++) {
    if (p[i] < '0' || p[i] > '9') {
      return -1;
    }
    value = value * 10 + (p[i] - '0');
  }
  return value;
}

void VncClient::setup() {
  this->setup_ms_ = App.get_loop_component_start_time();
  this->link_.begin(TAG);
  // Sized for the display now; a rotation changed later swaps the sides but keeps the cell count.
  const uint32_t wanted = static_cast<uint32_t>((this->display_->get_width() + 15) / 16) *
                          static_cast<uint32_t>((this->display_->get_height() + 15) / 16);
  const uint32_t cells = std::min<uint32_t>(wanted, UINT16_MAX);
  if (cells < wanted) {
    ESP_LOGW(TAG, "Display has %" PRIu32 " tiles, more than the %u the cache holds", wanted, UINT16_MAX);
  }
  RAMAllocator<uint32_t> allocator;
  this->tile_hashes_ = allocator.allocate(cells);
  if (this->tile_hashes_ == nullptr) {
    ESP_LOGW(TAG, "Not enough memory to skip unchanged tiles");
    return;
  }
  this->hash_capacity_ = cells;
  this->clear_tiles_();
}

void VncClient::refresh() {
  this->need_full_ = true;
  this->clear_tiles_();
}

void VncClient::clear_tiles_() {
  if (this->tile_hashes_ != nullptr) {
    std::memset(this->tile_hashes_, 0, sizeof(uint32_t) * this->hash_capacity_);
  }
}

void VncClient::dump_config() {
  ESP_LOGCONFIG(TAG,
                "VNC:\n"
                "  Server: %s:%u\n"
                "  Reconnect Interval: %" PRIu32 "ms\n"
                "  Update Delay: %" PRIu32 "ms\n"
                "  Max Update Delay: %" PRIu32 "ms\n"
                "  Pixel Format: %s\n"
                "  Tile Cache: %u tiles",
                this->link_.host(), this->link_.port(), this->link_.reconnect_interval(), this->update_delay_ms_,
                this->max_update_delay_ms_,
                LOG_STR_ARG(PixelFormatStrings::get_log_str(static_cast<uint8_t>(this->pixel_format_), 0)),
                this->hash_capacity_);
#ifdef USE_VNC_AUTH
  ESP_LOGCONFIG(TAG, "  Password: %s", this->auth_key_ != nullptr ? LOG_STR_LITERAL("set") : LOG_STR_LITERAL("none"));
#endif
}

void VncClient::on_shutdown() { this->link_.close(); }

void VncClient::loop() {
  // Latched by disconnect() or a wrong password until connect()
  if (this->state_ == VncState::VNC_STATE_STOPPED || this->state_ == VncState::VNC_STATE_AUTH_FAILED) {
    this->disable_loop();
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  this->pass_start_ms_ = millis();
  if (this->display_->is_failed()) {
    this->link_.close();  // a connect still in flight would otherwise keep its socket for good
    if (this->state_ != VncState::VNC_STATE_IDLE) {
      this->session_end_(now, VncFailure::VNC_FAILURE_TRANSIENT, LOG_STR("Display failed"));
    }
    this->status_set_error(LOG_STR("Display failed"));
    this->disable_loop();
    return;
  }
  // A display that parks its loop when idle is busy while that loop runs. Displays that never
  // park (no loop, or one that always runs) update synchronously, so they are never busy.
  const bool was_busy = this->display_busy_;
  if (this->display_->is_idle()) {
    this->display_parks_ = true;
    this->display_busy_ = false;
  } else {
    this->display_busy_ = this->display_parks_;
  }
  if (was_busy && !this->display_busy_) {
    // A refresh the driver gave up on (it sets its warning) leaves the panel behind the buffer: retry it
    // once and forget the cached tiles, which no longer say what the panel shows
    if (!this->display_->status_has_warning()) {
      this->refresh_retried_ = false;
    } else if (!this->refresh_retried_) {
      this->refresh_retried_ = true;
      this->clear_tiles_();
      if (!this->refresh_pending_) {
        this->first_change_ms_ = now;
      }
      this->last_change_ms_ = now;
      this->refresh_pending_ = true;
    }
  }
  // A stuck panel, or another owner of the display's loop, would otherwise freeze the screen silently
  if (!this->display_busy_) {
    this->busy_since_ms_ = now;
    if (this->busy_warned_) {
      this->busy_warned_ = false;
      // The connection path owns the warning while no session runs
      if (this->state_ == VncState::VNC_STATE_RUNNING) {
        this->status_clear_warning();
      }
    }
  } else if (!this->busy_warned_ && now - this->busy_since_ms_ >= DISPLAY_BUSY_TIMEOUT_MS) {
    this->busy_warned_ = true;
    this->status_set_warning(LOG_STR("Display busy too long"));
  }

  if (!this->link_.connected()) {
    if (this->state_ != VncState::VNC_STATE_IDLE) {
      this->session_lost_(now);
    }
    this->connect_step_(now);
    if (!this->link_.connected()) {
      return;
    }
    this->start_handshake_(now);
  }
  if (this->state_ == VncState::VNC_STATE_HANDSHAKE && now - this->session_start_ms_ >= HANDSHAKE_TIMEOUT_MS) {
    this->fail_(this->lost_failure_(), LOG_STR("Handshake timed out"));
    return;
  }

  const bool blocked = this->in_update_ && this->display_busy_;
  // A display still starting up (it8951) can go from its start-up straight into a refresh without parking.
  const bool display_starting =
      !this->display_parks_ && this->display_->has_overridden_loop() && now - this->setup_ms_ < DISPLAY_START_MS;
  // Wait until the screen stops changing, so an animation becomes one refresh showing where it ended.
  if (this->refresh_pending_ && !this->in_update_ && !this->display_busy_ && !display_starting &&
      (now - this->last_change_ms_ >= this->update_delay_ms_ ||
       now - this->first_change_ms_ >= this->max_update_delay_ms_)) {
    // Synchronous displays flush inside update(), so this pass does nothing else.
    this->refresh_pending_ = false;
    this->display_->update();
    this->display_busy_ = this->display_parks_ && !this->display_->is_idle();
  } else if (!blocked && (this->rx_pending_ || this->link_.ready())) {
    this->service_rx_(now);
  }
  if (this->state_ == VncState::VNC_STATE_RUNNING) {
    // A new session lays the screen out again for the new rotation and redraws all of it.
    if (static_cast<uint8_t>(this->display_->get_rotation() / 90) != this->hash_rotation_) {
      this->fail_(VncFailure::VNC_FAILURE_TRANSIENT, LOG_STR("Display rotation changed"));
      return;
    }
    // Mid-frame there is nothing to send, so a vanished server would never fail a write.
    if (this->in_update_ && !blocked && !this->rx_pending_ && now - this->last_rx_ms_ >= HEARTBEAT_MS) {
      this->fail_(VncFailure::VNC_FAILURE_TRANSIENT, LOG_STR("Server stopped sending"));
      return;
    }
    this->pace_(now);
  }
  this->link_.flush_tx();
  if (this->rx_pending_ && !(this->in_update_ && this->display_busy_)) {
    this->high_freq_.start();
  } else {
    this->high_freq_.stop();
  }
}

void VncClient::connect() {
  if (this->state_ == VncState::VNC_STATE_HANDSHAKE || this->state_ == VncState::VNC_STATE_RUNNING) {
    return;
  }
  this->backoff_ms_ = 0;
  this->hold_ms_ = 0;
#ifdef USE_VNC_AUTH
  this->auth_retried_ = false;
#endif
  this->status_clear_error();
  this->clear_link_warning_();
  this->state_ = VncState::VNC_STATE_IDLE;
  // Start a fresh attempt with its full timeout, even if one is in flight.
  this->link_.close();
  this->link_.begin(TAG);
  this->enable_loop();
}

void VncClient::disconnect() {
  if (this->state_ == VncState::VNC_STATE_STOPPED) {
    return;
  }
  const bool was_running = this->state_ == VncState::VNC_STATE_RUNNING;
  this->link_.close();
  this->reset_session_();
  this->state_ = VncState::VNC_STATE_STOPPED;
  this->clear_link_warning_();
  ESP_LOGI(TAG, "Disconnected");
  if (was_running) {
    this->disconnect_callback_.call();
  }
}

void VncClient::connect_step_(uint32_t now) {
  const bool up = network::is_connected();
  if (up && !this->network_was_up_) {
    this->backoff_ms_ = 0;
#ifdef USE_VNC_AUTH
    // The wait after an unanswered password protects the server's lockout counter, so it survives a network drop.
    if (!this->auth_retried_) {
      this->hold_ms_ = 0;
    }
#else
    this->hold_ms_ = 0;
#endif
  }
  this->network_was_up_ = up;
  if (!up) {
    return;
  }
  if (!this->link_.connecting() && (now - this->hold_start_ms_ < this->hold_ms_ || this->link_.in_backoff())) {
    return;
  }
  this->link_.poll();
  // DNS failures, refused connects and connect timeouts all leave no socket and start the link's backoff.
  if (!this->link_.connected() && !this->link_.connecting() && this->link_.in_backoff()) {
    this->session_end_(now, VncFailure::VNC_FAILURE_TRANSIENT, LOG_STR("Cannot reach the server"));
  }
}

void VncClient::clear_link_warning_() {
  // A stuck display keeps its warning through a reconnect
  if (!this->busy_warned_) {
    this->status_clear_warning();
  }
}

void VncClient::reset_session_() {
  // Pixels drawn but never shown would match the cache when the next session resends them.
  if (this->refresh_pending_ || this->frame_drawn_) {
    this->clear_tiles_();
  }
  // Hextile colours carry from tile to tile; a new server must not inherit the last one's
  std::memset(this->hex_bg_, 0, sizeof(this->hex_bg_));
  std::memset(this->hex_fg_, 0, sizeof(this->hex_fg_));
  this->rx_pos_ = 0;
  this->rx_len_ = 0;
  this->rx_pending_ = false;
  this->in_update_ = false;
  this->request_outstanding_ = false;
  this->refresh_pending_ = false;
  this->need_full_ = false;
  this->frame_drawn_ = false;
#ifdef USE_VNC_AUTH
  this->sec_type_ = 0;
#endif
  this->step_ = VncStep::VNC_STEP_VERSION;
  this->reason_failure_ = VncFailure::VNC_FAILURE_TRANSIENT;
  this->high_freq_.stop();
}

void VncClient::start_handshake_(uint32_t now) {
  this->reset_session_();
  this->state_ = VncState::VNC_STATE_HANDSHAKE;
  this->session_start_ms_ = now;
  this->last_rx_ms_ = now;
}

VncFailure VncClient::lost_failure_() const {
  if (this->state_ == VncState::VNC_STATE_HANDSHAKE) {
    if (this->step_ == VncStep::VNC_STEP_REASON_LENGTH || this->step_ == VncStep::VNC_STEP_REASON) {
      return this->reason_failure_;
    }
#ifdef USE_VNC_AUTH
    // The server got our password response but never said whether it was right.
    if (this->step_ == VncStep::VNC_STEP_SECURITY_RESULT && this->sec_type_ == SECURITY_VNC_AUTH) {
      return VncFailure::VNC_FAILURE_AUTH_AMBIGUOUS;
    }
#endif
  }
  return VncFailure::VNC_FAILURE_TRANSIENT;
}

void VncClient::session_lost_(uint32_t now) {
  this->session_end_(now, this->lost_failure_(), LOG_STR("Connection lost"));
}

VncParse VncClient::fail_(VncFailure failure, const LogString *reason) {
  this->link_.close();
  this->link_.note_attempt();
  this->session_end_(App.get_loop_component_start_time(), failure, reason);
  return VncParse::VNC_PARSE_STOP;
}

void VncClient::session_end_(uint32_t now, VncFailure failure, const LogString *reason) {
  const bool was_running = this->state_ == VncState::VNC_STATE_RUNNING;
#ifdef USE_VNC_AUTH
  // Only back-to-back unclear drops after the password count
  const bool retried = this->auth_retried_;
  this->auth_retried_ = false;
#endif
  this->reset_session_();
  this->state_ = VncState::VNC_STATE_IDLE;
  this->hold_start_ms_ = now;
  if (was_running) {
    const uint32_t lasted = now - this->session_start_ms_;
    ESP_LOGW(TAG, "%s after %" PRIu32 "s", LOG_STR_ARG(reason), lasted / 1000);
    this->disconnect_callback_.call();
    this->status_set_warning();
    if (lasted >= STABLE_SESSION_MS) {
      this->backoff_ms_ = 0;
      this->hold_ms_ = 0;
      return;
    }
    failure = VncFailure::VNC_FAILURE_TRANSIENT;
  }
  switch (failure) {
#ifdef USE_VNC_AUTH
    case VncFailure::VNC_FAILURE_AUTH_AMBIGUOUS:
      if (!retried) {
        this->auth_retried_ = true;
        this->hold_ms_ = AUTH_AMBIGUOUS_HOLD_MS;
        ESP_LOGW(TAG, "%s before the password was accepted; retrying in %" PRIu32 "s", LOG_STR_ARG(reason),
                 this->hold_ms_ / 1000);
        this->status_set_warning();
        return;
      }
      this->state_ = VncState::VNC_STATE_AUTH_FAILED;
      ESP_LOGE(TAG,
               "%s after the password twice. Not retrying: on some servers repeated failures lock out every user. "
               "Check the password and the server, then reboot or call vnc.connect",
               LOG_STR_ARG(reason));
      this->status_set_error(LOG_STR("Server dropped the connection after the password twice"));
      return;
    case VncFailure::VNC_FAILURE_AUTH:
      this->state_ = VncState::VNC_STATE_AUTH_FAILED;
      ESP_LOGE(TAG, "Authentication failed. Not retrying: on some servers repeated failures lock out every user. "
                    "Fix the password, then reboot or call vnc.connect");
      this->status_set_error(LOG_STR("Authentication failed"));
      return;
#endif
    case VncFailure::VNC_FAILURE_CONFIG:
      this->hold_ms_ = CONFIG_RETRY_MS;
      ESP_LOGE(TAG, "%s; retrying in %" PRIu32 "s", LOG_STR_ARG(reason), this->hold_ms_ / 1000);
      this->status_set_error(reason);
      return;
    default: {
      const uint32_t base = this->link_.reconnect_interval();
      this->backoff_ms_ =
          this->backoff_ms_ == 0 ? base : std::min(this->backoff_ms_ * 2, std::max(MAX_BACKOFF_MS, base));
      this->hold_ms_ = this->backoff_ms_;
      if (!was_running) {
        ESP_LOGW(TAG, "%s; retrying in %" PRIu32 "s", LOG_STR_ARG(reason), this->hold_ms_ / 1000);
      }
      this->status_set_warning();
      return;
    }
  }
}

bool VncClient::budget_exhausted_() const { return millis() - this->pass_start_ms_ >= LOOP_BUDGET_MS; }

void VncClient::service_rx_(uint32_t now) {
  for (;;) {
    VncParse result = this->parse_();
    if (result == VncParse::VNC_PARSE_STOP) {
      return;
    }
    if (result == VncParse::VNC_PARSE_PAUSE) {
      this->rx_pending_ = true;
      return;
    }
    // Less than one item is left, so the buffer always has room after compacting.
    if (this->rx_pos_ != 0) {
      this->rx_len_ -= this->rx_pos_;
      std::memmove(this->rx_, this->rx_ + this->rx_pos_, this->rx_len_);
      this->rx_pos_ = 0;
    }
    if (this->budget_exhausted_()) {
      this->rx_pending_ = true;
      return;
    }
    ssize_t count = this->link_.read(this->rx_ + this->rx_len_, RX_BUFFER_SIZE - this->rx_len_);
    if (count < 0) {
      this->session_lost_(now);
      return;
    }
    if (count == 0) {
      this->rx_pending_ = false;
      return;
    }
    this->rx_len_ += static_cast<uint16_t>(count);
    this->rx_total_ += count;
    this->last_rx_ms_ = now;
  }
}

void VncClient::skip_(uint32_t count, VncStep next) {
  this->skip_left_ = count;
  this->after_skip_ = next;
  this->step_ = VncStep::VNC_STEP_SKIP;
}

bool VncClient::send_client_init_() {
  const uint8_t shared = 1;
  return this->send_(&shared, 1);
}

bool VncClient::select_security_(uint8_t type) {
#ifdef USE_VNC_AUTH
  this->sec_type_ = type;
  if (type == SECURITY_VNC_AUTH) {
    this->step_ = VncStep::VNC_STEP_CHALLENGE;
    return true;
  }
#endif
  if (this->minor_ == 8) {
    this->step_ = VncStep::VNC_STEP_SECURITY_RESULT;
    return true;
  }
  this->step_ = VncStep::VNC_STEP_SERVER_INIT;
  return this->send_client_init_();
}

VncParse VncClient::parse_() {
  for (;;) {
    const uint8_t *p = this->rx_ + this->rx_pos_;
    const size_t avail = this->rx_len_ - this->rx_pos_;
    switch (this->step_) {
      case VncStep::VNC_STEP_VERSION: {
        if (avail < 12) {
          return VncParse::VNC_PARSE_MORE;
        }
        const int major = parse_three_digits(p + 4);
        const int minor = parse_three_digits(p + 8);
        if (std::memcmp(p, "RFB ", 4) != 0 || p[7] != '.' || p[11] != '\n' || major < 0 || minor < 0) {
          return this->fail_(VncFailure::VNC_FAILURE_CONFIG, LOG_STR("Not a VNC server"));
        }
        if (major < 3) {
          return this->fail_(VncFailure::VNC_FAILURE_CONFIG, LOG_STR("Unsupported RFB version"));
        }
        this->minor_ = (major > 3 || minor >= 8) ? 8 : (minor == 7 ? 7 : 3);
        this->rx_pos_ += 12;
        uint8_t reply[12] = {'R', 'F', 'B', ' ', '0', '0', '3', '.', '0', '0', '8', '\n'};
        reply[10] = '0' + this->minor_;
        if (!this->send_(reply, sizeof(reply))) {
          return this->fail_send_();
        }
        this->step_ = this->minor_ == 3 ? VncStep::VNC_STEP_SECURITY_TYPE_33 : VncStep::VNC_STEP_SECURITY_COUNT;
        break;
      }
      case VncStep::VNC_STEP_SECURITY_COUNT: {
        if (avail < 1) {
          return VncParse::VNC_PARSE_MORE;
        }
        this->sec_count_ = p[0];
        this->rx_pos_ += 1;
        if (this->sec_count_ == 0) {
          // A refusal before authentication, such as a lockout caused by another client.
          this->step_ = VncStep::VNC_STEP_REASON_LENGTH;
        } else {
          this->step_ = VncStep::VNC_STEP_SECURITY_LIST;
        }
        break;
      }
      case VncStep::VNC_STEP_SECURITY_LIST: {
        if (avail < this->sec_count_) {
          return VncParse::VNC_PARSE_MORE;
        }
        const bool has_key = this->has_auth_key_();
        uint8_t chosen = 0;
        bool wants_password = false;
        for (uint8_t i = 0; i < this->sec_count_; i++) {
          if (p[i] == SECURITY_NONE || (p[i] == SECURITY_VNC_AUTH && has_key)) {
            chosen = p[i];
            break;
          }
          wants_password |= p[i] == SECURITY_VNC_AUTH;
        }
        this->rx_pos_ += this->sec_count_;
        if (chosen == 0) {
          return this->fail_(VncFailure::VNC_FAILURE_CONFIG, wants_password ? LOG_STR("The server requires a password")
                                                                            : LOG_STR("No supported security type"));
        }
        if (!this->send_(&chosen, 1) || !this->select_security_(chosen)) {
          return this->fail_send_();
        }
        break;
      }
      case VncStep::VNC_STEP_SECURITY_TYPE_33: {
        if (avail < 4) {
          return VncParse::VNC_PARSE_MORE;
        }
        const uint32_t type = read_be32(p);
        this->rx_pos_ += 4;
        if (type == 0) {
          this->step_ = VncStep::VNC_STEP_REASON_LENGTH;
        } else if (type == SECURITY_NONE || (type == SECURITY_VNC_AUTH && this->has_auth_key_())) {
          if (!this->select_security_(type)) {
            return this->fail_send_();
          }
        } else {
          return this->fail_(VncFailure::VNC_FAILURE_CONFIG, type == SECURITY_VNC_AUTH
                                                                 ? LOG_STR("The server requires a password")
                                                                 : LOG_STR("No supported security type"));
        }
        break;
      }
#ifdef USE_VNC_AUTH
      case VncStep::VNC_STEP_CHALLENGE: {
        if (avail < 16) {
          return VncParse::VNC_PARSE_MORE;
        }
        uint8_t *challenge = this->rx_ + this->rx_pos_;
        vnc_auth_response(this->auth_key_, challenge);
        if (!this->send_(challenge, 16)) {
          return this->fail_send_();
        }
        this->rx_pos_ += 16;
        this->step_ = VncStep::VNC_STEP_SECURITY_RESULT;
        break;
      }
#endif
      case VncStep::VNC_STEP_SECURITY_RESULT: {
        if (avail < 4) {
          return VncParse::VNC_PARSE_MORE;
        }
        const uint32_t result = read_be32(p);
        this->rx_pos_ += 4;
        if (result == 0) {
          if (!this->send_client_init_()) {
            return this->fail_send_();
          }
          this->step_ = VncStep::VNC_STEP_SERVER_INIT;
          break;
        }
        // Fixed here, so a drop while the reason arrives cannot turn a wrong password into a retry.
#ifdef USE_VNC_AUTH
        this->reason_failure_ =
            this->sec_type_ == SECURITY_VNC_AUTH ? VncFailure::VNC_FAILURE_AUTH : VncFailure::VNC_FAILURE_CONFIG;
#else
        this->reason_failure_ = VncFailure::VNC_FAILURE_CONFIG;
#endif
        if (this->minor_ == 8) {
          this->step_ = VncStep::VNC_STEP_REASON_LENGTH;
          break;
        }
        return this->fail_(this->reason_failure_, LOG_STR("Security handshake failed"));
      }
      case VncStep::VNC_STEP_REASON_LENGTH: {
        if (avail < 4) {
          return VncParse::VNC_PARSE_MORE;
        }
        this->text_len_ = static_cast<uint8_t>(std::min(read_be32(p), MAX_TEXT_LEN));
        this->rx_pos_ += 4;
        this->step_ = VncStep::VNC_STEP_REASON;
        break;
      }
      case VncStep::VNC_STEP_REASON: {
        if (avail < this->text_len_) {
          return VncParse::VNC_PARSE_MORE;
        }
        ESP_LOGE(TAG, "Server: %.*s", static_cast<int>(this->text_len_), reinterpret_cast<const char *>(p));
        return this->fail_(this->reason_failure_, LOG_STR("Connection refused"));
      }
      case VncStep::VNC_STEP_SERVER_INIT: {
        if (avail < 24) {
          return VncParse::VNC_PARSE_MORE;
        }
        this->fb_w_ = read_be16(p);
        this->fb_h_ = read_be16(p + 2);
        // Reused for the length of the name that follows.
        this->skip_left_ = read_be32(p + 20);
        this->rx_pos_ += 24;
        if (this->fb_w_ == 0 || this->fb_h_ == 0) {
          return this->fail_(VncFailure::VNC_FAILURE_TRANSIENT, LOG_STR("The server screen is empty"));
        }
        this->text_len_ = static_cast<uint8_t>(std::min(this->skip_left_, MAX_TEXT_LEN));
        this->step_ = VncStep::VNC_STEP_SERVER_NAME;
        break;
      }
      case VncStep::VNC_STEP_SERVER_NAME: {
        if (avail < this->text_len_) {
          return VncParse::VNC_PARSE_MORE;
        }
        ESP_LOGI(TAG, "Server '%.*s', %ux%u, RFB 3.%u", static_cast<int>(this->text_len_),
                 reinterpret_cast<const char *>(p), this->fb_w_, this->fb_h_, this->minor_);
        this->rx_pos_ += this->text_len_;
        this->skip_(this->skip_left_ - this->text_len_, VncStep::VNC_STEP_SESSION_START);
        break;
      }
      case VncStep::VNC_STEP_SESSION_START:
        if (!this->start_session_(App.get_loop_component_start_time())) {
          return this->fail_send_();
        }
        this->step_ = VncStep::VNC_STEP_MESSAGE;
        break;
      case VncStep::VNC_STEP_MESSAGE: {
        if (avail < 1) {
          return VncParse::VNC_PARSE_MORE;
        }
        switch (p[0]) {
          case MSG_FRAMEBUFFER_UPDATE:
            if (avail < 4) {
              return VncParse::VNC_PARSE_MORE;
            }
            this->rects_left_ = read_be16(p + 2);
            this->rx_pos_ += 4;
            this->in_update_ = true;
            this->update_start_ms_ = millis();
            this->update_start_bytes_ = this->rx_total_ - (this->rx_len_ - this->rx_pos_);
            this->drawn_tiles_ = 0;
            this->rect_done_();
            break;
          case MSG_SET_COLOUR_MAP:
            if (avail < 6) {
              return VncParse::VNC_PARSE_MORE;
            }
            this->rx_pos_ += 6;
            this->skip_(6u * read_be16(p + 4), VncStep::VNC_STEP_MESSAGE);
            break;
          case MSG_BELL:
            this->rx_pos_ += 1;
            break;
          case MSG_SERVER_CUT_TEXT:
            if (avail < 8) {
              return VncParse::VNC_PARSE_MORE;
            }
            this->rx_pos_ += 8;
            this->skip_(read_be32(p + 4), VncStep::VNC_STEP_MESSAGE);
            break;
          default:
            ESP_LOGW(TAG, "Unknown message type %u", p[0]);
            return this->fail_(VncFailure::VNC_FAILURE_TRANSIENT, LOG_STR("Protocol error"));
        }
        break;
      }
      case VncStep::VNC_STEP_RECT: {
        if (this->display_busy_) {
          return VncParse::VNC_PARSE_PAUSE;
        }
        if (avail < 12) {
          return VncParse::VNC_PARSE_MORE;
        }
        this->rect_x_ = read_be16(p);
        this->rect_y_ = read_be16(p + 2);
        this->rect_w_ = read_be16(p + 4);
        this->rect_h_ = read_be16(p + 6);
        const auto encoding = static_cast<int32_t>(read_be32(p + 8));
        this->rx_pos_ += 12;
        this->rects_left_--;
        if (encoding == ENCODING_RAW || encoding == ENCODING_HEXTILE) {
          if (static_cast<uint32_t>(this->rect_x_) + this->rect_w_ > this->fb_w_ ||
              static_cast<uint32_t>(this->rect_y_) + this->rect_h_ > this->fb_h_) {
            ESP_LOGW(TAG, "Rectangle %ux%u at %u,%u is outside the %ux%u screen", this->rect_w_, this->rect_h_,
                     this->rect_x_, this->rect_y_, this->fb_w_, this->fb_h_);
            return this->fail_(VncFailure::VNC_FAILURE_TRANSIENT, LOG_STR("Protocol error"));
          }
          if (this->rect_w_ == 0 || this->rect_h_ == 0) {
            this->rect_done_();
          } else if (encoding == ENCODING_RAW) {
            this->forget_tiles_(this->rect_x_, this->rect_y_, this->rect_w_, this->rect_h_);
            this->pos_x_ = 0;
            this->pos_y_ = 0;
            this->step_ = VncStep::VNC_STEP_RAW;
          } else {
            this->pos_x_ = this->rect_x_;
            this->pos_y_ = this->rect_y_;
            this->step_ = VncStep::VNC_STEP_TILE;
          }
        } else if (encoding == ENCODING_RICH_CURSOR) {
          if (this->rect_w_ > MAX_CURSOR_SIDE || this->rect_h_ > MAX_CURSOR_SIDE) {
            return this->fail_(VncFailure::VNC_FAILURE_TRANSIENT, LOG_STR("Protocol error"));
          }
          const uint32_t size = static_cast<uint32_t>(this->rect_w_) * this->rect_h_ * this->bpp_ +
                                ((this->rect_w_ + 7u) / 8u) * this->rect_h_;
          this->skip_(size, VncStep::VNC_STEP_RECT_DONE);
        } else {
          ESP_LOGW(TAG, "Unsupported encoding %" PRId32, encoding);
          return this->fail_(VncFailure::VNC_FAILURE_TRANSIENT, LOG_STR("Protocol error"));
        }
        break;
      }
      case VncStep::VNC_STEP_RECT_DONE:
        this->rect_done_();
        break;
      case VncStep::VNC_STEP_RAW: {
        if (this->display_busy_) {
          return VncParse::VNC_PARSE_PAUSE;
        }
        const size_t pixels = avail / this->bpp_;
        if (pixels == 0) {
          return VncParse::VNC_PARSE_MORE;
        }
        if (this->budget_exhausted_()) {
          return VncParse::VNC_PARSE_PAUSE;
        }
        const auto count = static_cast<uint16_t>(std::min<size_t>(pixels, this->rect_w_ - this->pos_x_));
        this->draw_row_(this->rect_x_ + this->pos_x_, this->rect_y_ + this->pos_y_, count, p);
        this->rx_pos_ += count * this->bpp_;
        this->pos_x_ += count;
        if (this->pos_x_ == this->rect_w_) {
          this->pos_x_ = 0;
          if (++this->pos_y_ == this->rect_h_) {
            this->rect_done_();
          }
        }
        break;
      }
      case VncStep::VNC_STEP_TILE: {
        if (this->display_busy_) {
          return VncParse::VNC_PARSE_PAUSE;
        }
        if (avail < 1) {
          return VncParse::VNC_PARSE_MORE;
        }
        if (this->budget_exhausted_()) {
          return VncParse::VNC_PARSE_PAUSE;
        }
        const uint8_t flags = p[0];
        this->tile_w_ = std::min<uint32_t>(16, static_cast<uint32_t>(this->rect_x_) + this->rect_w_ - this->pos_x_);
        this->tile_h_ = std::min<uint32_t>(16, static_cast<uint32_t>(this->rect_y_) + this->rect_h_ - this->pos_y_);
        this->tile_hidden_ = this->pos_x_ >= this->vis_w_ || this->pos_y_ >= this->vis_h_;
        // With the raw bit set the other bits mean nothing and the pixels follow at once.
        if (flags & HEXTILE_RAW) {
          this->rx_pos_ += 1;
          this->tile_fill_ = 0;
          this->step_ = VncStep::VNC_STEP_TILE_RAW;
          break;
        }
        const size_t need = 1 + ((flags & HEXTILE_BACKGROUND) ? this->bpp_ : 0) +
                            ((flags & HEXTILE_FOREGROUND) ? this->bpp_ : 0) + ((flags & HEXTILE_ANY_SUBRECTS) ? 1 : 0);
        if (avail < need) {
          return VncParse::VNC_PARSE_MORE;
        }
        const uint8_t *q = p + 1;
        if (flags & HEXTILE_BACKGROUND) {
          std::memcpy(this->hex_bg_, q, this->bpp_);
          q += this->bpp_;
        }
        if (flags & HEXTILE_FOREGROUND) {
          std::memcpy(this->hex_fg_, q, this->bpp_);
          q += this->bpp_;
        }
        this->subrects_left_ = (flags & HEXTILE_ANY_SUBRECTS) ? *q : 0;
        this->subrects_coloured_ = (flags & HEXTILE_SUBRECTS_COLOURED) != 0;
        this->rx_pos_ += need;
        this->fill_tile_(0, 0, this->tile_w_, this->tile_h_, this->hex_bg_);
        this->step_ = VncStep::VNC_STEP_SUBRECTS;
        break;
      }
      case VncStep::VNC_STEP_TILE_RAW: {
        const size_t total = static_cast<size_t>(this->tile_w_) * this->tile_h_ * this->bpp_;
        const size_t count = std::min(avail, total - this->tile_fill_);
        if (count == 0) {
          return VncParse::VNC_PARSE_MORE;
        }
        std::memcpy(this->tile_ + this->tile_fill_, p, count);
        this->rx_pos_ += count;
        this->tile_fill_ += count;
        if (this->tile_fill_ == total) {
          this->draw_tile_();
          this->next_tile_();
        }
        break;
      }
      case VncStep::VNC_STEP_SUBRECTS: {
        if (this->subrects_left_ == 0) {
          this->draw_tile_();
          this->next_tile_();
          break;
        }
        const size_t need = (this->subrects_coloured_ ? this->bpp_ : 0) + 2;
        if (avail < need) {
          return VncParse::VNC_PARSE_MORE;
        }
        const uint8_t *pixel = this->hex_fg_;
        const uint8_t *q = p;
        if (this->subrects_coloured_) {
          pixel = q;
          q += this->bpp_;
        }
        this->fill_tile_(q[0] >> 4, q[0] & 0x0F, (q[1] >> 4) + 1, (q[1] & 0x0F) + 1, pixel);
        this->rx_pos_ += need;
        this->subrects_left_--;
        break;
      }
      case VncStep::VNC_STEP_SKIP: {
        const auto count = static_cast<uint32_t>(std::min<size_t>(avail, this->skip_left_));
        this->rx_pos_ += count;
        this->skip_left_ -= count;
        if (this->skip_left_ != 0) {
          return VncParse::VNC_PARSE_MORE;
        }
        this->step_ = this->after_skip_;
        break;
      }
    }
  }
}

void VncClient::rect_done_() {
  if (this->rects_left_ == 0) {
    this->frame_done_();
  } else {
    this->step_ = VncStep::VNC_STEP_RECT;
  }
}

void VncClient::next_tile_() {
  this->step_ = VncStep::VNC_STEP_TILE;
  if (static_cast<uint32_t>(this->pos_x_) + 16 < static_cast<uint32_t>(this->rect_x_) + this->rect_w_) {
    this->pos_x_ += 16;
    return;
  }
  this->pos_x_ = this->rect_x_;
  if (static_cast<uint32_t>(this->pos_y_) + 16 < static_cast<uint32_t>(this->rect_y_) + this->rect_h_) {
    this->pos_y_ += 16;
    return;
  }
  this->rect_done_();
}

void VncClient::frame_done_() {
  this->in_update_ = false;
  this->request_outstanding_ = false;
  this->step_ = VncStep::VNC_STEP_MESSAGE;
  if (this->frame_drawn_) {
    const uint32_t now = App.get_loop_component_start_time();
    this->frame_drawn_ = false;
    if (!this->refresh_pending_) {
      this->first_change_ms_ = now;
    }
    this->last_change_ms_ = now;
    this->refresh_pending_ = true;
    ESP_LOGD(TAG, "Update: %" PRIu32 " bytes in %" PRIu32 "ms, %u hextile tiles drawn",
             this->rx_total_ - (this->rx_len_ - this->rx_pos_) - this->update_start_bytes_,
             millis() - this->update_start_ms_, this->drawn_tiles_);
  }
}

void VncClient::draw_row_(uint16_t x, uint16_t y, uint16_t count, const uint8_t *pixels) {
  if (y >= this->vis_h_ || x >= this->vis_w_) {
    return;
  }
  const int visible = std::min<int>(count, this->vis_w_ - x);
  this->display_->draw_pixels_at(x, y, visible, 1, pixels, this->bitness_, this->big_endian_);
  this->frame_drawn_ = true;
}

void VncClient::draw_tile_() {
  if (this->tile_hidden_) {
    return;
  }
  const int w = std::min<int>(this->tile_w_, this->vis_w_ - this->pos_x_);
  const int h = std::min<int>(this->tile_h_, this->vis_h_ - this->pos_y_);
  uint32_t *cell = nullptr;
  uint32_t hash = 0;
  if (this->hash_cols_ != 0) {
    if ((this->pos_x_ | this->pos_y_) & 15) {
      // An off-grid tile spans several cells, and a later write to any of them would make its hash stale.
      this->forget_tiles_(this->pos_x_, this->pos_y_, w, h);
    } else {
      cell = this->tile_hashes_ + (this->pos_y_ / 16) * this->hash_cols_ + this->pos_x_ / 16;
      hash = this->tile_hash_();
      if (*cell == hash) {
        return;
      }
    }
  }
  this->display_->draw_pixels_at(this->pos_x_, this->pos_y_, w, h, this->tile_, this->bitness_, this->big_endian_, 0, 0,
                                 this->tile_w_ - w);
  this->frame_drawn_ = true;
  this->drawn_tiles_++;
  if (cell != nullptr) {
    *cell = hash;
  }
}

uint32_t VncClient::tile_hash_() const {
  // The tile's size, pixel size and pixels; the cell gives its place. Zero means "unknown".
  uint32_t hash = fnv1a_hash_extend(FNV1_OFFSET_BASIS,
                                    static_cast<uint32_t>(this->tile_w_) << 16 | this->tile_h_ << 8 | this->bpp_);
  const size_t len = static_cast<size_t>(this->tile_w_) * this->tile_h_ * this->bpp_;
  size_t i = 0;
  for (; i + 4 <= len; i += 4) {
    uint32_t word;
    std::memcpy(&word, this->tile_ + i, sizeof(word));  // tile_ is aligned, so this is one load
    // Byte by byte: XOR-ing whole words lets high-bit changes in two words cancel.
    hash = fnv1a_hash_extend(hash, word);
  }
  for (; i < len; i++) {
    hash = fnv1a_hash_extend(hash, this->tile_[i]);
  }
  return hash == 0 ? 1 : hash;
}

void VncClient::forget_tiles_(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  if (this->hash_cols_ == 0 || w == 0 || h == 0) {
    return;
  }
  const uint16_t col_end = std::min<uint16_t>((x + w - 1) / 16, this->hash_cols_ - 1);
  const uint16_t row_end = std::min<uint16_t>((y + h - 1) / 16, this->hash_rows_ - 1);
  for (uint16_t row = y / 16; row <= row_end; row++) {
    for (uint16_t col = x / 16; col <= col_end; col++) {
      this->tile_hashes_[row * this->hash_cols_ + col] = 0;
    }
  }
}

void VncClient::fill_tile_(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *pixel) {
  if (this->tile_hidden_ || x >= this->tile_w_ || y >= this->tile_h_) {
    return;
  }
  w = std::min<uint8_t>(w, this->tile_w_ - x);
  h = std::min<uint8_t>(h, this->tile_h_ - y);
  const size_t stride = static_cast<size_t>(this->tile_w_) * this->bpp_;
  const size_t row_len = static_cast<size_t>(w) * this->bpp_;
  uint8_t *first = this->tile_ + y * stride + x * this->bpp_;
  if (this->bpp_ == 1) {
    std::memset(first, pixel[0], w);
  } else {
    for (size_t i = 0; i < row_len; i += 2) {
      first[i] = pixel[0];
      first[i + 1] = pixel[1];
    }
  }
  for (uint8_t row = 1; row < h; row++) {
    std::memcpy(first + row * stride, first, row_len);
  }
}

bool VncClient::start_session_(uint32_t now) {
  VncPixelFormat format = this->pixel_format_;
  if (format == VncPixelFormat::VNC_PIXEL_FORMAT_AUTO) {
    format = this->display_->get_display_type() == display::DISPLAY_TYPE_BINARY
                 ? VncPixelFormat::VNC_PIXEL_FORMAT_RGB332
                 : VncPixelFormat::VNC_PIXEL_FORMAT_RGB565;
  }
  const bool rgb565 = format == VncPixelFormat::VNC_PIXEL_FORMAT_RGB565;
  this->bpp_ = rgb565 ? 2 : 1;
  this->bitness_ = rgb565 ? display::COLOR_BITNESS_565 : display::COLOR_BITNESS_332;

  const int disp_w = this->display_->get_width();
  const int disp_h = this->display_->get_height();
  this->vis_w_ = std::min<int>(this->fb_w_, disp_w);
  this->vis_h_ = std::min<int>(this->fb_h_, disp_h);
  if (this->fb_w_ != disp_w || this->fb_h_ != disp_h) {
    ESP_LOGW(TAG, "Server screen is %ux%u but the display is %dx%d; showing the top-left %ux%u", this->fb_w_,
             this->fb_h_, disp_w, disp_h, this->vis_w_, this->vis_h_);
  }
  const uint16_t cols = (this->vis_w_ + 15) / 16;
  const uint16_t rows = (this->vis_h_ + 15) / 16;
  const auto rotation = static_cast<uint8_t>(this->display_->get_rotation() / 90);
  const bool rotated = rotation != this->hash_rotation_;
  this->hash_rotation_ = rotation;
  if (this->tile_hashes_ == nullptr) {
    this->hash_cols_ = 0;
    this->hash_rows_ = 0;
  } else if (static_cast<uint32_t>(cols) * rows > this->hash_capacity_) {
    ESP_LOGW(TAG, "Screen has %u tiles, more than the %u cached; every tile is drawn", cols * rows,
             this->hash_capacity_);
    this->hash_cols_ = 0;
    this->hash_rows_ = 0;
  } else if (cols != this->hash_cols_ || rows != this->hash_rows_ || rotated) {
    this->hash_cols_ = cols;
    this->hash_rows_ = rows;
    this->clear_tiles_();
  }

  // SetPixelFormat: true colour, in the byte order the display takes
  uint8_t pixel_format[20] = {};
  pixel_format[4] = rgb565 ? 16 : 8;
  pixel_format[5] = rgb565 ? 16 : 8;
  pixel_format[6] = this->big_endian_;
  pixel_format[7] = 1;
  write_be16(pixel_format + 8, rgb565 ? 31 : 7);
  write_be16(pixel_format + 10, rgb565 ? 63 : 7);
  write_be16(pixel_format + 12, rgb565 ? 31 : 3);
  pixel_format[14] = rgb565 ? 11 : 5;
  pixel_format[15] = rgb565 ? 5 : 2;
  if (!this->send_(pixel_format, sizeof(pixel_format))) {
    return false;
  }

  // SetEncodings
  uint8_t encodings[4 + 4 * std::size(ENCODINGS)] = {2};
  write_be16(encodings + 2, std::size(ENCODINGS));
  for (size_t i = 0; i < std::size(ENCODINGS); i++) {
    write_be32(encodings + 4 + 4 * i, static_cast<uint32_t>(ENCODINGS[i]));
  }
  if (!this->send_(encodings, sizeof(encodings)) || !this->send_update_request_(false, now)) {
    return false;
  }
  this->state_ = VncState::VNC_STATE_RUNNING;
  this->session_start_ms_ = now;
  this->clear_link_warning_();
  this->status_clear_error();
  this->connect_callback_.call();
  return true;
}

void VncClient::pace_(uint32_t now) {
  if (this->display_busy_ || this->in_update_ || now - this->last_request_ms_ < MIN_REQUEST_SPACING_MS) {
    return;
  }
  if (this->need_full_) {
    if (this->send_update_request_(false, now)) {
      this->need_full_ = false;
    }
    return;
  }
  // Repeating a parked request costs nothing on the server and detects a dead link.
  if (!this->request_outstanding_ ||
      (now - this->last_rx_ms_ >= HEARTBEAT_MS && now - this->last_request_ms_ >= HEARTBEAT_MS)) {
    this->send_update_request_(true, now);
  }
}

bool VncClient::send_(const uint8_t *data, size_t len) {
  if (this->link_.tx_free() < len) {
    this->link_.flush_tx();
    if (this->link_.tx_free() < len) {
      return false;
    }
  }
  this->link_.queue(data, len);
  return true;
}

bool VncClient::send_update_request_(bool incremental, uint32_t now) {
  uint8_t msg[10] = {3, static_cast<uint8_t>(incremental ? 1 : 0)};
  write_be16(msg + 6, this->vis_w_);
  write_be16(msg + 8, this->vis_h_);
  if (!this->send_(msg, sizeof(msg))) {
    return false;
  }
  this->request_outstanding_ = true;
  this->last_request_ms_ = now;
  return true;
}

}  // namespace esphome::vnc
