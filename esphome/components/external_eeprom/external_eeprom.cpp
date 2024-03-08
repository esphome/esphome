#include "external_eeprom.h"

namespace esphome {
namespace external_eeprom {

static const char *const TAG = "external_eeprom";

void ExtEepromComponent::setup() {
  if (!this->is_connected(this->address_)) {
    ESP_LOGE(TAG, "Device on address 0x%x not found!", this->address_);
    this->mark_failed();
  } else {
    ESP_LOGE(TAG, "Memory detected!");
  }
}

void ExtEepromComponent::loop() {}

void ExtEepromComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "External Eeprom");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "Device Type = %s", this->device_type_text_.c_str());
  ESP_LOGCONFIG(TAG, "Size = %d", this->get_memory_size_());
  ESP_LOGCONFIG(TAG, "Page size = %d", this->get_page_size_());
  ESP_LOGCONFIG(TAG, "Number of Address Bytes = %d", this->get_address_size_bytes_());
  ESP_LOGCONFIG(TAG, "I2C HW buffer size = %d", this->get_i2c_buffer_size());
  ESP_LOGCONFIG(TAG, "Page write time = %d", this->get_page_write_time_());
}
/// @brief This checks whether the device is connected and not busy
/// @param Caller can pass in an 0xFF I2C address. This is helpful for larger EEPROMs that have two addresses (see block
/// bit 2).
/// @return an boolean True for connected
bool ExtEepromComponent::is_connected(uint8_t i2c_address) {
  i2c::ErrorCode err;
  if (i2c_address == 255)  // We can't set the default so we use 255 instead
    i2c_address = this->address_;
  err = this->bus_->write(i2c_address, nullptr, 0, true);
  if (err != i2c::ERROR_OK)
    ESP_LOGE(TAG, "EEPROM not connected and raise this error %d", err);
  return (err == i2c::ERROR_OK);
}

/// @brief Reads a byte from a given location
/// @param memaddr is the location to read
/// @return the byte read from device
uint8_t ExtEepromComponent::read8(uint32_t memaddr) {
  uint8_t temp_byte;
  this->read(memaddr, &temp_byte, 1);
  return temp_byte;
}
/// @brief Reads a 16 bit word from a given location
/// @param memaddr is the location to read
/// @return the word read from the device
uint16_t ExtEepromComponent::read16(uint32_t memaddr) {
  uint16_t val;
  this->read(memaddr, (uint8_t *) &val, sizeof(uint16_t));
  return val;
}
/// @brief Reads a 32 bit word from a given location
/// @param memaddr is the location to read
/// @return the word read from the device
uint32_t ExtEepromComponent::read32(uint32_t memaddr) {
  uint32_t val;
  this->read(memaddr, (uint8_t *) &val, sizeof(uint32_t));
  return val;
}
/// @brief Reads a float from a given location
/// @param memaddr is the location to read
/// @return the float read from the device
float ExtEepromComponent::read_float(uint32_t memaddr) {
  float val;
  this->read(memaddr, (uint8_t *) &val, sizeof(float));
  return val;
}
/// @brief Reads a double from a given location
/// @param memaddr is the location to read
/// @return the double read from the device
double ExtEepromComponent::read_double(uint32_t memaddr) {
  double val;
  this->read(memaddr, (uint8_t *) &val, sizeof(double));
  return val;
}
/// @brief Bulk read from the device
/// @note breaking up read amt into 32 byte chunks (can be overriden with setI2Cbuffer_size)
/// @note Handles a read that straddles the 512kbit barrier
/// @param memaddr is the starting location to read
/// @param buff is the pointer to an array of bytes that will be used to store the data received
/// @param buffer_size is the size of the buffer and also the number of bytes to be read
void ExtEepromComponent::read(uint32_t memaddr, uint8_t *buff, uint16_t buffer_size) {
  ESP_LOGVV(TAG, "Read %d bytes from address %d", buffer_size, memaddr);
  uint16_t size = buffer_size;
  uint8_t *p = buff;
  i2c::ErrorCode ret;
  while (size >= 1) {
    // Limit the amount to read to a page size
    uint16_t amt_to_read = size;
    if (amt_to_read > this->i2c_buffer_size_)  // I2C buffer size limit
      amt_to_read = this->i2c_buffer_size_;

    // Check if we are dealing with large (>512kbit) EEPROMs
    uint8_t i2c_address = this->address_;
    if (this->get_memory_size_() > 0xFFFF) {
      // Figure out if we are going to cross the barrier with this read
      if (memaddr < 0xFFFF) {
        if (0xFFFF - memaddr < amt_to_read)  // 0xFFFF - 0xFFFA < I2C_buffer_size
          amt_to_read = 0xFFFF - memaddr;    // Limit the read amt to go right up to edge of barrier
      }

      // Figure out if we are accessing the lower half or the upper half
      if (memaddr > 0xFFFF)
        i2c_address |= 0b100;  // Set the block bit to 1
    }
    if (this->get_address_size_bytes_() == 2) {
      uint8_t maddr[] = {(uint8_t) (memaddr >> 8), (uint8_t) (memaddr & 0xFF)};
      ret = this->bus_->write(i2c_address, maddr, 2, false);
    } else {
      uint8_t maddr[] = {(uint8_t) (memaddr & 0xFF)};
      ret = this->bus_->write(i2c_address, maddr, 1, false);
    }
    if (ret != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Read raise this error %d on setting address", ret);
    }
    ESP_LOGVV(TAG, "Read - Done Set address, Ammount to read %d", amt_to_read);
    ret = this->bus_->read(i2c_address, p, amt_to_read);
    if (ret != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Read raised this error %d on reading data", ret);
    }
    ESP_LOGVV(TAG, "Done Read");
    memaddr += amt_to_read;
    p += amt_to_read;
    size -= amt_to_read;
  }
}
/// @brief Read a std::string from the device
/// @note It write the string with an extra byte containing the size at the memaddr
/// @note It is limited to reading a max length of 254 bytes
/// @param memaddr is the starting location to read
/// @param str_to_read will hold the bytes read from the device on return of the fuction

