#include "epaper_spi_spectra_e6_dual_cs.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static constexpr const char *const TAG = "epaper_spi.e6.dual";

static constexpr uint8_t CMD_AN_TM = 0x74;
static constexpr uint8_t CMD_CMD66 = 0xF0;
static constexpr uint8_t CMD_PSR = 0x00;
static constexpr uint8_t CMD_CDI = 0x50;
static constexpr uint8_t CMD_TCON = 0x60;
static constexpr uint8_t CMD_AGID = 0x86;
static constexpr uint8_t CMD_PWS = 0xE3;
static constexpr uint8_t CMD_CCSET = 0xE0;
static constexpr uint8_t CMD_TRES = 0x61;
static constexpr uint8_t CMD_PWR_EPD = 0x01;
static constexpr uint8_t CMD_EN_BUF = 0xB6;
static constexpr uint8_t CMD_BTST_P = 0x06;
static constexpr uint8_t CMD_BOOST_VDDP_EN = 0xB7;
static constexpr uint8_t CMD_BTST_N = 0x05;
static constexpr uint8_t CMD_BUCK_BOOST_VDDN = 0xB0;
static constexpr uint8_t CMD_TFT_VCOM_POWER = 0xB1;
static constexpr uint8_t CMD_DTM = 0x10;
static constexpr uint8_t CMD_PON = 0x04;
static constexpr uint8_t CMD_DRF = 0x12;
static constexpr uint8_t CMD_POF = 0x02;
static constexpr uint8_t CMD_DSLP = 0x07;

// Each IC handles 600px wide × 1600 rows = 300 bytes/row × 1600 rows = 480000 bytes
static constexpr uint32_t HALF_FRAME_BYTES = 480000;
static constexpr uint16_t COLS_PER_IC = 300;  // bytes per row per IC

void EPaperSpectraE6DualCS::setup() {
  EPaperBase::setup();
  if (this->cs_slave_ != nullptr) {
    this->cs_slave_->setup();
    this->cs_slave_->digital_write(true);
  }
}

void EPaperSpectraE6DualCS::dump_config() {
  EPaperBase::dump_config();
  LOG_PIN("  Slave CS Pin: ", this->cs_slave_);
}

bool EPaperSpectraE6DualCS::initialise(bool partial) {
  this->transfer_to_slave_ = false;
  this->current_data_index_ = 0;

  // AN_TM — master only
  static constexpr uint8_t an_tm_data[] = {0xC0, 0x1C, 0x1C, 0xCC, 0xCC, 0xCC, 0x15, 0x15, 0x55};
  this->cmd_data(CMD_AN_TM, an_tm_data, sizeof an_tm_data);

  // CMD66 — both ICs
  static constexpr uint8_t cmd66_data[] = {0x49, 0x55, 0x13, 0x5D, 0x05, 0x10};
  this->both_cmd_data_(CMD_CMD66, cmd66_data, sizeof cmd66_data);

  // PSR — both ICs
  static constexpr uint8_t psr_data[] = {0xDF, 0x69};
  this->both_cmd_data_(CMD_PSR, psr_data, sizeof psr_data);

  // CDI — both ICs
  static constexpr uint8_t cdi_data[] = {0xF7};
  this->both_cmd_data_(CMD_CDI, cdi_data, sizeof cdi_data);

  // TCON — both ICs
  static constexpr uint8_t tcon_data[] = {0x03, 0x03};
  this->both_cmd_data_(CMD_TCON, tcon_data, sizeof tcon_data);

  // AGID — both ICs
  static constexpr uint8_t agid_data[] = {0x10};
  this->both_cmd_data_(CMD_AGID, agid_data, sizeof agid_data);

  // PWS — both ICs
  static constexpr uint8_t pws_data[] = {0x22};
  this->both_cmd_data_(CMD_PWS, pws_data, sizeof pws_data);

  // CCSET — both ICs
  static constexpr uint8_t ccset_data[] = {0x01};
  this->both_cmd_data_(CMD_CCSET, ccset_data, sizeof ccset_data);

  // TRES — both ICs: encodes per-IC resolution per community implementation
  static constexpr uint8_t tres_data[] = {0x04, 0xB0, 0x03, 0x20};
  this->both_cmd_data_(CMD_TRES, tres_data, sizeof tres_data);

  // PWR_EPD — master only
  static constexpr uint8_t pwr_epd_data[] = {0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38};
  this->cmd_data(CMD_PWR_EPD, pwr_epd_data, sizeof pwr_epd_data);

  // EN_BUF — master only
  static constexpr uint8_t en_buf_data[] = {0x07};
  this->cmd_data(CMD_EN_BUF, en_buf_data, sizeof en_buf_data);

  // BTST_P — master only
  static constexpr uint8_t btst_p_data[] = {0xE8, 0x28};
  this->cmd_data(CMD_BTST_P, btst_p_data, sizeof btst_p_data);

  // BOOST_VDDP_EN — master only
  static constexpr uint8_t boost_vddp_en_data[] = {0x01};
  this->cmd_data(CMD_BOOST_VDDP_EN, boost_vddp_en_data, sizeof boost_vddp_en_data);

  // BTST_N — master only
  static constexpr uint8_t btst_n_data[] = {0xE8, 0x28};
  this->cmd_data(CMD_BTST_N, btst_n_data, sizeof btst_n_data);

  // BUCK_BOOST_VDDN — master only
  static constexpr uint8_t buck_boost_vddn_data[] = {0x01};
  this->cmd_data(CMD_BUCK_BOOST_VDDN, buck_boost_vddn_data, sizeof buck_boost_vddn_data);

  // TFT_VCOM_POWER — master only
  static constexpr uint8_t tft_vcom_power_data[] = {0x02};
  this->cmd_data(CMD_TFT_VCOM_POWER, tft_vcom_power_data, sizeof tft_vcom_power_data);

  ESP_LOGI(TAG, "Init complete; buffer[0]=0x%02X buffer[300]=0x%02X", this->buffer_[0], this->buffer_[300]);
  return true;
}

