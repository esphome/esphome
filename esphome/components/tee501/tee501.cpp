#include "tee501.h"
#include "esphome/core/log.h"


namespace esphome {
namespace tee501 {

static const char *const TAG = "tee501";

void TEE501Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TEE501...");
  uint8_t address[] = {0x70,0x29};
  this->write(address,2,false);
  uint8_t identification[9];
  this->read(identification, 9);
  if(identification[8] != calcCrc8(identification, 0, 7)) {
    this->error_code_ = CRC_CHECK_FAILED;
    this->mark_failed();
    return;
  }
  uint32_t serialNumber1= (identification[0] << 24) + (identification[1] << 16) + (identification[2] << 8) + identification[3];
  uint32_t serialNumber2= (identification[4] << 24) + (identification[5] << 16) + (identification[6] << 8) + identification[7];
  ESP_LOGV(TAG, "    Serial Number: 0x%08X%08X", serialNumber1, serialNumber2);
}


void TEE501Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TEE501:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with TEE501 failed!");
      break;
    case CRC_CHECK_FAILED:
      ESP_LOGE(TAG, "The crc check failed");
      break;
    case NONE:
    default:
      break;
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "TEE501", this);
}


float TEE501Component::get_setup_priority() const { return setup_priority::DATA; }
void TEE501Component::update() {
  uint8_t address_1[] = {0x2C,0x1B};
  this->write(address_1, 2, true);
  this->set_timeout(50, [this]() {
  uint8_t i2cResponse[3];
  this->read(i2cResponse, 3);
  if(i2cResponse[2] != calcCrc8(i2cResponse, 0, 1)) {
    this->error_code_ = CRC_CHECK_FAILED;
    this->status_set_warning();
    return;
  }
  float temperature = ((float)(i2cResponse[0]) * 256 + i2cResponse[1]) / 100;
  ESP_LOGD(TAG, "Got temperature=%.2f°C", temperature);
  this->publish_state(temperature);
  this->status_clear_warning();
  });
}





unsigned char TEE501Component::calcCrc8 (unsigned char buf[], unsigned char from, unsigned char to)
{
  unsigned char crcVal = CRC8_ONEWIRE_START;
  unsigned char i = 0;
  unsigned char j = 0;
  for (i = from; i <= to; i++)
  {
    int curVal = buf[i];
    for (j = 0; j < 8; j++)
    {
      if (((crcVal ^ curVal) & 0x80) != 0)        //If MSBs are not equal
      {
        crcVal = ((crcVal << 1) ^ CRC8_ONEWIRE_POLY);
      }
      else {
        crcVal = (crcVal << 1);
      }
      curVal = curVal << 1;
    }
  }
  return crcVal;
}



}  // namespace tee501
}  // namespace esphome
