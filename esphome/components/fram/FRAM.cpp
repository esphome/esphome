//
//    FILE: FRAM.cpp
//  AUTHOR: Rob Tillaart
// VERSION: 0.5.3
//    DATE: 2018-01-24
// PURPOSE: Arduino library for I2C FRAM
//     URL: https://github.com/RobTillaart/FRAM_I2C
//
// ESPHome port: https://github.com/sharkydog/esphome-fram

#include "FRAM.h"

namespace esphome {
namespace fram {

// used for metadata and sleep
const uint8_t FRAM_SEC_ADDR = 0x7C;   //  == 0xF8
const uint8_t FRAM_SLEEP_CMD = 0x86;  //
static const char *const TAG = "fram";

/////////////////////////////////////////////////////////////////////////////
//
// FRAM PUBLIC
//

void FRAM::setup() {
  if (!this->is_connected()) {
    ESP_LOGE(TAG, "Device on address 0x%x not found!", this->address_);
    this->mark_failed();
  } else if (!this->get_size()) {
    ESP_LOGW(TAG, "Device on address 0x%x returned 0 size, set size in config!", this->address_);
  }
}

void FRAM::dump_config() {
  ESP_LOGCONFIG(TAG, "FRAM:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%x", this->address_);

  bool ok = this->is_connected();

  if (!ok) {
    ESP_LOGE(TAG, "  Device not found!");
  }

  if (this->size_bytes_) {
    ESP_LOGCONFIG(TAG, "  Size: %luKiB", this->size_bytes_ / 1024UL);
  } else if (ok) {
    ESP_LOGW(TAG, "  Size: 0KiB, set size in config!");
  }
}

bool FRAM::is_connected() {
  i2c::ErrorCode err = this->bus_->write(this->address_, nullptr, 0, true);
  return (err == i2c::ERROR_OK);
}

void FRAM::write8(uint16_t memaddr, uint8_t value) {
  uint8_t val = value;
  this->write_block(memaddr, (uint8_t *) &val, sizeof(uint8_t));
}

void FRAM::write16(uint16_t memaddr, uint16_t value) {
  uint16_t val = value;
  this->write_block(memaddr, (uint8_t *) &val, sizeof(uint16_t));
}

void FRAM::write32(uint16_t memaddr, uint32_t value) {
  uint32_t val = value;
  this->write_block(memaddr, (uint8_t *) &val, sizeof(uint32_t));
}

void FRAM::write_float(uint16_t memaddr, float value) {
  float val = value;
  this->write_block(memaddr, (uint8_t *) &val, sizeof(float));
}

void FRAM::write_double(uint16_t memaddr, double value) {
  double val = value;
  this->write_block(memaddr, (uint8_t *) &val, sizeof(double));
}

void FRAM::write(uint16_t memaddr, uint8_t *obj, uint16_t size) {
  const int blocksize = 24;
  uint8_t *p = obj;

  while (size >= blocksize) {
    this->write_block(memaddr, p, blocksize);
    memaddr += blocksize;
    p += blocksize;
    size -= blocksize;
  }
  //  remaining
  if (size > 0) {
    this->write_block(memaddr, p, size);
  }
}

uint8_t FRAM::read8(uint16_t memaddr) {
  uint8_t val;
  this->read_block(memaddr, (uint8_t *) &val, sizeof(uint8_t));
  return val;
}

uint16_t FRAM::read16(uint16_t memaddr) {
  uint16_t val;
  this->read_block(memaddr, (uint8_t *) &val, sizeof(uint16_t));
  return val;
}

uint32_t FRAM::read32(uint16_t memaddr) {
  uint32_t val;
  this->read_block(memaddr, (uint8_t *) &val, sizeof(uint32_t));
  return val;
}

float FRAM::read_float(uint16_t memaddr) {
  float val;
  this->read_block(memaddr, (uint8_t *) &val, sizeof(float));
  return val;
}

double FRAM::read_double(uint16_t memaddr) {
  double val;
  this->read_block(memaddr, (uint8_t *) &val, sizeof(double));
  return val;
}

void FRAM::read(uint16_t memaddr, uint8_t *obj, uint16_t size) {
  const uint8_t blocksize = 24;
  uint8_t *p = obj;

  while (size >= blocksize) {
    this->read_block(memaddr, p, blocksize);
    memaddr += blocksize;
    p += blocksize;
    size -= blocksize;
  }
  // remainder
  if (size > 0) {
    this->read_block(memaddr, p, size);
  }
}

////////////////////////////////////////////////////////////////////////

int32_t FRAM::read_until(uint16_t memaddr, char *buf, uint16_t buflen, char separator) {
  //  read and fill the buffer at once.
  this->read(memaddr, (uint8_t *) buf, buflen);

  for (uint16_t length = 0; length < buflen; length++) {
    if (buf[length] == separator) {
      buf[length] = 0;  //  replace separator => \0 EndChar
      return length;
    }
  }
  //  entry does not fit in given buffer.
  return (int32_t) -1;
}

int32_t FRAM::read_line(uint16_t memaddr, char *buf, uint16_t buflen) {
  //  read and fill the buffer at once.
  this->read(memaddr, (uint8_t *) buf, buflen);

  for (uint16_t length = 0; length < buflen - 1; length++) {
    if (buf[length] == '\n') {
      buf[length + 1] = 0;  //  add \0 EndChar after '\n'
      return length + 1;
    }
  }
  //  entry does not fit in given buffer.
  return (int32_t) -1;
}

template<class T> uint16_t FRAM::write_object(uint16_t memaddr, T &obj) {
  this->write(memaddr, (uint8_t *) &obj, sizeof(obj));
  return memaddr + sizeof(obj);
}

template<class T> uint16_t FRAM::read_object(uint16_t memaddr, T &obj) {
  this->read(memaddr, (uint8_t *) &obj, sizeof(obj));
  return memaddr + sizeof(obj);
}

////////////////////////////////////////////////////////////////////////

uint16_t FRAM::get_manufacturer_id() { return this->get_metadata_(0); }

uint16_t FRAM::get_product_id() { return this->get_metadata_(1); }

// NOTE: returns the size in kiloBYTE
uint16_t FRAM::get_size() {
  uint16_t density = this->get_metadata_(2);
  uint16_t size = 0;

  if (density > 0) {
    size = (1UL << density);
    this->size_bytes_ = size * 1024UL;
  }

  return size;
}

uint32_t FRAM::get_size_bytes() { return this->size_bytes_; }

//  override to be used when get_size() fails == 0
void FRAM::set_size_bytes(uint32_t value) { this->size_bytes_ = value; }

uint32_t FRAM::clear(uint8_t value) {
  uint8_t buffer[16];

  for (uint8_t &b : buffer) {
    b = value;
  }

  uint32_t start = 0;
  uint32_t end = this->size_bytes_;

  for (uint32_t address = start; address < end; address += 16) {
    this->write_block(address, buffer, 16);
  }

  return end - start;
}

//  EXPERIMENTAL - to be confirmed
//  page 12 datasheet
//  command = S 0xF8 A address A S 86 A P  (A = Ack from device )
void FRAM::sleep() {
  uint8_t addr = this->address_ << 1;
  this->bus_->write(FRAM_SEC_ADDR, &addr, 1, false);
  this->bus_->write(FRAM_SLEEP_CMD >> 1, nullptr, 0, true);
}

//  page 12 datasheet   trec <= 400us
bool FRAM::wakeup(uint32_t trec) {
  bool b = this->is_connected();  //  wakeup

  if (trec == 0) {
    return b;
  }
  //  wait recovery time
  delayMicroseconds(trec);
  return this->is_connected();  //  check recovery OK
}

/////////////////////////////////////////////////////////////////////////////
//
// FRAM PROTECTED
//

//  metadata is packed as  [....MMMM][MMMMDDDD][PPPPPPPP]
//  M = manufacturerID
//  D = density => memory size = 2^D KB
//  P = product ID (together with D)
//  P part might be proprietary
uint16_t FRAM::get_metadata_(uint8_t field) {
  if (field > 2) {
    return 0;
  }

  uint8_t addr = this->address_ << 1;
  this->bus_->write(FRAM_SEC_ADDR, &addr, 1, false);

  uint8_t data[3] = {0, 0, 0};
  i2c::ErrorCode err = this->bus_->read(FRAM_SEC_ADDR, data, 3);
  if (err != i2c::ERROR_OK) {
    return 0;
  }

  //  MANUFACTURER
  if (field == 0) {
    return (data[0] << 4) + (data[1] >> 4);
  }
  //  PRODUCT ID
  if (field == 1) {
    return ((data[1] & 0x0F) << 8) + data[2];
  }
  //  DENSITY
  //           Fujitsu data sheet
  //  3 =>     MB85RC64 = 64 Kbit.
  //  5 =>     MB85RC256
  //  6 =>     MB85RC512
  //  7 =>     MB85RC1M
  if (field == 2) {
    return data[1] & 0x0F;
  }

  return 0;
}

void FRAM::write_block(uint16_t memaddr, uint8_t *obj, uint8_t size) {
  i2c::WriteBuffer buff[2];
  uint8_t maddr[] = {(uint8_t) (memaddr >> 8), (uint8_t) (memaddr & 0xFF)};

  buff[0].data = maddr;
  buff[0].len = 2;
  buff[1].data = obj;
  buff[1].len = size;

  this->bus_->writev(this->address_, buff, 2, true);
}

void FRAM::read_block(uint16_t memaddr, uint8_t *obj, uint8_t size) {
  uint8_t maddr[] = {(uint8_t) (memaddr >> 8), (uint8_t) (memaddr & 0xFF)};
  this->bus_->write(this->address_, maddr, 2, false);
  this->bus_->read(this->address_, obj, size);
}

/////////////////////////////////////////////////////////////////////////////
//
//  FRAM32  PUBLIC
//

void FRAM32::write8(uint32_t memaddr, uint8_t value) {
  uint8_t val = value;
  this->write_block(memaddr, (uint8_t *) &val, sizeof(uint8_t));
}

void FRAM32::write16(uint32_t memaddr, uint16_t value) {
  uint16_t val = value;
  this->write_block(memaddr, (uint8_t *) &val, sizeof(uint16_t));
}

void FRAM32::write32(uint32_t memaddr, uint32_t value) {
  uint32_t val = value;
  this->write_block(memaddr, (uint8_t *) &val, sizeof(uint32_t));
}

void FRAM32::write_float(uint32_t memaddr, float value) {
  float val = value;
  this->write_block(memaddr, (uint8_t *) &val, sizeof(float));
}

void FRAM32::write_double(uint32_t memaddr, double value) {
  double val = value;
  this->write_block(memaddr, (uint8_t *) &val, sizeof(double));
}

void FRAM32::write(uint32_t memaddr, uint8_t *obj, uint16_t size) {
  const int blocksize = 24;
  uint8_t *p = obj;

  while (size >= blocksize) {
    this->write_block(memaddr, p, blocksize);
    memaddr += blocksize;
    p += blocksize;
    size -= blocksize;
  }
  // remaining
  if (size > 0) {
    this->write_block(memaddr, p, size);
  }
}

uint8_t FRAM32::read8(uint32_t memaddr) {
  uint8_t val;
  this->read_block(memaddr, (uint8_t *) &val, sizeof(uint8_t));
  return val;
}

uint16_t FRAM32::read16(uint32_t memaddr) {
  uint16_t val;
  this->read_block(memaddr, (uint8_t *) &val, sizeof(uint16_t));
  return val;
}

uint32_t FRAM32::read32(uint32_t memaddr) {
  uint32_t val;
  this->read_block(memaddr, (uint8_t *) &val, sizeof(uint32_t));
  return val;
}

float FRAM32::read_float(uint32_t memaddr) {
  float val;
  this->read_block(memaddr, (uint8_t *) &val, sizeof(float));
  return val;
}

double FRAM32::read_double(uint32_t memaddr) {
  double val;
  this->read_block(memaddr, (uint8_t *) &val, sizeof(double));
  return val;
}

void FRAM32::read(uint32_t memaddr, uint8_t *obj, uint16_t size) {
  const uint8_t blocksize = 24;
  uint8_t *p = obj;

  while (size >= blocksize) {
    this->read_block(memaddr, p, blocksize);
    memaddr += blocksize;
    p += blocksize;
    size -= blocksize;
  }
  // remainder
  if (size > 0) {
    this->read_block(memaddr, p, size);
  }
}

int32_t FRAM32::read_until(uint32_t memaddr, char *buf, uint16_t buflen, char separator) {
  //  read and fill the buffer at once.
  this->read(memaddr, (uint8_t *) buf, buflen);

  for (uint16_t length = 0; length < buflen; length++) {
    if (buf[length] == separator) {
      buf[length] = 0;  //  replace separator => \0 EndChar
      return length;
    }
  }
  //  entry does not fit in given buffer.
  return (int32_t) -1;
}

int32_t FRAM32::read_line(uint32_t memaddr, char *buf, uint16_t buflen) {
  //  read and fill the buffer at once.
  this->read(memaddr, (uint8_t *) buf, buflen);

  for (uint16_t length = 0; length < buflen - 1; length++) {
    if (buf[length] == '\n') {
      buf[length + 1] = 0;  //  add \0 EndChar after '\n'
      return length + 1;
    }
  }
  //  entry does not fit in given buffer.
  return (int32_t) -1;
}

template<class T> uint32_t FRAM32::write_object(uint32_t memaddr, T &obj) {
  this->write(memaddr, (uint8_t *) &obj, sizeof(obj));
  return memaddr + sizeof(obj);
};

template<class T> uint32_t FRAM32::read_object(uint32_t memaddr, T &obj) {
  this->read(memaddr, (uint8_t *) &obj, sizeof(obj));
  return memaddr + sizeof(obj);
}

/////////////////////////////////////////////////////////////////////////////
//
//  FRAM32  PROTECTED
//

void FRAM32::write_block(uint32_t memaddr, uint8_t *obj, uint8_t size) {
  uint8_t addr = this->address_;

  if (memaddr & 0x00010000) {
    addr += 0x01;
  }

  i2c::WriteBuffer buff[2];
  uint8_t maddr[] = {(uint8_t) (memaddr >> 8), (uint8_t) (memaddr & 0xFF)};

  buff[0].data = maddr;
  buff[0].len = 2;
  buff[1].data = obj;
  buff[1].len = size;

  this->bus_->writev(addr, buff, 2, true);
}

void FRAM32::read_block(uint32_t memaddr, uint8_t *obj, uint8_t size) {
  uint8_t addr = this->address_;

  if (memaddr & 0x00010000) {
    addr += 0x01;
  }

  uint8_t maddr[] = {(uint8_t) (memaddr >> 8), (uint8_t) (memaddr & 0xFF)};
  this->bus_->write(this->address_, maddr, 2, false);
  this->bus_->read(addr, obj, size);
}

/////////////////////////////////////////////////////////////////////////////
//
//  FRAM11
//

/////////////////////////////////////////////////////////////////////////////
//
//  FRAM11  PROTECTED
//

void FRAM11::write_block(uint16_t memaddr, uint8_t *obj, uint8_t size) {
  // Device uses Address Pages
  uint8_t device_addr_with_page_bits = this->address_ | ((memaddr & 0x0700) >> 8);

  i2c::WriteBuffer buff[2];
  uint8_t maddr = memaddr & 0xFF;

  buff[0].data = &maddr;
  buff[0].len = 1;
  buff[1].data = obj;
  buff[1].len = size;

  this->bus_->writev(device_addr_with_page_bits, buff, 2, true);
}

void FRAM11::read_block(uint16_t memaddr, uint8_t *obj, uint8_t size) {
  // Device uses Address Pages
  uint8_t device_addr_with_page_bits = this->address_ | ((memaddr & 0x0700) >> 8);
  uint8_t maddr = memaddr & 0xFF;

  this->bus_->write(device_addr_with_page_bits, &maddr, 1, false);
  this->bus_->read(device_addr_with_page_bits, obj, size);
}

/////////////////////////////////////////////////////////////////////////////
//
//  FRAM9
//

/////////////////////////////////////////////////////////////////////////////
//
//  FRAM9  PROTECTED
//

void FRAM9::write_block(uint16_t memaddr, uint8_t *obj, uint8_t size) {
  // Device uses Address Pages
  uint8_t device_addr_with_page_bits = this->address_ | ((memaddr & 0x0100) >> 8);

  i2c::WriteBuffer buff[2];
  uint8_t maddr = memaddr & 0xFF;

  buff[0].data = &maddr;
  buff[0].len = 1;
  buff[1].data = obj;
  buff[1].len = size;

  this->bus_->writev(device_addr_with_page_bits, buff, 2, true);
}

void FRAM9::read_block(uint16_t memaddr, uint8_t *obj, uint8_t size) {
  // Device uses Address Pages
  uint8_t device_addr_with_page_bits = this->address_ | ((memaddr & 0x0100) >> 8);
  uint8_t maddr = memaddr & 0xFF;

  this->bus_->write(device_addr_with_page_bits, &maddr, 1, false);
  this->bus_->read(device_addr_with_page_bits, obj, size);
}

}  // namespace fram
}  // namespace esphome

//  -- END OF FILE --
