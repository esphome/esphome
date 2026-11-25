#include "ubx_gnss.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::ubx {

ESPHOME_LOG_TAG(TAG, "ubx");

static constexpr uint32_t STEP_TIMEOUT_ID = 0;
static constexpr uint32_t RESTART_ID = 1;
static constexpr uint32_t STEP_TIMEOUT_MS = 1000;
static constexpr uint8_t STEP_RETRIES = 3;
// A receiver that does not answer MON-VER may not be powered yet
static constexpr uint32_t RESTART_DELAY_MS = 5000;
// No complete epoch for this long, or three measurement intervals if longer: the receiver lost its configuration
static constexpr uint32_t EPOCH_TIMEOUT_MS = 5000;

// Configuration steps, then one CFG-MSG step per message
enum : uint8_t {
  STEP_MON_VER,
  STEP_PRT_POLL,
  STEP_PRT,
  STEP_RATE,
  STEP_NAV5,
  STEP_MESSAGES,
};
static constexpr uint8_t STEP_DONE = 0xFF;

// NAV-TIMEUTC comes last in each epoch, so it completes one
static constexpr uint8_t PVT_MESSAGES[] = {UBX_NAV_DOP, UBX_NAV_PVT, UBX_NAV_TIMEUTC};
static constexpr uint8_t LEGACY_MESSAGES[] = {UBX_NAV_POSLLH, UBX_NAV_DOP, UBX_NAV_SOL, UBX_NAV_VELNED,
                                              UBX_NAV_TIMEUTC};

void UBXGNSS::setup() { this->start_setup_(); }

void UBXGNSS::clear_data_() {
  // UTC is kept: the time source ignores it once it is stale
  const gnss::GNSSData cleared{};
  const time_t utc = this->data_.utc;
  const uint64_t utc_millis = this->data_.utc_millis;
  this->data_ = cleared;
  this->data_.utc = utc;
  this->data_.utc_millis = utc_millis;
}

void UBXGNSS::start_setup_() {
  // Used if the receiver does not answer the port poll: UART1, 8N1 at the current baud rate
  memset(this->port_, 0, sizeof(this->port_));
  this->port_[0] = 1;
  put_le32(this->port_ + 4, 0x000008D0);
  put_le32(this->port_ + 8, this->parent_->get_baud_rate());
  this->step_ = STEP_MON_VER;
  this->retries_ = 0;
  this->send_step_();
}

const uint8_t *UBXGNSS::messages_(uint8_t &count) const {
  if (this->protocol_version_ >= UBX_PROTVER_NAV_PVT) {
    count = sizeof(PVT_MESSAGES);
    return PVT_MESSAGES;
  }
  count = sizeof(LEGACY_MESSAGES);
  return LEGACY_MESSAGES;
}

void UBXGNSS::send_(uint8_t msg_class, uint8_t msg_id, const uint8_t *payload, uint16_t len) {
  uint8_t frame[36 + UBX_FRAME_OVERHEAD];
  this->write_array(frame, build_frame(msg_class, msg_id, payload, len, frame));
}

