#include "hp303b_i2c.h"

namespace esphome {
namespace hp303b_spi {
static const char *TAG = "hp303b_i2c";

void HP303BComponentI2C::setup() { HP303BComponent::setup(); };

void HP303BComponentI2C::dump_config() {
  HP303BComponent::dump_config();
  LOG_I2C_DEVICE(this);
};

int16_t HP303BComponentSPI::read_byte(uint8_t reg_address) {
  /*
  m_i2cbus->beginTransmission(m_slaveAddress);
    m_i2cbus->write(regAddress);
      m_i2cbus->endTransmission(0);
    //request 1 byte from slave
    if(m_i2cbus->requestFrom(m_slaveAddress, 1U, 1U) > 0)
    {
      return m_i2cbus->read();	//return this byte on success
    }
    else
    {
      return HP303B__FAIL_UNKNOWN;	//if 0 bytes were read successfully
    }
  */
}
int16_t HP303BComponentSPI::read_block(uint8_t reg_address, uint8_t length, uint8_t *buffer) {
  /*
  //do not read if there is no buffer
if(buffer == NULL)
{
  return 0;	//0 bytes read successfully
}

m_i2cbus->beginTransmission(m_slaveAddress);
m_i2cbus->write(regAddress);
  m_i2cbus->endTransmission(0);
//request length bytes from slave
int16_t ret = m_i2cbus->requestFrom(m_slaveAddress, length, 1U);
//read all received bytes to buffer
for(int16_t count = 0; count < ret; count++)
{
  buffer[count] = m_i2cbus->read();
}
return ret;
  */
}
int16_t HP303BComponentSPI::write_byte(uint8_t reg_address, uint8_t data, uint8_t check) {
  /*
  m_i2cbus->beginTransmission(m_slaveAddress);
    m_i2cbus->write(regAddress);			//Write Register number to buffer
    m_i2cbus->write(data);					//Write data to buffer
    if(m_i2cbus->endTransmission() != 0) 	//Send buffer content to slave
    {
      return HP303B__FAIL_UNKNOWN;
    }
    else
    {
      if(check == 0) return 0;			//no checking
      if(readByte(regAddress) == data)	//check if desired by calling function
      {
        return HP303B__SUCCEEDED;
      }
      else
      {
        return HP303B__FAIL_UNKNOWN;
      }
    }
      */
}
int16_t HP303BComponentSPI::set_interrupt_polarity(uint8_t polarity) {
  return HP303BComponent::write_byte_bitfield(polarity, HP303B__REG_INFO_INT_HL);
}

}  // namespace hp303b_spi
}  // namespace esphome