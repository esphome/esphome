from dataclasses import dataclass

from esphome import pins
import esphome.codegen as cg
from esphome.components import light, rp2
from esphome.components.const import CONF_CHANNEL_COLORS
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHIPSET,
    CONF_ID,
    CONF_IS_RGBW,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_RGB_ORDER,
)
from esphome.types import ConfigType
from esphome.util import _LOGGER


def get_nops(timing: float) -> list[float | str]:
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


def generate_assembly_code(id: str, t0h: int, t0l: int, t1h: int, t1l: int) -> str:
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
    sm_config_set_out_shift(&c, false, true, 8);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // target frequency is 57.5MHz
    long clk = clock_get_hz(clk_sys);
    long target_freq = 57500000;
    int n = 2;
    int f = round(((clk / target_freq) - n ) * 256);
    sm_config_set_clkdiv_int_frac(&c, n, f);


    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}}
%}}"""

    assembly_template = f""".program rp2040_pio_led_strip_{id}

.wrap_target
awaiting_data:
    ; Wait for data in FIFO queue
    ; out null, 24 ; discard the byte lane replication of the FIFO since we only need 8 bits (not needed????)
    pull block ; this will block until there is data in the FIFO queue and then it will pull it into the shift register
    set y, 7 ; set y to the number of bits to write counting 0, (always 7 because we are doing one word at a time)

mainloop:
    ; go through each bit in the shift register and jump to the appropriate label
    ; depending on the value of the bit

    out x, 1
    jmp !x, writezero

writeone:
    ; Write T1H and T1L bits to the output pin
    set pins, 1 [{t1h}]
{nops_t1h}
    set pins, 0 [{t1l}]
{nops_t1l}
    jmp y--, mainloop
    jmp awaiting_data

writezero:
    ; Write T0H and T0L bits to the output pin
    set pins, 1 [{t0h}]
{nops_t0h}
    set pins, 0 [{t0l}]
{nops_t0l}
    jmp y--, mainloop
    jmp awaiting_data



.wrap"""

    return assembly_template + const_csdk_code


def time_to_cycles(time_us: float) -> int:
    cycles_per_us = 57.5
    return round(float(time_us) * cycles_per_us)


CONF_PIO = "pio"

AUTO_LOAD = ["rp2_pio"]
CODEOWNERS = ["@Papa-DMan"]
DEPENDENCIES = ["rp2"]

rp2040_pio_led_strip_ns = cg.esphome_ns.namespace("rp2040_pio_led_strip")
RP2040PIOLEDStripLightOutput = rp2040_pio_led_strip_ns.class_(
    "RP2040PIOLEDStripLightOutput", light.AddressableLight
)

Chipset = rp2040_pio_led_strip_ns.enum("Chipset")

CHIPSETS = {
    "WS2812": Chipset.CHIPSET_WS2812,
    "WS2812B": Chipset.CHIPSET_WS2812B,
    "SK6812": Chipset.CHIPSET_SK6812,
    "SM16703": Chipset.CHIPSET_SM16703,
}


@dataclass
class LEDStripTimings:
    T0H: int
    T0L: int
    T1H: int
    T1L: int


CHIPSET_TIMINGS = {
    "WS2812": LEDStripTimings(20, 40, 46, 34),
    "WS2812B": LEDStripTimings(23, 49, 46, 26),
    "SK6812": LEDStripTimings(20, 54, 38, 38),
    "SM16703": LEDStripTimings(17, 52, 52, 17),
}

CONF_BIT0_HIGH = "bit0_high"
CONF_BIT0_LOW = "bit0_low"
CONF_BIT1_HIGH = "bit1_high"
CONF_BIT1_LOW = "bit1_low"


def _validate_timing(value: str) -> float:
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
            cv.Optional(CONF_CHANNEL_COLORS): light.validate_channel_colors,
            # Deprecated in favour of CONF_CHANNEL_COLORS, remove in 2027.3.0
            cv.Optional(CONF_RGB_ORDER): cv.one_of(*light.RGB_ORDERS, upper=True),
            cv.Optional(CONF_IS_RGBW): cv.boolean,
            cv.Required(CONF_PIO): cv.one_of(0, 1, int=True),
            cv.Optional(CONF_CHIPSET): cv.enum(CHIPSETS, upper=True),
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
    light.migrate_channel_colors(
        removed_in="2027.3.0", component="rp2040_pio_led_strip"
    ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    id = config[CONF_ID].id
    await light.register_light(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_pin(config[CONF_PIN]))

    cg.add(
        var.set_channel_colors(light.channel_colors_struct(config[CONF_CHANNEL_COLORS]))
    )

    cg.add(var.set_pio(config[CONF_PIO]))
    cg.add(var.set_program(cg.RawExpression(f"&rp2040_pio_led_strip_{id}_program")))
    cg.add(
        var.set_init_function(
            cg.RawExpression(f"rp2040_pio_led_strip_driver_{id}_init")
        )
    )

    key = f"led_strip_{id}"

    if chipset := config.get(CONF_CHIPSET):
        cg.add(var.set_chipset(chipset))
        _LOGGER.info("Generating PIO assembly code")
        rp2.add_pio_file(
            __name__,
            key,
            generate_assembly_code(
                id,
                CHIPSET_TIMINGS[chipset].T0H,
                CHIPSET_TIMINGS[chipset].T0L,
                CHIPSET_TIMINGS[chipset].T1H,
                CHIPSET_TIMINGS[chipset].T1L,
            ),
        )
    else:
        cg.add(var.set_chipset(Chipset.CHIPSET_CUSTOM))
        _LOGGER.info("Generating custom PIO assembly code")
        rp2.add_pio_file(
            __name__,
            key,
            generate_assembly_code(
                id,
                time_to_cycles(config[CONF_BIT0_HIGH]),
                time_to_cycles(config[CONF_BIT0_LOW]),
                time_to_cycles(config[CONF_BIT1_HIGH]),
                time_to_cycles(config[CONF_BIT1_LOW]),
            ),
        )
