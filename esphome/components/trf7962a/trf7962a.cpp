#include "trf7962a.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::trf7962a {

ESPHOME_LOG_TAG(TAG, "trf7962a");

// Address/command word bits
static constexpr uint8_t ADDR_COMMAND = 0x80;
static constexpr uint8_t ADDR_READ = 0x40;
static constexpr uint8_t ADDR_CONTINUOUS = 0x20;

// Direct commands
static constexpr uint8_t CMD_IDLE = 0x00;
static constexpr uint8_t CMD_SOFT_INIT = 0x03;
static constexpr uint8_t CMD_RESET_FIFO = 0x0F;
static constexpr uint8_t CMD_TRANSMIT_CRC = 0x11;

// Registers
static constexpr uint8_t REG_CHIP_STATUS = 0x00;
static constexpr uint8_t REG_ISO_CONTROL = 0x01;
static constexpr uint8_t REG_RX_NO_RESPONSE_WAIT = 0x07;
static constexpr uint8_t REG_MODULATOR = 0x09;
static constexpr uint8_t REG_IRQ_STATUS = 0x0C;
static constexpr uint8_t REG_IRQ_MASK = 0x0D;
static constexpr uint8_t REG_FIFO_STATUS = 0x1C;
static constexpr uint8_t REG_TX_LENGTH_1 = 0x1D;
static constexpr uint8_t REG_FIFO = 0x1F;

static constexpr uint8_t CHIP_STATUS_RF_ON = 0x20;
static constexpr uint8_t CHIP_STATUS_HALF_POWER = 0x10;
static constexpr uint8_t CHIP_STATUS_5V = 0x01;
// ISO 15693, 26.48 kbps, one subcarrier, 1 out of 4, RX CRC checked
static constexpr uint8_t ISO_15693_HIGH_RATE = 0x02;
// 37.76 us steps; must be programmed explicitly in SPI mode (SLOA140B 1.4)
static constexpr uint8_t RX_NO_RESPONSE_WAIT = 0x14;
// 9.44 us steps, the ISO 15693 preset
static constexpr uint8_t RX_WAIT = 0x1F;
// OOK (100%) modulation, SYS_CLK output off
static constexpr uint8_t MODULATOR_OOK = 0x01;
// Reset value plus the no-response interrupt, so an empty scan ends early
static constexpr uint8_t IRQ_MASK = 0x3F;

static constexpr uint8_t IRQ_RX = 0x40;
static constexpr uint8_t IRQ_RX_ERRORS = 0x1E;  // CRC, parity, framing, collision
static constexpr uint8_t IRQ_COLLISION = 0x02;
static constexpr uint8_t IRQ_NO_RESPONSE = 0x01;
static constexpr uint8_t FIFO_COUNT_MASK = 0x0F;

// ISO 15693 INVENTORY, one slot: flags, command, mask length
static constexpr uint8_t INVENTORY_FLAGS = 0x26;
static constexpr uint8_t INVENTORY_COMMAND = 0x01;
// Response: flags, DSFID, 8 UID bytes LSB first (CRC is checked and removed by the chip)
static constexpr uint8_t INVENTORY_RESPONSE_LENGTH = 10;
static constexpr uint8_t UID_MSB = 0xE0;

// ISO 15693 tags need the field for a while before they answer
static constexpr uint32_t FIELD_SETTLE_MS = 10;
static constexpr uint32_t RESPONSE_TIMEOUT_MS = 30;
// Tags at the edge of the field miss single polls
static constexpr uint8_t MISS_THRESHOLD = 3;

void TRF7962A::setup() {
  this->select_pin_->setup();
  this->select_(false);
  this->irq_pin_->setup();

  this->spi_setup();
  this->read_client_.set_spi_parent(this->parent_);
  this->read_client_.set_data_rate(this->data_rate_);
  this->read_client_.spi_setup();
  if (!this->spi_is_ready() || !this->read_client_.spi_is_ready()) {
    this->mark_failed(LOG_STR("SPI device is not ready"));
    return;
  }

  if (!this->configure_()) {
    this->mark_failed(LOG_STR("No response"));
    return;
  }
  this->disable_loop();
}

