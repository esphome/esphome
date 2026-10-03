#include "fusb302b.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include "fusb302b_registers.h"

namespace esphome::fusb302b {

static const char *const TAG = "fusb302b";

// Task notification bits
static constexpr uint32_t NOTIFY_IRQ = 1 << 0;
static constexpr uint32_t NOTIFY_REQUEST = 1 << 1;
static constexpr uint32_t NOTIFY_STOP = 1 << 2;

static constexpr uint32_t TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 18;

static constexpr uint32_t STARTUP_DELAY_MS = 2000;
static constexpr uint32_t CC_SETTLE_MS = 5;
static constexpr uint32_t CC_POLL_INTERVAL_MS = 500;
static constexpr uint32_t PD_POLL_INTERVAL_MS = 100;
// Time to wait for the source to send its capabilities on its own after attach
static constexpr uint32_t SOURCE_CAP_WAIT_MS = 5000;
// Time to wait after each GET_SOURCE_CAP / soft reset before the next recovery step
static constexpr uint32_t SOURCE_CAP_RETRY_MS = 2500;
// Upper bound on back-to-back interrupt handling while INT_N stays asserted
static constexpr uint8_t MAX_IRQ_ROUNDS = 8;
// BC_LVL is read this many extra times to confirm a stable measurement
static constexpr uint8_t CC_STABLE_READS = 5;
static constexpr uint8_t CC_LEVEL_UNSTABLE = 0xFF;

// Type-C default power: 5 V (100 x 50 mV) at 500 mA (50 x 10 mA)
static constexpr PdContract DEFAULT_CONTRACT{PD_PDO_TYPE_FIXED_SUPPLY, 0, 100, 50, 0};

void IRAM_ATTR FUSB302B::gpio_intr(FUSB302B *arg) {
  TaskHandle_t handle = arg->task_.get_handle();
  if (handle == nullptr)
    return;
  BaseType_t woken = pdFALSE;
  xTaskNotifyFromISR(handle, NOTIFY_IRQ, eSetBits, &woken);
  portYIELD_FROM_ISR(woken);
}

void FUSB302B::task_func(void *arg) { static_cast<FUSB302B *>(arg)->run_task_(); }

void FUSB302B::setup() {
  uint8_t device_id = 0;
  if (!this->read_byte(FUSB_DEVICE_ID, &device_id)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (device_id != FUSB_DEVICE_ID_A && device_id != FUSB_DEVICE_ID_B) {
    ESP_LOGE(TAG, "Unknown device ID 0x%02X", device_id);
    this->mark_failed();
    return;
  }
  if (!this->init_chip_()) {
    this->mark_failed(LOG_STR("Initialization failed"));
    return;
  }

  this->interrupt_pin_->setup();
  // The task must exist before the interrupt is attached, since the ISR notifies it
  if (!this->task_.create(FUSB302B::task_func, "fusb302b", TASK_STACK_SIZE, this, TASK_PRIORITY, false)) {
    this->mark_failed(LOG_STR("Failed to create task"));
    return;
  }
  this->interrupt_pin_->attach_interrupt(&FUSB302B::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
}

void FUSB302B::loop() {
  const PdState state = this->state_;
  const uint32_t contract = this->contract_;
  if (state != this->published_state_ || contract != this->published_contract_) {
    const PdState previous = this->published_state_;
    this->published_state_ = state;
    if (contract != this->published_contract_) {
      this->published_contract_ = contract;
      this->publish_contract_(PackedContract{contract});
    }
    this->state_callback_.call(state, previous);
  }
  // Re-enabled by on_state_changed() from the PHY task
  this->disable_loop();
}

void FUSB302B::publish_contract_(PackedContract contract) {
#ifdef USE_SENSOR
  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(contract.voltage());
  if (this->current_sensor_ != nullptr)
    this->current_sensor_->publish_state(contract.current());
#endif
#ifdef USE_TEXT_SENSOR
  if (this->contract_text_sensor_ != nullptr) {
    if (!contract.is_set()) {
      this->contract_text_sensor_->publish_state("Detached");
    } else {
      char buf[24];
      snprintf(buf, sizeof(buf), "%.1fA @ %.0fV", contract.current(), contract.voltage());
      this->contract_text_sensor_->publish_state(buf);
    }
  }
#endif
}

void FUSB302B::dump_config() {
  ESP_LOGCONFIG(TAG,
                "FUSB302B:\n"
                "  Request Voltage: %u V",
                this->get_request_voltage());
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Contract", this->contract_text_sensor_);
#endif
}

void FUSB302B::on_shutdown() {
  this->interrupt_pin_->detach_interrupt();
  // The task parks itself instead of being deleted, so it can never be stopped in the middle of an I2C transfer
  if (this->task_.is_created())
    xTaskNotify(this->task_.get_handle(), NOTIFY_STOP, eSetBits);
}

void FUSB302B::request_voltage(uint8_t voltage) {
  this->set_request_voltage(voltage);
  if (this->task_.is_created())
    xTaskNotify(this->task_.get_handle(), NOTIFY_REQUEST, eSetBits);
}

void FUSB302B::run_task_() {
  // Give the source time to settle after power-up before the first CC measurement
  vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS));

  while (true) {
    uint32_t wait_ms = CC_POLL_INTERVAL_MS;
    if (this->attached_) {
      const bool busy = this->waiting_for_source_caps_ || this->request_pending_ || this->check_ams_();
      wait_ms = busy ? PD_POLL_INTERVAL_MS : UINT32_MAX;
    }
    uint32_t bits = 0;
    xTaskNotifyWait(0, UINT32_MAX, &bits, wait_ms == UINT32_MAX ? portMAX_DELAY : pdMS_TO_TICKS(wait_ms));

    if (bits & NOTIFY_STOP) {
      while (true)
        vTaskSuspend(nullptr);
    }
    if (bits & NOTIFY_REQUEST) {
      // When unattached, the new voltage is simply used at the next attach
      this->request_pending_ = this->attached_;
    }

    // INT_N is held low until the interrupt registers are read, so a new event that arrives while it is
    // already low has no falling edge: keep servicing while the pin stays asserted.
    if (bits & NOTIFY_IRQ)
      this->handle_interrupt_();
    for (uint8_t i = 0; i < MAX_IRQ_ROUNDS && !this->interrupt_pin_->digital_read(); i++)
      this->handle_interrupt_();

    if (!this->attached_) {
      this->try_attach_();
      continue;
    }
    if (this->request_pending_ && !this->check_ams_()) {
      this->request_pending_ = false;
      this->start_negotiation_();
    }
    this->check_source_caps_();
  }
}

bool FUSB302B::init_chip_() {
  // Software reset restores all registers to their defaults
  if (!this->write_byte(FUSB_RESET, FUSB_RESET_SW_RES))
    return false;
  if (!this->reset_pd_())
    return false;

  // Unmask the interrupts used for PD messaging and VBUS changes
  if (!this->write_byte(FUSB_MASK1, FUSB_MASK1_DEFAULT) || !this->write_byte(FUSB_MASKA, 0) ||
      !this->write_byte(FUSB_MASKB, 0))
    return false;

  uint8_t control0 = 0;
  if (!this->read_byte(FUSB_CONTROL0, &control0) ||
      !this->write_byte(FUSB_CONTROL0, control0 & ~FUSB_CONTROL0_INT_MASK))
    return false;

  // Let the chip retransmit unacknowledged messages up to 3 times
  uint8_t control3 = 0;
  if (!this->read_byte(FUSB_CONTROL3, &control3))
    return false;
  control3 =
      (control3 & ~FUSB_CONTROL3_N_RETRIES_MASK) | (3 << FUSB_CONTROL3_N_RETRIES_SHIFT) | FUSB_CONTROL3_AUTO_RETRY;
  if (!this->write_byte(FUSB_CONTROL3, control3))
    return false;

  if (!this->write_byte(FUSB_POWER, FUSB_POWER_ALL))
    return false;
  return this->reset_pd_();
}

bool FUSB302B::reset_pd_() {
  this->reset_protocol_();
  return this->write_byte(FUSB_CONTROL0, FUSB_CONTROL0_TX_FLUSH) &&
         this->write_byte(FUSB_CONTROL1, FUSB_CONTROL1_RX_FLUSH) && this->write_byte(FUSB_RESET, FUSB_RESET_PD_RESET);
}

bool FUSB302B::enable_auto_crc_() {
  uint8_t switches1 = 0;
  return this->read_byte(FUSB_SWITCHES1, &switches1) &&
         this->write_byte(FUSB_SWITCHES1, switches1 | FUSB_SWITCHES1_AUTO_CRC);
}

void FUSB302B::handle_interrupt_() {
  // STATUS0A, STATUS1A, INTERRUPTA, INTERRUPTB, STATUS0, STATUS1, INTERRUPT; reading clears the interrupts
  uint8_t regs[7];
  if (this->read_register(FUSB_STATUS0A, regs, sizeof(regs)) != i2c::ERROR_OK) {
    this->enter_error_();
    return;
  }
  const uint8_t interrupta = regs[2];
  const uint8_t status0 = regs[4];
  uint8_t status1 = regs[5];
  const uint8_t interrupt = regs[6];

  if ((interrupt & FUSB_INTERRUPT_I_VBUSOK) && !(status0 & FUSB_STATUS0_VBUSOK)) {
    if (this->attached_)
      this->detach_();
    return;
  }
  if (interrupta & FUSB_INTERRUPTA_I_HARDRST) {
    ESP_LOGD(TAG, "Hard reset received");
  }
  if (interrupta & FUSB_INTERRUPTA_I_SOFTRST) {
    ESP_LOGV(TAG, "Soft reset received");
  }
  if (interrupta & FUSB_INTERRUPTA_I_RETRYFAIL) {
    ESP_LOGV(TAG, "Message not acknowledged");
  }

  if (!this->attached_)
    return;
  PdMsg msg;
  while (!(status1 & FUSB_STATUS1_RX_EMPTY)) {
    bool is_sop = false;
    if (!this->read_message_(msg, is_sop)) {
      ESP_LOGW(TAG, "Reading message failed");
      // A partial read leaves the FIFO misaligned
      this->write_byte(FUSB_CONTROL1, FUSB_CONTROL1_RX_FLUSH);
      return;
    }
    // Only SOP packets are addressed to a sink; SOP'/SOP'' are for cable plugs
    if (is_sop)
      this->handle_message_(msg);
    if (!this->read_byte(FUSB_STATUS1, &status1))
      return;
  }
}

bool FUSB302B::read_message_(PdMsg &msg, bool &is_sop) {
  uint8_t token = 0;
  uint8_t header[2];
  if (!this->read_byte(FUSB_FIFOS, &token) || this->read_register(FUSB_FIFOS, header, 2) != i2c::ERROR_OK)
    return false;
  msg.set_header(encode_uint16(header[1], header[0]));

  // Data objects, followed by the CRC32 that the chip already checked
  uint8_t buf[PD_MAX_NUM_DATA_OBJECTS * 4 + 4];
  if (this->read_register(FUSB_FIFOS, buf, msg.num_of_obj * 4 + 4) != i2c::ERROR_OK)
    return false;
  for (uint8_t i = 0; i < msg.num_of_obj; i++) {
    const uint8_t *obj = &buf[i * 4];
    msg.data_objects[i] = encode_uint32(obj[3], obj[2], obj[1], obj[0]);
  }
  is_sop = (token & FUSB_FIFO_RX_TOKEN_BITS) == FUSB_FIFO_RX_SOP;
  return true;
}

bool FUSB302B::send_message(const PdMsg &msg) {
  uint8_t buf[4 + 1 + 2 + PD_MAX_NUM_DATA_OBJECTS * 4 + 4];
  size_t len = 0;
  const uint16_t header = msg.get_coded_header();

  buf[len++] = FUSB_TX_TOKEN_SYNC1;
  buf[len++] = FUSB_TX_TOKEN_SYNC1;
  buf[len++] = FUSB_TX_TOKEN_SYNC1;
  buf[len++] = FUSB_TX_TOKEN_SYNC2;
  buf[len++] = FUSB_TX_TOKEN_PACKSYM | (msg.num_of_obj * 4 + 2);
  buf[len++] = header & 0xFF;
  buf[len++] = header >> 8;
  for (uint8_t i = 0; i < msg.num_of_obj; i++) {
    const uint32_t obj = msg.data_objects[i];
    buf[len++] = obj & 0xFF;
    buf[len++] = (obj >> 8) & 0xFF;
    buf[len++] = (obj >> 16) & 0xFF;
    buf[len++] = obj >> 24;
  }
  buf[len++] = FUSB_TX_TOKEN_JAM_CRC;
  buf[len++] = FUSB_TX_TOKEN_EOP;
  buf[len++] = FUSB_TX_TOKEN_TXOFF;
  buf[len++] = FUSB_TX_TOKEN_TXON;

  i2c::ErrorCode err = this->write_register(FUSB_FIFOS, buf, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Sending message type %u failed (error %d)", msg.type, static_cast<int>(err));
    return false;
  }
  return true;
}

bool FUSB302B::measure_cc_(uint8_t meas_switch, uint8_t &level) {
  if (!this->write_byte(FUSB_SWITCHES0, FUSB_SWITCHES0_PDWN_1 | FUSB_SWITCHES0_PDWN_2 | meas_switch))
    return false;
  vTaskDelay(pdMS_TO_TICKS(CC_SETTLE_MS));
  uint8_t status0 = 0;
  if (!this->read_byte(FUSB_STATUS0, &status0))
    return false;
  level = status0 & FUSB_STATUS0_BC_LVL;
  for (uint8_t i = 0; i < CC_STABLE_READS; i++) {
    if (!this->read_byte(FUSB_STATUS0, &status0))
      return false;
    if ((status0 & FUSB_STATUS0_BC_LVL) != level) {
      level = CC_LEVEL_UNSTABLE;
      return true;
    }
  }
  return true;
}

void FUSB302B::try_attach_() {
  if (this->state_ == PdState::PD_STATE_ERROR) {
    if (!this->init_chip_())
      return;
    ESP_LOGI(TAG, "Communication restored");
    this->set_state_(PdState::PD_STATE_DISCONNECTED);
  }

  uint8_t cc1 = 0;
  uint8_t cc2 = 0;
  if (!this->write_byte(FUSB_POWER, FUSB_POWER_ALL) || !this->write_byte(FUSB_SWITCHES1, FUSB_SWITCHES1_SPECREV_REV2) ||
      !this->write_byte(FUSB_MEASURE, FUSB_MEASURE_MDAC_CC) || !this->measure_cc_(FUSB_SWITCHES0_MEAS_CC1, cc1) ||
      !this->measure_cc_(FUSB_SWITCHES0_MEAS_CC2, cc2)) {
    this->enter_error_();
    return;
  }
  // Exactly one CC line carries the source pull-up; the other one is open (or VCONN)
  uint8_t meas_switch;
  uint8_t tx_switch;
  if (cc1 != CC_LEVEL_UNSTABLE && cc1 > 0 && cc2 == 0) {
    meas_switch = FUSB_SWITCHES0_MEAS_CC1;
    tx_switch = FUSB_SWITCHES1_TXCC1;
  } else if (cc2 != CC_LEVEL_UNSTABLE && cc2 > 0 && cc1 == 0) {
    meas_switch = FUSB_SWITCHES0_MEAS_CC2;
    tx_switch = FUSB_SWITCHES1_TXCC2;
  } else {
    return;
  }

  if (!this->write_byte(FUSB_SWITCHES0, FUSB_SWITCHES0_PDWN_1 | FUSB_SWITCHES0_PDWN_2 | meas_switch) ||
      !this->write_byte(FUSB_SWITCHES1, tx_switch | FUSB_SWITCHES1_SPECREV_REV2) || !this->enable_auto_crc_() ||
      !this->reset_pd_()) {
    this->enter_error_();
    return;
  }
  ESP_LOGD(TAG, "Attached on CC%u", meas_switch == FUSB_SWITCHES0_MEAS_CC1 ? 1 : 2);
  this->attached_ = true;
  this->waiting_for_source_caps_ = true;
  this->source_cap_step_ = SourceCapStep::SOURCE_CAP_STEP_WAIT;
  this->source_cap_step_start_ = millis();
  this->set_contract_(DEFAULT_CONTRACT);
  this->set_state_(PdState::PD_STATE_DEFAULT_CONTRACT);
}

void FUSB302B::detach_() {
  ESP_LOGD(TAG, "Detached (VBUS lost)");
  this->attached_ = false;
  this->waiting_for_source_caps_ = false;
  this->request_pending_ = false;
  this->reset_pd_();
  this->clear_contract_();
  this->set_state_(PdState::PD_STATE_DISCONNECTED);
}

void FUSB302B::enter_error_() {
  if (this->state_ == PdState::PD_STATE_ERROR)
    return;
  ESP_LOGW(TAG, "Communication with the chip failed");
  this->attached_ = false;
  this->waiting_for_source_caps_ = false;
  this->request_pending_ = false;
  this->clear_contract_();
  this->set_state_(PdState::PD_STATE_ERROR);
}

void FUSB302B::start_negotiation_() {
  ESP_LOGD(TAG, "Requesting source capabilities for %u V", this->get_request_voltage());
  this->send_message(this->make_control_msg_(PD_CNTRL_GET_SOURCE_CAP));
  this->waiting_for_source_caps_ = true;
  this->source_cap_step_ = SourceCapStep::SOURCE_CAP_STEP_GET_SENT;
  this->source_cap_step_start_ = millis();
}

void FUSB302B::check_source_caps_() {
  if (!this->waiting_for_source_caps_ || this->check_ams_())
    return;
  const uint32_t elapsed = millis() - this->source_cap_step_start_;
  switch (this->source_cap_step_) {
    case SourceCapStep::SOURCE_CAP_STEP_WAIT:
      if (elapsed < SOURCE_CAP_WAIT_MS)
        return;
      ESP_LOGD(TAG, "No source capabilities received, requesting them");
      this->send_message(this->make_control_msg_(PD_CNTRL_GET_SOURCE_CAP));
      this->source_cap_step_ = SourceCapStep::SOURCE_CAP_STEP_GET_SENT;
      break;
    case SourceCapStep::SOURCE_CAP_STEP_GET_SENT:
      if (elapsed < SOURCE_CAP_RETRY_MS)
        return;
      ESP_LOGD(TAG, "No source capabilities received, sending soft reset");
      this->reset_pd_();
      this->send_message(this->make_control_msg_(PD_CNTRL_SOFT_RESET));
      this->source_cap_step_ = SourceCapStep::SOURCE_CAP_STEP_SOFT_RESET_SENT;
      break;
    case SourceCapStep::SOURCE_CAP_STEP_SOFT_RESET_SENT:
      if (elapsed < SOURCE_CAP_RETRY_MS)
        return;
      ESP_LOGW(TAG, "PD negotiation failed, staying at 5 V");
      this->waiting_for_source_caps_ = false;
      this->set_state_(PdState::PD_STATE_PD_TIMEOUT);
      return;
  }
  this->source_cap_step_start_ = millis();
}

}  // namespace esphome::fusb302b

#endif  // USE_ESP32
