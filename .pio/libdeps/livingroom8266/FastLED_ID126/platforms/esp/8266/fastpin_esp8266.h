#pragma once

FASTLED_NAMESPACE_BEGIN

struct FASTLED_ESP_IO {
  volatile uint32_t _GPO;
  volatile uint32_t _GPOS;
  volatile uint32_t _GPOC;
};

#define _GPB (*(FASTLED_ESP_IO*)(0x60000000+(0x300)))


template<uint8_t PIN, uint32_t MASK> class _ESPPIN {

public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  inline static void setOutput() { pinMode(PIN, OUTPUT); }
  inline static void setInput() { pinMode(PIN, INPUT); }

  inline static void hi() __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPOS = MASK; } else { GP16O |= MASK; } }
  inline static void lo() __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPOC = MASK; } else { GP16O &= ~MASK; } }
  inline static void set(register port_t val) __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPO = val; } else { GP16O = val; }}

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPO ^= MASK; } else { GP16O ^= MASK; } }

  inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { if (PIN<16) { return GPO | MASK;  } else { return GP16O | MASK; } }
  inline static port_t loval() __attribute__ ((always_inline)) { if (PIN<16) { return GPO & ~MASK; } else { return GP16O & ~MASK; } }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { if(PIN<16) { return &_GPB._GPO; } else { return &GP16O; } }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPB._GPOS; } // there is no GP160 support for this
	inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPB._GPOC; }
  inline static port_t mask() __attribute__ ((always_inline)) { return MASK; }

  inline static bool isset() __attribute__ ((always_inline)) { return (PIN < 16) ? (GPO & MASK) : (GP16O & MASK); }
};

#define _DEFPIN_ESP8266(PIN, REAL_PIN) template<> class FastPin<PIN> : public _ESPPIN<REAL_PIN, (1<<(REAL_PIN & 0xFF))> {};


#ifdef FASTLED_ESP8266_RAW_PIN_ORDER
#define MAX_PIN 16
_DEFPIN_ESP8266(0,0); _DEFPIN_ESP8266(1,1); _DEFPIN_ESP8266(2,2); _DEFPIN_ESP8266(3,3);
_DEFPIN_ESP8266(4,4); _DEFPIN_ESP8266(5,5);

// These pins should be disabled, as they always cause WDT resets
// _DEFPIN_ESP8266(6,6); _DEFPIN_ESP8266(7,7);
// _DEFPIN_ESP8266(8,8); _DEFPIN_ESP8266(9,9); _DEFPIN_ESP8266(10,10); _DEFPIN_ESP8266(11,11);

_DEFPIN_ESP8266(12,12); _DEFPIN_ESP8266(13,13); _DEFPIN_ESP8266(14,14); _DEFPIN_ESP8266(15,15);
_DEFPIN_ESP8266(16,16);

#define PORTA_FIRST_PIN 12
#elif defined(FASTLED_ESP8266_D1_PIN_ORDER)
#define MAX_PIN 15
_DEFPIN_ESP8266(0,3);
_DEFPIN_ESP8266(1,1);
_DEFPIN_ESP8266(2,16);
_DEFPIN_ESP8266(3,5);
_DEFPIN_ESP8266(4,4);
_DEFPIN_ESP8266(5,14);
_DEFPIN_ESP8266(6,12);
_DEFPIN_ESP8266(7,13);
_DEFPIN_ESP8266(8,0);
_DEFPIN_ESP8266(9,2);
_DEFPIN_ESP8266(10,15);
_DEFPIN_ESP8266(11,13);
_DEFPIN_ESP8266(12,12);
_DEFPIN_ESP8266(13,14);
_DEFPIN_ESP8266(14,4);
_DEFPIN_ESP8266(15,5);

#define PORTA_FIRST_PIN 12

#else // if defined(FASTLED_ESP8266_NODEMCU_PIN_ORDER)
#define MAX_PIN 10

// This seems to be the standard Dxx pin mapping on most of the esp boards that i've found
_DEFPIN_ESP8266(0,16); _DEFPIN_ESP8266(1,5); _DEFPIN_ESP8266(2,4); _DEFPIN_ESP8266(3,0);
_DEFPIN_ESP8266(4,2); _DEFPIN_ESP8266(5,14); _DEFPIN_ESP8266(6,12); _DEFPIN_ESP8266(7,13);
_DEFPIN_ESP8266(8,15); _DEFPIN_ESP8266(9,3); _DEFPIN_ESP8266(10,1);

#define PORTA_FIRST_PIN 6

// The rest of the pins - these are generally not available
// _DEFPIN_ESP8266(11,6);
// _DEFPIN_ESP8266(12,7); _DEFPIN_ESP8266(13,8); _DEFPIN_ESP8266(14,9); _DEFPIN_ESP8266(15,10);
// _DEFPIN_ESP8266(16,11);

#endif

#define HAS_HARDWARE_PIN_SUPPORT

#define FASTLED_NAMESPACE_END