bool HOT EPaperSpectraE6DualCS::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];

  if (!this->transfer_to_slave_) {
    // Phase 1: master IC — left 600px columns (bytes 0..299 of each row, all 1600 rows)
    // CS must remain asserted for the entire DTM command + data stream; pulsing CS between
    // chunks resets the IC's internal data pointer, so we call enable() once here and
    // disable() only after all data has been sent.
    if (this->current_data_index_ == 0) {
      ESP_LOGI(TAG, "DTM transfer to master IC (left half, %u bytes)", HALF_FRAME_BYTES);
      this->dc_command_();
      this->enable();  // assert master CS — held for entire transfer
      this->write_byte(CMD_DTM);
      this->dc_data_();
    }
    size_t buf_idx = 0;
    while (this->current_data_index_ != HALF_FRAME_BYTES) {
      uint32_t idx = this->current_data_index_++;
      bytes_to_send[buf_idx++] = this->buffer_[idx / COLS_PER_IC * (COLS_PER_IC * 2) + idx % COLS_PER_IC];

      if (buf_idx == sizeof bytes_to_send) {
        this->write_array(bytes_to_send, buf_idx);  // master CS remains low
        buf_idx = 0;
        if (millis() - start_time > MAX_TRANSFER_TIME) {
          return false;  // yield; CS stays asserted across loop iterations
        }
      }
    }
    if (buf_idx != 0) {
      this->write_array(bytes_to_send, buf_idx);
    }
    this->disable();  // deassert master CS after all data sent
    ESP_LOGI(TAG, "Master IC transfer complete");
    this->current_data_index_ = 0;
    this->transfer_to_slave_ = true;
    return false;
  }

  // Phase 2: slave IC — right 600px columns (bytes 300..599 of each row, all 1600 rows)
  // Same principle: slave CS held low for the entire DTM + data stream.
  if (this->current_data_index_ == 0) {
    ESP_LOGI(TAG, "DTM transfer to slave IC (right half, %u bytes)", HALF_FRAME_BYTES);
    this->dc_command_();
    this->cs_slave_->digital_write(false);  // assert slave CS — held for entire transfer
    this->enable();                         // begin_transaction; also asserts master CS briefly
    if (this->cs_ != nullptr)
      this->cs_->digital_write(true);  // deassert master CS; slave CS stays low
    this->write_byte(CMD_DTM);
    this->dc_data_();
  }
  size_t buf_idx = 0;
  while (this->current_data_index_ != HALF_FRAME_BYTES) {
    uint32_t idx = this->current_data_index_++;
    bytes_to_send[buf_idx++] = this->buffer_[idx / COLS_PER_IC * (COLS_PER_IC * 2) + COLS_PER_IC + idx % COLS_PER_IC];

    if (buf_idx == sizeof bytes_to_send) {
      this->write_array(bytes_to_send, buf_idx);  // slave CS remains low
      buf_idx = 0;
      if (millis() - start_time > MAX_TRANSFER_TIME) {
        return false;  // yield; slave CS stays asserted across loop iterations
      }
    }
  }
  if (buf_idx != 0) {
    this->write_array(bytes_to_send, buf_idx);
  }
  this->cs_slave_->digital_write(true);  // deassert slave CS after all data sent
  this->disable();                       // end_transaction (master CS already deasserted)
  ESP_LOGI(TAG, "Slave IC transfer complete — both ICs loaded");
  this->current_data_index_ = 0;
  this->transfer_to_slave_ = false;
  return true;
}

