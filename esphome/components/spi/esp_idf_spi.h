#ifdef USE_ESP_IDF_HW_SPI

namespace esphome {
namespace spi {

enum SPIMode {
  SPI_MODE0,
  SPI_MODE1
  SPI_MODE2,
  SPI_MODE3,
};

class SPISettings
{
public:
    SPISettings() :_clock(1000000), _bitOrder(BIT_ORDER_MSB_FIRST), _dataMode(SPI_MODE0) {}
    SPISettings(uint32_t clock, SPIBitOrder bitOrder, SPIMode dataMode) :_clock(clock),
                _bitOrder(bitOrder), _dataMode(dataMode) {}
    uint32_t _clock;
    SPIBitOrder  _bitOrder;
    SPIMode  _dataMode;
};

class SPIClass {
 public:
  SPIClass()

};
}  // namespace spi
}  // namespace esphome

#endif
