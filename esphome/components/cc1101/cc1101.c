/**
 * Copyright (c) 2011 panStamp <contact@panstamp.com>
 * Copyright (c) 2016 Tyler Sommer <contact@tylersommer.pro>
 * 
 * This file is part of the CC1101 project.
 * 
 * CC1101 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version.
 * 
 * CC1101 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with CC1101; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA	02110-1301 
 * USA
 * 
 * Author: Daniel Berenguer
 * Creation date: 03/03/2011
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "esp_log.h"

#include "cc1101.h"

#define TAG "CC1101"

// SPI Stuff
#if CONFIG_SPI2_HOST
#define HOST_ID SPI2_HOST
#elif CONFIG_SPI3_HOST
#define HOST_ID SPI3_HOST
#endif
static spi_device_handle_t _handle;

/*
 * RF state
 */
static uint8_t _rfState;

/**
 * Carrier frequency
 */
static uint8_t _carrierFreq;

/**
 * Working mode (speed, ...)
 */
static uint8_t _workMode;

/**
 * Frequency channel
 */
static uint8_t _channel;

/**
 * Synchronization word
 */
static uint8_t _syncWord[2];

/**
 * Device address
 */
static uint8_t _devAddress;

/**
 * Packet available
 */
static bool _packetAvailable;

/**
 * Power level
 */
static uint8_t _powerMin;
static uint8_t _power0db;
static uint8_t _powerMax;


/**
 * Macros
 */
// Select (SPI) CC1101
//#define cc1101_Select() digitalWrite(SS, LOW)
#define cc1101_Select() gpio_set_level(CONFIG_CSN_GPIO, LOW)
// Deselect (SPI) CC1101
//#define cc1101_Deselect() digitalWrite(SS, HIGH)
#define cc1101_Deselect() gpio_set_level(CONFIG_CSN_GPIO, HIGH)
// Wait until SPI MISO line goes low
//#define wait_Miso() while(digitalRead(MISO)>0)
#define wait_Miso() while(gpio_get_level(CONFIG_MISO_GPIO)>0)
//#define wait_Miso() (void)0
// Get GDO0 pin state
//#define getGDO0state() digitalRead(GDO0)
#define getGDO0state()gpio_get_level(CONFIG_GDO0_GPIO)
// Wait until GDO0 line goes high
#define wait_GDO0_high() while(!getGDO0state())
// Wait until GDO0 line goes low
#define wait_GDO0_low() while(getGDO0state())

/**
 * Arduino Macros
 */
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define delayMicroseconds(us) esp_rom_delay_us(us)
#define LOW  0
#define HIGH 1
#define byte uint8_t

/**
 * PATABLE
 */
//const byte paTable[8] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60};

