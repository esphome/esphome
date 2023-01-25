// LwRx.cpp
//
// LightwaveRF 434MHz receiver interface for Arduino
// 
// Author: Bob Tidey (robert@tideys.net)

#include "LwRx.h"
#include <cstring>


namespace esphome {
namespace lightwaverf {

static const uint8_t rx_nibble[] = {0xF6,0xEE,0xED,0xEB,0xDE,0xDD,0xDB,0xBE,0xBD,0xBB,0xB7,0x7E,0x7D,0x7B,0x77,0x6F};
static const uint8_t rx_cmd_off     = 0xF6; // raw 0
static const uint8_t rx_cmd_on      = 0xEE; // raw 1
static const uint8_t rx_cmd_mood    = 0xED; // raw 2
static const uint8_t rx_par0_alloff = 0x7D; // param 192-255 all off (12 in msb)
static const uint8_t rx_dev_15      = 0x6F; // device 15

static int EEPROMaddr = EEPROM_ADDR_DEFAULT;
static const uint8_t rx_msglen = 10; // expected length of rx message

//Receive mode constants and variables
static uint8_t rx_msg[rx_msglen]; // raw message received
static uint8_t rx_buf[rx_msglen]; // message buffer during reception

unsigned long rx_prev; // time of previous interrupt in microseconds

static bool rx_msgcomplete = false; //set high when message available
static bool rx_translate = true; // Set false to get raw data

static uint8_t rx_state = 0;
static const uint8_t rx_state_idle = 0;
static const uint8_t rx_state_msgstartfound = 1;
static const uint8_t rx_state_bytestartfound = 2;
static const uint8_t rx_state_getbyte = 3;

static uint8_t rx_num_bits = 0; // number of bits in the current uint8_t
static uint8_t rx_num_bytes = 0; // number of bytes received 

//Pairing data
static uint8_t rx_paircount = 0;
static uint8_t rx_pairs[rx_maxpairs][8];
static uint8_t rx_pairtimeout = 0; // 100msec units
//set false to responds to all messages if no pairs set up
static bool rx_pairEnforce = false;
//set false to use Address, Room and Device in pairs, true just the Address part
static bool rx_pairBaseOnly = false;

// Repeat filters
static uint8_t rx_repeats = 2; //msg must be repeated at least this number of times
static uint8_t rx_repeatcount = 0;
static uint8_t rx_timeout = 20; //reset repeat window after this in 100mSecs
static unsigned long rx_prevpkttime = 0; //last packet time in milliseconds
static unsigned long rx_pairstarttime = 0; //last msg time in milliseconds

// Gather stats for pulse widths (ave is x 16)
static const uint16_t lwrx_statsdflt[rx_stat_count] = {5000,0,5000,20000,0,2500,4000,0,500};	//usigned int
static uint16_t lwrx_stats[rx_stat_count];	//unsigned int
static bool lwrx_stats_enable = true;

/**
  Pin change interrupt routine that identifies 1 and 0 LightwaveRF bits
  and constructs a message when a valid packet of data is received.
**/


void IRAM_ATTR LwRx::rx_process_bits(LwRx *args) {
   uint8_t event =  args->rx_pin_isr.digital_read(); // start setting event to the current value
   unsigned long curr = micros(); // the current time in microseconds

   uint16_t dur = (curr-rx_prev);	//unsigned int
   rx_prev = curr;
   //set event based on input and duration of previous pulse
   if(dur < 120) { //120 very short
   } else if(dur < 500) { // normal short pulse
      event +=2;
   } else if(dur < 2000) { // normal long pulse
      event +=4;
   } else if(dur > 5000){ // gap between messages
      event +=6;
   } else { //2000 > 5000
      event = 8; //illegal gap
   } 
   //state machine transitions 
   switch(rx_state) {
      case rx_state_idle:
         switch(event) {
           case 7: //1 after a message gap
             rx_state = rx_state_msgstartfound;
             break;
         }
         break;
      case rx_state_msgstartfound:
         switch(event) {
            case 2: //0 160->500
             //nothing to do wait for next positive edge
               break;
            case 3: //1 160->500
               rx_num_bytes = 0;
               rx_state = rx_state_bytestartfound;
               break;
            default:
               //not good start again
               rx_state = rx_state_idle;
               break;
         }
         break;
      case rx_state_bytestartfound:
         switch(event) {
            case 2: //0 160->500
               //nothing to do wait for next positive edge
               break;
            case 3: //1 160->500
               rx_state = rx_state_getbyte;
               rx_num_bits = 0;
               break;
            case 5: //0 500->1500
               rx_state = rx_state_getbyte;
               // Starts with 0 so put this into byte
               rx_num_bits = 1;
               rx_buf[rx_num_bytes] = 0;
               break;
            default: 
               //not good start again
               rx_state = rx_state_idle;
               break;
         }
         break;
      case rx_state_getbyte:
         switch(event) {
            case 2: //0 160->500
               //nothing to do wait for next positive edge but do stats
               if(lwrx_stats_enable) {
                  lwrx_stats[rx_stat_high_max] = std::max(lwrx_stats[rx_stat_high_max], dur);
                  lwrx_stats[rx_stat_high_min] = std::min(lwrx_stats[rx_stat_high_min], dur);
                  lwrx_stats[rx_stat_high_ave] = lwrx_stats[rx_stat_high_ave] - (lwrx_stats[rx_stat_high_ave] >> 4) + dur;
               }
               break;
            case 3: //1 160->500
               // a single 1
               rx_buf[rx_num_bytes] = rx_buf[rx_num_bytes] << 1 | 1;
               rx_num_bits++;
               if(lwrx_stats_enable) {
                  lwrx_stats[rx_stat_low1_max] = std::max(lwrx_stats[rx_stat_low1_max], dur);
                  lwrx_stats[rx_stat_low1_min] = std::min(lwrx_stats[rx_stat_low1_min], dur);
                  lwrx_stats[rx_stat_low1_ave] = lwrx_stats[rx_stat_low1_ave] - (lwrx_stats[rx_stat_low1_ave] >> 4) + dur;
               }
               break;
            case 5: //1 500->1500
               // a 1 followed by a 0
               rx_buf[rx_num_bytes] = rx_buf[rx_num_bytes] << 2 | 2;
               rx_num_bits++;
               rx_num_bits++;
               if(lwrx_stats_enable) {
                  lwrx_stats[rx_stat_low0_max] = std::max(lwrx_stats[rx_stat_low0_max], dur);
                  lwrx_stats[rx_stat_low0_min] = std::min(lwrx_stats[rx_stat_low0_min], dur);
                  lwrx_stats[rx_stat_low0_ave] = lwrx_stats[rx_stat_low0_ave] - (lwrx_stats[rx_stat_low0_ave] >> 4) + dur;
               }
               break;
            default:
               //not good start again
               rx_state = rx_state_idle;
               break;
         }
         if(rx_num_bits >= 8) {
            rx_num_bytes++;
            rx_num_bits = 0;
            if(rx_num_bytes >= rx_msglen) {
               unsigned long currMillis = millis();
               if(rx_repeats > 0) {
                  if((currMillis - rx_prevpkttime) / 100 > rx_timeout) { 
                     rx_repeatcount = 1;
                  } else {
                     //Test message same as last one
                     int16_t i = rx_msglen;	//int
                     do {
                        i--;
                     }
                     while((i >= 0) && (rx_msg[i] == rx_buf[i]));
                     if(i < 0) {
                        rx_repeatcount++;
                     } else {
                        rx_repeatcount = 1;
                     }
                  }
               } else {
                  rx_repeatcount = 0;
               }
               rx_prevpkttime = currMillis;
               //If last message hasn't been read it gets overwritten
               memcpy(rx_msg, rx_buf, rx_msglen); 
               if(rx_repeats == 0 || rx_repeatcount == rx_repeats) {
                  if(rx_pairtimeout != 0) {
                     if((currMillis - rx_pairstarttime) / 100 <= rx_pairtimeout) {
                        if(rx_msg[3] == rx_cmd_on) {
                           args->rx_addpairfrommsg();
                        } else if(rx_msg[3] == rx_cmd_off) {
                           args->rx_removePair(&rx_msg[2]);
                        }
                     }
                  }
                  if(args->rx_reportMessage()) {
                     rx_msgcomplete = true;
                  }
                  rx_pairtimeout = 0;
               }
               // And cycle round for next one
               rx_state = rx_state_idle;
            } else {
              rx_state = rx_state_bytestartfound;
            }
         }
         break;
   }
}

/**
  Test if a message has arrived
**/
bool LwRx::lwrx_message() {
   return (rx_msgcomplete);
}

/**
  Set translate mode
**/
void LwRx::lwrx_settranslate(bool rxtranslate) {
   rx_translate = rxtranslate;
}
/**
  Transfer a message to user buffer
**/
bool LwRx::lwrx_getmessage(uint8_t *buf, uint8_t len) {
   bool ret = true;
   int16_t j=0,k=0;		//int
   if(rx_msgcomplete && len <= rx_msglen) {
      for(uint8_t i=0; ret && i < rx_msglen; i++) {
         if(rx_translate || (len != rx_msglen)) {
            j = this->rx_findNibble(rx_msg[i]);
            if(j<0) ret = false;
         } else {
            j = rx_msg[i];
         }
         switch(len) {
            case 4:
               if(i==9) buf[2]=j;
               if(i==2) buf[3]=j;
            case 2:
               if(i==3) buf[0]=j;
               if(i==0) buf[1]=j<<4;
               if(i==1) buf[1]+=j;
               break;
            case 10:
               buf[i]=j;
               break;
         }
      }
      rx_msgcomplete= false; 
   } else {
      ret = false;
   }
   return ret;
}

/**
  Return time in milliseconds since last packet received
**/
unsigned long LwRx::lwrx_packetinterval() {
   return millis() - rx_prevpkttime;
}

/**
  Set up repeat filtering of received messages
**/
void LwRx::lwrx_setfilter(uint8_t repeats, uint8_t timeout) {
   rx_repeats = repeats;
   rx_timeout = timeout;
}

/**
  Add a pair to filter received messages
  pairdata is device,dummy,5*addr,room
  pairdata is held in translated form to make comparisons quicker
**/
uint8_t LwRx::lwrx_addpair(uint8_t* pairdata) {
   if(rx_paircount < rx_maxpairs) {
      for(uint8_t i=0; i<8; i++) {
         rx_pairs[rx_paircount][i] = rx_nibble[pairdata[i]];
      }
      this->rx_paircommit();
   }
   return rx_paircount;
}

/**
  Make a pair from next message successfully received
**/
void LwRx::lwrx_makepair(uint8_t timeout) {
   rx_pairtimeout = timeout;
   rx_pairstarttime = millis();
}

/**
  Get pair data (translated back to nibble form
**/
uint8_t LwRx::lwrx_getpair(uint8_t* pairdata, uint8_t pairnumber) {
   if(pairnumber < rx_paircount) {
      int16_t j;	//int
      for(uint8_t i=0; i<8; i++) {
         j = this->rx_findNibble(rx_pairs[pairnumber][i]);
         if(j>=0) pairdata[i] = j;
      }
   }
   return rx_paircount;
}

/**
  Clear all pairing
**/
void LwRx::lwrx_clearpairing() {
   rx_paircount = 0;
#if EEPROM_EN
   EEPROM.write(EEPROMaddr, 0);
#endif
}

/**
  Set EEPROMAddr
**/
void LwRx::lwrx_setEEPROMaddr(int addr) {
   EEPROMaddr = addr;
}

/**
  Return stats on high and low pulses
**/
bool LwRx::lwrx_getstats(uint16_t *stats) {	//unsigned int
   if(lwrx_stats_enable) {
      memcpy(stats, lwrx_stats, 2 * rx_stat_count);
      return true;
   } else {
      return false;
   }
}

/**
  Set stats mode
**/
void LwRx::lwrx_setstatsenable(bool rx_stats_enable) {
   lwrx_stats_enable = rx_stats_enable;
   if(!lwrx_stats_enable) {
      //clear down stats when disabling
      memcpy(lwrx_stats, lwrx_statsdflt, sizeof(lwrx_statsdflt));
   }
}
/**
  Set pairs behaviour
**/
void LwRx::lwrx_setPairMode(bool pairEnforce, bool pairBaseOnly) {
   rx_pairEnforce = pairEnforce;
   rx_pairBaseOnly = pairBaseOnly;
}


/**
  Set things up to receive LightWaveRF 434Mhz messages
  pin must be 2 or 3 to trigger interrupts
  !!! For Spark, any pin will work
**/
void LwRx::lwrx_setup(InternalGPIOPin *pin) {
   restoreEEPROMPairing();
	//rx_pin = pin;
   pin->setup();
   rx_pin_isr = pin->to_isr();
   pin->attach_interrupt(&LwRx::rx_process_bits, this, gpio::INTERRUPT_ANY_EDGE);

   memcpy(lwrx_stats, lwrx_statsdflt, sizeof(lwrx_statsdflt));
}

/**
  Check a message to see if it should be reported under pairing / mood / all off rules
  returns -1 if none found
**/
bool LwRx::rx_reportMessage() {
   if(rx_pairEnforce && rx_paircount == 0) {
      return false;
   } else {
      bool allDevices;
      // True if mood to device 15 or Off cmd with Allof paramater
      allDevices = ((rx_msg[3] == rx_cmd_mood && rx_msg[2] == rx_dev_15) || 
                    (rx_msg[3] == rx_cmd_off && rx_msg[0] == rx_par0_alloff));
      return (rx_checkPairs(&rx_msg[2], allDevices) != -1);
   }
}
/**
  Find nibble from byte
  returns -1 if none found
**/
int16_t LwRx::rx_findNibble(uint8_t data) {	//int
   int16_t i = 15;	//int
   do {
      if(rx_nibble[i] == data) break;
      i--;
   } while (i >= 0);
   return i;
}

/**
  add pair from message buffer
**/
void LwRx::rx_addpairfrommsg() {
   if(rx_paircount < rx_maxpairs) {
      memcpy(rx_pairs[rx_paircount], &rx_msg[2], 8);
      this->rx_paircommit();
   }
}

/**
  check and commit pair
**/
void LwRx::rx_paircommit() {
   if(rx_paircount == 0 || this->rx_checkPairs(rx_pairs[rx_paircount], false) < 0) {
#if EEPROM_EN
		for(uint8_t i=0; i<8; i++) {
			EEPROM.write(EEPROMaddr + 1 + 8 * rx_paircount + i, rx_pairs[rx_paircount][i]);
		}
      EEPROM.write(EEPROMaddr, rx_paircount+1);
#endif
      rx_paircount++;
   }
}

/**
  Check to see if message matches one of the pairs
    if mode is pairBase only then ignore device and room
    if allDevices is true then ignore the device number
  Returns matching pair number, -1 if not found, -2 if no pairs defined
**/
int16_t LwRx::rx_checkPairs(uint8_t *buf, bool allDevices ) {	//int
   if(rx_paircount ==0) {
      return -2;
   } else {
      int16_t pair= rx_paircount;	//int
      int16_t j = -1;	//int
      int16_t jstart,jend;	//int
      if(rx_pairBaseOnly) {
         // skip room(8) and dev/cmd (0,1)
         jstart = 7;
         jend = 2;
      } else {
         //include room in comparison
         jstart = 8;
         //skip device comparison if allDevices true
         jend = (allDevices) ? 2 : 0;
      }
      while (pair>0 && j<0) {
         pair--;
         j = jstart;
         while(j>jend){
            j--;
            if(j != 1) {
               if(rx_pairs[pair][j] != buf[j]) {
                  j = -1;
               }
            }
         }
      }
      return (j >= 0) ? pair : -1;
   }
}

/**
  Remove an existing pair matching the buffer
**/
void LwRx::rx_removePair(uint8_t *buf) {
   int16_t pair = this->rx_checkPairs(buf, false);	//int
   if(pair >= 0) {
      while (pair < rx_paircount - 1) {
         for(uint8_t j=0; j<8;j++) {
            rx_pairs[pair][j] = rx_pairs[pair+1][j];
#if EEPROM_EN
            if(EEPROMaddr >= 0) {
               EEPROM.write(EEPROMaddr + 1 + 8 * pair + j, rx_pairs[pair][j]);
            }
#endif
         }
         pair++;
      }
      rx_paircount--;
#if EEPROM_EN
      if(EEPROMaddr >= 0) {
         EEPROM.write(EEPROMaddr, rx_paircount);
      }
#endif
   }
}

/**
   Retrieve and set up pairing data from EEPROM if used
**/
void LwRx::restoreEEPROMPairing() {
#if EEPROM_EN
	rx_paircount = EEPROM.read(EEPROMaddr);
	if(rx_paircount > rx_maxpairs) {
		rx_paircount = 0;
		EEPROM.write(EEPROMaddr, 0);
	} else {
		for( uint8_t i=0; i < rx_paircount; i++) {
			for(uint8_t j=0; j<8; j++) {
				rx_pairs[i][j] = EEPROM.read(EEPROMaddr + 1 + 8 * i + j);
			}
		}
	}
#endif
}

}
}