void UBXGNSS::send_step_() {
  uint8_t payload[36]{};
  switch (this->step_) {
    case STEP_MON_VER:
      this->send_(UBX_CLASS_MON, UBX_MON_VER, nullptr, 0);
      break;
    case STEP_PRT_POLL:
      // Without a payload the receiver answers with the settings of the port it heard this on
      this->send_(UBX_CLASS_CFG, UBX_CFG_PRT, nullptr, 0);
      break;
    case STEP_PRT:
      // That port with its framing and baud rate unchanged: UBX added to the input, UBX only out
      memcpy(payload, this->port_, sizeof(this->port_));
      put_le16(payload + 12, encode_value_le<uint16_t>(this->port_ + 12) | 0x0001);
      put_le16(payload + 14, 0x0001);
      this->send_(UBX_CLASS_CFG, UBX_CFG_PRT, payload, sizeof(this->port_));
      break;
    case STEP_RATE:
      // one navigation solution per measurement, aligned to GPS time
      put_le16(payload, this->measurement_rate_ms_);
      put_le16(payload + 2, 1);
      put_le16(payload + 4, 1);
      this->send_(UBX_CLASS_CFG, UBX_CFG_RATE, payload, 6);
      break;
    case STEP_NAV5:
      if (this->dynamic_model_ == UBX_DYNAMIC_MODEL_UNSET) {
        this->step_++;
        this->send_step_();
        return;
      }
      put_le16(payload, 0x0001);  // apply the dynamic model only
      payload[2] = this->dynamic_model_;
      this->send_(UBX_CLASS_CFG, UBX_CFG_NAV5, payload, 36);
      break;
    default: {
      uint8_t count;
      const uint8_t *messages = this->messages_(count);
      const uint8_t index = this->step_ - STEP_MESSAGES;
      if (index >= count) {
        this->step_ = STEP_DONE;
        this->last_epoch_ms_ = App.get_loop_component_start_time();
        ESP_LOGD(TAG, "Receiver configured");
        return;
      }
      payload[0] = UBX_CLASS_NAV;
      payload[1] = messages[index];
      payload[2] = 1;  // every solution
      this->send_(UBX_CLASS_CFG, UBX_CFG_MSG, payload, 3);
      break;
    }
  }
  this->set_timeout(STEP_TIMEOUT_ID, STEP_TIMEOUT_MS, [this]() { this->step_timeout_(); });
}

void UBXGNSS::step_done_() {
  this->cancel_timeout(STEP_TIMEOUT_ID);
  // A MON-VER answer may arrive while a restart after the last attempt is pending
  this->cancel_timeout(RESTART_ID);
  this->retries_ = 0;
  this->step_++;
  this->send_step_();
}

void UBXGNSS::step_timeout_() {
  if (++this->retries_ < STEP_RETRIES) {
    this->send_step_();
    return;
  }
  if (this->step_ == STEP_MON_VER) {
    ESP_LOGW(TAG, "No answer to MON-VER, check the wiring and baud rate");
    this->status_set_warning();
    this->clear_data_();
    this->set_timeout(RESTART_ID, RESTART_DELAY_MS, [this]() { this->start_setup_(); });
    return;
  }
  ESP_LOGW(TAG, "No answer to configuration step %u, skipping it", this->step_);
  this->step_done_();
}

void UBXGNSS::handle_frame_() {
  const uint8_t cls = this->parser_.msg_class();
  const uint8_t id = this->parser_.msg_id();
  const uint8_t *p = this->parser_.payload();
  const uint16_t len = this->parser_.length();

  if (cls == UBX_CLASS_NAV) {
    // Every NAV message starts with the time of week of its epoch; a new one starts collecting afresh
    if (len < 4)
      return;
    const uint32_t itow = encode_value_le<uint32_t>(p);
    if (itow != this->epoch_itow_) {
      this->epoch_ = gnss::GNSSData{};
      this->fix_ = false;
      this->epoch_itow_ = itow;
    }
    if (decode_nav(id, p, len, millis_64(), this->epoch_, this->fix_) && id == UBX_NAV_TIMEUTC)
      this->commit_epoch_();
    return;
  }
  if (this->step_ == STEP_PRT_POLL && cls == UBX_CLASS_CFG && id == UBX_CFG_PRT && len >= sizeof(this->port_)) {
    memcpy(this->port_, p, sizeof(this->port_));
    this->step_done_();
    return;
  }
  if (this->step_ == STEP_MON_VER && cls == UBX_CLASS_MON && id == UBX_MON_VER) {
    if (len < 40)
      return;
    this->protocol_version_ = parse_protocol_version(p, len);
    memcpy(this->sw_version_, p, sizeof(this->sw_version_) - 1);
    memcpy(this->hw_version_, p + 30, sizeof(this->hw_version_) - 1);
    ESP_LOGD(TAG, "Firmware %s, hardware %s, protocol %u.%02u", this->sw_version_, this->hw_version_,
             this->protocol_version_ / 100, this->protocol_version_ % 100);
    this->step_done_();
    return;
  }
  // ACK and NAK name the class and id of the configuration message they answer
  if (cls == UBX_CLASS_ACK && len >= 2 && p[0] == UBX_CLASS_CFG && this->step_ != STEP_DONE &&
      this->step_ != STEP_MON_VER) {
    // The port poll completes on its answer, not on its ACK
    static constexpr uint8_t STEP_IDS[] = {0, 0xFF, UBX_CFG_PRT, UBX_CFG_RATE, UBX_CFG_NAV5};
    const uint8_t expected = this->step_ < STEP_MESSAGES ? STEP_IDS[this->step_] : UBX_CFG_MSG;
    if (p[1] != expected)
      return;
    if (id == UBX_ACK_NAK) {
      ESP_LOGW(TAG, "Receiver rejected configuration step %u", this->step_);
    }
    this->step_done_();
  }
}

