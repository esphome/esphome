#include "sdspi_defines.h"
#include "sdspi_driver.h"

namespace esphome {
namespace fatfs_esp32_sdspi {

static const char *const TAG = "sdspi_driver";
#ifndef USE_SD_CRC
#define USE_SD_CRC 2  // NOLINT
#endif

//==============================================================================
#if USE_SD_CRC
// CRC functions
//------------------------------------------------------------------------------
static uint8_t crc7(const uint8_t *data, uint8_t n) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < n; i++) {
    uint8_t d = data[i];
    for (uint8_t j = 0; j < 8; j++) {
      crc <<= 1;
      if ((d & 0x80) ^ (crc & 0x80)) {
        crc ^= 0x09;
      }
      d <<= 1;
    }
  }
  return (crc << 1) | 1;
}
//------------------------------------------------------------------------------
#if USE_SD_CRC == 1
// Shift based CRC-CCITT
// uses the x^16,x^12,x^5,x^1 polynomial.
static uint16_t crc_ccitt(const uint8_t *data, size_t n) {
  uint16_t crc = 0;
  for (size_t i = 0; i < n; i++) {
    crc = (uint8_t) (crc >> 8) | (crc << 8);
    crc ^= data[i];
    crc ^= (uint8_t) (crc & 0xff) >> 4;
    crc ^= crc << 12;
    crc ^= (crc & 0xff) << 5;
  }
  return crc;
}
#elif USE_SD_CRC > 1  // crc_ccitt
//------------------------------------------------------------------------------
// Table based CRC-CCITT
// uses the x^16,x^12,x^5,x^1 polynomial.
#ifdef __AVR__
static const uint16_t CRCTAB[] PROGMEM = {
#else   // __AVR__
static const uint16_t CRCTAB[] = {
#endif  // __AVR__
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD,
    0xE1CE, 0xF1EF, 0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A,
    0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B,
    0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC, 0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861,
    0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B, 0x5AF5, 0x4AD4, 0x7AB7, 0x6A96,
    0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87,
    0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A,
    0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3,
    0x5004, 0x4025, 0x7046, 0x6067, 0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290,
    0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E,
    0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F,
    0x99C8, 0x89E9, 0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3, 0xCB7D, 0xDB5C,
    0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83,
    0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74,
    0x2E93, 0x3EB2, 0x0ED1, 0x1EF0};
static uint16_t crc_ccitt(const uint8_t *data, size_t n) {
  uint16_t crc = 0;
  for (size_t i = 0; i < n; i++) {
#ifdef __AVR__
    crc = pgm_read_word(&CRCTAB[(crc >> 8 ^ data[i]) & 0XFF]) ^ (crc << 8);
#else   // __AVR__
    crc = CRCTAB[(crc >> 8 ^ data[i]) & 0XFF] ^ (crc << 8);
#endif  // __AVR__
  }
  return crc;
}
#endif  // crc_ccitt
#endif  // USE_SD_CRC

// ////------------------------------------------------------------------------------
// uint8_t SDSPIDriver::connection_state()
// {
//   ESP_LOGD(TAG,"Connection state: %d", card_con_state_);
//   return card_con_state_;
// }
//------------------------------------------------------------------------------

class Timeout {
 public:
  Timeout() {}
  explicit Timeout(uint16_t ms) { set(ms); }
  uint16_t millis16() { return millis(); }
  void set(uint16_t ms) { end_tile_ = ms + millis16(); }
  bool timed_out() { return (int16_t) (end_tile_ - millis16()) < 0; }

 private:
  uint16_t end_tile_;
};

//------------------------------------------------------------------------------

bool SDSPIDriver::wait(uint16_t ms) {  // waitReady
  Timeout timeout(ms);
  while (this->transfer_byte(0xFF) != 0XFF) {
    if (timeout.timed_out()) {
      return false;
    }
  }
  return true;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::spi_start() {  //  spiStart
  is_active_ = true;
  this->enable();
  this->write_byte(0XFF);
  // uint8_t dummy_byte = this->read_byte();
  // ESP_LOGD(TAG,"SPI check. reply=%02X",dummy_byte);
  // this->disable();
  return is_active_;
}

//------------------------------------------------------------------------------

void SDSPIDriver::spi_stop() {  //    spiStop
  if (is_active_) {
    // this->disable();
    // Insure MISO goes to low Z.
    this->write_byte(0XFF);
    this->disable();
    is_active_ = false;
  }
}

//------------------------------------------------------------------------------

void SDSPIDriver::uninit() {
  // this->disable();
  card_con_state_ = STA_NOINIT;
}

// card_con_state_ &= ~STA_NOINIT;
//------------------------------------------------------------------------------

bool SDSPIDriver::init() {
  // Check if card already inited

  last_err_ = 0;
  if (!(card_con_state_ & STA_NOINIT)) {
    ESP_LOGD(TAG, "IO driver already initilized.");
    return true;
  }

  Timeout timeout;

  ESP_LOGD(TAG, "spi_setup");
  this->spi_teardown();
  this->spi_setup();  // !!!!!!

  ESP_LOGD(TAG, "spi_disable");
  state_ = IDLE_STATE;
  // this->disable();

  ESP_LOGD(TAG, "spi_start");
  if (!this->spi_start()) {
    ESP_LOGE(TAG, "SPI not active.");
    return false;
  }

  // must supply min of 74 clock cycles with CS high.
  this->disable();  // digital_write(true)   //     spiUnselect();
  for (uint8_t i = 0; i < 10; i++) {
    this->write_byte(0XFF);
  }
  this->enable();  // digital_write(false)  //     spiSelect();

  ESP_LOGD(TAG, "Send SPI command");
  // Command to go idle in SPI mode
  for (uint8_t i = 1;; i++) {
    if (this->spi_command(CMD0, 0) == R1_IDLE_STATE) {
      break;
    }
    if (i == SD_CMD0_RETRY) {
      last_err_ = SD_CARD_ERROR_CMD0;
      this->spi_stop();
      return false;
    }
  }

  ESP_LOGD(TAG, "Check CRC");

#if USE_SD_CRC
  if (spi_command(CMD59, 1) != R1_IDLE_STATE) {
    last_err_ = SD_CARD_ERROR_CMD59;
    this->spi_stop();
    return false;
  }
#endif  // USE_SD_CRC

  ESP_LOGD(TAG, "Check SD_TYPE");
  // check SD version
  if (!(spi_command(CMD8, 0x1AA) & R1_ILLEGAL_COMMAND)) {
    this->set_type_(SD_CARD_TYPE_SD2);
    uint8_t response = 0xFF;
    for (uint8_t i = 0; i < 4; i++) {
      response = this->transfer_byte(0xFF);
    }
    if (response != 0XAA) {
      last_err_ = SD_CARD_ERROR_CMD8;
      this->spi_stop();
      return false;
    }
  } else {
    this->set_type_(SD_CARD_TYPE_SD1);
  }

  // initialize card and send host supports SDHC if SD2
  uint32_t arg;
  arg = this->get_type() == SD_CARD_TYPE_SD2 ? 0X40000000 : 0;
  timeout.set(SD_INIT_TIMEOUT);
  while (this->spi_app_command(ACMD41, arg) != R1_READY_STATE) {
    // check for timeout
    if (timeout.timed_out()) {
      last_err_ = SD_CARD_ERROR_ACMD41;
      this->spi_stop();
      return false;
    }
  }

  // if SD2 read OCR register to check for SDHC card
  if (this->get_type() == SD_CARD_TYPE_SD2) {
    if (this->spi_command(CMD58, 0)) {
      last_err_ = SD_CARD_ERROR_CMD58;
      this->spi_stop();
      return false;
    }
    if ((this->transfer_byte(0xFF) & 0XC0) == 0XC0) {
      this->set_type_(SD_CARD_TYPE_SDHC);
    }
    // Discard rest of ocr - contains allowed voltage range.
    for (uint8_t i = 0; i < 3; i++) {
      this->transfer_byte(0xFF);
    }
  }

  card_con_state_ &= ~STA_NOINIT;
  this->spi_stop();

  ESP_LOGD(TAG, "IO Drivet initialized. card_con_state = %d", card_con_state_);
  return true;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::reset_io() {  // syncDevice
  if (state_ == WRITE_STATE) {
    return this->write_stop();
  }
  if (state_ == READ_STATE) {
    return this->read_stop();
  }
  return true;
}

//------------------------------------------------------------------------------

uint8_t SDSPIDriver::spi_app_command(uint8_t cmd, uint32_t arg) {
  this->spi_command(CMD55, 0);
  return this->spi_command(cmd, arg);
}

//------------------------------------------------------------------------------

uint8_t SDSPIDriver::spi_command(uint8_t cmd, uint32_t arg) {  //  cardCommand
  if (!this->reset_io()) {
    return 0XFF;
  }
  // select card
  if (!is_active_) {
    this->spi_start();
  }
  if (cmd != CMD12) {
    if (!this->wait(SD_CMD_TIMEOUT) && cmd != CMD0) {
      return 0XFF;
    }
  }

#if USE_SD_CRC
  // form message
  uint8_t buf[6];
  buf[0] = (uint8_t) 0x40U | cmd;
  buf[1] = (uint8_t) (arg >> 24U);
  buf[2] = (uint8_t) (arg >> 16U);
  buf[3] = (uint8_t) (arg >> 8U);
  buf[4] = (uint8_t) arg;

  // add CRC
  buf[5] = crc7(buf, 5);

  // send message
  this->write_array(buf, 6);

#else   // USE_SD_CRC
  // send command
  this->write_byte(cmd | 0x40);

  // send argument
  uint8_t *pa = reinterpret_cast<uint8_t *>(&arg);
  for (int8_t i = 3; i >= 0; i--) {
    this->write_byte(pa[i]);
  }

  // send CRC - correct for CMD0 with arg zero or CMD8 with arg 0X1AA
  this->write_byte(cmd == CMD0 ? 0X95 : 0X87);
#endif  // USE_SD_CRC

  // discard first fill byte to avoid MISO pull-up problem.
  this->transfer_byte(0xFF);

  // there are 1-8 fill bytes before response.  fill bytes should be 0XFF.
  uint16_t n = 0;
  do {
    response_ = this->transfer_byte(0xFF);
  } while (response_ & 0X80 && ++n < 10);
  return response_;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::write_start(uint32_t sector) {
  // use address if not SDHC card
  if (this->get_type() != SD_CARD_TYPE_SDHC) {
    sector <<= 9;
  }
  if (this->spi_command(CMD25, sector)) {
    last_err_ = SD_CARD_ERROR_CMD25;
    this->spi_stop();
    return false;
  }
  state_ = WRITE_STATE;
  return true;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::write_stop() {
  if (!this->wait(SD_WRITE_TIMEOUT)) {
    last_err_ = SD_CARD_ERROR_STOP_TRAN;
    this->spi_stop();
    return false;
  }
  this->write_byte(STOP_TRAN_TOKEN);
  this->spi_stop();
  state_ = IDLE_STATE;
  return true;
}

//------------------------------------------------------------------------------
bool SDSPIDriver::write_data(uint8_t token, const uint8_t *src) {
#if USE_SD_CRC
  uint16_t crc = crc_ccitt(src, this->sector_size());
#else                                           // USE_SD_CRC
  uint16_t crc = 0XFFFF;
#endif                                          // USE_SD_CRC
  this->write_byte(token);                      //  spiSend
  this->write_array(src, this->sector_size());  //  spiSend
  this->write_byte(crc >> 8);                   //  spiSend
  this->write_byte(crc & 0XFF);

  response_ = this->transfer_byte(0xFF);  // spiReceive
  if ((response_ & DATA_RES_MASK) != DATA_RES_ACCEPTED) {
    last_err_ = SD_CARD_ERROR_WRITE_DATA;
    this->spi_stop();
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::write_data(const uint8_t *src) {
  // wait for previous write to finish
  if (!this->wait(SD_WRITE_TIMEOUT)) {
    last_err_ = SD_CARD_ERROR_WRITE_TIMEOUT;
    this->spi_stop();
    return false;
  }
  if (!this->write_data(WRITE_MULTIPLE_TOKEN, src)) {
    this->spi_stop();
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::write_sectors(uint32_t sector, const uint8_t *src, size_t ns) {
  if (!this->write_start(sector)) {
    this->spi_stop();
    return false;
  }
  for (size_t i = 0; i < ns; i++, src += this->sector_size()) {
    if (!this->write_data(src)) {
      this->spi_stop();
      return false;
    }
  }
  return this->write_stop();
}

//------------------------------------------------------------------------------

bool SDSPIDriver::write_sector(uint32_t sector, const uint8_t *src) {
  // use address if not SDHC card
  if (this->get_type() != SD_CARD_TYPE_SDHC) {
    sector <<= 9;
  }
  if (this->spi_command(CMD24, sector)) {
    last_err_ = SD_CARD_ERROR_CMD24;
    this->spi_stop();
    return false;
  }
  if (!this->write_data(DATA_START_SECTOR, src)) {
    this->spi_stop();
    return false;
  }

#if CHECK_FLASH_PROGRAMMING
  // wait for flash programming to complete
  if (!wait(SD_WRITE_TIMEOUT)) {
    last_err_ = SD_CARD_ERROR_WRITE_PROGRAMMING;
    this->spi_stop();
    return false;
  }
  // response is r2 so get and check two bytes for nonzero
  if (this->spi_command(CMD13, 0) || this->transfer_byte(0xFF)) {
    last_err_ = SD_CARD_ERROR_CMD13;
    this->spi_stop();
    return false;
  }
#endif  // CHECK_FLASH_PROGRAMMING

  this->spi_stop();
  return true;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::read_start(uint32_t sector) {
  if (this->get_type() != SD_CARD_TYPE_SDHC) {
    sector <<= 9;
  }
  if (this->spi_command(CMD18, sector)) {
    last_err_ = SD_CARD_ERROR_CMD18;
    this->spi_stop();
    return false;
  }
  state_ = READ_STATE;
  return true;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::read_stop() {  // readStop
  state_ = IDLE_STATE;
  uint8_t ret = this->spi_command(CMD12, 0);
  this->spi_stop();
  if (ret) {
    last_err_ = SD_CARD_ERROR_CMD12;
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------

uint8_t SDSPIDriver::read_data(uint8_t *dst) { return this->read_data(dst, this->sector_size()); }

//------------------------------------------------------------------------------

uint8_t SDSPIDriver::read_data(uint8_t *dst, size_t count) {
#if USE_SD_CRC
  uint16_t crc;
#endif  // USE_SD_CRC

  // wait for start sector token
  Timeout timeout(SD_READ_TIMEOUT);
  while ((response_ = this->transfer_byte(0xFF)) == 0XFF) {
    if (timeout.timed_out()) {
      last_err_ = SD_CARD_ERROR_READ_TIMEOUT;
      this->spi_stop();
      return false;
    }
  }

  if (response_ != DATA_START_SECTOR) {
    last_err_ = SD_CARD_ERROR_READ_TOKEN;
    this->spi_stop();
    return false;
  }

  this->read_array(dst, count);

  // // transfer data
  // if ((m_status = read_array(dst, count))) {
  //   error(SD_CARD_ERROR_DMA);
  //   goto fail;
  // }

#if USE_SD_CRC
  // get crc
  crc = (this->transfer_byte(0xFF) << 8) | this->transfer_byte(0xFF);
  if (crc != crc_ccitt(dst, count)) {
    last_err_ = SD_CARD_ERROR_READ_CRC;
    this->spi_stop();
    return false;
  }
#else   // USE_SD_CRC
  // discard crc
  this->transfer_byte(0xFF);
  this->transfer_byte(0xFF);
#endif  // USE_SD_CRC

  // this->spi_stop(); // ????
  return true;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::read_sector(uint32_t sector, uint8_t *dst) {
  // use address if not SDHC card
  if (this->get_type() != SD_CARD_TYPE_SDHC) {
    sector <<= 9;
  }
  if (this->spi_command(CMD17, sector)) {
    last_err_ = SD_CARD_ERROR_CMD17;
    this->spi_stop();
    return false;
  }
  if (!read_data(dst, this->sector_size())) {
    this->spi_stop();
    return false;
  }
  this->spi_stop();
  return true;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::read_sectors(uint32_t sector, uint8_t *dst, size_t ns) {
  if (!this->read_start(sector)) {
    return false;
  }
  for (size_t i = 0; i < ns; i++, dst += this->sector_size()) {
    if (!this->read_data(dst, this->sector_size())) {
      this->spi_stop();
      return false;
    }
  }
  return this->read_stop();
}

//------------------------------------------------------------------------------

bool SDSPIDriver::read_status(uint8_t *status) {
  // retrun is R2 so read extra status byte.
  bool ret = true;
  ESP_LOGD(TAG, "Read status");
  if (this - spi_app_command(ACMD13, 0) || this->transfer_byte(0xFF)) {
    last_err_ = SD_CARD_ERROR_ACMD13;
    this->spi_stop();
    return false;
  }

  response_ = this->read_data(status, 64);
  this->spi_stop();
  if (!response_) {
    ret = false;
  }
  return ret;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::read_register(uint8_t cmd, void *buf) {
  uint8_t *dst = reinterpret_cast<uint8_t *>(buf);
  if (spi_command(cmd, 0)) {
    last_err_ = SD_CARD_ERROR_READ_REG;
    spi_stop();
    return false;
  }

  response_ = this->read_data(dst, 16);
  spi_stop();
  if (!response_) {
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------

uint32_t SDSPIDriver::sector_count() {
  csd_t csd;
  uint32_t s_count = read_csd(&csd) ? sd_card_capacity(&csd) : 0;
  ESP_LOGV(TAG, "Read sectors count = %d", s_count);
  return s_count;
}

//------------------------------------------------------------------------------

uint32_t SDSPIDriver::sector_size() {
  csd_t csd;
  uint32_t s_size = 512;
  return s_size;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::erase(uint32_t first_sector, uint32_t last_sector) {
  csd_t csd;
  if (!this->read_csd(&csd)) {
    this->spi_stop();
    return false;
  }
  // check for single sector erase
  if (!csd.v1.erase_blk_en) {
    // erase size mask
    uint8_t m = (csd.v1.sector_size_high << 1) | csd.v1.sector_size_low;
    if ((first_sector & m) != 0 || ((last_sector + 1) & m) != 0) {
      // error card can't erase specified area
      last_err_ = SD_CARD_ERROR_ERASE_SINGLE_SECTOR;
      this->spi_stop();
      return false;
    }
  }
  if (card_type_ != SD_CARD_TYPE_SDHC) {
    first_sector <<= 9;
    last_sector <<= 9;
  }
  if (this->spi_command(CMD32, first_sector) || this->spi_command(CMD33, last_sector) || this->spi_command(CMD38, 0)) {
    last_err_ = SD_CARD_ERROR_ERASE;
    this->spi_stop();
    return false;
  }
  if (!this->wait(SD_ERASE_TIMEOUT)) {
    last_err_ = SD_CARD_ERROR_ERASE_TIMEOUT;
    this->spi_stop();
    return false;
  }
  this->spi_stop();
  return true;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::erase_sector_enable() {
  csd_t csd;
  return read_csd(&csd) ? csd.v1.erase_blk_en : false;
}

//------------------------------------------------------------------------------

bool SDSPIDriver::is_busy() {
  if (state_ == READ_STATE) {
    return false;
  }
  bool spi_active = is_active_;
  if (!spi_active) {
    this->spi_start();
  }
  bool rtn = 0XFF != this->transfer_byte(0xFF);
  if (!spi_active) {
    this->spi_stop();
  }
  return rtn;
}

//------------------------------------------------------------------------------
bool SDSPIDriver::is_ready() {
  if (state_ != IDLE_STATE) {
    for (uint8_t i; i < 3; i++) {
      if (wait(SD_READ_TIMEOUT))
        break;
    }
  }
  return !this->is_busy();
}

//------------------------------------------------------------------------------
// RES_OK (0) The function succeeded.
// RES_ERROR     An error occured.
// RES_PARERR    The command code or parameter is invalid.
// RES_NOTRDY    The device has not been initialized.

uint8_t SDSPIDriver::ioctl(uint8_t cmd, void *buff) {
  ESP_LOGD(TAG, "ioctl cmd=%d", cmd);
  switch (cmd) {
    case CTRL_SYNC: {
      if (this->reset_io()) {
        return RES_OK;
      } else {
        ESP_LOGE(TAG, "ioctl error. Cmd CTRL_SYNC, error %d", last_err_);
        return RES_ERROR;
      }
    }

    case GET_SECTOR_COUNT: {
      uint32_t s_cnt = this->sector_count();
      *((uint32_t *) buff) = s_cnt;  // s_cards[pdrv]->sectors; // this->sector_count()
      return RES_OK;
    }
    case GET_SECTOR_SIZE:
      *((WORD *) buff) = this->sector_size();
      // *((WORD *) buff) = 512;
      return RES_OK;

    case GET_BLOCK_SIZE:
      *((uint32_t *) buff) = 1;
      return RES_OK;
  }
  return RES_PARERR;
}

}  // namespace fatfs_esp32_sdspi
}  // namespace esphome
