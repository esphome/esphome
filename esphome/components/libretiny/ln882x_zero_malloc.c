// LN882x: a zero-size pvPortMalloc trips the SDK's malloc-failed LN_ASSERT; ask for 1 byte instead.

#ifdef USE_LN882X

#include <stddef.h>

void *__real_pvPortMalloc(size_t size);  // NOLINT(readability-identifier-naming)

void *__wrap_pvPortMalloc(size_t size) {  // NOLINT(readability-identifier-naming)
  return __real_pvPortMalloc(size == 0 ? 1 : size);
}

#endif  // USE_LN882X
