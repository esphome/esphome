// LxTx.h
//
// LightwaveRF 434MHz tx interface for Arduino
// 
// Author: Bob Tidey (robert@tideys.net)
//Choose one of the following environments to compile for. Only one should be defined
//For SparkCore the SparkIntervalTimer Library code needs to be present
//For Due the DueTimer library code needs to be present

//#define SPARK_CORE 1
//#define DUE 1
//#define PRO32U4 1
//#define AVR328 1
//#define ESP8266 1
#define ESP8266 1
//Use following define to which timer to use (0 or 1)
#define ESP8266_TIMER 1
//For AVR328 normal assumption is 16MHz clock, to use 8MHz uncomment define
//#define AVR328_8MHZ 1

//Choose whether to include EEPROM support, comment or set to 0 to disable, 1 use with library support, 2 use with native support
#define EEPROM_EN 0

//Include basic library header and set default TX pin
#ifdef SPARK_CORE
#include "application.h"
#define TX_PIN_DEFAULT D3
#elif ESP8266
//#include <Arduino.h>
#define TX_PIN_DEFAULT 13
#elif DUE
#include <Arduino.h>
#define TX_PIN_DEFAULT 3
#else
#include <Arduino.h>
#define TX_PIN_DEFAULT 3
#endif

//Include EEPROM if required to include storing device paramters in EEPROM
#if EEPROM_EN == 1
#include <EEPROM.h>
#endif
//define default EEPROMaddr to location to store message addr
#define EEPROM_ADDR_DEFAULT 0


//Sets up basic parameters must be called at least once
extern void lwtx_setup(int pin, byte repeats, byte invert, int uSec);

//Allows changing basic tick counts from their defaults
extern void lwtx_setTickCounts( byte lowCount, byte highCount, byte trailCount, byte gapCount);

//Allws multiplying the gap period for creating very large gaps
extern void lwtx_setGapMultiplier(byte gapMultiplier);

// determines whether incoming data or should be translated from nibble data
extern void lwtx_settranslate(boolean txtranslate);

//Checks whether tx is free to accept a new message
extern boolean lwtx_free();

//Basic send of new 10 char message, not normally needed if setaddr and cmd are used.
extern void lwtx_send(byte* msg);

//Sets up 5 char address which will be used to form messages for lwtx_cmd
extern void lwtx_setaddr(byte* addr);

//Send Command
extern void lwtx_cmd(byte command, byte parameter, byte room, byte device);

//Set base address for EEPROM storage
extern void lwtx_setEEPROMaddr(int addr);

//Genralised timer routines go here
//Sets up timer and the callback to the interrupt service routine
void lw_timer_Setup(void (*isrCallback)(), int period);

//Allows changing basic tick counts from their defaults
void lw_timer_Start();

//Allws multiplying the gap period for creating very large gaps
void lw_timer_Stop();
