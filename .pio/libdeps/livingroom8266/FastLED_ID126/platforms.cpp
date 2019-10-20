#define FASTLED_INTERNAL


// Interrupt handlers cannot be defined in the header.
// They must be defined as C functions, or they won't
// be found (due to name mangling), and thus won't
// override any default weak definition.
#if defined(NRF52_SERIES)

    #include "platforms/arm/nrf52/led_sysdefs_arm_nrf52.h"
    #include "platforms/arm/nrf52/arbiter_nrf52.h"

    uint32_t isrCount;

    #ifdef __cplusplus
        extern "C" {
    #endif
            // NOTE: Update platforms.cpp in root of FastLED library if this changes        
            #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE0)
                void PWM0_IRQHandler(void) { isrCount++; PWM_Arbiter<0>::isr_handler(); }
            #endif
            #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE1)
                void PWM1_IRQHandler(void) { isrCount++; PWM_Arbiter<1>::isr_handler(); }
            #endif
            #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE2)
                void PWM2_IRQHandler(void) { isrCount++; PWM_Arbiter<2>::isr_handler(); }
            #endif
            #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE3)
                void PWM3_IRQHandler(void) { isrCount++; PWM_Arbiter<3>::isr_handler(); }
            #endif
    #ifdef __cplusplus
        }
    #endif

#endif // defined(NRF52_SERIES)



// FASTLED_NAMESPACE_BEGIN
// FASTLED_NAMESPACE_END
