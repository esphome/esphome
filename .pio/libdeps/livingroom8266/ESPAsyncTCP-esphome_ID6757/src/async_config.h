#ifndef LIBRARIES_ESPASYNCTCP_SRC_ASYNC_CONFIG_H_
#define LIBRARIES_ESPASYNCTCP_SRC_ASYNC_CONFIG_H_

#ifndef ASYNC_TCP_SSL_ENABLED
#define ASYNC_TCP_SSL_ENABLED 0
#endif

#ifndef TCP_MSS
// May have been definded as a -DTCP_MSS option on the compile line or not.
// Arduino core 2.3.0 or earlier does not do the -DTCP_MSS option.
// Later versions may set this option with info from board.txt.
// However, Core 2.4.0 and up board.txt does not define TCP_MSS for lwIP v1.4
#define TCP_MSS (1460)
#endif

// #define ASYNC_TCP_DEBUG(...) ets_printf(__VA_ARGS__)
// #define TCP_SSL_DEBUG(...) ets_printf(__VA_ARGS__)
// #define ASYNC_TCP_ASSERT( a ) do{ if(!(a)){ets_printf("ASSERT: %s %u \n", __FILE__, __LINE__);}}while(0)

// Starting with Arduino Core 2.4.0 and up the define of DEBUG_ESP_PORT
// can be handled through the Arduino IDE Board options instead of here.
// #define DEBUG_ESP_PORT Serial

// #define DEBUG_ESP_ASYNC_TCP 1
// #define DEBUG_ESP_TCP_SSL 1
#include <DebugPrintMacros.h>

#ifndef ASYNC_TCP_ASSERT
#define ASYNC_TCP_ASSERT(...) do { (void)0;} while(false)
#endif
#ifndef ASYNC_TCP_DEBUG
#define ASYNC_TCP_DEBUG(...) do { (void)0;} while(false)
#endif
#ifndef TCP_SSL_DEBUG
#define TCP_SSL_DEBUG(...) do { (void)0;} while(false)
#endif

#endif /* LIBRARIES_ESPASYNCTCP_SRC_ASYNC_CONFIG_H_ */
