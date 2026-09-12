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
  EPaperSpectraE6::setup();
  if (this->cs_secondary_ != nullptr) {
    this->cs_secondary_->setup();
    this->cs_secondary_->digital_write(true);
  }
}

void EPaperSpectraE6DualCS::dump_config() {
  EPaperBase::dump_config();
  LOG_PIN("  Secondary CS Pin: ", this->cs_secondary_);
}

bool EPaperSpectraE6DualCS::initialise(bool partial) {
  this->transfer_to_secondary_ = false;
  this->current_data_index_ = 0;

  // AN_TM — master only
  static constexpr uint8_t AN_TM_DATA[] = {0xC0, 0x1C, 0x1C, 0xCC, 0xCC, 0xCC, 0x15, 0x15, 0x55};
  this->cmd_data(CMD_AN_TM, AN_TM_DATA, sizeof AN_TM_DATA);

  // CMD66 — both ICs
  static constexpr uint8_t CMD66_DATA[] = {0x49, 0x55, 0x13, 0x5D, 0x05, 0x10};
  this->both_cmd_data_(CMD_CMD66, CMD66_DATA, sizeof CMD66_DATA);

  // PSR — both ICs
  static constexpr uint8_t PSR_DATA[] = {0xDF, 0x69};
  this->both_cmd_data_(CMD_PSR, PSR_DATA, sizeof PSR_DATA);

  // CDI — both ICs
  static constexpr uint8_t CDI_DATA[] = {0xF7};
  this->both_cmd_data_(CMD_CDI, CDI_DATA, sizeof CDI_DATA);

  // TCON — both ICs
  static constexpr uint8_t TCON_DATA[] = {0x03, 0x03};
  this->both_cmd_data_(CMD_TCON, TCON_DATA, sizeof TCON_DATA);

  // AGID — both ICs
  static constexpr uint8_t AGID_DATA[] = {0x10};
  this->both_cmd_data_(CMD_AGID, AGID_DATA, sizeof AGID_DATA);

  // PWS — both ICs
  static constexpr uint8_t PWS_DATA[] = {0x22};
  this->both_cmd_data_(CMD_PWS, PWS_DATA, sizeof PWS_DATA);

  // CCSET — both ICs
  static constexpr uint8_t CCSET_DATA[] = {0x01};
  this->both_cmd_data_(CMD_CCSET, CCSET_DATA, sizeof CCSET_DATA);

  // TRES — both ICs: encodes per-IC resolution per community implementation
  static constexpr uint8_t TRES_DATA[] = {0x04, 0xB0, 0x03, 0x20};
  this->both_cmd_data_(CMD_TRES, TRES_DATA, sizeof TRES_DATA);

  // PWR_EPD — master only
  static constexpr uint8_t PWR_EPD_DATA[] = {0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38};
  this->cmd_data(CMD_PWR_EPD, PWR_EPD_DATA, sizeof PWR_EPD_DATA);

  // EN_BUF — master only
  static constexpr uint8_t EN_BUF_DATA[] = {0x07};
  this->cmd_data(CMD_EN_BUF, EN_BUF_DATA, sizeof EN_BUF_DATA);

  // BTST_P — master only
  static constexpr uint8_t BTST_P_DATA[] = {0xE8, 0x28};
  this->cmd_data(CMD_BTST_P, BTST_P_DATA, sizeof BTST_P_DATA);

  // BOOST_VDDP_EN — master only
  static constexpr uint8_t BOOST_VDDP_EN_DATA[] = {0x01};
  this->cmd_data(CMD_BOOST_VDDP_EN, BOOST_VDDP_EN_DATA, sizeof BOOST_VDDP_EN_DATA);

  // BTST_N — master only
  static constexpr uint8_t BTST_N_DATA[] = {0xE8, 0x28};
  this->cmd_data(CMD_BTST_N, BTST_N_DATA, sizeof BTST_N_DATA);

  // BUCK_BOOST_VDDN — master only
  static constexpr uint8_t BUCK_BOOST_VDDN_DATA[] = {0x01};
  this->cmd_data(CMD_BUCK_BOOST_VDDN, BUCK_BOOST_VDDN_DATA, sizeof BUCK_BOOST_VDDN_DATA);

  // TFT_VCOM_POWER — master only
  static constexpr uint8_t TFT_VCOM_POWER_DATA[] = {0x02};
  this->cmd_data(CMD_TFT_VCOM_POWER, TFT_VCOM_POWER_DATA, sizeof TFT_VCOM_POWER_DATA);

  return true;
}

