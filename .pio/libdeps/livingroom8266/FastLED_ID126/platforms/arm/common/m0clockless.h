#ifndef __INC_M0_CLOCKLESS_H
#define __INC_M0_CLOCKLESS_H

struct M0ClocklessData {
  uint8_t d[3];
  uint8_t e[3];
  uint8_t adj;
  uint8_t pad;
  uint32_t s[3];
};


template<int HI_OFFSET, int LO_OFFSET, int T1, int T2, int T3, EOrder RGB_ORDER, int WAIT_TIME>int
showLedData(volatile uint32_t *_port, uint32_t _bitmask, const uint8_t *_leds, uint32_t num_leds, struct M0ClocklessData *pData) {
  // Lo register variables
  register uint32_t scratch=0;
  register struct M0ClocklessData *base = pData;
  register volatile uint32_t *port = _port;
  register uint32_t d=0;
  register uint32_t counter=num_leds;
  register uint32_t bn=0;
  register uint32_t b=0;
  register uint32_t bitmask = _bitmask;

  // high register variable
  register const uint8_t *leds = _leds;
#if (FASTLED_SCALE8_FIXED == 1)
  pData->s[0]++;
  pData->s[1]++;
  pData->s[2]++;
#endif
  asm __volatile__ (
    ///////////////////////////////////////////////////////////////////////////
    //
    // asm macro definitions - used to assemble the clockless output
    //
    ".ifnotdef fl_delay_def;"
#ifdef FASTLED_ARM_M0_PLUS
    "  .set fl_is_m0p, 1;"
    "  .macro m0pad;"
    "    nop;"
    "  .endm;"
#else
    "  .set fl_is_m0p, 0;"
    "  .macro m0pad;"
    "  .endm;"
#endif
    "  .set fl_delay_def, 1;"
    "  .set fl_delay_mod, 4;"
    "  .if fl_is_m0p == 1;"
    "    .set fl_delay_mod, 3;"
    "  .endif;"
    "  .macro fl_delay dtime, reg=r0;"
    "    .if (\\dtime > 0);"
    "      .set dcycle, (\\dtime / fl_delay_mod);"
    "      .set dwork, (dcycle * fl_delay_mod);"
    "      .set drem, (\\dtime - dwork);"
    "      .rept (drem);"
    "        nop;"
    "      .endr;"
    "      .if dcycle > 0;"
    "        mov \\reg, #dcycle;"
    "        delayloop_\\@:;"
    "        sub \\reg, #1;"
    "        bne delayloop_\\@;"
    "	     .if fl_is_m0p == 0;"
    "          nop;"
    "        .endif;"
    "      .endif;"
    "    .endif;"
    "  .endm;"

    "  .macro mod_delay dtime,b1,b2,reg;"
    "    .set adj, (\\b1 + \\b2);"
    "    .if adj < \\dtime;"
    "      .set dtime2, (\\dtime - adj);"
    "      fl_delay dtime2, \\reg;"
    "    .endif;"
    "  .endm;"

    // check the bit and drop the line low if it isn't set
    "  .macro qlo4 b,bitmask,port,loff	;"
    "    lsl \\b, #1			;"
    "    bcs skip_\\@			;"
    "    str \\bitmask, [\\port, \\loff]	;"
    "    skip_\\@:			;"
    "    m0pad;"
    "  .endm				;"

    // set the pin hi or low (determined by the offset passed in )
    "  .macro qset2 bitmask,port,loff;"
    "    str \\bitmask, [\\port, \\loff];"
    "    m0pad;"
    "  .endm;"

    // Load up the next led byte to work with, put it in bn
    "  .macro loadleds3 leds, bn, rled, scratch;"
    "    mov \\scratch, \\leds;"
    "    ldrb \\bn, [\\scratch, \\rled];"
    "  .endm;"

    // check whether or not we should dither
    "  .macro loaddither7 bn,d,base,rdither;"
    "    ldrb \\d, [\\base, \\rdither];"
    "    lsl \\d, #24;"  //; shift high for the qadd w/bn
    "    lsl \\bn, #24;" //; shift high for the qadd w/d
    "    bne chkskip_\\@;" //; if bn==0, clear d;"
    "    eor \\d, \\d;" //; clear d;"
    "    m0pad;"
    "    chkskip_\\@:;"
    "  .endm;"

    // Do the qadd8 for dithering -- there's two versions of this.  The m0 version
    // takes advantage of the 3 cycle branch to do two things after the branch,
    // while keeping timing constant.  The m0+, however, branches in 2 cycles, so
    // we have to work around that a bit more.  This is one of the few times
    // where the m0 will actually be _more_ efficient than the m0+
    "  .macro dither5 bn,d;"
    "  .syntax unified;"
    "    .if fl_is_m0p == 0;"
    "      adds \\bn, \\d;"         // do the add
    "      bcc dither5_1_\\@;"
    "      mvns \\bn, \\bn;"        // set the low 24bits ot 1's
    "      lsls \\bn, \\bn, #24;"   // move low 8 bits to the high bits
    "      dither5_1_\\@:;"
    "      nop;"                    // nop to keep timing in line
    "    .else;"
    "      adds \\bn, \\d;"         // do the add"
    "      bcc dither5_2_\\@;"
    "      mvns \\bn, \\bn;"        // set the low 24bits ot 1's
    "      dither5_2_\\@:;"
    "      bcc dither5_3_\\@;"
    "      lsls \\bn, \\bn, #24;"   // move low 8 bits to the high bits
    "      dither5_3_\\@:;"
    "    .endif;"
    "  .syntax divided;"
    "  .endm;"

    // Do our scaling
    "  .macro scale4 bn, base, scale, scratch;"
    "    ldr \\scratch, [\\base, \\scale];"
    "    lsr \\bn, \\bn, #24;"                  // bring bn back down to its low 8 bits
    "    mul \\bn, \\scratch;"                  // do the multiply
    "  .endm;"

    // swap bn into b
    "  .macro swapbbn1 b,bn;"
    "    lsl \\b, \\bn, #16;"  // put the 8 bits we want for output high
    "  .endm;"

    // adjust the dithering value for the next time around (load e from memory
    // to do the math)
    "  .macro adjdither7 base,d,rled,eoffset,scratch;"
    "    ldrb \\d, [\\base, \\rled];"
    "    ldrb \\scratch,[\\base,\\eoffset];"          // load e
    "    .syntax unified;"
    "    subs \\d, \\scratch, \\d;"                   // d=e-d
    "    .syntax divided;"
    "    strb \\d, [\\base, \\rled];"                 // save d
    "  .endm;"

    // increment the led pointer (base+6 has what we're incrementing by)
    "  .macro incleds3   leds, base, scratch;"
    "    ldrb \\scratch, [\\base, #6];"               // load incremen
    "    add \\leds, \\leds, \\scratch;"              // update leds pointer
    "  .endm;"

    // compare and loop
    "  .macro cmploop5 counter,label;"
    "    .syntax unified;"
    "    subs \\counter, #1;"
    "    .syntax divided;"
    "    beq done_\\@;"
    "    m0pad;"
    "    b \\label;"
    "    done_\\@:;"
    "  .endm;"

    " .endif;"
  );

#define M0_ASM_ARGS     :             \
      [leds] "+h" (leds),             \
      [counter] "+l" (counter),       \
      [scratch] "+l" (scratch),       \
      [d] "+l" (d),                   \
      [bn] "+l" (bn),                 \
      [b] "+l" (b)                    \
    :                                 \
      [port] "l" (port),              \
      [base] "l" (base),              \
      [bitmask] "l" (bitmask),        \
      [hi_off] "I" (HI_OFFSET),       \
      [lo_off] "I" (LO_OFFSET),       \
      [led0] "I" (RO(0)),             \
      [led1] "I" (RO(1)),             \
      [led2] "I" (RO(2)),             \
      [e0] "I" (3+RO(0)),             \
      [e1] "I" (3+RO(1)),             \
      [e2] "I" (3+RO(2)),             \
      [scale0] "I" (4*(2+RO(0))),         \
      [scale1] "I" (4*(2+RO(1))),         \
      [scale2] "I" (4*(2+RO(2))),         \
      [T1] "I" (T1),                  \
      [T2] "I" (T2),                  \
      [T3] "I" (T3)                   \
    :

    /////////////////////////////////////////////////////////////////////////
    // now for some convinience macros to make building our lines a bit cleaner
#define LOOP            "  loop_%=:"
#define HI2             "  qset2 %[bitmask], %[port], %[hi_off];"
#define _D1             "  mod_delay %c[T1],2,0,%[scratch];"
#define QLO4            "  qlo4 %[b],%[bitmask],%[port], %[lo_off];"
#define LOADLEDS3(X)    "  loadleds3 %[leds], %[bn], %[led" #X "] ,%[scratch];"
#define _D2(ADJ)        "  mod_delay %c[T2],4," #ADJ ",%[scratch];"
#define LO2             "  qset2 %[bitmask], %[port], %[lo_off];"
#define _D3(ADJ)        "  mod_delay %c[T3],2," #ADJ ",%[scratch];"
#define LOADDITHER7(X)  "  loaddither7 %[bn], %[d], %[base], %[led" #X "];"
#define DITHER5         "  dither5 %[bn], %[d];"
#define SCALE4(X)       "  scale4 %[bn], %[base], %[scale" #X "], %[scratch];"
#define SWAPBBN1        "  swapbbn1 %[b], %[bn];"
#define ADJDITHER7(X)   "  adjdither7 %[base],%[d],%[led" #X "],%[e" #X "],%[scratch];"
#define INCLEDS3        "  incleds3 %[leds],%[base],%[scratch];"
#define CMPLOOP5        "  cmploop5 %[counter], loop_%=;"
#define NOTHING         ""

#if (defined(SEI_CHK) && (FASTLED_ALLOW_INTERRUPTS == 1))
    // We're allowing interrupts and have hardware timer support defined -
    // track the loop outside the asm code, to allow inserting the interrupt
    // overrun checks.
    asm __volatile__ (
      // pre-load byte 0
      LOADLEDS3(0) LOADDITHER7(0) DITHER5 SCALE4(0) ADJDITHER7(0) SWAPBBN1
      M0_ASM_ARGS);

    do {
      asm __volatile__ (
      // Write out byte 0, prepping byte 1
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(1)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(1)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(1)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(1)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      // Write out byte 1, prepping byte 2
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(2)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(2)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(2)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(2)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      // Write out byte 2, prepping byte 0
      HI2 _D1 QLO4 INCLEDS3        _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(0)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(0)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(0)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(0)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(5)

      M0_ASM_ARGS
      );
      SEI_CHK; INNER_SEI; --counter; CLI_CHK;
    } while(counter);
#elif (FASTLED_ALLOW_INTERRUPTS == 1)
    // We're allowing interrupts - track the loop outside the asm code, and
    // re-enable interrupts in between each iteration.
    asm __volatile__ (
      // pre-load byte 0
      LOADLEDS3(0) LOADDITHER7(0) DITHER5 SCALE4(0) ADJDITHER7(0) SWAPBBN1
      M0_ASM_ARGS);

    do {
      asm __volatile__ (
      // Write out byte 0, prepping byte 1
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(1)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(1)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(1)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(1)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      // Write out byte 1, prepping byte 2
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(2)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(2)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(2)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(2)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 INCLEDS3        _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      // Write out byte 2, prepping byte 0
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(0)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(0)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(0)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(0)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(5)

      M0_ASM_ARGS
      );

      uint32_t ticksBeforeInterrupts = SysTick->VAL;
      sei();
      --counter;
      cli();

      // If more than 45 uSecs have elapsed, give up on this frame and start over.
      // Note: this isn't completely correct. It's possible that more than one
      // millisecond will elapse, and so SysTick->VAL will lap
      // ticksBeforeInterrupts.
      // Note: ticksBeforeInterrupts DECREASES
      const uint32_t kTicksPerMs = VARIANT_MCK / 1000;
      const uint32_t kTicksPerUs = kTicksPerMs / 1000;
      const uint32_t kTicksIn45us = kTicksPerUs * 45;

      const uint32_t currentTicks = SysTick->VAL;

      if (ticksBeforeInterrupts < currentTicks) {
        // Timer started over
        if ((ticksBeforeInterrupts + (kTicksPerMs - currentTicks)) > kTicksIn45us) {
          return 0;
        }
      } else {
        if ((ticksBeforeInterrupts - currentTicks) > kTicksIn45us) {
          return 0;
        }
      }
    } while(counter);
#else
    // We're not allowing interrupts - run the entire loop in asm to keep things
    // as tight as possible.  In an ideal world, we should be pushing out ws281x
    // leds (or other 3-wire leds) with zero gaps between pixels.
    asm __volatile__ (
      // pre-load byte 0
    LOADLEDS3(0) LOADDITHER7(0) DITHER5 SCALE4(0) ADJDITHER7(0) SWAPBBN1

    // loop over writing out the data
    LOOP
      // Write out byte 0, prepping byte 1
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(1)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(1)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(1)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(1)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      // Write out byte 1, prepping byte 2
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(2)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(2)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(2)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(2)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 INCLEDS3        _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(0)

      // Write out byte 2, prepping byte 0
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 LOADLEDS3(0)    _D2(3) LO2 _D3(0)
      HI2 _D1 QLO4 LOADDITHER7(0)  _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 DITHER5         _D2(5) LO2 _D3(0)
      HI2 _D1 QLO4 SCALE4(0)       _D2(4) LO2 _D3(0)
      HI2 _D1 QLO4 ADJDITHER7(0)   _D2(7) LO2 _D3(0)
      HI2 _D1 QLO4 NOTHING         _D2(0) LO2 _D3(0)
      HI2 _D1 QLO4 SWAPBBN1        _D2(1) LO2 _D3(5) CMPLOOP5

      M0_ASM_ARGS
    );
#endif
    return num_leds;
}

#endif