uint32_t ExtEepromComponent::read_string_from_eeprom(uint32_t memaddr, std::string &str_to_read) {
  uint8_t new_str_len = this->read8(memaddr);
  uint8_t data[new_str_len + 1];
  this->read(memaddr + 1, (uint8_t *) data, new_str_len);
  data[new_str_len] = '\0';
  str_to_read = (char *) data;
  return memaddr + 1 + new_str_len;
}
/// @brief Writes a byte to a given location
/// @note It will check first to see if loccation already has the value to protect write cycles
/// @param memaddr is the location to write
/// @param data_to_write contains the byte to be written
void ExtEepromComponent::write8(uint32_t memaddr, uint8_t data_to_write) {
  if (this->read8(memaddr) != data_to_write) {  // Update only if data is new
    this->write(memaddr, &data_to_write, 1);
  }
}
/// @brief Writes a 16 bit word to a given location
/// @note It will check first to see if loccation already has the value to protect write cycles
/// @param memaddr is the location to write
/// @param value contains the word to be written
void ExtEepromComponent::write16(uint32_t memaddr, uint16_t value) {
  if (this->read16(memaddr) != value) {  // Update only if data is new
    uint16_t val = value;
    this->write(memaddr, (uint8_t *) &val, sizeof(uint16_t));
  }
}
/// @brief Writes a 32 bit word to a given location
/// @note It will check first to see if loccation already has the value to protect write cycles
/// @param memaddr is the location to write
/// @param value contains the word to be written
void ExtEepromComponent::write32(uint32_t memaddr, uint32_t value) {
  if (this->read32(memaddr) != value) {  // Update only if data is new
    uint32_t val = value;
    this->write(memaddr, (uint8_t *) &val, sizeof(uint32_t));
  }
}
/// @brief Writes a float to a given location
/// @note It will check first to see if loccation already has the value to protect write cycles
/// @param memaddr is the location to write
/// @param value contains the float to be written
void ExtEepromComponent::write_float(uint32_t memaddr, float value) {
  if (this->read_float(memaddr) != value) {  // Update only if data is new
    float val = value;
    this->write(memaddr, (uint8_t *) &val, sizeof(float));
  }
}
/// @brief Writes a double to a given location
/// @note It will check first to see if loccation already has the value to protect write cycles
/// @param memaddr is the location to write
/// @param value contains the double to be written
void ExtEepromComponent::write_double(uint32_t memaddr, double value) {
  if (this->read_double(memaddr) != value)  // Update only if data is new
  {
    double val = value;
    this->write(memaddr, (uint8_t *) &val, sizeof(double));
  }
}
/// @brief Bulk write to the device
/// @note breaking up read amt into 32 byte chunks (can be overriden with setI2Cbuffer_size)
/// @note Handles a write that straddles the 512kbit barrier
/// @param memaddr is the starting location to write
/// @param data_to_write is the pointer to an array of bytes that will be written
/// @param buffer_size is the size of the buffer and also the number of bytes to be written
void ExtEepromComponent::write(uint32_t memaddr, uint8_t *data_to_write, uint16_t buffer_size) {
  ESP_LOGVV(TAG, "Write %d bytes to address %d", buffer_size, memaddr);
  uint16_t size = buffer_size;
  uint8_t *p = data_to_write;
  // Check to make sure write is inside device range
  if (memaddr + buffer_size >= this->memory_size_bytes_) {
    buffer_size = this->memory_size_bytes_ - memaddr;  // if not shorten the write to fit
    ESP_LOGE(TAG, "Trying write data beyond device size, Address %d", (memaddr + buffer_size));
  }

  uint16_t max_write_size = this->memory_page_size_bytes_;
  if (max_write_size > this->i2c_buffer_size_)
    max_write_size = this->i2c_buffer_size_;

  /// Break the buffer into page sized chunks

  while (size >= 1) {
    /// Limit the amount to write to either the page size or the I2C limit
    uint16_t amt_to_write = size;
    if (amt_to_write > max_write_size)
      amt_to_write = max_write_size;

    if (amt_to_write > 1) {
      /// Check for crossing of a page line. Writes cannot cross a page line.
      uint16_t page_number_1 = memaddr / this->memory_page_size_bytes_;
      uint16_t page_number_2 = (memaddr + amt_to_write - 1) / this->memory_page_size_bytes_;
      if (page_number_2 > page_number_1) {
        amt_to_write = (page_number_2 * this->memory_page_size_bytes_) -
                       memaddr;  /// Limit the write amt to go right up to edge of page barrier
      }
    }
    /// Check if we are dealing with large (>512kbit) EEPROMs
    uint8_t i2c_address = this->address_;
    if (this->get_memory_size_() > 0xFFFF) {
      /// Figure out if we are going to cross the barrier with this write
      if (memaddr < 0xFFFF) {
        if (0xFFFF - memaddr < amt_to_write)  /// 0xFFFF - 0xFFFA < I2C_buffer_size
          amt_to_write = 0xFFFF - memaddr;    /// Limit the write amt to go right up to edge of barrier
      }

      /// Figure out if we are accessing the lower half or the upper half
      if (memaddr > 0xFFFF)
        i2c_address |= 0b100;  /// Set the block bit to 1
    }

    ESP_LOGVV(TAG, "Write block %d bytes to address %d", amt_to_write, memaddr);
    this->write_block_(i2c_address, memaddr, p, amt_to_write);
    memaddr += amt_to_write;
    p += amt_to_write;
    size -= amt_to_write;
    ESP_LOGVV(TAG, "After write size %d amt  %d add %d", size, amt_to_write, memaddr);
    delay(this->memory_page_write_time_ms_);  /// Delay the amount of time to record a page
  }
}

