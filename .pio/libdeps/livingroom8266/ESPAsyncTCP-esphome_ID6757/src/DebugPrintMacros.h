#ifndef _DEBUG_PRINT_MACROS_H
#define _DEBUG_PRINT_MACROS_H
// Some customizable print macros to suite the debug needs de jour.

// Debug macros
// #include <pgmspace.h>
// https://stackoverflow.com/questions/8487986/file-macro-shows-full-path
// This value is resolved at compile time.
#define _FILENAME_ strrchr("/" __FILE__, '/')

// #define DEBUG_ESP_ASYNC_TCP 1
// #define DEBUG_ESP_TCP_SSL 1
// #define DEBUG_ESP_PORT Serial

#if defined(DEBUG_ESP_PORT) && !defined(DEBUG_TIME_STAMP_FMT)
#define DEBUG_TIME_STAMP_FMT "%06u.%03u "
struct _DEBUG_TIME_STAMP {
  unsigned dec;
  unsigned whole;
};
inline struct _DEBUG_TIME_STAMP debugTimeStamp(void) {
  struct _DEBUG_TIME_STAMP st;
  unsigned now = millis() % 1000000000;
  st.dec = now % 1000;
  st.whole = now / 1000;
  return st;
}
#endif

#if defined(DEBUG_ESP_PORT) && !defined(DEBUG_GENERIC)
  #define DEBUG_GENERIC( module, format, ... ) \
    do { \
      struct _DEBUG_TIME_STAMP st = debugTimeStamp(); \
      DEBUG_ESP_PORT.printf( DEBUG_TIME_STAMP_FMT module " " format, st.whole, st.dec, ##__VA_ARGS__ ); \
    } while(false)
#endif
#if defined(DEBUG_ESP_PORT) && !defined(DEBUG_GENERIC_P)
  #define DEBUG_GENERIC_P( module, format, ... ) \
    do { \
      struct _DEBUG_TIME_STAMP st = debugTimeStamp(); \
      DEBUG_ESP_PORT.printf_P(PSTR( DEBUG_TIME_STAMP_FMT module " " format ), st.whole, st.dec, ##__VA_ARGS__ ); \
    } while(false)
#endif

#if defined(DEBUG_GENERIC) && !defined(ASSERT_GENERIC)
#define ASSERT_GENERIC( a, module ) \
  do { \
    if ( !(a) ) { \
      DEBUG_GENERIC( module, "%s:%s:%u: ASSERT("#a") failed!\n", __FILE__, __func__, __LINE__); \
      DEBUG_ESP_PORT.flush(); \
    } \
  } while(false)
#endif
#if defined(DEBUG_GENERIC_P) && !defined(ASSERT_GENERIC_P)
#define ASSERT_GENERIC_P( a, module ) \
  do { \
    if ( !(a) ) { \
      DEBUG_GENERIC_P( module, "%s:%s:%u: ASSERT("#a") failed!\n", __FILE__, __func__, __LINE__); \
      DEBUG_ESP_PORT.flush(); \
    } \
  } while(false)
#endif

#ifndef DEBUG_GENERIC
#define DEBUG_GENERIC(...) do { (void)0;} while(false)
#endif

#ifndef DEBUG_GENERIC_P
#define DEBUG_GENERIC_P(...) do { (void)0;} while(false)
#endif

#ifndef ASSERT_GENERIC
#define ASSERT_GENERIC(...) do { (void)0;} while(false)
#endif

#ifndef ASSERT_GENERIC_P
#define ASSERT_GENERIC_P(...) do { (void)0;} while(false)
#endif

#ifndef DEBUG_ESP_PRINTF
#define DEBUG_ESP_PRINTF( format, ...) DEBUG_GENERIC_P("[%s]", format, &_FILENAME_[1], ##__VA_ARGS__)
#endif

#if defined(DEBUG_ESP_ASYNC_TCP) && !defined(ASYNC_TCP_DEBUG)
#define ASYNC_TCP_DEBUG( format, ...) DEBUG_GENERIC_P("[ASYNC_TCP]", format, ##__VA_ARGS__)
#endif

#ifndef ASYNC_TCP_ASSERT
#define ASYNC_TCP_ASSERT( a ) ASSERT_GENERIC_P( (a), "[ASYNC_TCP]")
#endif

#if defined(DEBUG_ESP_TCP_SSL) && !defined(TCP_SSL_DEBUG)
#define TCP_SSL_DEBUG( format, ...) DEBUG_GENERIC_P("[TCP_SSL]", format, ##__VA_ARGS__)
#endif

#endif //_DEBUG_PRINT_MACROS_H
