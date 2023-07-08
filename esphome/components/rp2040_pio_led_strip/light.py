from dataclasses import dataclass

from esphome import pins
from esphome.components import light, rp2040
from esphome.const import (
    CONF_CHIPSET,
    CONF_ID,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_RGB_ORDER,
)

import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.util import _LOGGER


def get_nops(timing):
    """
    Calculate the number of NOP instructions required to wait for a given amount of time.
    """
    time_remaining = timing
    nops = []
    if time_remaining < 32:
        nops.append(time_remaining - 1)
        return nops
    nops.append(31)
    time_remaining -= 32
    while time_remaining > 0:
        if time_remaining >= 32:
            nops.append("nop [31]")
            time_remaining -= 32
        else:
            nops.append("nop [" + str(time_remaining) + " - 1 ]")
            time_remaining = 0
    return nops


def generate_assembly_code(id, rgbw, t0h, t0l, t1h, t1l):
    """
    Generate assembly code with the given timing values.
    """
    nops_t0h = get_nops(t0h)
    nops_t0l = get_nops(t0l)
    nops_t1h = get_nops(t1h)
    nops_t1l = get_nops(t1l)

    t0h = nops_t0h.pop(0)
    t0l = nops_t0l.pop(0)
    t1h = nops_t1h.pop(0)
    t1l = nops_t1l.pop(0)

    nops_t0h = "\n".join(" " * 4 + nop for nop in nops_t0h)
    nops_t0l = "\n".join(" " * 4 + nop for nop in nops_t0l)
    nops_t1h = "\n".join(" " * 4 + nop for nop in nops_t1h)
    nops_t1l = "\n".join(" " * 4 + nop for nop in nops_t1l)

    const_csdk_code = f"""
% c-sdk {{
#include "hardware/clocks.h"

static inline void rp2040_pio_led_strip_driver_{id}_init(PIO pio, uint sm, uint offset, uint pin, float freq) {{
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config c = rp2040_pio_led_strip_{id}_program_get_default_config(offset);
    sm_config_set_set_pins(&c, pin, 1);
    sm_config_set_out_shift(&c, false, true, {32 if rgbw else 24});
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    int cycles_per_bit = 69;
    float div = 2.409;
    sm_config_set_clkdiv(&c, div);


    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}}
%}}"""

    assembly_template = f""".program rp2040_pio_led_strip_{id}

.wrap_target
awaiting_data:
    ; Wait for data in FIFO queue
    pull block ; this will block until there is data in the FIFO queue and then it will pull it into the shift register
    set y, {31 if rgbw else 23} ; set y to the number of bits to write counting 0, (23 if RGB, 31 if RGBW)

mainloop:
    ; go through each bit in the shift register and jump to the appropriate label
    ; depending on the value of the bit

    out x, 1
    jmp !x, writezero
    jmp writeone

writezero:
    ; Write T0H and T0L bits to the output pin
    set pins, 1 [{t0h}]
{nops_t0h}
    set pins, 0 [{t0l}]
{nops_t0l}
    jmp y--, mainloop
    jmp awaiting_data

writeone:
    ; Write T1H and T1L bits to the output pin
    set pins, 1 [{t1h}]
{nops_t1h}
    set pins, 0 [{t1l}]
{nops_t1l}
    jmp y--, mainloop
    jmp awaiting_data

.wrap"""

    return assembly_template + const_csdk_code


def time_to_cycles(time_us):
    cycles_per_us = 57.5
    cycles = round(float(time_us) * cycles_per_us)
    return cycles


CONF_PIO = "pio"

AUTO_LOAD = ["rp2040_pio"]
CODEOWNERS = ["@Papa-DMan"]
DEPENDENCIES = ["rp2040"]

rp2040_pio_led_strip_ns = cg.esphome_ns.namespace("rp2040_pio_led_strip")
RP2040PIOLEDStripLightOutput = rp2040_pio_led_strip_ns.class_(
    "RP2040PIOLEDStripLightOutput", light.AddressableLight
)

RGBOrder = rp2040_pio_led_strip_ns.enum("RGBOrder")