/// @brief Write a std::string to the device
/// @note It writes the string with an extra byte containing the size at the memaddr eg address 0
/// @note it is limited to writing a max length of the string of 254 bytes and will trim extra bytes
/// @param memaddr is the starting location to write
/// @param str_to_write contains the std::string to be wriiten
uint32_t ExtEepromComponent::write_string_to_eeprom(uint32_t memaddr, std::string &str_to_write) {
  if (str_to_write.length() > 254) {
    ESP_LOGE(TAG, "String to long. Limit is 254 chars");
    str_to_write.resize(254);
  }
  uint8_t len = str_to_write.length();
  const char *p = str_to_write.c_str();
  this->write8(memaddr, len);
  this->write(memaddr + 1, (uint8_t *) p, len);
  return memaddr + 1 + len;
}
void ExtEepromComponent::dump_eeprom(uint32_t start_addr, uint16_t word_count) {
  std::vector<uint16_t> words;
  uint16_t address;
  uint16_t data;
  std::string res;
  char adbuf[8];
  char buf[5];
  size_t len;
  address = start_addr;
  while (address < (word_count + start_addr)) {
    for (size_t i = address; i < (address + 16); i += 2) {
      this->read_object(i, data);
      words.push_back(data);
    }
    sprintf(adbuf, "%04X : ", address);
    res = adbuf;
    len = words.size();
    for (size_t u = 0; u < len; u++) {
      if (u > 0) {
        res += " ";
      }
      sprintf(buf, "%04X", words[u]);
      res += buf;
    }
    ESP_LOGD(TAG, "%s", res.c_str());
    words.clear();
    address = address + 16;
  }
}