void TRF7962A::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TRF7962A:\n"
                "  Supply: %s, RF power: %s",
                (this->base_chip_status_ & CHIP_STATUS_5V) ? LOG_STR_LITERAL("5V") : LOG_STR_LITERAL("3.3V"),
                (this->base_chip_status_ & CHIP_STATUS_HALF_POWER) ? LOG_STR_LITERAL("half") : LOG_STR_LITERAL("full"));
  LOG_PIN("  CS Pin: ", this->select_pin_);
  LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  LOG_UPDATE_INTERVAL(this);
}

bool TRF7962A::configure_() {
  this->direct_command_(CMD_SOFT_INIT);
  delay(1);
  this->direct_command_(CMD_IDLE);
  const uint8_t status_iso[] = {ADDR_CONTINUOUS | REG_CHIP_STATUS, this->base_chip_status_, ISO_15693_HIGH_RATE};
  this->write_frame_(status_iso, sizeof(status_iso));
  // Writing the ISO control register reloads the wait times, so this block follows it
  const uint8_t timing[] = {ADDR_CONTINUOUS | REG_RX_NO_RESPONSE_WAIT, RX_NO_RESPONSE_WAIT, RX_WAIT, MODULATOR_OOK};
  this->write_frame_(timing, sizeof(timing));
  this->write_register_(REG_IRQ_MASK, IRQ_MASK);
  this->read_irq_status_();
  // The chip has no ID register; a protocol register reading back proves it is there
  return this->read_register_(REG_ISO_CONTROL) == ISO_15693_HIGH_RATE &&
         this->read_register_(REG_MODULATOR) == MODULATOR_OOK;
}

void TRF7962A::update() {
  if (this->state_ != State::STATE_IDLE)
    return;
  this->set_field_(true);
  this->state_ = State::STATE_FIELD_SETTLE;
  // Same clock as loop(), which can lag millis()
  this->state_start_ = App.get_loop_component_start_time();
  this->enable_loop();
}

void TRF7962A::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t elapsed = now - this->state_start_;
  switch (this->state_) {
    case State::STATE_IDLE:
      this->disable_loop();
      break;
    case State::STATE_FIELD_SETTLE:
      if (elapsed >= FIELD_SETTLE_MS) {
        this->send_inventory_();
        this->state_ = State::STATE_WAIT_RESPONSE;
        this->state_start_ = now;
      }
      break;
    case State::STATE_WAIT_RESPONSE:
      // One IRQ read may carry TX, RX and FIFO bits if loop() ran late
      if (this->irq_pin_->digital_read()) {
        const uint8_t status = this->read_irq_status_();
        ESP_LOGV(TAG, "IRQ 0x%02X", status);
        uint8_t rx[INVENTORY_RESPONSE_LENGTH];
        if ((status & IRQ_RX) && !(status & IRQ_RX_ERRORS) && this->read_response_(rx)) {
          this->finish_scan_(rx);
        } else if (status & IRQ_COLLISION) {
          // Several tags answered; the present one is still there, so this is not a miss
          ESP_LOGD(TAG, "Collision, more than one tag in the field");
          this->end_scan_();
        } else if (status & (IRQ_RX_ERRORS | IRQ_NO_RESPONSE)) {
          this->finish_scan_(nullptr);
        }
      }
      // Also covers an IRQ line that stays asserted without a terminal status
      if (this->state_ == State::STATE_WAIT_RESPONSE && elapsed >= RESPONSE_TIMEOUT_MS) {
        ESP_LOGV(TAG, "Response timed out");
        this->finish_scan_(nullptr);
      }
      break;
  }
}

void TRF7962A::select_(bool selected) { this->select_pin_->digital_write(!selected); }

void TRF7962A::write_frame_(const uint8_t *data, size_t len) {
  this->select_(true);
  this->enable();
  this->write_array(data, len);
  this->disable();
  this->select_(false);
}

void TRF7962A::direct_command_(uint8_t command) {
  // A stand-alone command needs extra clocks before CS rises (SLOA140B 1.3)
  const uint8_t data[2] = {static_cast<uint8_t>(ADDR_COMMAND | command), 0x00};
  this->write_frame_(data, sizeof(data));
}

void TRF7962A::write_register_(uint8_t reg, uint8_t value) {
  const uint8_t data[2] = {reg, value};
  this->write_frame_(data, sizeof(data));
}