bool HOT EPaperSpectraE6DualCS::transfer_data() {
  const uint32_t start_time = App.get_loop_component_start_time();
  uint8_t bytes_to_send[MAX_TRANSFER_SIZE];

  if (!this->transfer_to_secondary_) {
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
    this->transfer_to_secondary_ = true;
    return false;
  }

  // Phase 2: secondary IC — right 600px columns (bytes 300..599 of each row, all 1600 rows)
  // Same principle: secondary CS held low for the entire DTM + data stream.
  if (this->current_data_index_ == 0) {
    ESP_LOGI(TAG, "DTM transfer to secondary IC (right half, %u bytes)", HALF_FRAME_BYTES);
    this->dc_command_();
    this->cs_secondary_->digital_write(false);  // assert secondary CS — held for entire transfer
    this->enable();                             // begin_transaction; also asserts master CS briefly
    if (this->cs_ != nullptr)
      this->cs_->digital_write(true);  // deassert master CS; secondary CS stays low
    this->write_byte(CMD_DTM);
    this->dc_data_();
  }
  size_t buf_idx = 0;
  while (this->current_data_index_ != HALF_FRAME_BYTES) {
    uint32_t idx = this->current_data_index_++;
    bytes_to_send[buf_idx++] = this->buffer_[idx / COLS_PER_IC * (COLS_PER_IC * 2) + COLS_PER_IC + idx % COLS_PER_IC];

    if (buf_idx == sizeof bytes_to_send) {
      this->write_array(bytes_to_send, buf_idx);  // secondary CS remains low
      buf_idx = 0;
      if (millis() - start_time > MAX_TRANSFER_TIME) {
        return false;  // yield; secondary CS stays asserted across loop iterations
      }
    }
  }
  if (buf_idx != 0) {
    this->write_array(bytes_to_send, buf_idx);
  }
  this->cs_secondary_->digital_write(true);  // deassert secondary CS after all data sent
  this->disable();                           // end_transaction (master CS already deasserted)
  ESP_LOGI(TAG, "Secondary IC transfer complete — both ICs loaded");
  this->current_data_index_ = 0;
  this->transfer_to_secondary_ = false;
  return true;
}

void EPaperSpectraE6DualCS::power_on() {
  ESP_LOGD(TAG, "PON → both ICs");
  this->both_command_(CMD_PON);
  this->next_delay_ = 30;  // 30ms settling time before DRF per manufacturer spec
}

void EPaperSpectraE6DualCS::refresh_screen(bool partial) {
  ESP_LOGD(TAG, "DRF → both ICs");
  static constexpr uint8_t DRF_DATA[] = {0x01};
  this->both_cmd_data_(CMD_DRF, DRF_DATA, sizeof DRF_DATA);
}

void EPaperSpectraE6DualCS::power_off() {
  ESP_LOGD(TAG, "POF → both ICs");
  static constexpr uint8_t POF_DATA[] = {0x00};
  this->both_cmd_data_(CMD_POF, POF_DATA, sizeof POF_DATA);
}

void EPaperSpectraE6DualCS::deep_sleep() {
  ESP_LOGD(TAG, "DSLP → both ICs");
  static constexpr uint8_t DSLP_DATA[] = {0xA5};
  this->both_cmd_data_(CMD_DSLP, DSLP_DATA, sizeof DSLP_DATA);
  for (auto *pin : this->enable_pins_)
    pin->digital_write(false);
}

void EPaperSpectraE6DualCS::both_command_(uint8_t cmd) { this->both_cmd_data_(cmd, nullptr, 0); }

void EPaperSpectraE6DualCS::both_cmd_data_(uint8_t cmd, const uint8_t *data, size_t len) {
  this->dc_command_();
  this->cs_secondary_->digital_write(false);
  this->enable();
  this->write_byte(cmd);
  if (len > 0) {
    this->dc_data_();
    this->write_array(data, len);
  }
  this->disable();
  this->cs_secondary_->digital_write(true);
}

}  // namespace esphome::epaper_spi
