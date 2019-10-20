#ifndef __INC_CLOCKLESS_ARM_NRF52
#define __INC_CLOCKLESS_ARM_NRF52

#if defined(NRF52_SERIES)


//FASTLED_NAMESPACE_BEGIN

#define FASTLED_HAS_CLOCKLESS 1
#define FASTLED_NRF52_MAXIMUM_PIXELS_PER_STRING 144 // TODO: Figure out how to safely let this be calller-defined....

// nRF52810 has a single PWM peripheral (PWM0)
// nRF52832 has three PWM peripherals (PWM0, PWM1, PWM2)
// nRF52840 has four PWM peripherals (PWM0, PWM1, PWM2, PWM3)
// NOTE: Update platforms.cpp in root of FastLED library if this changes
#define FASTLED_NRF52_PWM_ID 0


extern uint32_t isrCount;


template <uint8_t _DATA_PIN, int _T1, int _T2, int _T3, EOrder _RGB_ORDER = RGB, int _XTRA0 = 0, bool _FLIP = false, int _WAIT_TIME_MICROSECONDS = 10>
class ClocklessController : public CPixelLEDController<_RGB_ORDER> {
    static_assert(FASTLED_NRF52_MAXIMUM_PIXELS_PER_STRING > 0, "Maximum string length must be positive value (FASTLED_NRF52_MAXIMUM_PIXELS_PER_STRING)");
    static_assert(_T1         >             0 , "negative values are not allowed");
    static_assert(_T2         >             0 , "negative values are not allowed");
    static_assert(_T3         >             0 , "negative values are not allowed");
    static_assert(_T1         <  (0x8000u-2u), "_T1 must fit in 15 bits");
    static_assert(_T2         <  (0x8000u-2u), "_T2 must fit in 15 bits");
    static_assert(_T3         <  (0x8000u-2u), "_T3 must fit in 15 bits");
    static_assert(_T1         <  (0x8000u-2u), "_T0H must fit in 15 bits");
    static_assert(_T1+_T2     <  (0x8000u-2u), "_T1H must fit in 15 bits");
    static_assert(_T1+_T2+_T3 <  (0x8000u-2u), "_TOP must fit in 15 bits");
    static_assert(_T1+_T2+_T3 <= PWM_COUNTERTOP_COUNTERTOP_Msk, "_TOP too large for peripheral");

private:
    static const bool     _INITIALIZE_PIN_HIGH = (_FLIP ? 1 : 0);
    static const uint16_t _POLARITY_BIT        = (_FLIP ? 0 : 0x8000);

    static const uint8_t  _BITS_PER_PIXEL   = (8 + _XTRA0) * 3; // NOTE: 3 means RGB only...
    static const uint16_t _PWM_BUFFER_COUNT = (_BITS_PER_PIXEL * FASTLED_NRF52_MAXIMUM_PIXELS_PER_STRING);
    static const uint8_t  _T0H = ((uint16_t)(_T1        ));
    static const uint8_t  _T1H = ((uint16_t)(_T1+_T2    ));
    static const uint8_t  _TOP = ((uint16_t)(_T1+_T2+_T3));

    // may as well be static, as can only attach one LED string per _DATA_PIN....
    static uint16_t s_SequenceBuffer[_PWM_BUFFER_COUNT];
    static uint16_t s_SequenceBufferValidElements;
    static uint32_t s_SequenceBufferInUse;
    static CMinWait<_WAIT_TIME_MICROSECONDS> mWait;  // ensure data has time to latch

