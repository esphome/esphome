#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

#include "dsp408.h"
#include "protocol.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/text/text.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"

#include <algorithm>
#include <cmath>

namespace esphome {
namespace dsp408 {

static const char *const TAG = "dsp408";

static const char *cmd_name(uint32_t cmd) {
  switch (cmd) {
    case CMD_CONNECT:
      return "CONNECT";
    case CMD_GET_INFO:
      return "GET_INFO";
    case CMD_MASTER:
      return "MASTER";
    case CMD_IDLE_POLL:
      return "IDLE_POLL";
    case CMD_PRESET_NAME:
      return "PRESET_NAME";
    case CMD_STATUS:
      return "STATUS";
    default:
      if (cmd >= 0x7700 && cmd <= 0x7707)
        return "READ_CH";
      if (cmd >= 0x1F00 && cmd <= 0x1F07)
        return "WRITE_CH";
      if (cmd >= 0x12000 && cmd <= 0x12007)
        return "WRITE_XOVER";
      return "?";
  }
}

void DSP408::setup() {
  USBClient::setup();
  ESP_LOGCONFIG(TAG, "DSP-408 USB host client initialised (VID 0x%04X PID 0x%04X)", DSP408_VID, DSP408_PID);
}

void DSP408::dump_config() {
  USBClient::dump_config();
  static const char *const PHASE_NAMES[] = {
      "IDLE", "WAIT_AFTER_CONNECT", "SENT_CONNECT", "SENT_GET_INFO", "SENT_GET_MASTER", "WARMUP_CHANNELS", "RUNNING",
  };
  uint8_t phase_idx = static_cast<uint8_t>(this->phase_);
  const char *phase_name = (phase_idx < 7) ? PHASE_NAMES[phase_idx] : "?";

  ESP_LOGCONFIG(TAG,
                "  DSP-408\n"
                "    Interface claimed: %s (intf=%u)\n"
                "    EP-IN:  0x%02X (mps=%u)\n"
                "    EP-OUT: 0x%02X (mps=%u)\n"
                "    Phase: %s\n"
                "    Master: %s (%+d dB, %s)\n"
                "    Preset name: %s\n"
                "    Master poll interval: %u ms\n"
                "    Republish stagger: %u ms",
                YESNO(this->interface_claimed_), this->intf_number_, this->ep_in_ ? this->ep_in_->bEndpointAddress : 0,
                this->ep_in_ ? this->ep_in_->wMaxPacketSize : 0, this->ep_out_ ? this->ep_out_->bEndpointAddress : 0,
                this->ep_out_ ? this->ep_out_->wMaxPacketSize : 0, phase_name, YESNO(this->master_known_),
                this->master_db_, this->master_muted_ ? "muted" : "audible",
                this->preset_name_known_ ? this->preset_name_ : "<unknown>",
                static_cast<unsigned>(MASTER_POLL_INTERVAL_MS), static_cast<unsigned>(REPUBLISH_STAGGER_MS));
  // Per-channel dump if any state has been read
  for (uint8_t ch = 0; ch < 8; ch++) {
    const ChannelState &c = this->ch_state_[ch];
    if (!c.primed)
      continue;
    ESP_LOGCONFIG(TAG, "    Ch%u: %+.1f dB %s%s name=\"%s\"  HPF=%uHz/F=%u/S=%u  LPF=%uHz/F=%u/S=%u", ch,
                  static_cast<float>(c.db_x10) / 10.0f, c.muted ? "(muted)" : "(audible)", c.polar ? " (POLAR)" : "",
                  c.name, c.hpf_freq_hz, c.hpf_filter, c.hpf_slope, c.lpf_freq_hz, c.lpf_filter, c.lpf_slope);
  }
}

void DSP408::on_connected() {
  ESP_LOGI(TAG, "USB device opened — searching for HID interface...");
  if (!this->find_hid_endpoints_()) {
    ESP_LOGE(TAG, "No HID interface with EP-IN + EP-OUT found");
    this->status_set_error(LOG_STR("No HID interface"));
    this->disconnect();
    return;
  }

  auto err = usb_host_interface_claim(this->handle_, this->device_handle_, this->intf_number_, /*alt=*/0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_host_interface_claim(intf=%u) failed: %s", this->intf_number_, esp_err_to_name(err));
    this->status_set_error(LOG_STR("interface_claim failed"));
    this->disconnect();
    return;
  }
  this->interface_claimed_ = true;
  this->status_clear_error();
  ESP_LOGI(TAG, "Claimed HID interface %u, EP-IN=0x%02X EP-OUT=0x%02X", this->intf_number_,
           this->ep_in_->bEndpointAddress, this->ep_out_->bEndpointAddress);

  // Kick off the IN pump.
  this->input_pending_.store(false);
  this->start_input_();

  // Defer the first OUT command by a short interval so the device
  // settles after enumeration. dsp408-py does this implicitly via the
  // hidapi open_path round-trip.
  this->phase_ = StartupPhase::WAIT_AFTER_CONNECT;
  this->phase_started_ms_ = millis();
  this->connected_at_ms_ = this->phase_started_ms_;
  this->cmd_in_flight_ = 0;
  this->enable_loop();
}

void DSP408::on_disconnected() {
  ESP_LOGI(TAG, "USB device disconnected");
  // Reset multi-frame state so a stale partial blob doesn't confuse a
  // subsequent re-attach.
  this->mf_in_progress_ = false;
  this->mf_collected_len_ = 0;
  if (this->interface_claimed_ && this->ep_in_ != nullptr) {
    usb_host_endpoint_halt(this->device_handle_, this->ep_in_->bEndpointAddress);
    usb_host_endpoint_flush(this->device_handle_, this->ep_in_->bEndpointAddress);
  }
  if (this->interface_claimed_ && this->ep_out_ != nullptr) {
    usb_host_endpoint_halt(this->device_handle_, this->ep_out_->bEndpointAddress);
    usb_host_endpoint_flush(this->device_handle_, this->ep_out_->bEndpointAddress);
  }
  if (this->interface_claimed_) {
    usb_host_interface_release(this->handle_, this->device_handle_, this->intf_number_);
    this->interface_claimed_ = false;
  }
  this->ep_in_ = nullptr;
  this->ep_out_ = nullptr;
  this->intf_number_ = 0xFF;
  this->phase_ = StartupPhase::IDLE;
  this->cmd_in_flight_ = 0;
  this->master_known_ = false;
  this->snapshot_pending_ch_ = 0xFF;
  USBClient::on_disconnected();
}

bool DSP408::find_hid_endpoints_() {
  const usb_config_desc_t *config_desc;
  if (usb_host_get_active_config_descriptor(this->device_handle_, &config_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_active_config_descriptor failed");
    return false;
  }
  int conf_offset = 0;
  for (uint8_t intf_idx = 0; intf_idx < config_desc->bNumInterfaces; intf_idx++) {
    const auto *intf = usb_parse_interface_descriptor(config_desc, intf_idx, /*alt=*/0, &conf_offset);
    if (!intf)
      continue;
    ESP_LOGD(TAG, "intf %u: class=0x%02X subclass=0x%02X proto=0x%02X eps=%u", intf->bInterfaceNumber,
             intf->bInterfaceClass, intf->bInterfaceSubClass, intf->bInterfaceProtocol, intf->bNumEndpoints);
    if (intf->bInterfaceClass != 0x03) {  // not HID — skip
      continue;
    }
    const usb_ep_desc_t *ep_in = nullptr;
    const usb_ep_desc_t *ep_out = nullptr;
    for (uint8_t i = 0; i < intf->bNumEndpoints; i++) {
      int ep_offset = conf_offset;
      const auto *ep = usb_parse_endpoint_descriptor_by_index(intf, i, config_desc->wTotalLength, &ep_offset);
      if (!ep)
        break;
      ESP_LOGD(TAG, "  ep %u: addr=0x%02X attr=0x%02X mps=%u", i, ep->bEndpointAddress, ep->bmAttributes,
               ep->wMaxPacketSize);
      if ((ep->bmAttributes & 0x03) != USB_BM_ATTRIBUTES_XFER_INT)
        continue;
      if (ep->bEndpointAddress & usb_host::USB_DIR_IN) {
        if (ep_in == nullptr)
          ep_in = ep;
      } else {
        if (ep_out == nullptr)
          ep_out = ep;
      }
    }
    if (ep_in != nullptr && ep_out != nullptr) {
      this->intf_number_ = intf->bInterfaceNumber;
      this->ep_in_ = ep_in;
      this->ep_out_ = ep_out;
      return true;
    }
  }
  return false;
}

void DSP408::start_input_() {
  if (!this->interface_claimed_ || this->ep_in_ == nullptr)
    return;
  // Compare-exchange to ensure single in-flight IN at a time.
  bool expected = false;
  if (!this->input_pending_.compare_exchange_strong(expected, true))
    return;
  auto cb = [this](const usb_host::TransferStatus &status) { this->on_in_report_usb_task_(status); };
  if (!this->transfer_in(this->ep_in_->bEndpointAddress, cb, 64)) {
    ESP_LOGE(TAG, "transfer_in submit failed");
    this->input_pending_.store(false);
  }
}

// USB-task context. Keep it short: copy data into a queued slot,
// wake main loop, restart IN.
void DSP408::on_in_report_usb_task_(const usb_host::TransferStatus &status) {
  if (status.success && status.data_len > 0 && status.data_len <= 64) {
    InReport *rep = this->in_pool_.allocate();
    if (rep != nullptr) {
      memcpy(rep->data, status.data, status.data_len);
      rep->length = static_cast<uint16_t>(status.data_len);
      this->in_queue_.push(rep);
      this->enable_loop_soon_any_context();
      App.wake_loop_threadsafe();
    } else {
      // Pool empty — drop the report; we'll catch up next iteration.
      ESP_LOGW(TAG, "IN pool exhausted, dropping report");
    }
  } else if (!status.success) {
    ESP_LOGW(TAG, "IN transfer failed status=0x%X", status.error_code);
  }
  this->input_pending_.store(false);
  // Restart the IN pump unconditionally — the device may still be alive.
  this->start_input_();
}

void DSP408::loop() {
  // Always drain the USB-event queue inherited from USBClient.
  bool had_work = this->process_usb_events_();

  // Drain inbound HID reports.
  this->process_in_queue_();

  // Drive the startup state machine.
  uint32_t now = millis();
  switch (this->phase_) {
    case StartupPhase::WAIT_AFTER_CONNECT:
      // 50ms grace post-claim before sending CMD_CONNECT
      if (now - this->phase_started_ms_ >= 50) {
        if (this->send_connect_()) {
          this->phase_ = StartupPhase::SENT_CONNECT;
          this->phase_started_ms_ = now;
          had_work = true;
        }
      }
      break;
    case StartupPhase::SENT_CONNECT:
    case StartupPhase::SENT_GET_INFO:
    case StartupPhase::SENT_GET_MASTER:
    case StartupPhase::WARMUP_CHANNELS:
      // Watchdog timeout — if no reply in REQUEST_TIMEOUT_MS, retry the
      // whole startup from CMD_CONNECT.
      if (this->cmd_in_flight_ != 0 && now - this->cmd_in_flight_started_ms_ > REQUEST_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Timeout waiting for reply to %s — restarting startup", cmd_name(this->cmd_in_flight_));
        this->cmd_in_flight_ = 0;
        this->mf_in_progress_ = false;
        this->mf_collected_len_ = 0;
        this->phase_ = StartupPhase::WAIT_AFTER_CONNECT;
        this->phase_started_ms_ = now;
      }
      break;
    case StartupPhase::RUNNING:
      // Periodic master state poll if no command in flight.
      if (this->cmd_in_flight_ == 0 && now - this->last_master_poll_ms_ > MASTER_POLL_INTERVAL_MS) {
        this->send_get_master_();
        this->last_master_poll_ms_ = now;
        had_work = true;
      }
      // Round-robin re-publish of ONE channel's cached state per
      // REPUBLISH_STAGGER_MS tick. Channel state is only read on
      // warmup; if the API client wasn't connected then (or churned
      // its connection), entities stay 'unavailable' forever. We
      // re-publish to make any HA reconnect see fresh state — but
      // staggered to avoid bursting 8 × 6 entity states in one tick,
      // which overruns the API send buffer for large surfaces.
      if (now - this->last_republish_ms_ > REPUBLISH_STAGGER_MS) {
        uint8_t ch = this->republish_channel_rr_;
        if (this->ch_state_[ch].primed) {
          this->publish_channel_state_(ch);
        }
        this->republish_channel_rr_ = (ch + 1) & 0x07;
        this->last_republish_ms_ = now;
        // Also republish preset name occasionally on the 0th channel
        // tick so it doesn't fall through cracks.
        if (ch == 0 && this->preset_name_known_ && this->preset_name_text_ != nullptr) {
          this->preset_name_text_->publish_state(std::string(this->preset_name_));
        }
        had_work = true;
      }
      break;
    case StartupPhase::IDLE:
    default:
      break;
  }

  // ESPHome will keep loop() running while we have work; otherwise we
  // can ride along with the next inbound report or scheduled poll.
  if (!had_work) {
    // Don't disable_loop() here — USBClient's loop() handles event-pump
    // and we may still be polling. Keep it cheap and let ESPHome's
    // scheduler run us at its normal cadence.
  }
}

void DSP408::process_in_queue_() {
  InReport *rep;
  while ((rep = this->in_queue_.pop()) != nullptr) {
    this->dispatch_frame_(*rep);
    this->in_pool_.release(rep);
  }
}

void DSP408::dispatch_frame_(const InReport &report) {
  // Multi-frame continuation path — when reassembly is in progress, we
  // expect the next inbound HID report(s) to be RAW continuation bytes
  // (no DSP-408 header). The Python lib does the same: it stops parsing
  // headers on continuation reports and just concatenates the next
  // (declared_len - collected) bytes. The chk + end markers live
  // in-stream at offset declared_len within the continuation payload
  // of the LAST continuation report.
  if (this->mf_in_progress_) {
    size_t want = this->mf_declared_len_ - this->mf_collected_len_;
    size_t take = report.length;
    if (take > want)
      take = want;
    memcpy(this->mf_buffer_ + this->mf_collected_len_, report.data, take);
    this->mf_collected_len_ += take;
    if (this->mf_collected_len_ >= this->mf_declared_len_) {
      // Fully collected. Validate checksum (lenient — Python lib doesn't
      // either; the firmware appears to always emit valid checksums but
      // we don't gate dispatch on it).
      uint8_t want_chk = xor_checksum(this->mf_header_, sizeof(this->mf_header_));
      want_chk ^= xor_checksum(this->mf_buffer_, this->mf_declared_len_);
      uint8_t got_chk = 0;
      if (take < report.length)
        got_chk = report.data[take];
      else
        got_chk = 0xFF;  // chk wasn't in this report; we'd need yet another
      bool chk_ok = (got_chk == want_chk);
      ESP_LOGV(TAG, "<- multi-frame complete cmd=0x%X len=%u chk=%s", static_cast<unsigned>(this->mf_cmd_),
               this->mf_declared_len_, chk_ok ? "ok" : "MISMATCH");

      uint32_t cmd = this->mf_cmd_;
      this->mf_in_progress_ = false;
      this->mf_collected_len_ = 0;

      // Clear in-flight marker — match the cmd loosely (firmware can
      // return seq=0 for reads).
      if (this->cmd_in_flight_ == cmd)
        this->cmd_in_flight_ = 0;

      // Dispatch the assembled blob.
      if (cmd >= CMD_READ_CHANNEL_BASE && cmd < CMD_READ_CHANNEL_BASE + 8) {
        uint8_t ch = static_cast<uint8_t>(cmd - CMD_READ_CHANNEL_BASE);
        this->handle_channel_state_blob_(ch, this->mf_buffer_, this->mf_declared_len_);
      } else {
        ESP_LOGW(TAG, "Multi-frame complete for unhandled cmd 0x%X", static_cast<unsigned>(cmd));
      }
    }
    return;
  }

  ParsedFrame f = parse_frame(report.data, report.length);
  if (!f.valid) {
    ESP_LOGV(TAG, "Inbound report not a DSP-408 frame (len=%u)", report.length);
    return;
  }
  // Only accept replies (RESP / WRITE_ACK).
  if (f.direction != DIR_RESP && f.direction != DIR_WRITE_ACK) {
    ESP_LOGV(TAG, "Inbound frame with unexpected direction 0x%02X", f.direction);
    return;
  }
  ESP_LOGV(TAG, "<- dir=0x%02X cat=0x%02X cmd=0x%X seq=%u len=%u%s", f.direction, f.category,
           static_cast<unsigned>(f.cmd), f.seq, f.payload_len, f.is_multi_frame_first ? " (MF)" : "");

  // Multi-frame first — start reassembly, don't dispatch yet.
  if (f.is_multi_frame_first) {
    if (f.payload_len > sizeof(this->mf_buffer_)) {
      ESP_LOGE(TAG, "Multi-frame declared len=%u exceeds buffer (%u)", f.payload_len,
               static_cast<unsigned>(sizeof(this->mf_buffer_)));
      return;
    }
    this->mf_in_progress_ = true;
    this->mf_cmd_ = f.cmd;
    this->mf_direction_ = f.direction;
    this->mf_category_ = f.category;
    this->mf_declared_len_ = f.payload_len;
    this->mf_collected_len_ = 0;
    memcpy(this->mf_header_, report.data + 4, sizeof(this->mf_header_));
    // Copy first chunk of payload (50 bytes for a multi-frame first).
    memcpy(this->mf_buffer_, f.payload, f.payload_bytes_in_frame);
    this->mf_collected_len_ = f.payload_bytes_in_frame;
    return;
  }

  // Single-frame: clear in-flight then dispatch by cmd.
  if (this->cmd_in_flight_ == f.cmd)
    this->cmd_in_flight_ = 0;

  switch (f.cmd) {
    case CMD_CONNECT:
      this->handle_connect_reply_(f.payload, f.payload_bytes_in_frame);
      break;
    case CMD_GET_INFO:
      this->handle_get_info_reply_(f.payload, f.payload_bytes_in_frame);
      break;
    case CMD_MASTER:
      this->handle_master_reply_(f.payload, f.payload_bytes_in_frame, f.direction == DIR_WRITE_ACK);
      break;
    case CMD_PRESET_NAME:
      this->handle_preset_name_reply_(f.payload, f.payload_bytes_in_frame, f.direction == DIR_WRITE_ACK);
      break;
    default:
      if (f.cmd >= CMD_WRITE_CHANNEL_BASE && f.cmd < CMD_WRITE_CHANNEL_BASE + 8) {
        this->handle_channel_write_ack_(f.cmd, f.payload, f.payload_bytes_in_frame);
      } else if (f.cmd >= CMD_WRITE_CROSSOVER_BASE && f.cmd < CMD_WRITE_CROSSOVER_BASE + 8) {
        this->handle_crossover_write_ack_(f.cmd, f.payload, f.payload_bytes_in_frame);
      } else if (f.cmd >= CMD_ROUTING_BASE && f.cmd < CMD_ROUTING_BASE + 8) {
        this->handle_routing_write_ack_(f.cmd, f.payload, f.payload_bytes_in_frame);
      } else if (f.cmd >= CMD_WRITE_EQ_BAND_BASE && f.cmd < CMD_WRITE_EQ_BAND_BASE + (10 * 0x100) + 8) {
        this->handle_eq_band_write_ack_(f.cmd, f.payload, f.payload_bytes_in_frame);
      } else if (f.cmd >= CMD_WRITE_CHANNEL_NAME_BASE && f.cmd < CMD_WRITE_CHANNEL_NAME_BASE + 8) {
        this->handle_channel_name_write_ack_(f.cmd, f.payload, f.payload_bytes_in_frame);
      } else if (f.cmd >= CMD_WRITE_COMPRESSOR_BASE && f.cmd < CMD_WRITE_COMPRESSOR_BASE + 8) {
        ESP_LOGD(TAG, "Compressor ch%u write ack (len=%u)", static_cast<uint8_t>(f.cmd - CMD_WRITE_COMPRESSOR_BASE),
                 static_cast<unsigned>(f.payload_bytes_in_frame));
      } else if (f.cmd >= 0x10000u && f.cmd < 0x10008u) {
        // Full-channel-state write ACK (cmd=0x10000+ch). Single 0-byte
        // WRITE_ACK comes after the device finishes consuming all 5
        // multi-frame continuation reports.
        ESP_LOGI(TAG, "Full-channel-state ch%u write ack", static_cast<uint8_t>(f.cmd - 0x10000u));
      } else if (f.cmd >= 0x0900u && f.cmd < 0x0908u) {
        // Input MISC (cat=0x03) write ack.
        ESP_LOGD(TAG, "Input ch%u MISC write ack", static_cast<uint8_t>(f.cmd - 0x0900u));
      } else {
        ESP_LOGV(TAG, "Unhandled cmd 0x%X", static_cast<unsigned>(f.cmd));
      }
      break;
  }
}

bool DSP408::send_cmd_(uint8_t direction, uint32_t cmd, uint8_t category, const uint8_t *payload, size_t payload_len,
                       uint8_t seq) {
  if (!this->interface_claimed_ || this->ep_out_ == nullptr) {
    ESP_LOGW(TAG, "send_cmd: interface not ready");
    return false;
  }
  uint8_t frame[FRAME_SIZE];
  if (!build_frame(frame, direction, seq, cmd, category, payload, payload_len)) {
    ESP_LOGE(TAG, "build_frame failed (payload too large?)");
    return false;
  }
  ESP_LOGV(TAG, "-> dir=0x%02X cat=0x%02X cmd=0x%X seq=%u len=%u", direction, category, static_cast<unsigned>(cmd), seq,
           static_cast<unsigned>(payload_len));
  auto cb = [this, cmd](const usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGW(TAG, "OUT submit for cmd=0x%X failed status=0x%X", static_cast<unsigned>(cmd), status.error_code);
    }
  };
  bool ok = this->transfer_out(this->ep_out_->bEndpointAddress, cb, frame, FRAME_SIZE);
  if (ok) {
    this->cmd_in_flight_ = cmd;
    this->cmd_in_flight_started_ms_ = millis();
    this->cmd_in_flight_seq_ = seq;
    this->cmd_in_flight_dir_ = direction;
  }
  return ok;
}

bool DSP408::send_connect_() {
  return this->send_cmd_(DIR_CMD, CMD_CONNECT, CAT_STATE, nullptr, 0, this->next_read_seq_++);
}

bool DSP408::send_get_info_() {
  return this->send_cmd_(DIR_CMD, CMD_GET_INFO, CAT_STATE, nullptr, 0, this->next_read_seq_++);
}

bool DSP408::send_get_master_() {
  return this->send_cmd_(DIR_CMD, CMD_MASTER, CAT_STATE, nullptr, 0, this->next_read_seq_++);
}

bool DSP408::send_set_master_(uint8_t lvl_raw, bool muted) {
  // Per dsp408-py: payload = [lvl, 00, 00, 32, 00, 32, mute_bit, 00]
  // mute_bit: 1 = unmuted/audible, 0 = muted.
  uint8_t payload[8] = {lvl_raw, 0x00, 0x00, 0x32, 0x00, 0x32, static_cast<uint8_t>(muted ? 0 : 1), 0x00};
  return this->send_cmd_(DIR_WRITE, CMD_MASTER, CAT_STATE, payload, sizeof(payload), 0);
}

bool DSP408::send_read_channel_(uint8_t ch) {
  if (ch >= 8)
    return false;
  uint32_t cmd = CMD_READ_CHANNEL_BASE + ch;
  // 8 zero bytes is what the Windows GUI sends as the read payload.
  uint8_t payload[8] = {0};
  return this->send_cmd_(DIR_CMD, cmd, CAT_PARAM, payload, sizeof(payload), this->next_read_seq_++);
}

bool DSP408::send_set_crossover_(uint8_t ch) {
  if (ch >= 8)
    return false;
  ChannelState &c = this->ch_state_[ch];
  uint8_t payload[8] = {
      static_cast<uint8_t>(c.hpf_freq_hz & 0xFF),
      static_cast<uint8_t>((c.hpf_freq_hz >> 8) & 0xFF),
      c.hpf_filter,
      c.hpf_slope,
      static_cast<uint8_t>(c.lpf_freq_hz & 0xFF),
      static_cast<uint8_t>((c.lpf_freq_hz >> 8) & 0xFF),
      c.lpf_filter,
      c.lpf_slope,
  };
  uint32_t cmd = CMD_WRITE_CROSSOVER_BASE + ch;
  return this->send_cmd_(DIR_WRITE, cmd, CAT_PARAM, payload, sizeof(payload), 0);
}

bool DSP408::send_set_routing_(uint8_t ch) {
  if (ch >= 8)
    return false;
  ChannelState &c = this->ch_state_[ch];
  uint32_t cmd = CMD_ROUTING_BASE + ch;
  return this->send_cmd_(DIR_WRITE, cmd, CAT_PARAM, c.routing_lo, sizeof(c.routing_lo), 0);
}

bool DSP408::send_set_eq_band_(uint8_t ch, uint8_t band, uint16_t freq_hz, int16_t gain_raw, uint8_t b4_byte) {
  if (ch >= 8 || band >= EQ_BAND_COUNT)
    return false;
  uint8_t payload[8] = {
      static_cast<uint8_t>(freq_hz & 0xFF),
      static_cast<uint8_t>((freq_hz >> 8) & 0xFF),
      static_cast<uint8_t>(gain_raw & 0xFF),
      static_cast<uint8_t>((gain_raw >> 8) & 0xFF),
      b4_byte,
      0x00,
      0x00,
      0x00,
  };
  uint32_t cmd = CMD_WRITE_EQ_BAND_BASE + (static_cast<uint32_t>(band) << 8) + static_cast<uint32_t>(ch);
  return this->send_cmd_(DIR_WRITE, cmd, CAT_PARAM, payload, sizeof(payload), 0);
}

bool DSP408::send_get_preset_name_() {
  return this->send_cmd_(DIR_CMD, CMD_PRESET_NAME, CAT_STATE, nullptr, 0, this->next_read_seq_++);
}

bool DSP408::send_set_preset_name_(const std::string &name) {
  // Preset name is 15-byte ASCII NUL-padded.
  uint8_t payload[15] = {};
  size_t n = std::min(name.size(), sizeof(payload));
  memcpy(payload, name.data(), n);
  return this->send_cmd_(DIR_WRITE, CMD_PRESET_NAME, CAT_STATE, payload, sizeof(payload), 0);
}

bool DSP408::submit_raw_frame_(const uint8_t *frame64) {
  if (!this->interface_claimed_ || this->ep_out_ == nullptr)
    return false;
  auto cb = [](const usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGW(TAG, "raw OUT submit failed status=0x%X", status.error_code);
    }
  };
  return this->transfer_out(this->ep_out_->bEndpointAddress, cb, frame64, FRAME_SIZE);
}

bool DSP408::send_set_full_channel_state_(uint8_t ch, const uint8_t *blob, size_t blob_len) {
  if (ch >= 8)
    return false;
  if (blob_len != CHANNEL_BLOB_SIZE) {
    ESP_LOGE(TAG, "set_full_channel_state: expected %u bytes, got %u", static_cast<unsigned>(CHANNEL_BLOB_SIZE),
             static_cast<unsigned>(blob_len));
    return false;
  }
  // Use the LO-base path (0x10000+ch) — the variant the Windows GUI's
  // "Load preset from disk" flow emits. cat=CAT_PARAM, dir=a1, seq=0.
  uint32_t cmd = 0x10000u + ch;
  uint8_t frames[FRAME_SIZE * 6] = {};  // 5 frames + 1 spill safety
  size_t n_frames = build_frames_multi(frames, sizeof(frames), DIR_WRITE, 0, cmd, CAT_PARAM, blob, blob_len);
  if (n_frames == 0) {
    ESP_LOGE(TAG, "build_frames_multi returned 0");
    return false;
  }
  ESP_LOGD(TAG, "Multi-frame WRITE ch%u: %u frames, cmd=0x%X", ch, static_cast<unsigned>(n_frames),
           static_cast<unsigned>(cmd));

  // Submit all frames back-to-back. The device receives them as one
  // logical packet because the FIRST frame's payload_len header field
  // declares 296 bytes; the firmware reads continuation reports until
  // declared length is satisfied.
  for (size_t i = 0; i < n_frames; i++) {
    if (!this->submit_raw_frame_(frames + i * FRAME_SIZE)) {
      ESP_LOGE(TAG, "Multi-frame ch%u submit failed at frame %u", ch, static_cast<unsigned>(i));
      return false;
    }
  }
  // Track in-flight on the FIRST frame's cmd. Single 0-byte WRITE_ACK
  // is expected after the LAST frame.
  this->cmd_in_flight_ = cmd;
  this->cmd_in_flight_started_ms_ = millis();
  this->cmd_in_flight_seq_ = 0;
  this->cmd_in_flight_dir_ = DIR_WRITE;
  return true;
}

bool DSP408::send_set_compressor_(uint8_t ch) {
  if (ch >= 8)
    return false;
  ChannelState &c = this->ch_state_[ch];
  uint8_t payload[8] = {
      static_cast<uint8_t>(c.comp_all_pass_q & 0xFF),
      static_cast<uint8_t>((c.comp_all_pass_q >> 8) & 0xFF),
      static_cast<uint8_t>(c.comp_attack_ms & 0xFF),
      static_cast<uint8_t>((c.comp_attack_ms >> 8) & 0xFF),
      static_cast<uint8_t>(c.comp_release_ms & 0xFF),
      static_cast<uint8_t>((c.comp_release_ms >> 8) & 0xFF),
      c.comp_threshold,
      c.comp_linkgroup,
  };
  uint32_t cmd = CMD_WRITE_COMPRESSOR_BASE + ch;
  return this->send_cmd_(DIR_WRITE, cmd, CAT_PARAM, payload, sizeof(payload), 0);
}

bool DSP408::send_set_input_misc_(uint8_t input_ch) {
  if (input_ch >= 8)
    return false;
  // dsp408-py CMD_WRITE_INPUT_MISC_BASE = 0x0900. Payload layout (per
  // dsp408-py protocol.py): [feedback, polar, mode, mute, delay_le16,
  // volume, spare]. We only have polar wired up — write zeros for the
  // rest. The firmware appears to apply each field independently
  // (verified for polar live; the others are firmware-inert anyway).
  InputState &is = this->input_state_[input_ch];
  uint8_t payload[8] = {
      0,  // feedback
      static_cast<uint8_t>(is.polar ? 1 : 0),
      0,
      0,  // mode, mute
      0,
      0,  // delay LE16
      0,  // volume
      0,  // spare
  };
  uint32_t cmd = 0x0900u + input_ch;  // CMD_WRITE_INPUT_MISC_BASE + ch
  return this->send_cmd_(DIR_WRITE, cmd, CAT_INPUT, payload, sizeof(payload), 0);
}

bool DSP408::send_set_channel_name_(uint8_t ch, const std::string &name) {
  if (ch >= 8)
    return false;
  uint8_t payload[8] = {};
  size_t n = std::min(name.size(), sizeof(payload));
  memcpy(payload, name.data(), n);
  uint32_t cmd = CMD_WRITE_CHANNEL_NAME_BASE + ch;
  return this->send_cmd_(DIR_WRITE, cmd, CAT_PARAM, payload, sizeof(payload), 0);
}

bool DSP408::send_set_channel_(uint8_t ch) {
  if (ch >= 8)
    return false;
  ChannelState &c = this->ch_state_[ch];
  uint8_t subidx = c.subidx != 0 ? c.subidx : CHANNEL_SUBIDX_DEFAULT[ch];

  // gain raw = db_x10 + 600, clamped to [0..600] (= -60..0 dB)
  int gain = static_cast<int>(c.db_x10) + CHANNEL_VOL_OFFSET;
  if (gain < CHANNEL_VOL_MIN)
    gain = CHANNEL_VOL_MIN;
  if (gain > CHANNEL_VOL_MAX)
    gain = CHANNEL_VOL_MAX;
  uint16_t gain_u16 = static_cast<uint16_t>(gain);

  uint16_t delay = c.delay_samples;
  if (delay > CHANNEL_DELAY_MAX)
    delay = CHANNEL_DELAY_MAX;

  uint8_t payload[8] = {
      static_cast<uint8_t>(c.muted ? 0 : 1),  // [0] enable: 1=audible, 0=muted
      static_cast<uint8_t>(c.polar ? 1 : 0),  // [1] polar: 1=inverted
      static_cast<uint8_t>(gain_u16 & 0xFF),  // [2..3] gain LE
      static_cast<uint8_t>((gain_u16 >> 8) & 0xFF),
      static_cast<uint8_t>(delay & 0xFF),  // [4..5] delay LE
      static_cast<uint8_t>((delay >> 8) & 0xFF),
      c.byte_254,  // [6] preserved verbatim
      subidx,      // [7] DSP channel-type
  };
  uint32_t cmd = CMD_WRITE_CHANNEL_BASE + ch;
  // cmd=0x1F00..0x1F07 is in the parameter range — Python's
  // ``category_hint()`` returns CAT_PARAM (0x04) for it. The DSP-408
  // firmware happens to also accept CAT_STATE (0x09) here, but matching
  // the Windows GUI / Python wire format exactly is more conservative.
  return this->send_cmd_(DIR_WRITE, cmd, CAT_PARAM, payload, sizeof(payload), 0);
}

void DSP408::handle_connect_reply_(const uint8_t *payload, size_t len) {
  if (len == 0) {
    ESP_LOGW(TAG, "CONNECT reply with empty payload");
    return;
  }
  ESP_LOGI(TAG, "CONNECT ok (status=0x%02X)", payload[0]);
  // Move to GET_INFO
  if (this->send_get_info_()) {
    this->phase_ = StartupPhase::SENT_GET_INFO;
    this->phase_started_ms_ = millis();
  }
}

void DSP408::handle_get_info_reply_(const uint8_t *payload, size_t len) {
  // ASCII identity, e.g. "MYDW-AV1.06"
  std::string s;
  for (size_t i = 0; i < len; i++) {
    if (payload[i] == 0)
      break;
    s.push_back(static_cast<char>(payload[i]));
  }
  ESP_LOGI(TAG, "GET_INFO: '%s'", s.c_str());
  if (this->model_ts_ != nullptr)
    this->model_ts_->publish_state(s);
  // Move on to read master state
  if (this->send_get_master_()) {
    this->phase_ = StartupPhase::SENT_GET_MASTER;
    this->phase_started_ms_ = millis();
  }
}

void DSP408::handle_master_reply_(const uint8_t *payload, size_t len, bool is_ack) {
  if (is_ack && len == 0) {
    // WRITE_ACKs to CMD_MASTER carry no payload — that's normal. Don't
    // overwrite the cached state from the optimistic-publish path; the
    // periodic poll will reaffirm it within MASTER_POLL_INTERVAL_MS.
    ESP_LOGD(TAG, "MASTER write ack");
    return;
  }
  if (len < 8) {
    ESP_LOGW(TAG, "MASTER reply too short (%u bytes)", static_cast<unsigned>(len));
    return;
  }
  uint8_t lvl = payload[0];
  bool muted = payload[6] == 0;  // 1 = audible, 0 = muted
  this->master_known_ = true;
  this->master_db_ = static_cast<int8_t>(static_cast<int>(lvl) - MASTER_LEVEL_OFFSET);
  this->master_muted_ = muted;
  ESP_LOGI(TAG, "MASTER %s: %+d dB %s", is_ack ? "ack" : "read", this->master_db_, muted ? "(muted)" : "(audible)");

  if (this->master_vol_num_ != nullptr)
    this->master_vol_num_->publish_state(static_cast<float>(this->master_db_));
  if (this->master_mute_sw_ != nullptr)
    this->master_mute_sw_->publish_state(this->master_muted_);

  if (this->phase_ == StartupPhase::SENT_GET_MASTER) {
    // Move on to channel warmup — read each channel's state to drain
    // the firmware's startup write-drop quirk AND populate our cache
    // from device truth before any user-driven writes go out.
    this->phase_ = StartupPhase::WARMUP_CHANNELS;
    this->warmup_channel_ = 0;
    this->warmup_attempt_ = 0;
    this->warmup_have_prev_ = false;
    this->phase_started_ms_ = millis();
    if (this->send_read_channel_(this->warmup_channel_)) {
      ESP_LOGD(TAG, "Warmup: reading channel %u (attempt 1)", this->warmup_channel_);
    }
  }
}

void DSP408::handle_channel_state_blob_(uint8_t ch, const uint8_t *blob, size_t len) {
  if (ch >= 8)
    return;
  if (len < CHANNEL_BLOB_SIZE) {
    ESP_LOGW(TAG, "Channel %u blob too short: %u bytes", ch, static_cast<unsigned>(len));
    return;
  }

  // Read-divergence retry — the firmware sometimes emits a 2-byte-shifted
  // (or worse) variant of the EQ region. dsp408-py's
  // ``read_channel_state(retry_on_divergence=True)`` re-reads up to
  // MAX_BLOB_RETRY times, accepting the first blob that agrees with its
  // predecessor. We mirror that here during the warmup phase. After
  // warmup we don't re-read, so steady-state operation is unaffected.
  if (this->phase_ == StartupPhase::WARMUP_CHANNELS && ch == this->warmup_channel_) {
    bool converged = this->warmup_have_prev_ && memcmp(blob, this->warmup_prev_blob_, CHANNEL_BLOB_SIZE) == 0;
    if (!converged && this->warmup_attempt_ + 1 < MAX_BLOB_RETRY) {
      // Save this blob and re-read.
      memcpy(this->warmup_prev_blob_, blob, CHANNEL_BLOB_SIZE);
      this->warmup_have_prev_ = true;
      this->warmup_attempt_++;
      ESP_LOGD(TAG, "Ch%u: read divergence — retry attempt %u", ch, this->warmup_attempt_ + 1);
      if (!this->send_read_channel_(ch)) {
        ESP_LOGW(TAG, "Ch%u: failed to submit retry read", ch);
      }
      return;
    }
    if (!converged) {
      ESP_LOGW(TAG, "Ch%u: %u attempts did not converge — accepting last blob", ch, MAX_BLOB_RETRY);
    } else if (this->warmup_attempt_ > 0) {
      ESP_LOGD(TAG, "Ch%u: converged after %u attempts", ch, this->warmup_attempt_ + 1);
    }
  }

  ChannelState &c = this->ch_state_[ch];
  // Decode the 296-byte blob — offsets verified live by dsp408-py
  // against Windows GUI captures.
  uint16_t gain_raw = static_cast<uint16_t>(blob[OFF_GAIN]) | (static_cast<uint16_t>(blob[OFF_GAIN + 1]) << 8);
  c.muted = (blob[OFF_MUTE] == 0);  // INVERTED polarity (1 = audible)
  c.polar = (blob[OFF_POLAR] != 0);
  // dB×10 from raw: db_x10 = raw - 600. Range -600..0 = -60.0..0.0 dB.
  c.db_x10 = static_cast<int16_t>(static_cast<int>(gain_raw) - CHANNEL_VOL_OFFSET);
  c.delay_samples = static_cast<uint16_t>(blob[OFF_DELAY]) | (static_cast<uint16_t>(blob[OFF_DELAY + 1]) << 8);
  c.byte_254 = blob[OFF_BYTE_254];
  c.subidx = blob[OFF_SPK_TYPE];

  c.hpf_freq_hz = static_cast<uint16_t>(blob[OFF_HPF_FREQ]) | (static_cast<uint16_t>(blob[OFF_HPF_FREQ + 1]) << 8);
  c.hpf_filter = blob[OFF_HPF_FILTER];
  c.hpf_slope = blob[OFF_HPF_SLOPE];
  c.lpf_freq_hz = static_cast<uint16_t>(blob[OFF_LPF_FREQ]) | (static_cast<uint16_t>(blob[OFF_LPF_FREQ + 1]) << 8);
  c.lpf_filter = blob[OFF_LPF_FILTER];
  c.lpf_slope = blob[OFF_LPF_SLOPE];

  // Output routing matrix (low bank — IN1..IN8 levels at offsets 264..271)
  memcpy(c.routing_lo, blob + OFF_MIXER, sizeof(c.routing_lo));

  // Per-channel name (8 bytes ASCII at offsets 288..295, NUL-padded)
  memcpy(c.name, blob + OFF_NAME, NAME_LEN);
  c.name[NAME_LEN] = '\0';

  // Compressor block (firmware-inert in v1.06 but tracked for parity).
  c.comp_all_pass_q =
      static_cast<uint16_t>(blob[OFF_ALL_PASS_Q]) | (static_cast<uint16_t>(blob[OFF_ALL_PASS_Q + 1]) << 8);
  c.comp_attack_ms = static_cast<uint16_t>(blob[OFF_ATTACK_MS]) | (static_cast<uint16_t>(blob[OFF_ATTACK_MS + 1]) << 8);
  c.comp_release_ms =
      static_cast<uint16_t>(blob[OFF_RELEASE_MS]) | (static_cast<uint16_t>(blob[OFF_RELEASE_MS + 1]) << 8);
  c.comp_threshold = blob[OFF_THRESHOLD];
  c.comp_linkgroup = blob[OFF_LINKGROUP];

  c.primed = true;

  ESP_LOGI(TAG, "Ch%u: %+.1f dB %s%s delay=%u  HPF=%uHz/%u/%u  LPF=%uHz/%u/%u  subidx=0x%02X", ch,
           static_cast<float>(c.db_x10) / 10.0f, c.muted ? "(muted)" : "(audible)", c.polar ? " (POLAR)" : "",
           c.delay_samples, c.hpf_freq_hz, c.hpf_filter, c.hpf_slope, c.lpf_freq_hz, c.lpf_filter, c.lpf_slope,
           c.subidx);

  this->publish_channel_state_(ch);

  // Snapshot save (read-modify-write): if a snapshot is pending for
  // this channel, overlay the cached basic-record fields onto the
  // live blob (preserving EQ region + everything else byte-for-byte)
  // and submit a multi-frame WRITE to commit to the device's flash.
  if (this->snapshot_pending_ch_ == ch && this->phase_ == StartupPhase::RUNNING) {
    uint8_t out_blob[CHANNEL_BLOB_SIZE];
    memcpy(out_blob, blob, CHANNEL_BLOB_SIZE);  // start from device truth
    // Overlay basic-record (offsets 248..295) from cache. Any HA-driven
    // writes since the last warmup are reflected in the cache; this
    // ensures they're committed to flash via the multi-frame WRITE.
    out_blob[OFF_MUTE] = c.muted ? 0 : 1;
    out_blob[OFF_POLAR] = c.polar ? 1 : 0;
    uint16_t gain_raw = static_cast<uint16_t>(static_cast<int>(c.db_x10) + CHANNEL_VOL_OFFSET);
    out_blob[OFF_GAIN] = gain_raw & 0xFF;
    out_blob[OFF_GAIN + 1] = (gain_raw >> 8) & 0xFF;
    out_blob[OFF_DELAY] = c.delay_samples & 0xFF;
    out_blob[OFF_DELAY + 1] = (c.delay_samples >> 8) & 0xFF;
    out_blob[OFF_BYTE_254] = c.byte_254;
    out_blob[OFF_SPK_TYPE] = c.subidx ? c.subidx : CHANNEL_SUBIDX_DEFAULT[ch];
    out_blob[OFF_HPF_FREQ] = c.hpf_freq_hz & 0xFF;
    out_blob[OFF_HPF_FREQ + 1] = (c.hpf_freq_hz >> 8) & 0xFF;
    out_blob[OFF_HPF_FILTER] = c.hpf_filter;
    out_blob[OFF_HPF_SLOPE] = c.hpf_slope;
    out_blob[OFF_LPF_FREQ] = c.lpf_freq_hz & 0xFF;
    out_blob[OFF_LPF_FREQ + 1] = (c.lpf_freq_hz >> 8) & 0xFF;
    out_blob[OFF_LPF_FILTER] = c.lpf_filter;
    out_blob[OFF_LPF_SLOPE] = c.lpf_slope;
    memcpy(out_blob + OFF_MIXER, c.routing_lo, sizeof(c.routing_lo));
    out_blob[OFF_ALL_PASS_Q] = c.comp_all_pass_q & 0xFF;
    out_blob[OFF_ALL_PASS_Q + 1] = (c.comp_all_pass_q >> 8) & 0xFF;
    out_blob[OFF_ATTACK_MS] = c.comp_attack_ms & 0xFF;
    out_blob[OFF_ATTACK_MS + 1] = (c.comp_attack_ms >> 8) & 0xFF;
    out_blob[OFF_RELEASE_MS] = c.comp_release_ms & 0xFF;
    out_blob[OFF_RELEASE_MS + 1] = (c.comp_release_ms >> 8) & 0xFF;
    out_blob[OFF_THRESHOLD] = c.comp_threshold;
    out_blob[OFF_LINKGROUP] = c.comp_linkgroup;
    memcpy(out_blob + OFF_NAME, c.name, NAME_LEN);
    ESP_LOGI(TAG, "Snapshot save ch%u: writing %u-byte blob (EQ preserved)", ch,
             static_cast<unsigned>(CHANNEL_BLOB_SIZE));
    this->send_set_full_channel_state_(ch, out_blob, sizeof(out_blob));
    this->snapshot_pending_ch_ = 0xFF;
  }

  // Warmup phase advance.
  if (this->phase_ == StartupPhase::WARMUP_CHANNELS && ch == this->warmup_channel_) {
    this->warmup_channel_++;
    this->warmup_attempt_ = 0;
    this->warmup_have_prev_ = false;
    if (this->warmup_channel_ >= 8) {
      this->phase_ = StartupPhase::RUNNING;
      uint32_t now = millis();
      this->last_master_poll_ms_ = now;
      ESP_LOGI(TAG, "DSP-408 ready (full state read; startup took %u ms)",
               static_cast<unsigned>(now - this->connected_at_ms_));
      // Fire-and-forget preset-name read — the reply arrives async and
      // the entity (if any) gets published. Failure here is non-fatal.
      this->send_get_preset_name_();
    } else {
      // Kick off the next channel read.
      if (!this->send_read_channel_(this->warmup_channel_)) {
        ESP_LOGW(TAG, "Failed to submit channel %u read", this->warmup_channel_);
      }
    }
  }
}

void DSP408::handle_crossover_write_ack_(uint32_t cmd, const uint8_t *payload, size_t len) {
  uint8_t ch = static_cast<uint8_t>(cmd - 0x12000);
  ESP_LOGD(TAG, "Crossover ch%u write ack (len=%u)", ch, static_cast<unsigned>(len));
  if (ch < 8) {
    ChannelState &c = this->ch_state_[ch];
    if (this->ch_hpf_freq_num_[ch] != nullptr)
      this->ch_hpf_freq_num_[ch]->publish_state(static_cast<float>(c.hpf_freq_hz));
    if (this->ch_lpf_freq_num_[ch] != nullptr)
      this->ch_lpf_freq_num_[ch]->publish_state(static_cast<float>(c.lpf_freq_hz));
  }
  (void) payload;
}

void DSP408::handle_routing_write_ack_(uint32_t cmd, const uint8_t *payload, size_t len) {
  uint8_t ch = static_cast<uint8_t>(cmd - CMD_ROUTING_BASE);
  ESP_LOGD(TAG, "Routing ch%u write ack (len=%u)", ch, static_cast<unsigned>(len));
  (void) payload;
}

void DSP408::handle_eq_band_write_ack_(uint32_t cmd, const uint8_t *payload, size_t len) {
  uint32_t off = cmd - CMD_WRITE_EQ_BAND_BASE;
  uint8_t band = static_cast<uint8_t>((off >> 8) & 0xFF);
  uint8_t ch = static_cast<uint8_t>(off & 0xFF);
  ESP_LOGD(TAG, "EQ ch%u band%u write ack (len=%u)", ch, band, static_cast<unsigned>(len));
  (void) payload;
}

void DSP408::handle_channel_name_write_ack_(uint32_t cmd, const uint8_t *payload, size_t len) {
  uint8_t ch = static_cast<uint8_t>(cmd - CMD_WRITE_CHANNEL_NAME_BASE);
  ESP_LOGD(TAG, "Channel name ch%u write ack (len=%u)", ch, static_cast<unsigned>(len));
  if (ch < 8 && this->ch_name_text_[ch] != nullptr) {
    // Re-publish from cache (we cached the desired name in request_channel_name).
    this->ch_name_text_[ch]->publish_state(std::string(this->ch_state_[ch].name));
  }
  (void) payload;
}

void DSP408::handle_preset_name_reply_(const uint8_t *payload, size_t len, bool is_ack) {
  if (is_ack) {
    ESP_LOGD(TAG, "Preset-name write ack");
    return;
  }
  // Read reply: 15 bytes ASCII (firmware NUL-pads at the end).
  size_t copy_len = std::min<size_t>(len, sizeof(this->preset_name_) - 1);
  memcpy(this->preset_name_, payload, copy_len);
  this->preset_name_[copy_len] = '\0';
  this->preset_name_known_ = true;
  ESP_LOGI(TAG, "Preset name: '%s'", this->preset_name_);
  if (this->preset_name_text_ != nullptr)
    this->preset_name_text_->publish_state(std::string(this->preset_name_));
}

void DSP408::publish_channel_state_(uint8_t ch) {
  if (ch >= 8)
    return;
  ChannelState &c = this->ch_state_[ch];
  if (this->ch_vol_num_[ch] != nullptr)
    this->ch_vol_num_[ch]->publish_state(static_cast<float>(c.db_x10) / 10.0f);
  if (this->ch_mute_sw_[ch] != nullptr)
    this->ch_mute_sw_[ch]->publish_state(c.muted);
  if (this->ch_delay_num_[ch] != nullptr)
    this->ch_delay_num_[ch]->publish_state(static_cast<float>(c.delay_samples));
  if (this->ch_polar_sw_[ch] != nullptr)
    this->ch_polar_sw_[ch]->publish_state(c.polar);
  if (this->ch_hpf_freq_num_[ch] != nullptr)
    this->ch_hpf_freq_num_[ch]->publish_state(static_cast<float>(c.hpf_freq_hz));
  if (this->ch_lpf_freq_num_[ch] != nullptr)
    this->ch_lpf_freq_num_[ch]->publish_state(static_cast<float>(c.lpf_freq_hz));
  // Filter type / slope select entities — index-as-string (the select
  // sub-platform decodes index→option; we publish via the option string).
  static const char *FILTER_NAMES[4] = {"Butterworth", "Bessel", "Linkwitz-Riley", "Linkwitz-Riley"};
  static const char *SLOPE_NAMES_[9] = {"6 dB/oct",  "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct",
                                        "36 dB/oct", "42 dB/oct", "48 dB/oct", "Off"};
  if (this->ch_hpf_filter_sel_[ch] != nullptr && c.hpf_filter < 4)
    this->ch_hpf_filter_sel_[ch]->publish_state(FILTER_NAMES[c.hpf_filter]);
  if (this->ch_hpf_slope_sel_[ch] != nullptr && c.hpf_slope < 9)
    this->ch_hpf_slope_sel_[ch]->publish_state(SLOPE_NAMES_[c.hpf_slope]);
  if (this->ch_lpf_filter_sel_[ch] != nullptr && c.lpf_filter < 4)
    this->ch_lpf_filter_sel_[ch]->publish_state(FILTER_NAMES[c.lpf_filter]);
  if (this->ch_lpf_slope_sel_[ch] != nullptr && c.lpf_slope < 9)
    this->ch_lpf_slope_sel_[ch]->publish_state(SLOPE_NAMES_[c.lpf_slope]);
  if (this->ch_name_text_[ch] != nullptr)
    this->ch_name_text_[ch]->publish_state(std::string(c.name));
  if (this->ch_comp_attack_num_[ch] != nullptr)
    this->ch_comp_attack_num_[ch]->publish_state(static_cast<float>(c.comp_attack_ms));
  if (this->ch_comp_release_num_[ch] != nullptr)
    this->ch_comp_release_num_[ch]->publish_state(static_cast<float>(c.comp_release_ms));
  if (this->ch_comp_threshold_num_[ch] != nullptr)
    this->ch_comp_threshold_num_[ch]->publish_state(static_cast<float>(c.comp_threshold));
}

void DSP408::handle_channel_write_ack_(uint32_t cmd, const uint8_t *payload, size_t len) {
  uint8_t ch = static_cast<uint8_t>(cmd - CMD_WRITE_CHANNEL_BASE);
  ESP_LOGD(TAG, "Channel %u write ack (len=%u)", ch, static_cast<unsigned>(len));
  if (ch < 8) {
    if (this->ch_vol_num_[ch] != nullptr)
      this->ch_vol_num_[ch]->publish_state(static_cast<float>(this->ch_state_[ch].db_x10) / 10.0f);
    if (this->ch_mute_sw_[ch] != nullptr)
      this->ch_mute_sw_[ch]->publish_state(this->ch_state_[ch].muted);
  }
  (void) payload;
}

// ── Public command entry points ─────────────────────────────────────────
//
// All request_* entry points gate on ``phase_ == StartupPhase::RUNNING``.
// During WAIT_AFTER_CONNECT / SENT_CONNECT / SENT_GET_INFO /
// SENT_GET_MASTER / WARMUP_CHANNELS the per-channel cache is either
// empty (defaults: 0 dB / unmuted / no polar / 0 delay / HPF=20 Hz /
// LPF=20 kHz / slope=8 Off) or being populated from device truth. Any
// write that goes out before warmup finishes will combine the user's
// new field with stale defaults for every OTHER field — silently
// clobbering whatever state the device had stored. This is exactly the
// "fresh-boot HA touch surges channel to 0 dB" hazard documented at
// length in dsp408-py's _prime_channel_cache (device.py:1248-1279).
//
// The gate drops the write with a WARN. HA's optimistic publish_state
// from the entity's control() handler is left in place — the next
// periodic master poll (or future channel-state re-poll) will reaffirm
// the actual device state to HA.

void DSP408::request_master_volume(float db) {
  if (this->phase_ != StartupPhase::RUNNING || !this->master_known_) {
    ESP_LOGW(TAG, "Dropping master_volume write — phase=%d master_known=%d", static_cast<int>(this->phase_),
             this->master_known_);
    return;
  }
  int lvl = static_cast<int>(std::round(db + MASTER_LEVEL_OFFSET));
  if (lvl < MASTER_LEVEL_MIN)
    lvl = MASTER_LEVEL_MIN;
  if (lvl > MASTER_LEVEL_MAX)
    lvl = MASTER_LEVEL_MAX;
  this->master_db_ = static_cast<int8_t>(lvl - MASTER_LEVEL_OFFSET);
  this->send_set_master_(static_cast<uint8_t>(lvl), this->master_muted_);
}

void DSP408::request_master_mute(bool muted) {
  if (this->phase_ != StartupPhase::RUNNING || !this->master_known_) {
    ESP_LOGW(TAG, "Dropping master_mute write — phase=%d master_known=%d", static_cast<int>(this->phase_),
             this->master_known_);
    return;
  }
  uint8_t lvl_raw = static_cast<uint8_t>(this->master_db_ + MASTER_LEVEL_OFFSET);
  this->master_muted_ = muted;
  this->send_set_master_(lvl_raw, muted);
}

void DSP408::request_channel_volume(uint8_t ch, float db) {
  if (ch >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u volume write — phase=%d primed=%d", ch, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  // 0.1 dB resolution: encode as db_x10. Range -600..0.
  int v = static_cast<int>(std::round(db * 10.0f));
  if (v < -600)
    v = -600;
  if (v > 0)
    v = 0;
  this->ch_state_[ch].db_x10 = static_cast<int16_t>(v);
  this->send_set_channel_(ch);
}

void DSP408::request_channel_mute(uint8_t ch, bool muted) {
  if (ch >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u mute write — phase=%d primed=%d", ch, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  this->ch_state_[ch].muted = muted;
  this->send_set_channel_(ch);
}

void DSP408::request_channel_delay(uint8_t ch, uint16_t samples) {
  if (ch >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u delay write — phase=%d primed=%d", ch, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  if (samples > CHANNEL_DELAY_MAX)
    samples = CHANNEL_DELAY_MAX;
  this->ch_state_[ch].delay_samples = samples;
  this->send_set_channel_(ch);
}

void DSP408::request_channel_polar(uint8_t ch, bool inverted) {
  if (ch >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u polar write — phase=%d primed=%d", ch, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  this->ch_state_[ch].polar = inverted;
  this->send_set_channel_(ch);
}

void DSP408::request_channel_hpf_freq(uint8_t ch, uint16_t hz) {
  if (ch >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u hpf_freq write — phase=%d primed=%d", ch, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  if (hz < 10)
    hz = 10;
  if (hz > 20000)
    hz = 20000;
  this->ch_state_[ch].hpf_freq_hz = hz;
  this->send_set_crossover_(ch);
}

void DSP408::request_channel_lpf_freq(uint8_t ch, uint16_t hz) {
  if (ch >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u lpf_freq write — phase=%d primed=%d", ch, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  if (hz < 100)
    hz = 100;
  if (hz > 22000)
    hz = 22000;
  this->ch_state_[ch].lpf_freq_hz = hz;
  this->send_set_crossover_(ch);
}

void DSP408::request_channel_hpf_filter(uint8_t ch, uint8_t filter) {
  if (ch >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u hpf_filter write — phase=%d primed=%d", ch, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  if (filter > 3)
    filter = 3;
  this->ch_state_[ch].hpf_filter = filter;
  this->send_set_crossover_(ch);
}

void DSP408::request_channel_hpf_slope(uint8_t ch, uint8_t slope) {
  if (ch >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u hpf_slope write — phase=%d primed=%d", ch, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  if (slope > 8)
    slope = 8;
  this->ch_state_[ch].hpf_slope = slope;
  this->send_set_crossover_(ch);
}

void DSP408::request_channel_lpf_filter(uint8_t ch, uint8_t filter) {
  if (ch >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u lpf_filter write — phase=%d primed=%d", ch, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  if (filter > 3)
    filter = 3;
  this->ch_state_[ch].lpf_filter = filter;
  this->send_set_crossover_(ch);
}

void DSP408::request_channel_lpf_slope(uint8_t ch, uint8_t slope) {
  if (ch >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u lpf_slope write — phase=%d primed=%d", ch, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  if (slope > 8)
    slope = 8;
  this->ch_state_[ch].lpf_slope = slope;
  this->send_set_crossover_(ch);
}

void DSP408::request_eq_band(uint8_t ch, uint8_t band, uint16_t freq_hz, float gain_db, float q) {
  if (ch >= 8 || band >= EQ_BAND_COUNT)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[ch].primed) {
    ESP_LOGW(TAG, "Dropping ch%u band%u EQ write — phase=%d primed=%d", ch, band, static_cast<int>(this->phase_),
             this->ch_state_[ch].primed);
    return;
  }
  // Bound freq + gain to envelopes.
  if (freq_hz < 20)
    freq_hz = 20;
  if (freq_hz > 20000)
    freq_hz = 20000;
  int gain_raw = static_cast<int>(std::round(gain_db * 10.0f)) + EQ_GAIN_RAW_OFFSET;
  if (gain_raw < EQ_GAIN_RAW_MIN)
    gain_raw = EQ_GAIN_RAW_MIN;
  if (gain_raw > EQ_GAIN_RAW_MAX)
    gain_raw = EQ_GAIN_RAW_MAX;

  // Q encoding: b4_byte ≈ 256 / Q, clamped to [1..255]. Higher b4_byte
  // = lower Q (wider peak). Default 0x34 = 52 → Q ≈ 4.9.
  if (q < 0.1f)
    q = 0.1f;
  int b4 = static_cast<int>(std::round(EQ_Q_BW_CONSTANT / q));
  if (b4 < 1)
    b4 = 1;
  if (b4 > 255)
    b4 = 255;

  ESP_LOGD(TAG, "EQ ch%u band%u: f=%u Hz gain=%.1f dB Q=%.2f -> raw=%d b4=%u", ch, band, freq_hz, gain_db, q, gain_raw,
           b4);
  this->send_set_eq_band_(ch, band, freq_hz, static_cast<int16_t>(gain_raw), static_cast<uint8_t>(b4));
}

void DSP408::request_routing(uint8_t channel, uint8_t input_idx, bool on) {
  if (channel >= 8 || input_idx >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[channel].primed) {
    ESP_LOGW(TAG, "Dropping ch%u routing[%u]=%d — phase=%d primed=%d", channel, input_idx, on,
             static_cast<int>(this->phase_), this->ch_state_[channel].primed);
    return;
  }
  this->ch_state_[channel].routing_lo[input_idx] = on ? ROUTING_ON : ROUTING_OFF;
  this->send_set_routing_(channel);
}

void DSP408::request_preset_name(const std::string &name) {
  if (this->phase_ != StartupPhase::RUNNING) {
    ESP_LOGW(TAG, "Dropping preset_name write — phase=%d", static_cast<int>(this->phase_));
    return;
  }
  // Cache it so we can publish back on ack.
  size_t copy_len = std::min(name.size(), sizeof(this->preset_name_) - 1);
  memcpy(this->preset_name_, name.data(), copy_len);
  this->preset_name_[copy_len] = '\0';
  this->preset_name_known_ = true;
  this->send_set_preset_name_(name);
  if (this->preset_name_text_ != nullptr)
    this->preset_name_text_->publish_state(std::string(this->preset_name_));
}

void DSP408::request_channel_name(uint8_t channel, const std::string &name) {
  if (channel >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[channel].primed) {
    ESP_LOGW(TAG, "Dropping ch%u name write — phase=%d primed=%d", channel, static_cast<int>(this->phase_),
             this->ch_state_[channel].primed);
    return;
  }
  size_t copy_len = std::min(name.size(), NAME_LEN);
  memset(this->ch_state_[channel].name, 0, sizeof(this->ch_state_[channel].name));
  memcpy(this->ch_state_[channel].name, name.data(), copy_len);
  this->send_set_channel_name_(channel, name);
}

void DSP408::request_full_channel_state(uint8_t channel, const uint8_t *blob, size_t blob_len) {
  if (channel >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING) {
    ESP_LOGW(TAG, "Dropping ch%u full-state write — phase=%d", channel, static_cast<int>(this->phase_));
    return;
  }
  this->send_set_full_channel_state_(channel, blob, blob_len);
}

bool DSP408::request_save_channel_snapshot(uint8_t channel) {
  if (channel >= 8)
    return false;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[channel].primed) {
    ESP_LOGW(TAG, "Dropping ch%u snapshot save — phase=%d primed=%d", channel, static_cast<int>(this->phase_),
             this->ch_state_[channel].primed);
    return false;
  }
  if (this->snapshot_pending_ch_ != 0xFF) {
    ESP_LOGW(TAG, "Snapshot already pending (ch=%u); dropping ch=%u request", this->snapshot_pending_ch_, channel);
    return false;
  }
  // Read-modify-write: kick off a fresh channel-state read; once the
  // blob arrives, handle_channel_state_blob_ sees snapshot_pending_ch_
  // matches, overlays cached basic-record fields onto the live blob
  // (preserving EQ region + everything else byte-for-byte), and
  // submits a multi-frame WRITE.
  this->snapshot_pending_ch_ = channel;
  ESP_LOGI(TAG, "Snapshot save ch%u: read-modify-write initiated", channel);
  return this->send_read_channel_(channel);
}

// Compressor field-specific setters do read-modify-write via the
// cache: update just the field this setter owns, leave the others as
// they were on warmup (or last write). The wire format always sends
// the full 8-byte payload, so without this pattern an attack-only
// change would zero out release/threshold/linkgroup.

void DSP408::request_compressor_attack(uint8_t channel, uint16_t attack_ms) {
  if (channel >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[channel].primed) {
    ESP_LOGW(TAG, "Dropping ch%u compressor.attack — phase=%d primed=%d", channel, static_cast<int>(this->phase_),
             this->ch_state_[channel].primed);
    return;
  }
  this->ch_state_[channel].comp_attack_ms = attack_ms;
  this->send_set_compressor_(channel);
}

void DSP408::request_compressor_release(uint8_t channel, uint16_t release_ms) {
  if (channel >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[channel].primed) {
    ESP_LOGW(TAG, "Dropping ch%u compressor.release — phase=%d primed=%d", channel, static_cast<int>(this->phase_),
             this->ch_state_[channel].primed);
    return;
  }
  this->ch_state_[channel].comp_release_ms = release_ms;
  this->send_set_compressor_(channel);
}

void DSP408::request_compressor_threshold(uint8_t channel, uint8_t threshold) {
  if (channel >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[channel].primed) {
    ESP_LOGW(TAG, "Dropping ch%u compressor.threshold — phase=%d primed=%d", channel, static_cast<int>(this->phase_),
             this->ch_state_[channel].primed);
    return;
  }
  this->ch_state_[channel].comp_threshold = threshold;
  this->send_set_compressor_(channel);
}

void DSP408::request_compressor_linkgroup(uint8_t channel, uint8_t linkgroup) {
  if (channel >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING || !this->ch_state_[channel].primed) {
    ESP_LOGW(TAG, "Dropping ch%u compressor.linkgroup — phase=%d primed=%d", channel, static_cast<int>(this->phase_),
             this->ch_state_[channel].primed);
    return;
  }
  this->ch_state_[channel].comp_linkgroup = linkgroup;
  this->send_set_compressor_(channel);
}

void DSP408::request_input_polar(uint8_t input_channel, bool inverted) {
  if (input_channel >= 8)
    return;
  if (this->phase_ != StartupPhase::RUNNING) {
    ESP_LOGW(TAG, "Dropping input ch%u polar write — phase=%d", input_channel, static_cast<int>(this->phase_));
    return;
  }
  this->input_state_[input_channel].polar = inverted;
  this->send_set_input_misc_(input_channel);
  if (this->input_polar_sw_[input_channel] != nullptr)
    this->input_polar_sw_[input_channel]->publish_state(inverted);
}

}  // namespace dsp408
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2/S3/P4