/// @brief Erase the entire device
/// @note **** to be used carefully, as there is no recovery ****
/// @param value_to_write optional value to be written to all locations defaults to 0x00
void ExtEepromComponent::erase(uint8_t value_to_write) {
  uint8_t temp_buffer[this->memory_page_size_bytes_];
  for (uint32_t x = 0; x < this->memory_page_size_bytes_; x++)
    temp_buffer[x] = value_to_write;

  for (uint32_t addr = 0; addr < this->get_memory_size_(); addr += this->memory_page_size_bytes_)
    this->write(addr, temp_buffer, this->memory_page_size_bytes_);
}
void ExtEepromComponent::set_memory_type(EEEDeviceType device_type) {
  device_type_ = device_type;
  // Set settings based on known memory types
  switch (device_type_) {
    default:
      // Unknown type number
      break;
    case EEE_24XX00:
      this->device_type_text_ = "24XX00";
      this->set_device_config_(16, 1, 1, 5);
      break;
    case EEE_24XX01:
      this->device_type_text_ = "24XX01";
      this->set_device_config_(128, 1, 8, 5);  // 128
      break;
    case EEE_24XX02:
      this->device_type_text_ = "24XX02";
      this->set_device_config_(256, 1, 8, 5);  // 256
      break;
    case EEE_24XX04:
      this->device_type_text_ = "24XX04";
      this->set_device_config_(512, 1, 16, 5);  // 512
      break;
    case EEE_24XX08:
      this->device_type_text_ = "24XX08";
      this->set_device_config_(1024, 1, 16, 5);  // 1024
      break;
    case EEE_24XX16:
      this->device_type_text_ = "24XX16";
      this->set_device_config_(2048, 1, 16, 1);  // 2048
      break;
    case EEE_24XX32:
      this->device_type_text_ = "24XX32";
      this->set_device_config_(4096, 2, 32, 5);  // 4096
      break;
    case EEE_24XX64:
      this->device_type_text_ = "24XX64";
      this->set_device_config_(8192, 2, 32, 5);  // 8192
      break;
    case EEE_24XX128:
      this->device_type_text_ = "24XX128";
      this->set_device_config_(16384, 2, 64, 5);  // 16384
      break;
    case EEE_24XX256:
      this->device_type_text_ = "24XX256";
      this->set_device_config_(32768, 2, 64, 5);  // 32768
      break;
    case EEE_24XX512:
      this->device_type_text_ = "24XX512";
      this->set_device_config_(65536, 2, 64, 5);  // 65536
      break;
    case EEE_24XX1025:
      this->device_type_text_ = "24XX1025";
      this->set_device_config_(128000, 2, 128, 5);  // 128000
      break;
    case EEE_24XX2048:
      this->device_type_text_ = "24XX2048";
      this->set_device_config_(262144, 2, 256, 5);  // 262144
      break;
  }
}
void ExtEepromComponent::set_device_config_(uint32_t mem_size, uint8_t address_bytes, uint16_t page_size,
                                            uint8_t write_time_ms) {
  this->set_memory_size_(mem_size);
  this->set_address_size_bytes_(address_bytes);
  this->set_page_size_(page_size);
  this->set_page_write_time_(write_time_ms);
}