void spi_init() {
	gpio_reset_pin(CONFIG_CSN_GPIO);
	gpio_set_direction(CONFIG_CSN_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(CONFIG_CSN_GPIO, 1);

	spi_bus_config_t buscfg = {
		.sclk_io_num = CONFIG_SCK_GPIO, // set SPI CLK pin
		.mosi_io_num = CONFIG_MOSI_GPIO, // set SPI MOSI pin
		.miso_io_num = CONFIG_MISO_GPIO, // set SPI MISO pin
		.quadwp_io_num = -1,
		.quadhd_io_num = -1
	};

	esp_err_t ret;
	ret = spi_bus_initialize( HOST_ID, &buscfg, SPI_DMA_CH_AUTO );
	ESP_LOGI(TAG, "spi_bus_initialize=%d",ret);
	assert(ret==ESP_OK);

	spi_device_interface_config_t devcfg = {
		.clock_speed_hz = 5000000, // SPI clock is 5 MHz!
		.queue_size = 7,
		.mode = 0, // SPI mode 0
		.spics_io_num = -1, // we will use manual CS control
		.flags = SPI_DEVICE_NO_DUMMY
	};

	ret = spi_bus_add_device( HOST_ID, &devcfg, &_handle);
	ESP_LOGI(TAG, "spi_bus_add_device=%d",ret);
	assert(ret==ESP_OK);
}

bool spi_write_byte(uint8_t* Dataout, size_t DataLength )
{
	spi_transaction_t SPITransaction;

	if ( DataLength > 0 ) {
		memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
		SPITransaction.length = DataLength * 8;
		SPITransaction.tx_buffer = Dataout;
		SPITransaction.rx_buffer = NULL;
		spi_device_transmit( _handle, &SPITransaction );
	}

	return true;
}

bool spi_read_byte(uint8_t* Datain, uint8_t* Dataout, size_t DataLength )
{
	spi_transaction_t SPITransaction;

	if ( DataLength > 0 ) {
		memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
		SPITransaction.length = DataLength * 8;
		SPITransaction.tx_buffer = Dataout;
		SPITransaction.rx_buffer = Datain;
		spi_device_transmit( _handle, &SPITransaction );
	}

	return true;
}

uint8_t spi_transfer(uint8_t address)
{
	uint8_t datain[1];
	uint8_t dataout[1];
	dataout[0] = address;
	//spi_write_byte(dev, dataout, 1 );
	//spi_read_byte(datain, dataout, 1 );

	spi_transaction_t SPITransaction;
	memset( &SPITransaction, 0, sizeof( spi_transaction_t ) );
	SPITransaction.length = 8;
	SPITransaction.tx_buffer = dataout;
	SPITransaction.rx_buffer = datain;
	spi_device_transmit( _handle, &SPITransaction );

	return datain[0];
}


/**
 * wakeUp
 * 
 * Wake up CC1101 from Power Down state
 */
void wakeUp(void)
{
	cc1101_Select();			// Select CC1101
	wait_Miso();				// Wait until MISO goes low
	cc1101_Deselect();			// Deselect CC1101
}

/**
 * writeReg
 * 
 * Write single register into the CC1101 IC via SPI
 * 
 * @param regAddr Register address
 * @param value Value to be writen
 */
void writeReg(byte regAddr, byte value) 
{
	cc1101_Select();			// Select CC1101
	wait_Miso();				// Wait until MISO goes low
	spi_transfer(regAddr);			// Send register address
	spi_transfer(value);			// Send value
	cc1101_Deselect();			// Deselect CC1101
}

/**
 * writeBurstReg
 * 
 * Write multiple registers into the CC1101 IC via SPI
 * 
 * @param regAddr	Register address
 * @param buffer Data to be writen
 * @param len Data length
 */
void writeBurstReg(byte regAddr, byte* buffer, byte len)
{
	byte addr, i;
	
	addr = regAddr | WRITE_BURST;		// Enable burst transfer
	cc1101_Select();			// Select CC1101
	wait_Miso();				// Wait until MISO goes low
	spi_transfer(addr);			// Send register address
	
	for(i=0 ; i<len ; i++)
		spi_transfer(buffer[i]);	// Send value

	cc1101_Deselect();			// Deselect CC1101	
}

/**
 * cmdStrobe
 * 
 * Send command strobe to the CC1101 IC via SPI
 * 
 * @param cmd Command strobe
 */			
void cmdStrobe(byte cmd) 
{
	cc1101_Select();			// Select CC1101
	wait_Miso();				// Wait until MISO goes low
	spi_transfer(cmd);			// Send strobe command
	cc1101_Deselect();			// Deselect CC1101
}

/**
 * readReg
 * 
 * Read CC1101 register via SPI
 * 
 * @param regAddr Register address
 * @param regType Type of register: CONFIG_REGISTER or STATUS_REGISTER
 * 
 * Return:
 *	Data byte returned by the CC1101 IC
 */
byte readReg(byte regAddr, byte regType)
{
	byte addr, val;

	addr = regAddr | regType;
	cc1101_Select();			// Select CC1101
	wait_Miso();				// Wait until MISO goes low
	spi_transfer(addr);			// Send register address
	val = spi_transfer(0x00);		// Read result
	cc1101_Deselect();			// Deselect CC1101

	return val;
}

/**
 * readBurstReg
 * 
 * Read burst data from CC1101 via SPI
 * 
 * @param buffer Buffer where to copy the result to
 * @param regAddr Register address
 * @param len Data length
 */
void readBurstReg(byte * buffer, byte regAddr, byte len) 
{
	byte addr, i;
	
	addr = regAddr | READ_BURST;
	cc1101_Select();				// Select CC1101
	wait_Miso();					// Wait until MISO goes low
	spi_transfer(addr);				// Send register address
	for(i=0 ; i<len ; i++)
		buffer[i] = spi_transfer(0x00);		// Read result byte by byte
	cc1101_Deselect();				// Deselect CC1101
}

/**
 * reset
 * 
 * Reset CC1101
 */
void reset(void) 
{
	cc1101_Deselect();				// Deselect CC1101
	delayMicroseconds(5);
	cc1101_Select();				// Select CC1101
	delayMicroseconds(10);
	cc1101_Deselect();				// Deselect CC1101
	delayMicroseconds(41);
	cc1101_Select();				// Select CC1101

	wait_Miso();					// Wait until MISO goes low
	spi_transfer(CC1101_SRES);			// Send reset command strobe
	wait_Miso();					// Wait until MISO goes low

	cc1101_Deselect();				// Deselect CC1101

	setCCregs();					// Reconfigure CC1101
}

/**
 * setCCregs
 * 
 * Configure CC1101 registers
 */
void setCCregs(void) 
{
	writeReg(CC1101_IOCFG2, CC1101_DEFVAL_IOCFG2);
	writeReg(CC1101_IOCFG1, CC1101_DEFVAL_IOCFG1);
	writeReg(CC1101_IOCFG0, CC1101_DEFVAL_IOCFG0);
	writeReg(CC1101_FIFOTHR, CC1101_DEFVAL_FIFOTHR);
	writeReg(CC1101_PKTLEN, CC1101_DEFVAL_PKTLEN);
	writeReg(CC1101_PKTCTRL1, CC1101_DEFVAL_PKTCTRL1);
	writeReg(CC1101_PKTCTRL0, CC1101_DEFVAL_PKTCTRL0);

	// Set default synchronization word
	setSyncWordArray(_syncWord);

	// Set default device address
	setDevAddress(_devAddress);

	// Set default frequency channel
	setChannel(_channel);
	
	writeReg(CC1101_FSCTRL1, CC1101_DEFVAL_FSCTRL1);
	writeReg(CC1101_FSCTRL0, CC1101_DEFVAL_FSCTRL0);

	// Set default carrier frequency = 868 MHz
	setCarrierFreq(_carrierFreq);

	// RF speed
	switch(_workMode)
	{
		case CSPEED_4800:
			writeReg(CC1101_MDMCFG4, CC1101_DEFVAL_MDMCFG4_4800);
			break;
		case CSPEED_9600:
			writeReg(CC1101_MDMCFG4, CC1101_DEFVAL_MDMCFG4_9600);
			break;
		case CSPEED_19200:
			writeReg(CC1101_MDMCFG4, CC1101_DEFVAL_MDMCFG4_19200);
			break;
		case CSPEED_38400:
			writeReg(CC1101_MDMCFG4, CC1101_DEFVAL_MDMCFG4_38400);
			break;
	}

		
	writeReg(CC1101_MDMCFG3, CC1101_DEFVAL_MDMCFG3);
	writeReg(CC1101_MDMCFG2, CC1101_DEFVAL_MDMCFG2);
	writeReg(CC1101_MDMCFG1, CC1101_DEFVAL_MDMCFG1);
	writeReg(CC1101_MDMCFG0, CC1101_DEFVAL_MDMCFG0);
	writeReg(CC1101_DEVIATN, CC1101_DEFVAL_DEVIATN);
	writeReg(CC1101_MCSM2, CC1101_DEFVAL_MCSM2);
	writeReg(CC1101_MCSM1, CC1101_DEFVAL_MCSM1);
	writeReg(CC1101_MCSM0, CC1101_DEFVAL_MCSM0);
	writeReg(CC1101_FOCCFG, CC1101_DEFVAL_FOCCFG);
	writeReg(CC1101_BSCFG, CC1101_DEFVAL_BSCFG);
	writeReg(CC1101_AGCCTRL2, CC1101_DEFVAL_AGCCTRL2);
	writeReg(CC1101_AGCCTRL1, CC1101_DEFVAL_AGCCTRL1);
	writeReg(CC1101_AGCCTRL0, CC1101_DEFVAL_AGCCTRL0);
	writeReg(CC1101_WOREVT1, CC1101_DEFVAL_WOREVT1);
	writeReg(CC1101_WOREVT0, CC1101_DEFVAL_WOREVT0);
	writeReg(CC1101_WORCTRL, CC1101_DEFVAL_WORCTRL);
	writeReg(CC1101_FREND1, CC1101_DEFVAL_FREND1);
	writeReg(CC1101_FREND0, CC1101_DEFVAL_FREND0);
	writeReg(CC1101_FSCAL3, CC1101_DEFVAL_FSCAL3);
	writeReg(CC1101_FSCAL2, CC1101_DEFVAL_FSCAL2);
	writeReg(CC1101_FSCAL1, CC1101_DEFVAL_FSCAL1);
	writeReg(CC1101_FSCAL0, CC1101_DEFVAL_FSCAL0);
	writeReg(CC1101_RCCTRL1, CC1101_DEFVAL_RCCTRL1);
	writeReg(CC1101_RCCTRL0, CC1101_DEFVAL_RCCTRL0);
	writeReg(CC1101_FSTEST, CC1101_DEFVAL_FSTEST);
	writeReg(CC1101_PTEST, CC1101_DEFVAL_PTEST);
	writeReg(CC1101_AGCTEST, CC1101_DEFVAL_AGCTEST);
	writeReg(CC1101_TEST2, CC1101_DEFVAL_TEST2);
	writeReg(CC1101_TEST1, CC1101_DEFVAL_TEST1);
	writeReg(CC1101_TEST0, CC1101_DEFVAL_TEST0);
	
	// Send empty packet
	CCPACKET packet;
	packet.length = 0;
	sendData(packet);
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
	_packetAvailable = true;
}

/**
 * init
 * 
 * Initialize CC1101 radio
 *
 * @param freq Carrier frequency
 * @param mode Working mode (speed, ...)
 */
esp_err_t init(uint8_t freq, uint8_t mode)
{
	_carrierFreq = freq; // Frequency
	_workMode = mode; // Transfer Speed
	//_carrierFreq = CFREQ_868;
	_channel = CC1101_DEFVAL_CHANNR; // 0x00
	_syncWord[0] = CC1101_DEFVAL_SYNC1; // 0xB5
	_syncWord[1] = CC1101_DEFVAL_SYNC0; // 0x47
	_devAddress = CC1101_DEFVAL_ADDR; // 0xFF
	_packetAvailable = false;

	if (freq == CFREQ_315) {
		_powerMin = PA_MinPower_315;
		_power0db = PA_0dbPower_315;
		_powerMax = PA_MaxPower_315;
	} else if (freq == CFREQ_433) {
		_powerMin = PA_MinPower_433;
		_power0db = PA_0dbPower_433;
		_powerMax = PA_MaxPower_433;
	} else if (freq == CFREQ_868) {
		_powerMin = PA_MinPower_868;
		_power0db = PA_0dbPower_868;
		_powerMax = PA_MaxPower_868;
	} else if (freq == CFREQ_915) {
		_powerMin = PA_MinPower_915;
		_power0db = PA_0dbPower_915;
		_powerMax = PA_MaxPower_915;
	} else {
		ESP_LOGE(TAG, "Illegal Freqiency");
		vTaskDelete(NULL);
	}

	// Initialize SPI
	spi_init();

	//interrupt setting
	gpio_config_t io_conf;
	//interrupt of falling edge
	io_conf.intr_type = GPIO_INTR_NEGEDGE; // GPIO interrupt type : falling edge
	//bit mask of the pins
	io_conf.pin_bit_mask = 1ULL<<CONFIG_GDO0_GPIO;
	//set as input mode
	io_conf.mode = GPIO_MODE_INPUT;
	//enable pull-up mode
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;
	gpio_config(&io_conf);
	//install gpio isr service
	gpio_install_isr_service(0);
	//hook isr handler for specific gpio pin
	gpio_isr_handler_add(CONFIG_GDO0_GPIO, gpio_isr_handler, (void*) CONFIG_GDO0_GPIO);

	// Reset CC1101
	reset();

	// Read PATABLE
	uint8_t ptable[8];
	readBurstReg(ptable, CC1101_PATABLE, 8);
	ESP_LOG_BUFFER_HEXDUMP(TAG, ptable, 8, ESP_LOG_INFO);
#if 0
	setTxPowerAmp(PA_LowPower);
#endif

	// Check Chip ID
	uint8_t CHIP_PARTNUM = readReg(CC1101_PARTNUM, CC1101_STATUS_REGISTER);
	uint8_t CHIP_VERSION = readReg(CC1101_VERSION, CC1101_STATUS_REGISTER);
	ESP_LOGI(TAG, "CC1101_PARTNUM %d", CHIP_PARTNUM);
	ESP_LOGI(TAG, "CC1101_VERSION %d", CHIP_VERSION);
	if (CHIP_PARTNUM != 0 || CHIP_VERSION != 20) {
		ESP_LOGE(TAG, "CC1101 not installed");
		return ESP_FAIL;
	}
	return ESP_OK;
}

/**
 * setSyncWord
 * 
 * Set synchronization word
 * 
 * @param syncH	Synchronization word - High byte
 * @param syncL	Synchronization word - Low byte
 */
void setSyncWord(uint8_t syncH, uint8_t syncL) 
{
	writeReg(CC1101_SYNC1, syncH);
	writeReg(CC1101_SYNC0, syncL);
	_syncWord[0] = syncH;
	_syncWord[1] = syncL;
}

/**
 * setSyncWordArray (overriding method)
 * 
 * Set synchronization word
 * 
 * @param syncH	Synchronization word - pointer to 2-byte array
 */
void setSyncWordArray(byte *sync) 
{
	setSyncWord(sync[0], sync[1]);
}

/**
 * setDevAddress
 * 
 * Set device address
 * 
 * @param addr Device address
 */
void setDevAddress(byte addr) 
{
	writeReg(CC1101_ADDR, addr);
	_devAddress = addr;
}

/**
 * setChannel
 * 
 * Set frequency channel
 * 
 * @param chnl Frequency channel
 */
void setChannel(byte chnl) 
{
	writeReg(CC1101_CHANNR, chnl);
	_channel = chnl;
}

/**
 * setCarrierFreq
 * 
 * Set carrier frequency
 * 
 * @param freq New carrier frequency
 */
void setCarrierFreq(byte freq)
{
	switch(freq)
	{
		case CFREQ_315:
			writeReg(CC1101_FREQ2, CC1101_DEFVAL_FREQ2_315);
			writeReg(CC1101_FREQ1, CC1101_DEFVAL_FREQ1_315);
			writeReg(CC1101_FREQ0, CC1101_DEFVAL_FREQ0_315);
			break;
		case CFREQ_433:
			writeReg(CC1101_FREQ2, CC1101_DEFVAL_FREQ2_433);
			writeReg(CC1101_FREQ1, CC1101_DEFVAL_FREQ1_433);
			writeReg(CC1101_FREQ0, CC1101_DEFVAL_FREQ0_433);
			break;
		case CFREQ_868:
			writeReg(CC1101_FREQ2, CC1101_DEFVAL_FREQ2_868);
			writeReg(CC1101_FREQ1, CC1101_DEFVAL_FREQ1_868);
			writeReg(CC1101_FREQ0, CC1101_DEFVAL_FREQ0_868);
			break;
		case CFREQ_915:
			writeReg(CC1101_FREQ2, CC1101_DEFVAL_FREQ2_915);
			writeReg(CC1101_FREQ1, CC1101_DEFVAL_FREQ1_915);
			writeReg(CC1101_FREQ0, CC1101_DEFVAL_FREQ0_915);
			break;
#if 0
		case CFREQ_918:
			writeReg(CC1101_FREQ2, CC1101_DEFVAL_FREQ2_918);
			writeReg(CC1101_FREQ1, CC1101_DEFVAL_FREQ1_918);
			writeReg(CC1101_FREQ0, CC1101_DEFVAL_FREQ0_918);
			break;
#endif
	}
	 
	_carrierFreq = freq;
}

/**
 * setPowerDownState
 * 
 * Put CC1101 into power-down state
 */
void setPowerDownState() 
{
	// Comming from RX state, we need to enter the IDLE state first
	cmdStrobe(CC1101_SIDLE);
	// Enter Power-down state
	cmdStrobe(CC1101_SPWD);
}

/**
 * sendData
 * 
 * Send data packet via RF
 * 
 * @param packet Packet to be transmitted. First byte is the destination address
 *
 * Return:
 *	True if the transmission succeeds
 *	False otherwise
 */
bool sendData(CCPACKET packet)
{
	byte marcState;
	bool res = false;
 
	// Declare to be in Tx state. This will avoid receiving packets whilst
	// transmitting
	_rfState = RFSTATE_TX;

	// Enter RX state
	setRxState();

	int tries = 0;
	// Check that the RX state has been entered
	while (tries++ < 1000 && ((marcState = readStatusReg(CC1101_MARCSTATE)) & 0x1F) != 0x0D)
	{
		if (marcState == 0x11)		// RX_OVERFLOW
			flushRxFifo();		// flush receive queue
	}
	if (tries >= 1000) {
		// TODO: MarcState sometimes never enters the expected state; this is a hack workaround.
		return false;
	}

	delayMicroseconds(500);

	if (packet.length > 0)
	{
		// Set data length at the first position of the TX FIFO
		writeReg(CC1101_TXFIFO,  packet.length);
		// Write data into the TX FIFO
		writeBurstReg(CC1101_TXFIFO, packet.data, packet.length);

		// CCA enabled: will enter TX state only if the channel is clear
		setTxState();
	}

	// Check that TX state is being entered (state = RXTX_SETTLING)
	marcState = readStatusReg(CC1101_MARCSTATE) & 0x1F;
	if((marcState != 0x13) && (marcState != 0x14) && (marcState != 0x15))
	{
		setIdleState();		// Enter IDLE state
		flushTxFifo();		// Flush Tx FIFO
		setRxState();		// Back to RX state

		// Declare to be in Rx state
		_rfState = RFSTATE_RX;
		return false;
	}

	// Wait for the sync word to be transmitted
	ESP_LOGD(TAG, "b wait_GDO0_high");
	wait_GDO0_high();

	// Wait until the end of the packet transmission
	ESP_LOGD(TAG, "b wait_GDO0_low");
	wait_GDO0_low();

	// Check that the TX FIFO is empty
	ESP_LOGD(TAG, "b readStatusReg");
	if((readStatusReg(CC1101_TXBYTES) & 0x7F) == 0)
		res = true;

	setIdleState();		// Enter IDLE state
	flushTxFifo();		// Flush Tx FIFO

	// Enter back into RX state
	setRxState();

	// Declare to be in Rx state
	_rfState = RFSTATE_RX;

	return res;
}

/**
 * receiveData
 * 
 * Read data packet from RX FIFO
 *
 * @param packet Container for the packet received
 * 
 * Return:
 *	Amount of bytes received
 */
byte receiveData(CCPACKET * packet)
{
	byte val;
	byte rxBytes = readStatusReg(CC1101_RXBYTES);

	// Any byte waiting to be read and no overflow?
	if (rxBytes & 0x7F && !(rxBytes & 0x80))
	{
		// Read data length
		packet->length = readConfigReg(CC1101_RXFIFO);
		// If packet is too long
		if (packet->length > CCPACKET_DATA_LEN)
			packet->length = 0;		// Discard packet
		else
		{
			// Read data packet
			readBurstReg(packet->data, CC1101_RXFIFO, packet->length);
			// Read RSSI
			packet->rssi = readConfigReg(CC1101_RXFIFO);
			// Read LQI and CRC_OK
			val = readConfigReg(CC1101_RXFIFO);
			packet->lqi = val & 0x7F;
			packet->crc_ok = bitRead(val, 7);
		}
	}
	else
		packet->length = 0;

	setIdleState();				// Enter IDLE state
	flushRxFifo();				// Flush Rx FIFO
	//cmdStrobe(SCAL);

	// Back to RX state
	setRxState();

	return packet->length;
}

/**
 * setRxState
 * 
 * Enter Rx state
 */
void setRxState(void)
{
	cmdStrobe(CC1101_SRX);
	_rfState = RFSTATE_RX;
}

/**
 * setTxState
 * 
 * Enter Tx state
 */
void setTxState(void)
{
	cmdStrobe(CC1101_STX);
	_rfState = RFSTATE_TX;
}

/**
 * setTxPowerAmp
 *
 * Set PATABLE value
 *
 * @param paLevel amplification value
 */
void setTxPowerAmp(uint8_t paLevel)
{
#if 0
	writeReg(CC1101_PATABLE, paLevel);
#endif
	ESP_LOGI(TAG, "setTxPowerAmp paLevel=%d", paLevel);
	if (paLevel == POWER_MIN) {
		ESP_LOGI(TAG, "setTxPowerAmp _powerMin=0x%x", _powerMin);
		writeReg(CC1101_PATABLE, _powerMin);
	} else if (paLevel == POWER_0db) {
		ESP_LOGI(TAG, "setTxPowerAmp _power0db=0x%x", _power0db);
		writeReg(CC1101_PATABLE, _power0db);
	} else if (paLevel == POWER_MAX) {
		ESP_LOGI(TAG, "setTxPowerAmp _powerMax=0x%x", _powerMax);
		writeReg(CC1101_PATABLE, _powerMax);
	}
	uint8_t ptable[8];
	readBurstReg(ptable, CC1101_PATABLE, 8);
	ESP_LOG_BUFFER_HEXDUMP(TAG, ptable, 8, ESP_LOG_INFO);
}

/**
 * packet_available
 *
 * Check if Packet is received
 */
uint8_t packet_available()
{
	if (_packetAvailable) {
		ESP_LOGD(TAG, "Packet available");
		_packetAvailable = 0;
		return 1;
	}
	return 0;
}