void EPaperSpectraE6DualCS::power_on() {
  // Diagnostic: sample BUSY for 2s after PON to verify pin is wired correctly.
  // Normal: BUSY goes LOW (active) within ~100ms, then HIGH (idle) after power-on completes.
  // If BUSY never goes LOW: IC is not responding — check BUSY pin wiring or reset timing.
  ESP_LOGI(TAG, "PON → both ICs; BUSY before=%s", this->is_idle_() ? "IDLE" : "ACTIVE");
  this->both_command_(CMD_PON);
  bool seen_busy = false;
  uint32_t const deadline = millis() + 2000;
  while (millis() < deadline) {
    delay(5);
    if (!this->is_idle_()) {
      seen_busy = true;
      ESP_LOGI(TAG, "BUSY went ACTIVE after PON — IC is responding");
      break;
    }
  }
  if (!seen_busy) {
    ESP_LOGE(TAG, "BUSY never went ACTIVE within 2s of PON — check BUSY pin wiring, polarity, or reset timing!");
  }
}

void EPaperSpectraE6DualCS::refresh_screen(bool partial) {
  ESP_LOGD(TAG, "DRF (30ms delay then refresh) → both ICs");
  delay(30);  // required settling time after PON before DRF per manufacturer spec
  static constexpr uint8_t drf_data[] = {0x01};
  this->both_cmd_data_(CMD_DRF, drf_data, sizeof drf_data);
}

void EPaperSpectraE6DualCS::power_off() {
  ESP_LOGD(TAG, "POF → both ICs");
  static constexpr uint8_t pof_data[] = {0x00};
  this->both_cmd_data_(CMD_POF, pof_data, sizeof pof_data);
}

void EPaperSpectraE6DualCS::deep_sleep() {
  ESP_LOGD(TAG, "DSLP → both ICs");
  static constexpr uint8_t dslp_data[] = {0xA5};
  this->both_cmd_data_(CMD_DSLP, dslp_data, sizeof dslp_data);
}

void EPaperSpectraE6DualCS::both_command_(uint8_t cmd) {
  this->dc_command_();
  this->cs_slave_->digital_write(false);
  this->enable();
  this->write_byte(cmd);
  this->disable();
  this->cs_slave_->digital_write(true);
}

void EPaperSpectraE6DualCS::both_cmd_data_(uint8_t cmd, const uint8_t *data, size_t len) {
  this->dc_command_();
  this->cs_slave_->digital_write(false);
  this->enable();
  this->write_byte(cmd);
  if (len > 0) {
    this->dc_data_();
    this->write_array(data, len);
  }
  this->disable();
  this->cs_slave_->digital_write(true);
}

void EPaperSpectraE6DualCS::slave_command_(uint8_t cmd) {
  this->dc_command_();
  this->cs_slave_->digital_write(false);
  this->enable();  // begin_transaction + asserts master CS as side effect
  if (this->cs_ != nullptr)
    this->cs_->digital_write(true);  // deassert master CS; slave CS remains low
  this->write_byte(cmd);
  this->cs_slave_->digital_write(true);
  this->disable();  // end_transaction; master CS already deasserted
}

void EPaperSpectraE6DualCS::slave_start_data_() {
  this->dc_data_();
  this->cs_slave_->digital_write(false);
  this->enable();  // begin_transaction + asserts master CS as side effect
  if (this->cs_ != nullptr)
    this->cs_->digital_write(true);  // deassert master CS; slave CS remains low
}

void EPaperSpectraE6DualCS::slave_stop_data_() {
  this->cs_slave_->digital_write(true);
  this->disable();  // end_transaction
}

}  // namespace esphome::epaper_spi
