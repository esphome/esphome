#define FASTLED_INTERNAL
#include "FastLED.h"

FASTLED_USING_NAMESPACE

#if 0

#if defined(FASTLED_AVR) && !defined(TEENSYDUINO) && !defined(LIB8_ATTINY)
extern "C" {
// the prescaler is set so that timer0 ticks every 64 clock cycles, and the
// the overflow handler is called every 256 ticks.
#define MICROSECONDS_PER_TIMER0_OVERFLOW (clockCyclesToMicroseconds(64 * 256))

typedef union { unsigned long _long; uint8_t raw[4]; } tBytesForLong;
// tBytesForLong FastLED_timer0_overflow_count;
volatile unsigned long FastLED_timer0_overflow_count=0;
volatile unsigned long FastLED_timer0_millis = 0;

LIB8STATIC void  __attribute__((always_inline)) fastinc32 (volatile uint32_t & _long) {
  uint8_t b = ++((tBytesForLong&)_long).raw[0];
  if(!b) {
    b = ++((tBytesForLong&)_long).raw[1];
    if(!b) {
      b = ++((tBytesForLong&)_long).raw[2];
      if(!b) {
        ++((tBytesForLong&)_long).raw[3];
      }
    }
  }
}

#if defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
ISR(TIM0_OVF_vect)
#else
ISR(TIMER0_OVF_vect)
#endif
{
  fastinc32(FastLED_timer0_overflow_count);
  // FastLED_timer0_overflow_count++;
}

// there are 1024 microseconds per overflow counter tick.
unsigned long millis()
{
        unsigned long m;
        uint8_t oldSREG = SREG;

        // disable interrupts while we read FastLED_timer0_millis or we might get an
        // inconsistent value (e.g. in the middle of a write to FastLED_timer0_millis)
        cli();
        m = FastLED_timer0_overflow_count;  //._long;
        SREG = oldSREG;

        return (m*(MICROSECONDS_PER_TIMER0_OVERFLOW/8))/(1000/8);
}

unsigned long micros() {
        unsigned long m;
        uint8_t oldSREG = SREG, t;

        cli();
        m = FastLED_timer0_overflow_count; // ._long;
#if defined(TCNT0)
        t = TCNT0;
#elif defined(TCNT0L)
        t = TCNT0L;
#else
        #error TIMER 0 not defined
#endif


#ifdef TIFR0
        if ((TIFR0 & _BV(TOV0)) && (t < 255))
                m++;
#else
        if ((TIFR & _BV(TOV0)) && (t < 255))
                m++;
#endif

        SREG = oldSREG;

        return ((m << 8) + t) * (64 / clockCyclesPerMicrosecond());
}

void delay(unsigned long ms)
{
        uint16_t start = (uint16_t)micros();

        while (ms > 0) {
                if (((uint16_t)micros() - start) >= 1000) {
                        ms--;
                        start += 1000;
                }
        }
}

#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
void init()
{
  // this needs to be called before setup() or some functions won't
  // work there
  sei();

  // on the ATmega168, timer 0 is also used for fast hardware pwm
  // (using phase-correct PWM would mean that timer 0 overflowed half as often
  // resulting in different millis() behavior on the ATmega8 and ATmega168)
#if defined(TCCR0A) && defined(WGM01)
  sbi(TCCR0A, WGM01);
  sbi(TCCR0A, WGM00);
#endif

  // set timer 0 prescale factor to 64
#if defined(__AVR_ATmega128__)
  // CPU specific: different values for the ATmega128
  sbi(TCCR0, CS02);
#elif defined(TCCR0) && defined(CS01) && defined(CS00)
  // this combination is for the standard atmega8
  sbi(TCCR0, CS01);
  sbi(TCCR0, CS00);
#elif defined(TCCR0B) && defined(CS01) && defined(CS00)
  // this combination is for the standard 168/328/1280/2560
  sbi(TCCR0B, CS01);
  sbi(TCCR0B, CS00);
#elif defined(TCCR0A) && defined(CS01) && defined(CS00)
  // this combination is for the __AVR_ATmega645__ series
  sbi(TCCR0A, CS01);
  sbi(TCCR0A, CS00);
#else
  #error Timer 0 prescale factor 64 not set correctly
#endif

  // enable timer 0 overflow interrupt
#if defined(TIMSK) && defined(TOIE0)
  sbi(TIMSK, TOIE0);
#elif defined(TIMSK0) && defined(TOIE0)
  sbi(TIMSK0, TOIE0);
#else
  #error	Timer 0 overflow interrupt not set correctly
#endif

  // timers 1 and 2 are used for phase-correct hardware pwm
  // this is better for motors as it ensures an even waveform
  // note, however, that fast pwm mode can achieve a frequency of up
  // 8 MHz (with a 16 MHz clock) at 50% duty cycle

#if defined(TCCR1B) && defined(CS11) && defined(CS10)
  TCCR1B = 0;

  // set timer 1 prescale factor to 64
  sbi(TCCR1B, CS11);
#if F_CPU >= 8000000L
  sbi(TCCR1B, CS10);
#endif
#elif defined(TCCR1) && defined(CS11) && defined(CS10)
  sbi(TCCR1, CS11);
#if F_CPU >= 8000000L
  sbi(TCCR1, CS10);
#endif
#endif
  // put timer 1 in 8-bit phase correct pwm mode
#if defined(TCCR1A) && defined(WGM10)
  sbi(TCCR1A, WGM10);
#elif defined(TCCR1)
  #warning this needs to be finished
#endif

  // set timer 2 prescale factor to 64
#if defined(TCCR2) && defined(CS22)
  sbi(TCCR2, CS22);
#elif defined(TCCR2B) && defined(CS22)
  sbi(TCCR2B, CS22);
#else
  #warning Timer 2 not finished (may not be present on this CPU)
#endif

  // configure timer 2 for phase correct pwm (8-bit)
#if defined(TCCR2) && defined(WGM20)
  sbi(TCCR2, WGM20);
#elif defined(TCCR2A) && defined(WGM20)
  sbi(TCCR2A, WGM20);
#else
  #warning Timer 2 not finished (may not be present on this CPU)
#endif

#if defined(TCCR3B) && defined(CS31) && defined(WGM30)
  sbi(TCCR3B, CS31);		// set timer 3 prescale factor to 64
  sbi(TCCR3B, CS30);
  sbi(TCCR3A, WGM30);		// put timer 3 in 8-bit phase correct pwm mode
#endif

#if defined(TCCR4A) && defined(TCCR4B) && defined(TCCR4D) /* beginning of timer4 block for 32U4 and similar */
  sbi(TCCR4B, CS42);		// set timer4 prescale factor to 64
  sbi(TCCR4B, CS41);
  sbi(TCCR4B, CS40);
  sbi(TCCR4D, WGM40);		// put timer 4 in phase- and frequency-correct PWM mode
  sbi(TCCR4A, PWM4A);		// enable PWM mode for comparator OCR4A
  sbi(TCCR4C, PWM4D);		// enable PWM mode for comparator OCR4D
#else /* beginning of timer4 block for ATMEGA1280 and ATMEGA2560 */
#if defined(TCCR4B) && defined(CS41) && defined(WGM40)
  sbi(TCCR4B, CS41);		// set timer 4 prescale factor to 64
  sbi(TCCR4B, CS40);
  sbi(TCCR4A, WGM40);		// put timer 4 in 8-bit phase correct pwm mode
#endif
#endif /* end timer4 block for ATMEGA1280/2560 and similar */

#if defined(TCCR5B) && defined(CS51) && defined(WGM50)
  sbi(TCCR5B, CS51);		// set timer 5 prescale factor to 64
  sbi(TCCR5B, CS50);
  sbi(TCCR5A, WGM50);		// put timer 5 in 8-bit phase correct pwm mode
#endif

#if defined(ADCSRA)
  // set a2d prescale factor to 128
  // 16 MHz / 128 = 125 KHz, inside the desired 50-200 KHz range.
  // XXX: this will not work properly for other clock speeds, and
  // this code should use F_CPU to determine the prescale factor.
  sbi(ADCSRA, ADPS2);
  sbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);

  // enable a2d conversions
  sbi(ADCSRA, ADEN);
#endif

  // the bootloader connects pins 0 and 1 to the USART; disconnect them
  // here so they can be used as normal digital i/o; they will be
  // reconnected in Serial.begin()
#if defined(UCSRB)
  UCSRB = 0;
#elif defined(UCSR0B)
  UCSR0B = 0;
#endif
}
};
#endif

#endif

