// -------------------------------------------------- //
// This file is autogenerated by pioasm; do not edit! //
// -------------------------------------------------- //

#pragma once

#ifdef USE_RP2040

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

// ----------------------------- //
// rp2040_pio_led_sm16703_driver //
// ----------------------------- //

namespace esphome {
namespace rp2040_pio_led_strip {

static const uint8_t rp2040_pio_led_sm16703_driver_wrap_target = 0;
static const uint8_t rp2040_pio_led_sm16703_driver_wrap = 19;

static const uint16_t rp2040_pio_led_sm16703_driver_program_instructions[] = {
            //     .wrap_target
    0x80a0, //  0: pull   block
    0xe058, //  1: set    y, 24
    0x6021, //  2: out    x, 1
    0x002a, //  3: jmp    !x, 10
    0x000f, //  4: jmp    15
    0xe000, //  5: set    pins, 0
    0xbd42, //  6: nop                           [29]
    0xbd42, //  7: nop                           [29]
    0xb342, //  8: nop                           [19]
    0x0002, //  9: jmp    2
    0xf001, // 10: set    pins, 1                [16]
    0xfd00, // 11: set    pins, 0                [29]
    0xb542, // 12: nop                           [21]
    0x0082, // 13: jmp    y--, 2
    0x0000, // 14: jmp    0
    0xfd01, // 15: set    pins, 1                [29]
    0xb542, // 16: nop                           [21]
    0xf000, // 17: set    pins, 0                [16]
    0x0082, // 18: jmp    y--, 2
    0x0000, // 19: jmp    0
            //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program rp2040_pio_led_sm16703_driver_program = {
    .instructions = rp2040_pio_led_sm16703_driver_program_instructions,
    .length = 20,
    .origin = -1,
};

static inline pio_sm_config rp2040_pio_led_sm16703_driver_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + rp2040_pio_led_sm16703_driver_wrap_target, offset + rp2040_pio_led_sm16703_driver_wrap);
    return c;
}

#include "hardware/clocks.h"
static inline void rp2040_pio_SM16703_init(PIO pio, uint sm, uint offset, uint pin, float freq) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    pio_sm_config c = rp2040_pio_led_sm16703_driver_program_get_default_config(offset);
    sm_config_set_set_pins(&c, pin, 1);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    int cycles_per_bit = 69;
    float div = 2.409;
    sm_config_set_clkdiv(&c, div);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

#endif

} // namespace rp2040_pio_led_strip
} // namespace esphome

#endif