void TRF7962A::read_registers_(uint8_t reg, uint8_t *data, size_t len) {
  // CS stays low across the address write and the read, which use opposite clock phases
  this->select_(true);
  this->enable();
  this->write_byte(ADDR_READ | (len > 1 ? ADDR_CONTINUOUS : 0) | reg);
  this->disable();
  this->read_client_.enable();
  this->read_client_.read_array(data, len);
  this->read_client_.disable();
  this->select_(false);
}

uint8_t TRF7962A::read_register_(uint8_t reg) {
  uint8_t value = 0;
  this->read_registers_(reg, &value, 1);
  return value;
}

uint8_t TRF7962A::read_irq_status_() {
  // The register only clears after an extra byte is clocked out
  uint8_t data[2] = {};
  this->read_registers_(REG_IRQ_STATUS, data, sizeof(data));
  return data[0];
}

void TRF7962A::set_field_(bool on) {
  this->write_register_(REG_CHIP_STATUS, this->base_chip_status_ | (on ? CHIP_STATUS_RF_ON : 0));
}

void TRF7962A::send_inventory_() {
  this->direct_command_(CMD_RESET_FIFO);
  // Command, TX length (3 bytes, in the high nibble of byte 2) and FIFO data in one CS window
  const uint8_t data[] = {
      ADDR_COMMAND | CMD_TRANSMIT_CRC,
      ADDR_CONTINUOUS | REG_TX_LENGTH_1,
      0x00,
      0x30,
      INVENTORY_FLAGS,
      INVENTORY_COMMAND,
      0x00,
  };
  this->write_frame_(data, sizeof(data));
}

bool TRF7962A::read_response_(uint8_t *rx) {
  // Read once the whole response is in the FIFO; the count field holds bytes - 1
  const uint8_t available = (this->read_register_(REG_FIFO_STATUS) & FIFO_COUNT_MASK) + 1;
  if (available < INVENTORY_RESPONSE_LENGTH)
    return false;
  this->read_registers_(REG_FIFO, rx, INVENTORY_RESPONSE_LENGTH);
  return true;
}

void TRF7962A::end_scan_() {
  this->direct_command_(CMD_RESET_FIFO);
  this->set_field_(false);
  this->state_ = State::STATE_IDLE;
  this->disable_loop();
}

void TRF7962A::finish_scan_(const uint8_t *rx) {
  this->end_scan_();

  // Response flags bit 0 is the tag's error flag
  const bool tag_present = !this->tag_.get_uid().empty();
  if (rx == nullptr || (rx[0] & 0x01) != 0 || rx[INVENTORY_RESPONSE_LENGTH - 1] != UID_MSB) {
    if (tag_present && ++this->miss_count_ >= MISS_THRESHOLD)
      this->remove_tag_();
    return;
  }
  this->miss_count_ = 0;

  // The UID arrives LSB first; report it MSB first, as printed on tags
  nfc::NfcTagUid uid;
  for (uint8_t i = INVENTORY_RESPONSE_LENGTH; i > 2; i--)
    uid.push_back(rx[i - 1]);

  if (tag_present) {
    const auto &current = this->tag_.get_uid();
    if (std::equal(current.begin(), current.end(), uid.begin(), uid.end()))
      return;
    this->remove_tag_();
  }

  this->tag_.get_uid() = uid;
  char uid_buf[nfc::FORMAT_UID_BUFFER_SIZE];
  nfc::format_uid_to(uid_buf, uid);
  ESP_LOGI(TAG, "Tag detected: %s", uid_buf);
  if (!this->on_tag_callback_.empty())
    this->on_tag_callback_.call(std::string(uid_buf), this->tag_);
#ifdef NFC_TAG_LISTENER_COUNT
  for (auto *listener : this->tag_listeners_)
    listener->tag_on(this->tag_);
#endif
}

void TRF7962A::remove_tag_() {
  char uid_buf[nfc::FORMAT_UID_BUFFER_SIZE];
  nfc::format_uid_to(uid_buf, this->tag_.get_uid());
  ESP_LOGI(TAG, "Tag removed: %s", uid_buf);
  if (!this->on_tag_removed_callback_.empty())
    this->on_tag_removed_callback_.call(std::string(uid_buf), this->tag_);
#ifdef NFC_TAG_LISTENER_COUNT
  for (auto *listener : this->tag_listeners_)
    listener->tag_off(this->tag_);
#endif
  this->tag_.get_uid().clear();
  this->miss_count_ = 0;
}

}  // namespace esphome::trf7962a
