#include <cstdint>

// Build information is passed in via symbols defined in a linker script
// as that is the simplest way to include build timestamps without the
// changed timestamp itself causing a rebuild through dependencies, as
// it would if it were in a header file like version.h.
//
// It's passed in in *string* form so that it can go directly into the
// flash as .rodate instead of using precious RAM to build a date string
// from a time_t at runtime.
//
// Determining the target endianness and word size from the generation
// side is problematic, so it emits *four* sets of symbols into the
// linker script, for each of little-endian and big-endiand, 32-bit and
// 64-bit targets.
//
// The LINKERSYM macro gymnastics select the correct symbol for the
// target, named e.g. 'ESPHOME_BUILD_TIME_STR_32LE_0'.

// Not all targets have <endian.h> (e.g. LibreTiny on BK72xx).
// Use the compiler built-in macros but defensively default to
// little-endian and 32-bit.
#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BO LE
#else
#define BO BE
#endif

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8
#define WS 64  // NOLINT
#else
#define WS 32  // NOLINT
#define USE_32BIT
#endif

// If you have to ask, you don't want to know...
#define LINKERSYM2(name, ws, bo, us, num) ESPHOME_##name##_##ws##bo##us##num
#define LINKERSYM1(name, ws, bo, us, num) LINKERSYM2(name, ws, bo, us, num)
#define LINKERSYM(name, num) LINKERSYM1(name, WS, BO, _, num)

extern "C" {
extern const char ESPHOME_BUILD_TIME[];
extern const char LINKERSYM(CONFIG_HASH_STR, 0)[];
extern const char LINKERSYM(CONFIG_HASH_STR, 1)[];
extern const char LINKERSYM(BUILD_TIME_STR, 0)[];
extern const char LINKERSYM(BUILD_TIME_STR, 1)[];
extern const char LINKERSYM(BUILD_TIME_STR, 2)[];
extern const char LINKERSYM(BUILD_TIME_STR, 3)[];
extern const char LINKERSYM(BUILD_TIME_STR, 4)[];
extern const char LINKERSYM(BUILD_TIME_STR, 5)[];
}

namespace esphome::buildinfo {

// An 8-byte string plus terminating NUL.
struct ConfigHashStruct {
  uintptr_t data0;
#ifdef USE_32BIT
  uintptr_t data1;
#endif
  char nul;
} __attribute__((packed));

extern const ConfigHashStruct CONFIG_HASH_STR = {(uintptr_t) &LINKERSYM(CONFIG_HASH_STR, 0),
#ifdef USE_32BIT
                                                 (uintptr_t) &LINKERSYM(CONFIG_HASH_STR, 1),
#endif
                                                 0};

// A 21-byte string plus terminating NUL, in 24 bytes
extern const uintptr_t BUILD_TIME_STR[] = {
    (uintptr_t) &LINKERSYM(BUILD_TIME_STR, 0), (uintptr_t) &LINKERSYM(BUILD_TIME_STR, 1),
    (uintptr_t) &LINKERSYM(BUILD_TIME_STR, 2),
#ifdef USE_32BIT
    (uintptr_t) &LINKERSYM(BUILD_TIME_STR, 3), (uintptr_t) &LINKERSYM(BUILD_TIME_STR, 4),
    (uintptr_t) &LINKERSYM(BUILD_TIME_STR, 5),
#endif
};

extern const uintptr_t BUILD_TIME = (uintptr_t) &ESPHOME_BUILD_TIME;

}  // namespace esphome::buildinfo
