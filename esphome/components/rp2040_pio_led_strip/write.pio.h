// -------------------------------------------------- //
// This file is autogenerated by pioasm; do not edit! //
// -------------------------------------------------- //

#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// --------------------------- //
// rp2040_pio_led_strip_driver //
// --------------------------- //

#define rp2040_pio_led_strip_driver_wrap_target 0
#define rp2040_pio_led_strip_driver_wrap 20

static const uint16_t rp2040_pio_led_strip_driver_program_instructions[] = {
            //     .wrap_target
    0x80a0, //  0: pull   block                      
    0xe058, //  1: set    y, 24                      
    0x0006, //  2: jmp    6                          
    0x6021, //  3: out    x, 1                       
    0x002b, //  4: jmp    !x, 11                     
    0x0010, //  5: jmp    16                         
    0xe000, //  6: set    pins, 0                    
    0xbd42, //  7: nop                           [29]
    0xbd42, //  8: nop                           [29]
    0xb342, //  9: nop                           [19]
    0x0003, // 10: jmp    3                          
    0xeb01, // 11: set    pins, 1                [11]
    0xfd00, // 12: set    pins, 0                [29]
    0xa942, // 13: nop                           [9] 
    0x0083, // 14: jmp    y--, 3                     
    0x0000, // 15: jmp    0                          
    0xfd01, // 16: set    pins, 1                [29]
    0xa942, // 17: nop                           [9] 
    0xeb00, // 18: set    pins, 0                [11]
    0x0083, // 19: jmp    y--, 3                     
    0x0000, // 20: jmp    0                          
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program rp2040_pio_led_strip_driver_program = {
    .instructions = rp2040_pio_led_strip_driver_program_instructions,
    .length = 21,
    .origin = -1,
};

static inline pio_sm_config rp2040_pio_led_strip_driver_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + rp2040_pio_led_strip_driver_wrap_target, offset + rp2040_pio_led_strip_driver_wrap);
    return c;
}

#include "hardware/clocks.h"
static inline void rp2040_pio_program_init(PIO pio, uint sm, uint offset, uint pin, float freq) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_config c = rp2040_pio_led_strip_driver_program_get_default_config(offset);
    sm_config_set_set_pins(&c, pin, 1);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    int cycles_per_bit = 52;
    float div = clock_get_hz(clk_sys) / (freq * cycles_per_bit);
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

#endif