/// @brief Sets the hw I2C buffer size -2, as 2 bytes are needed for control & addr
/// @param buffer size in bytes, (ESP devices has a 128 I2C buffer so it is set to 126)
void ExtEepromComponent::set_i2c_buffer_size(uint8_t i2c_buffer_size) { this->i2c_buffer_size_ = i2c_buffer_size - 2; }
/// @brief Gets the hw I2C buffer size -2, as 2 bytes are needed for control & addr
/// @return buffer size in bytes
uint8_t ExtEepromComponent::get_i2c_buffer_size() { return this->i2c_buffer_size_; }

// private functions
void ExtEepromComponent::write_block_(uint8_t deviceaddr, uint32_t memaddr, const uint8_t *obj, uint8_t size) {
  i2c::WriteBuffer buff[2];
  i2c::ErrorCode ret;
  // Check if the device has two address bytes
  if (this->get_address_size_bytes_() == 2) {
    uint8_t maddr[] = {(uint8_t) (memaddr >> 8), (uint8_t) (memaddr & 0xFF)};
    buff[0].data = maddr;
    buff[0].len = 2;
    buff[1].data = obj;
    buff[1].len = size;
    ret = this->bus_->writev(this->address_, buff, 2, true);
  } else {
    uint8_t maddr[] = {(uint8_t) (memaddr & 0xFF)};
    buff[0].data = maddr;
    buff[0].len = 1;
    buff[1].data = obj;
    buff[1].len = size;
    ret = this->bus_->writev(this->address_, buff, 2, true);
  }
  if (ret != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Write raise this error %d on writing data to this address %d", ret, memaddr);
  }
}
// @brief Sets the size of the device in bytes
/// @param memSize contains the size of the device
void ExtEepromComponent::set_memory_size_(uint32_t mem_size) { this->memory_size_bytes_ = mem_size; }
/// @brief Gets the user specified size of the device in bytes
/// @return size in bytes
uint32_t ExtEepromComponent::get_memory_size_() { return this->memory_size_bytes_; }
/// @brief Sets the page size of the device in bytes
/// @param page_size contains the size of the device pages
void ExtEepromComponent::set_page_size_(uint16_t page_size) { this->memory_page_size_bytes_ = page_size; }
/// @brief Gets the user specified size of the device pages in bytes
/// @return Page size in bytes
uint16_t ExtEepromComponent::get_page_size_() { return this->memory_page_size_bytes_; }
/// @brief Sets the page write for the device in ms
/// @param write_time_ms contains the time to write a page of the device
void ExtEepromComponent::set_page_write_time_(uint8_t write_time_ms) { this->memory_page_write_time_ms_ = write_time_ms; }
/// @brief Gets the user specified write time for a device page in ms
/// @return page write time in ms
uint8_t ExtEepromComponent::get_page_write_time_() { return this->memory_page_write_time_ms_; }
/// @brief Set address_bytes for the device
/// @param address_bytes contains the number of bytes the device uses for address
void ExtEepromComponent::set_address_size_bytes_(uint8_t address_size_bytes) {
  this->address_size_bytes_ = address_size_bytes;
}
/// @brief Gets the number of bytes used for the address
/// @return size in bytes
uint8_t ExtEepromComponent::get_address_size_bytes_() { return this->address_size_bytes_; }
}  // namespace external_eeprom
}  // namespace esphome
