/*
 * RP2040 is Cortex-M0+ (ARMv6-M), which has no LDREX/STREX exclusive-access
 * instructions. GCC therefore cannot inline std::atomic's built-ins and instead emits
 * calls to the standard __atomic_* functions. Zephyr's ATOMIC_OPERATIONS_C fallback
 * already provides __atomic_compare_exchange_{1,2,4} for exactly this case
 * (zephyr/lib/cpp/abi/cpp_atomics.c), but not the remaining ops std::atomic can lower
 * to (load/store/exchange/fetch_*). This file completes that shim using the identical
 * irq_lock()/irq_unlock() technique so std::atomic works in general on RP2040, not just
 * for compare_exchange.
 *
 * The `memorder` parameters are ignored because irq_lock() provides a full memory
 * barrier, at least as strong as any requested __ATOMIC_* order.
 * TODO: Remove when Zephyr fixes this
 */

#include <zephyr/kernel.h>
#include <stdint.h>

#define DEFINE_ATOMIC_LOAD(n, type) \
  type __atomic_load_##n(const volatile void *ptr, int memorder) { \
    unsigned int key = irq_lock(); \
    type ret = *(const volatile type *) ptr; \
    irq_unlock(key); \
    return ret; \
  }

#define DEFINE_ATOMIC_STORE(n, type) \
  void __atomic_store_##n(volatile void *ptr, type val, int memorder) { \
    unsigned int key = irq_lock(); \
    *(volatile type *) ptr = val; \
    irq_unlock(key); \
  }

#define DEFINE_ATOMIC_EXCHANGE(n, type) \
  type __atomic_exchange_##n(volatile void *ptr, type val, int memorder) { \
    unsigned int key = irq_lock(); \
    volatile type *p = ptr; \
    type ret = *p; \
    *p = val; \
    irq_unlock(key); \
    return ret; \
  }

/* fetch_* ops return the *old* value, matching Zephyr's own kernel/atomic_c.c shape. */
#define DEFINE_ATOMIC_FETCH_OP(name, n, type, op) \
  type __atomic_fetch_##name##_##n(volatile void *ptr, type val, int memorder) { \
    unsigned int key = irq_lock(); \
    volatile type *p = ptr; \
    type ret = *p; \
    *p = ret op val; \
    irq_unlock(key); \
    return ret; \
  }

#define DEFINE_ATOMIC_FETCH_NAND(n, type) \
  type __atomic_fetch_nand_##n(volatile void *ptr, type val, int memorder) { \
    unsigned int key = irq_lock(); \
    volatile type *p = ptr; \
    type ret = *p; \
    *p = ~(ret & val); \
    irq_unlock(key); \
    return ret; \
  }

#define DEFINE_ATOMIC_OPS(n, type) \
  DEFINE_ATOMIC_LOAD(n, type) \
  DEFINE_ATOMIC_STORE(n, type) \
  DEFINE_ATOMIC_EXCHANGE(n, type) \
  DEFINE_ATOMIC_FETCH_OP(add, n, type, +) \
  DEFINE_ATOMIC_FETCH_OP(sub, n, type, -) \
  DEFINE_ATOMIC_FETCH_OP(and, n, type, &) \
  DEFINE_ATOMIC_FETCH_OP(or, n, type, |) \
  DEFINE_ATOMIC_FETCH_OP(xor, n, type, ^) \
  DEFINE_ATOMIC_FETCH_NAND(n, type)

DEFINE_ATOMIC_OPS(1, uint8_t)
DEFINE_ATOMIC_OPS(2, uint16_t)
DEFINE_ATOMIC_OPS(4, uint32_t)
