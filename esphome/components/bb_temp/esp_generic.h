#ifndef __ESP_GENERIC__
#define __ESP_GENERIC__

#ifndef __BB_I2C__
#define __BB_I2C__
typedef struct _tagbbi2c {
  int file_i2c;
  uint8_t iSDA, iSCL;
  uint8_t bWire;
} BBI2C;
#endif  // __BB_I2C__

int I2CTest(BBI2C *pI2C, uint8_t addr);
void I2CInit(BBI2C *pI2C, unsigned int iClock);
int I2CWrite(BBI2C *pI2C, unsigned char iAddr, unsigned char *pData, int iLen);
int I2CRead(BBI2C *pI2C, unsigned char iAddr, unsigned char *pData, int iLen);
int I2CReadRegister(BBI2C *pI2C, unsigned char iAddr, unsigned char u8Register, unsigned char *pData, int iLen);

#endif  // __ESP_GENERIC__