Chipsets = rp2040_pio_led_strip_ns.enum("Chipset")


@dataclass
class LEDStripTimings:
    T0H: int
    T0L: int
    T1H: int
    T1L: int


RGB_ORDERS = {
    "RGB": RGBOrder.ORDER_RGB,
    "RBG": RGBOrder.ORDER_RBG,
    "GRB": RGBOrder.ORDER_GRB,
    "GBR": RGBOrder.ORDER_GBR,
    "BGR": RGBOrder.ORDER_BGR,
    "BRG": RGBOrder.ORDER_BRG,
}

CHIPSETS = {
    "WS2812": LEDStripTimings(20, 43, 41, 31),
    "WS2812B": LEDStripTimings(23, 46, 46, 23),
    "SK6812": LEDStripTimings(17, 52, 31, 31),
    "SM16703": LEDStripTimings(17, 52, 52, 17),
}

CONF_IS_RGBW = "is_rgbw"
CONF_BIT0_HIGH = "bit0_high"
CONF_BIT0_LOW = "bit0_low"
CONF_BIT1_HIGH = "bit1_high"
CONF_BIT1_LOW = "bit1_low"


def _validate_timing(value):
    # if doesn't end with us, raise error
    if not value.endswith("us"):
        raise cv.Invalid("Timing must be in microseconds (us)")
    value = float(value[:-2])
    nops = get_nops(value)
    nops.pop(0)
    if len(nops) > 3:
        raise cv.Invalid("Timing is too long, please try again.")
    return value


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(RP2040PIOLEDStripLightOutput),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Required(CONF_RGB_ORDER): cv.enum(RGB_ORDERS, upper=True),
            cv.Required(CONF_PIO): cv.one_of(0, 1, int=True),
            cv.Optional(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Optional(CONF_IS_RGBW, default=False): cv.boolean,
            cv.Inclusive(
                CONF_BIT0_HIGH,
                "custom",
            ): _validate_timing,
            cv.Inclusive(
                CONF_BIT0_LOW,
                "custom",
            ): _validate_timing,
            cv.Inclusive(
                CONF_BIT1_HIGH,
                "custom",
            ): _validate_timing,
            cv.Inclusive(
                CONF_BIT1_LOW,
                "custom",
            ): _validate_timing,
        }
    ),
    cv.has_exactly_one_key(CONF_CHIPSET, CONF_BIT0_HIGH),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    id = config[CONF_ID].id
    await light.register_light(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_pin(config[CONF_PIN]))

    cg.add(var.set_rgb_order(config[CONF_RGB_ORDER]))
    cg.add(var.set_is_rgbw(config[CONF_IS_RGBW]))

    cg.add(var.set_pio(config[CONF_PIO]))
    cg.add(var.set_program(cg.RawExpression(f"&rp2040_pio_led_strip_{id}_program")))
    cg.add(
        var.set_init_function(
            cg.RawExpression(f"rp2040_pio_led_strip_driver_{id}_init")
        )
    )

    key = f"led_strip_{id}"

    if CONF_CHIPSET in config:
        _LOGGER.info("Generating PIO assembly code")
        rp2040.add_pio_file(
            __name__,
            key,
            generate_assembly_code(
                id,
                config[CONF_IS_RGBW],
                CHIPSETS[config[CONF_CHIPSET]].T0H,
                CHIPSETS[config[CONF_CHIPSET]].T0L,
                CHIPSETS[config[CONF_CHIPSET]].T1H,
                CHIPSETS[config[CONF_CHIPSET]].T1L,
            ),
        )
    else:
        _LOGGER.info("Generating custom PIO assembly code")
        rp2040.add_pio_file(
            __name__,
            key,
            generate_assembly_code(
                id,
                config[CONF_IS_RGBW],
                time_to_cycles(config[CONF_BIT0_HIGH]),
                time_to_cycles(config[CONF_BIT0_LOW]),
                time_to_cycles(config[CONF_BIT1_HIGH]),
                time_to_cycles(config[CONF_BIT1_LOW]),
            ),
        )
