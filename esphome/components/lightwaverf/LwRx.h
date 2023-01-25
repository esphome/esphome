#pragma once


#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace lightwaverf {

// LwRx.h
//
// LightwaveRF 434MHz receiver for Arduino
// 
// Author: Bob Tidey (robert@tideys.net)
//Choose environment to compile for. Only one should be defined
//For SparkCore the SparkIntervalTimer Library code needs to be present
//For Due the DueTimer library code needs to be present
//#define SPARK_CORE 1
//#define DUE 1

#define ESP8266CPU 1
//Choose whether to include EEPROM support, comment or set to 0 to disable, 1 use with library support, 2 use with native support
#define EEPROM_EN 0

//Include EEPROM if required to include storing device paramters in EEPROM
#if EEPROM_EN == 1
#include <../EEPROM/EEPROM.h>
#endif
//define default EEPROMaddr to location to store message addr
#define EEPROM_ADDR_DEFAULT 0


#define rx_stat_high_ave 0
#define rx_stat_high_max 1
#define rx_stat_high_min 2
#define rx_stat_low0_ave 3
#define rx_stat_low0_max 4
#define rx_stat_low0_min 5
#define rx_stat_low1_ave 6
#define rx_stat_low1_max 7
#define rx_stat_low1_min 8
#define rx_stat_count 9

//sets maximum number of pairings which can be held
#define rx_maxpairs 10

class LwRx {

    public:
        //Seup must be called once, set up pin used to receive data
        void lwrx_setup(InternalGPIOPin *pin);

        //Set translate to determine whether translating from nibbles to bytes in message
        //Translate off only applies to 10char message returns
        void lwrx_settranslate(bool translate);

        // Check to see whether message available
        bool lwrx_message();

        //Get a message, len controls format (2 cmd+param, 4 cmd+param+room+device),10 full message
        bool lwrx_getmessage(uint8_t* buf, uint8_t len);

        //Setup repeat filter
        void lwrx_setfilter(uint8_t repeats, uint8_t timeout);

        //Add pair, if no pairing set then all messages are received, returns number of pairs
        uint8_t lwrx_addpair(uint8_t* pairdata);

        // Get pair data into buffer  for the pairnumber. Returns current paircount
        // Use pairnumber 255 to just get current paircount
        uint8_t lwrx_getpair(uint8_t* pairdata, uint8_t pairnumber);

        //Make a pair from next message received within timeout 100mSec
        //This call returns immediately whilst message checking continues
        void lwrx_makepair(uint8_t timeout);

        //Set pair mode controls
        void lwrx_setPairMode(bool pairEnforce, bool pairBaseOnly);

        //Returns time from last packet received in msec
        // Can be used to determine if Rx may be still receiving repeats
        unsigned long lwrx_packetinterval();

        static void rx_process_bits(LwRx *arg);

    protected:
        void lwrx_clearpairing();

        //Return stats on pulse timings
        bool lwrx_getstats(uint16_t* stats);

        //Enable collection of stats on pulse timings
        void lwrx_setstatsenable(bool rx_stats_enable);

        //Set base address for EEPROM storage
        void lwrx_setEEPROMaddr(int addr);

        //internal support functions
        bool rx_reportMessage();
        int16_t rx_findNibble(uint8_t data);	//int
        void rx_addpairfrommsg();
        void rx_paircommit();
        void rx_removePair(uint8_t *buf);
        int16_t rx_checkPairs(uint8_t *buf, bool allDevices);	//int
        void restoreEEPROMPairing();

        //InternalGPIOPin rx_pin;
        ISRInternalGPIOPin rx_pin_isr;
        InternalGPIOPin *rx_pin;


};

}
}