    FASTLED_NRF52_INLINE_ATTRIBUTE static void startPwmPlayback_InitializePinState() {
        FastPin<_DATA_PIN>::setOutput();
        if (_INITIALIZE_PIN_HIGH) {
            FastPin<_DATA_PIN>::hi();
        } else {
            FastPin<_DATA_PIN>::lo();
        }
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void startPwmPlayback_InitializePwmInstance(NRF_PWM_Type * pwm) {

        // Pins must be set before enabling the peripheral
        pwm->PSEL.OUT[0] = FastPin<_DATA_PIN>::nrf_pin();
        pwm->PSEL.OUT[1] = NRF_PWM_PIN_NOT_CONNECTED;
        pwm->PSEL.OUT[2] = NRF_PWM_PIN_NOT_CONNECTED;
        pwm->PSEL.OUT[3] = NRF_PWM_PIN_NOT_CONNECTED;
        nrf_pwm_enable(pwm);
        nrf_pwm_configure(pwm, NRF_PWM_CLK_16MHz, NRF_PWM_MODE_UP, _TOP);
        nrf_pwm_decoder_set(pwm, NRF_PWM_LOAD_COMMON, NRF_PWM_STEP_AUTO);

        // clear any prior shorts / interrupt enable bits
        nrf_pwm_shorts_set(pwm, 0);
        nrf_pwm_int_set(pwm, 0);
        // clear all prior events
        nrf_pwm_event_clear(pwm, NRF_PWM_EVENT_STOPPED);
        nrf_pwm_event_clear(pwm, NRF_PWM_EVENT_SEQSTARTED0);
        nrf_pwm_event_clear(pwm, NRF_PWM_EVENT_SEQSTARTED1);
        nrf_pwm_event_clear(pwm, NRF_PWM_EVENT_SEQEND0);
        nrf_pwm_event_clear(pwm, NRF_PWM_EVENT_SEQEND1);
        nrf_pwm_event_clear(pwm, NRF_PWM_EVENT_PWMPERIODEND);
        nrf_pwm_event_clear(pwm, NRF_PWM_EVENT_LOOPSDONE);
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void startPwmPlayback_ConfigurePwmSequence(NRF_PWM_Type * pwm) {
        // config is easy, using SEQ0, no loops...
        nrf_pwm_sequence_t sequenceConfig;
        sequenceConfig.values.p_common = &(s_SequenceBuffer[0]);
        sequenceConfig.length          = s_SequenceBufferValidElements;
        sequenceConfig.repeats         = 0; // send the data once, and only once
        sequenceConfig.end_delay       = 0; // no extra delay at the end of SEQ[0] / SEQ[1]
        nrf_pwm_sequence_set(pwm, 0, &sequenceConfig);
        nrf_pwm_sequence_set(pwm, 1, &sequenceConfig);
        nrf_pwm_loop_set(pwm, 0);

    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void startPwmPlayback_EnableInterruptsAndShortcuts(NRF_PWM_Type * pwm) {
        IRQn_Type irqn = PWM_Arbiter<FASTLED_NRF52_PWM_ID>::getIRQn();
        // TODO: check API results...
        uint32_t result;

        result = sd_nvic_SetPriority(irqn, configMAX_SYSCALL_INTERRUPT_PRIORITY);
        (void)result;
        result = sd_nvic_EnableIRQ(irqn);
        (void)result;

        // shortcuts prevent (up to) 4-cycle delay from interrupt handler to next action
        uint32_t shortsToEnable = 0;
        shortsToEnable |= NRF_PWM_SHORT_SEQEND0_STOP_MASK;        ///< SEQEND[0] --> STOP task.
        shortsToEnable |= NRF_PWM_SHORT_SEQEND1_STOP_MASK;        ///< SEQEND[1] --> STOP task.
        //shortsToEnable |= NRF_PWM_SHORT_LOOPSDONE_SEQSTART0_MASK; ///< LOOPSDONE --> SEQSTART[0] task.
        //shortsToEnable |= NRF_PWM_SHORT_LOOPSDONE_SEQSTART1_MASK; ///< LOOPSDONE --> SEQSTART[1] task.
        shortsToEnable |= NRF_PWM_SHORT_LOOPSDONE_STOP_MASK;      ///< LOOPSDONE --> STOP task.
        nrf_pwm_shorts_set(pwm, shortsToEnable);

        // mark which events should cause interrupts...
        uint32_t interruptsToEnable = 0;
        interruptsToEnable |= NRF_PWM_INT_SEQEND0_MASK;
        interruptsToEnable |= NRF_PWM_INT_SEQEND1_MASK;
        interruptsToEnable |= NRF_PWM_INT_LOOPSDONE_MASK;
        interruptsToEnable |= NRF_PWM_INT_STOPPED_MASK;
        nrf_pwm_int_set(pwm, interruptsToEnable);

    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void startPwmPlayback_StartTask(NRF_PWM_Type * pwm) {
        nrf_pwm_task_trigger(pwm, NRF_PWM_TASK_SEQSTART0);
    }

public:
    static void isr_handler() {
        NRF_PWM_Type * pwm = PWM_Arbiter<FASTLED_NRF52_PWM_ID>::getPWM();
        IRQn_Type irqn = PWM_Arbiter<FASTLED_NRF52_PWM_ID>::getIRQn();

        // Currently, only use SEQUENCE 0, so only event
        // of consequence is LOOPSDONE ...
        if (nrf_pwm_event_check(pwm,NRF_PWM_EVENT_STOPPED)) {
            nrf_pwm_event_clear(pwm,NRF_PWM_EVENT_STOPPED);

            // mark the sequence as no longer in use -- pointer, comparator, exchange value
            __sync_fetch_and_and(&s_SequenceBufferInUse, 0);
            // prevent further interrupts from PWM events
            nrf_pwm_int_set(pwm, 0);
            // disable PWM interrupts - None of the PWM IRQs are shared
            // with other peripherals, avoiding complexity of shared IRQs.
            sd_nvic_DisableIRQ(irqn);
            // disable the PWM instance
            nrf_pwm_disable(pwm);
            // may take up to 4 cycles for writes to propagate (APB bus @ 16MHz)
            asm __volatile__ ( "NOP; NOP; NOP; NOP;" );
            // release the PWM arbiter to be re-used by another LED string
            PWM_Arbiter<FASTLED_NRF52_PWM_ID>::releaseFromIsr();
        }
    }


    virtual void init() {
        FASTLED_NRF52_DEBUGPRINT("Clockless Timings:\n");
        FASTLED_NRF52_DEBUGPRINT("    T0H == %d", _T0H);
        FASTLED_NRF52_DEBUGPRINT("    T1H == %d", _T1H);
        FASTLED_NRF52_DEBUGPRINT("    TOP == %d\n", _TOP);
    }
    virtual uint16_t getMaxRefreshRate() const { return 800; }

    virtual void showPixels(PixelController<_RGB_ORDER> & pixels) {
        // wait for the only sequence buffer to become available
        while (s_SequenceBufferInUse != 0);
        prepareSequenceBuffers(pixels);
        mWait.wait(); // ensure min time between updates
        startPwmPlayback(s_SequenceBufferValidElements);
        return;
    }

    template<uint8_t _BIT>
    FASTLED_NRF52_INLINE_ATTRIBUTE static void WriteBitToSequence(uint8_t byte, uint16_t * e) {
        *e = _POLARITY_BIT | (((byte & (1u << _BIT)) == 0) ? _T0H : _T1H);
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void prepareSequenceBuffers(PixelController<_RGB_ORDER> & pixels) {
        s_SequenceBufferValidElements = 0;
        int32_t    remainingSequenceElements = _PWM_BUFFER_COUNT;
        uint16_t * e = s_SequenceBuffer;
        uint32_t size_needed = pixels.size(); // count of pixels
        size_needed *= (8 + _XTRA0);          // bits per pixel
        size_needed *= 2;                     // each bit takes two bytes

        if (size_needed > _PWM_BUFFER_COUNT) {
            // TODO: assert()?
            return;
        }

        while (pixels.has(1) && (remainingSequenceElements >= _BITS_PER_PIXEL)) {
            uint8_t b0 = pixels.loadAndScale0();
            WriteBitToSequence<7>(b0, e); e++;
            WriteBitToSequence<6>(b0, e); e++;
            WriteBitToSequence<5>(b0, e); e++;
            WriteBitToSequence<4>(b0, e); e++;
            WriteBitToSequence<3>(b0, e); e++;
            WriteBitToSequence<2>(b0, e); e++;
            WriteBitToSequence<1>(b0, e); e++;
            WriteBitToSequence<0>(b0, e); e++;
            if (_XTRA0 > 0) {
                for (int i = 0; i < _XTRA0; i++) {
                    WriteBitToSequence<0>(0,e); e++;
                }
            }
            uint8_t b1 = pixels.loadAndScale1();
            WriteBitToSequence<7>(b1, e); e++;
            WriteBitToSequence<6>(b1, e); e++;
            WriteBitToSequence<5>(b1, e); e++;
            WriteBitToSequence<4>(b1, e); e++;
            WriteBitToSequence<3>(b1, e); e++;
            WriteBitToSequence<2>(b1, e); e++;
            WriteBitToSequence<1>(b1, e); e++;
            WriteBitToSequence<0>(b1, e); e++;
            if (_XTRA0 > 0) {
                for (int i = 0; i < _XTRA0; i++) {
                    WriteBitToSequence<0>(0,e); e++;
                }
            }
            uint8_t b2 = pixels.loadAndScale2();
            WriteBitToSequence<7>(b2, e); e++;
            WriteBitToSequence<6>(b2, e); e++;
            WriteBitToSequence<5>(b2, e); e++;
            WriteBitToSequence<4>(b2, e); e++;
            WriteBitToSequence<3>(b2, e); e++;
            WriteBitToSequence<2>(b2, e); e++;
            WriteBitToSequence<1>(b2, e); e++;
            WriteBitToSequence<0>(b2, e); e++;
            if (_XTRA0 > 0) {
                for (int i = 0; i < _XTRA0; i++) {
                    WriteBitToSequence<0>(0,e); e++;
                }
            }

            // advance pixel and sequence pointers
            s_SequenceBufferValidElements += _BITS_PER_PIXEL;
            remainingSequenceElements     -= _BITS_PER_PIXEL;
            pixels.advanceData();
            pixels.stepDithering();
        }
    }


    FASTLED_NRF52_INLINE_ATTRIBUTE static void startPwmPlayback(uint16_t bytesToSend) {
        PWM_Arbiter<FASTLED_NRF52_PWM_ID>::acquire(isr_handler);
        NRF_PWM_Type * pwm = PWM_Arbiter<FASTLED_NRF52_PWM_ID>::getPWM();

        // mark the sequence as being in-use
        __sync_fetch_and_or(&s_SequenceBufferInUse, 1);

        startPwmPlayback_InitializePinState();
        startPwmPlayback_InitializePwmInstance(pwm);
        startPwmPlayback_ConfigurePwmSequence(pwm);
        startPwmPlayback_EnableInterruptsAndShortcuts(pwm);
        startPwmPlayback_StartTask(pwm);
        return;
    }


#if 0
    FASTLED_NRF52_INLINE_ATTRIBUTE static uint16_t* getRawSequenceBuffer() { return s_SequenceBuffer; }
    FASTLED_NRF52_INLINE_ATTRIBUTE static uint16_t getRawSequenceBufferSize() { return _PWM_BUFFER_COUNT; }
    FASTLED_NRF52_INLINE_ATTRIBUTE static uint16_t getSequenceBufferInUse() { return s_SequenceBufferInUse; }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void sendRawSequenceBuffer(uint16_t bytesToSend) {
        mWait.wait(); // ensure min time between updates
        startPwmPlayback(bytesToSend);
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void sendRawBytes(uint8_t * arrayOfBytes, uint16_t bytesToSend) {
        // wait for sequence buffer to be available
        while (s_SequenceBufferInUse != 0);

        s_SequenceBufferValidElements = 0;
        int32_t    remainingSequenceElements = _PWM_BUFFER_COUNT;
        uint16_t * e           = s_SequenceBuffer;
        uint8_t  * nextByte    = arrayOfBytes;
        for (uint16_t bytesRemain = bytesToSend;
            (remainingSequenceElements >= 8) && (bytesRemain > 0);
            bytesRemain--,
            remainingSequenceElements     -= 8,
            s_SequenceBufferValidElements += 8
            ) {
            uint8_t b = *nextByte;
            WriteBitToSequence<7,false>(b, e); e++;
            WriteBitToSequence<6,false>(b, e); e++;
            WriteBitToSequence<5,false>(b, e); e++;
            WriteBitToSequence<4,false>(b, e); e++;
            WriteBitToSequence<3,false>(b, e); e++;
            WriteBitToSequence<2,false>(b, e); e++;
            WriteBitToSequence<1,false>(b, e); e++;
            WriteBitToSequence<0,false>(b, e); e++;
            if (_XTRA0 > 0) {
                for (int i = 0; i < _XTRA0; i++) {
                    WriteBitToSequence<0,_FLIP>(0,e); e++;
                }
            }
        }
        mWait.wait(); // ensure min time between updates

        startPwmPlayback(s_SequenceBufferValidElements);
    }
#endif // 0

};

template <uint8_t _DATA_PIN, int _T1, int _T2, int _T3, EOrder _RGB_ORDER, int _XTRA0, bool _FLIP, int _WAIT_TIME_MICROSECONDS>
uint16_t ClocklessController<_DATA_PIN, _T1, _T2, _T3, _RGB_ORDER, _XTRA0, _FLIP, _WAIT_TIME_MICROSECONDS>::s_SequenceBufferValidElements = 0;
template <uint8_t _DATA_PIN, int _T1, int _T2, int _T3, EOrder _RGB_ORDER, int _XTRA0, bool _FLIP, int _WAIT_TIME_MICROSECONDS>
uint32_t ClocklessController<_DATA_PIN, _T1, _T2, _T3, _RGB_ORDER, _XTRA0, _FLIP, _WAIT_TIME_MICROSECONDS>::s_SequenceBufferInUse = 0;
template <uint8_t _DATA_PIN, int _T1, int _T2, int _T3, EOrder _RGB_ORDER, int _XTRA0, bool _FLIP, int _WAIT_TIME_MICROSECONDS>
uint16_t ClocklessController<_DATA_PIN, _T1, _T2, _T3, _RGB_ORDER, _XTRA0, _FLIP, _WAIT_TIME_MICROSECONDS>::s_SequenceBuffer[_PWM_BUFFER_COUNT];
template <uint8_t _DATA_PIN, int _T1, int _T2, int _T3, EOrder _RGB_ORDER, int _XTRA0, bool _FLIP, int _WAIT_TIME_MICROSECONDS>
CMinWait<_WAIT_TIME_MICROSECONDS> ClocklessController<_DATA_PIN, _T1, _T2, _T3, _RGB_ORDER, _XTRA0, _FLIP, _WAIT_TIME_MICROSECONDS>::mWait;

/* nrf_pwm solution
// 
// When the nRF52 softdevice (e.g., BLE) is enabled, the CPU can be pre-empted
// at any time for radio interrupts.  These interrupts cannot be disabled.
// The problem is, even simple BLE advertising interrupts may take **`348μs`**
// (per softdevice 1.40, see http://infocenter.nordicsemi.com/pdf/S140_SDS_v1.3.pdf)
// 
// The nRF52 chips have a decent Easy-DMA-enabled PWM peripheral.
//
// The major downside:
// [] The PWM peripheral has a fixed input buffer size at 16 bits per clock cycle.
//    (each clockless protocol bit == 2 bytes)
//
// The major upsides include:
// [] Fully asynchronous, freeing CPU for other tasks
// [] Softdevice interrupts do not affect PWM clocked output (reliable clocking)
//
// The initial solution generally does the following for showPixels():
// [] wait for a sequence buffer to become available
// [] prepare the entire LED string's sequence (see `prepareSequenceBuffers()`)
// [] ensures minimum wait time from prior sequence's end
//
// Options after initial solution working:
// [] 

// TODO: Double-buffers, so one can be doing DMA while the second
//       buffer is being prepared.
// TODO: Pool of buffers, so can keep N-1 active in DMA, while
//       preparing data in the final buffer?
//       Write another class similar to PWM_Arbiter, only for
//       tracking use of sequence buffers?
// TODO: Use volatile variable to track buffers that the
//       prior DMA operation is finished with, so can fill
//       in those buffers with newly-prepared data...
// apis to send the pre-generated buffer.  This would be essentially asynchronous,
// and result in efficient run time if the pixels are either (a) static, or
// (b) cycle through a limited number of options whose converted results can
// be cached and re-used.  While simple, this method takes lots of extra RAM...
// 16 bits for every full clock (high/low) cycle.
//
// Clockless chips typically send 24 bits (3x 8-bit) per pixel.
// One odd clockless chip sends 36 bits (3x 12-bit) per pixel.
// Each bit requires a 16-bit sequence entry for the PWM peripheral.
// This gives approximately:
//                 24 bpp           36 bpp
// ==========================================
//  1 pixel        48 bytes        72 bytes          
// 32 pixels    1,536 bytes     2,304 bytes
// 64 pixels    3,072 bytes     4,608 bytes
//
//
// UPDATE: this is the method I'm choosing, to get _SOMETHING_
//         clockless working...  3k RAM for 64 pixels is acceptable
//         for a first release, as it allows re-use of FASTLED
//         color correction, dithering, etc. ....
*/

//FASTLED_NAMESPACE_END

#endif // NRF52_SERIES
#endif // __INC_CLOCKLESS_ARM_NRF52