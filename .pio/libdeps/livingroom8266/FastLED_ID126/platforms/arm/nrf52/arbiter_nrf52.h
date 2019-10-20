#ifndef __INC_ARBITER_NRF52
#define __INC_ARBITER_NRF52

#if defined(NRF52_SERIES)

#include "led_sysdefs_arm_nrf52.h"

//FASTLED_NAMESPACE_BEGIN

typedef void (*FASTLED_NRF52_PWM_INTERRUPT_HANDLER)();

// a trick learned from other embedded projects .. 
// use the enum as an index to a statically-allocated array
// to store unique information for that instance.
// also provides a count of how many instances were enabled.
//
// See led_sysdefs_arm_nrf52.h for selection....
//
typedef enum _FASTLED_NRF52_ENABLED_PWM_INSTANCE {
#if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE0)
    FASTLED_NRF52_PWM0_INSTANCE_IDX,
#endif
#if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE1)
    FASTLED_NRF52_PWM1_INSTANCE_IDX,
#endif
#if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE2)
    FASTLED_NRF52_PWM2_INSTANCE_IDX,
#endif
#if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE3)
    FASTLED_NRF52_PWM3_INSTANCE_IDX,
#endif
    FASTLED_NRF52_PWM_INSTANCE_COUNT
} FASTLED_NRF52_ENABLED_PWM_INSTANCES;

static_assert(FASTLED_NRF52_PWM_INSTANCE_COUNT > 0, "Instance count must be greater than zero -- define FASTLED_NRF52_ENABLE_PWM_INSTNACE[n] (replace `[n]` with digit)");

template <uint32_t _PWM_ID>
class PWM_Arbiter {

private:
    static_assert(_PWM_ID < 32, "PWM_ID over 31 breaks current arbitration bitmask");
    //const  uint32_t _ACQUIRE_MASK =             (1u << _PWM_ID) ;
    //const  uint32_t _CLEAR_MASK   = ~((uint32_t)(1u << _PWM_ID));
    static uint32_t                              s_PwmInUse;
    static NRF_PWM_Type * const                  s_PWM;
    static IRQn_Type      const                           s_PWM_IRQ;
    static FASTLED_NRF52_PWM_INTERRUPT_HANDLER volatile   s_Isr;

public:
    static void isr_handler() {
        return s_Isr();
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static bool            isAcquired() {
        return (0u != (s_PwmInUse & 1u)); // _ACQUIRE_MASK
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void            acquire(FASTLED_NRF52_PWM_INTERRUPT_HANDLER isr) {
        while (!tryAcquire(isr));
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static bool            tryAcquire(FASTLED_NRF52_PWM_INTERRUPT_HANDLER isr) {
        uint32_t oldValue = __sync_fetch_and_or(&s_PwmInUse, 1u); // _ACQUIRE_MASK
        if (0u == (oldValue & 1u)) { // _ACQUIRE_MASK
            s_Isr = isr;
            return true;
        }
        return false;
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void            releaseFromIsr() {
        uint32_t oldValue = __sync_fetch_and_and(&s_PwmInUse, ~1u); // _CLEAR_MASK
        if (0u == (oldValue & 1u)) { // _ACQUIRE_MASK
            // TODO: This should never be true... indicates was not held.
            // Assert here?
            (void)oldValue;
        }
        return;
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static NRF_PWM_Type *  getPWM() {
        return s_PWM;
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static IRQn_Type       getIRQn() { return s_PWM_IRQ; }
};
template <uint32_t _PWM_ID> NRF_PWM_Type * const PWM_Arbiter<_PWM_ID>::s_PWM           =
    #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE0)
        (_PWM_ID == 0 ? NRF_PWM0 :
    #endif
    #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE1)
        (_PWM_ID == 1 ? NRF_PWM1 :
    #endif
    #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE2)
        (_PWM_ID == 2 ? NRF_PWM2 :
    #endif
    #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE3)
        (_PWM_ID == 3 ? NRF_PWM3 :
    #endif
        (NRF_PWM_Type*)-1
    #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE0)
        )
    #endif
    #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE1)
        )
    #endif
    #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE2)
        )
    #endif
    #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE3)
        )
    #endif
    ;
template <uint32_t _PWM_ID> IRQn_Type    const                            PWM_Arbiter<_PWM_ID>::s_PWM_IRQ   = ((IRQn_Type)((uint8_t)((uint32_t)(s_PWM) >> 12)));
template <uint32_t _PWM_ID> uint32_t                                      PWM_Arbiter<_PWM_ID>::s_PwmInUse  = 0;
template <uint32_t _PWM_ID> FASTLED_NRF52_PWM_INTERRUPT_HANDLER volatile  PWM_Arbiter<_PWM_ID>::s_Isr       = NULL;

//FASTLED_NAMESPACE_END

#endif // NRF52_SERIES
#endif // __INC_ARBITER_NRF52