void UBXGNSS::commit_epoch_() {
  auto &e = this->epoch_;
  if (!this->fix_) {
    e.latitude = e.longitude = NAN;
    e.altitude = e.speed = e.course = e.horizontal_accuracy = e.vertical_accuracy = NAN;
  }
  // The last valid UTC stays; the time source ignores it once it is stale
  if (e.utc == 0) {
    e.utc = this->data_.utc;
    e.utc_millis = this->data_.utc_millis;
  }
  this->data_ = e;
  this->last_epoch_ms_ = App.get_loop_component_start_time();
  this->status_clear_warning();
  this->publish_fix_();
}

void UBXGNSS::loop() {
  this->read_available([this](uint8_t byte) {
    if (this->parser_.feed(byte))
      this->handle_frame_();
  });
  if (this->step_ != STEP_DONE)
    return;
  const uint32_t timeout = std::max<uint32_t>(EPOCH_TIMEOUT_MS, 3u * this->measurement_rate_ms_);
  if (App.get_loop_component_start_time() - this->last_epoch_ms_ > timeout) {
    ESP_LOGW(TAG, "No navigation data, configuring the receiver again");
    this->status_set_warning();
    this->clear_data_();
    this->start_setup_();
  }
}

void UBXGNSS::dump_config() {
  ESP_LOGCONFIG(TAG, "UBX GNSS:");
  if (this->sw_version_[0] == '\0') {
    ESP_LOGCONFIG(TAG, "  Receiver: no answer to MON-VER yet");
  } else if (this->protocol_version_ == 0) {
    // u-blox 6 and older do not report it
    ESP_LOGCONFIG(TAG,
                  "  Firmware: %s\n"
                  "  Hardware: %s\n"
                  "  Protocol: older than 14.00",
                  this->sw_version_, this->hw_version_);
  } else {
    ESP_LOGCONFIG(TAG,
                  "  Firmware: %s\n"
                  "  Hardware: %s\n"
                  "  Protocol: %u.%02u",
                  this->sw_version_, this->hw_version_, this->protocol_version_ / 100, this->protocol_version_ % 100);
  }
  ESP_LOGCONFIG(TAG,
                "  Messages: %s\n"
                "  Measurement rate: %u ms",
                this->protocol_version_ >= UBX_PROTVER_NAV_PVT ? LOG_STR_LITERAL("NAV-PVT, NAV-DOP, NAV-TIMEUTC")
                                                               : LOG_STR_LITERAL("NAV-POSLLH, NAV-VELNED, NAV-SOL, "
                                                                                 "NAV-DOP, NAV-TIMEUTC"),
                this->measurement_rate_ms_);
  if (this->dynamic_model_ != UBX_DYNAMIC_MODEL_UNSET) {
    ESP_LOGCONFIG(TAG, "  Dynamic model: %u", this->dynamic_model_);
  }
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::ubx
