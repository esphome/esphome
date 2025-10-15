from esphome import pins
import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_CLK_PIN,
    CONF_HEIGHT,
    CONF_ID,
    CONF_LAMBDA,
    CONF_OE_PIN,
    CONF_ROWS,
    CONF_WIDTH,
)

from . import hub75_ns

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@stuartparmenter"]

CONF_HUB75_ID = "hub75_id"
CONF_CHAIN_LENGTH = "chain_length"

CONF_R1_PIN = "r1_pin"
CONF_G1_PIN = "g1_pin"
CONF_B1_PIN = "b1_pin"
CONF_R2_PIN = "r2_pin"
CONF_G2_PIN = "g2_pin"
CONF_B2_PIN = "b2_pin"

CONF_A_PIN = "a_pin"
CONF_B_PIN = "b_pin"
CONF_C_PIN = "c_pin"
CONF_D_PIN = "d_pin"
CONF_E_PIN = "e_pin"

CONF_LAT_PIN = "lat_pin"

CONF_DRIVER = "driver"
CONF_I2S_SPEED = "i2s_speed"
CONF_LATCH_BLANKING = "latch_blanking"
CONF_CLOCK_PHASE = "clock_phase"
CONF_DOUBLE_BUFFER = "double_buffer"

CONF_VIRTUAL_MATRIX = "virtual_matrix"
CONF_COLS = "cols"
CONF_CHAIN_TYPE = "chain_type"
CONF_SCAN_TYPE = "scan_type"

# Accepted values map to ESP32-HUB75-VirtualMatrixPanel_T.hpp enums.
VM_CHAIN_CHOICES = cv.one_of(
    "CHAIN_TOP_LEFT_DOWN",
    "CHAIN_TOP_RIGHT_DOWN",
    "CHAIN_BOTTOM_LEFT_UP",
    "CHAIN_BOTTOM_RIGHT_UP",
    "CHAIN_TOP_LEFT_DOWN_ZZ",
    "CHAIN_TOP_RIGHT_DOWN_ZZ",
    "CHAIN_BOTTOM_LEFT_UP_ZZ",
    "CHAIN_BOTTOM_RIGHT_UP_ZZ",
    upper=True,
)

VM_SCAN_CHOICES = cv.one_of(
    "STANDARD_TWO_SCAN",
    "FOUR_SCAN_16PX_HIGH",
    "FOUR_SCAN_32PX_HIGH",
    "FOUR_SCAN_40PX_HIGH",
    "FOUR_SCAN_40_80PX_HFARCAN",
    "FOUR_SCAN_64PX_HIGH",
    upper=True,
)

VIRTUAL_MATRIX_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ROWS, default=1): cv.positive_int,
        cv.Optional(CONF_COLS, default=1): cv.positive_int,
        cv.Optional(CONF_CHAIN_TYPE, default="CHAIN_TOP_LEFT_DOWN"): VM_CHAIN_CHOICES,
        cv.Optional(CONF_SCAN_TYPE, default="STANDARD_TWO_SCAN"): VM_SCAN_CHOICES,
    }
)


def _validate_virtual_matrix(config):
    """Enforce chain_length == rows*cols when virtual matrix mapping is enabled."""
    vm = config.get(CONF_VIRTUAL_MATRIX)
    if vm:
        rows = vm.get(CONF_ROWS, 1)
        cols = vm.get(CONF_COLS, 1)
        required = rows * cols
        chain_len = config.get(CONF_CHAIN_LENGTH, 1)
        if chain_len != required:
            raise cv.Invalid(
                f"When virtual_matrix is configured, chain_length must equal rows*cols "
                f"({rows}*{cols}={required}), but chain_length is {chain_len}."
            )
    return config


HUB75Display = hub75_ns.class_(
    "HUB75Display", cg.PollingComponent, display.DisplayBuffer
)

shift_driver = cg.global_ns.namespace("HUB75_I2S_CFG").enum("shift_driver")
DRIVERS = {
    "SHIFTREG": shift_driver.SHIFTREG,
    "FM6124": shift_driver.FM6124,
    "FM6126A": shift_driver.FM6126A,
    "ICN2038S": shift_driver.ICN2038S,
    "MBI5124": shift_driver.MBI5124,
    "SM5266": shift_driver.SM5266P,
    "DP3246_SM5368": shift_driver.DP3246_SM5368,
}

clk_speed = cg.global_ns.namespace("HUB75_I2S_CFG").enum("clk_speed")
CLOCK_SPEEDS = {
    "HZ_8M": clk_speed.HZ_8M,
    "HZ_10M": clk_speed.HZ_10M,
    "HZ_15M": clk_speed.HZ_15M,
    "HZ_16M": clk_speed.HZ_16M,
    "HZ_20M": clk_speed.HZ_20M,
}

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(HUB75Display),
            cv.Required(CONF_WIDTH): cv.positive_int,
            cv.Required(CONF_HEIGHT): cv.positive_int,
            cv.Optional(CONF_DOUBLE_BUFFER, default=True): cv.boolean,
            cv.Optional(CONF_CHAIN_LENGTH, default=1): cv.positive_int,
            cv.Optional(CONF_BRIGHTNESS, default=128): cv.int_range(min=0, max=255),
            cv.Optional(CONF_R1_PIN, default=25): pins.gpio_output_pin_schema,
            cv.Optional(CONF_G1_PIN, default=26): pins.gpio_output_pin_schema,
            cv.Optional(CONF_B1_PIN, default=27): pins.gpio_output_pin_schema,
            cv.Optional(CONF_R2_PIN, default=14): pins.gpio_output_pin_schema,
            cv.Optional(CONF_G2_PIN, default=12): pins.gpio_output_pin_schema,
            cv.Optional(CONF_B2_PIN, default=13): pins.gpio_output_pin_schema,
            cv.Optional(CONF_A_PIN, default=23): pins.gpio_output_pin_schema,
            cv.Optional(CONF_B_PIN, default=19): pins.gpio_output_pin_schema,
            cv.Optional(CONF_C_PIN, default=5): pins.gpio_output_pin_schema,
            cv.Optional(CONF_D_PIN, default=17): pins.gpio_output_pin_schema,
            cv.Optional(CONF_E_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_LAT_PIN, default=4): pins.gpio_output_pin_schema,
            cv.Optional(CONF_OE_PIN, default=15): pins.gpio_output_pin_schema,
            cv.Optional(CONF_CLK_PIN, default=16): pins.gpio_output_pin_schema,
            cv.Optional(CONF_DRIVER): cv.enum(DRIVERS, upper=True, space="_"),
            cv.Optional(CONF_I2S_SPEED): cv.enum(CLOCK_SPEEDS, upper=True, space="_"),
            cv.Optional(CONF_LATCH_BLANKING): cv.positive_int,
            cv.Optional(CONF_CLOCK_PHASE): cv.boolean,
            cv.Optional(CONF_VIRTUAL_MATRIX): VIRTUAL_MATRIX_SCHEMA,
        }
    ).extend(cv.polling_component_schema("16ms")),
    _validate_virtual_matrix,
)


async def to_code(config):
    cg.add_build_flag("-DNO_GFX=1")
    cg.add_library(
        "https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA",
        None,
    )
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_panel_width(config[CONF_WIDTH]))
    cg.add(var.set_panel_height(config[CONF_HEIGHT]))
    cg.add(var.set_chain_length(config[CONF_CHAIN_LENGTH]))
    cg.add(var.set_initial_brightness(config[CONF_BRIGHTNESS]))
    cg.add(var.set_double_buffer(config[CONF_DOUBLE_BUFFER]))

    r1_pin = await cg.gpio_pin_expression(config[CONF_R1_PIN])
    g1_pin = await cg.gpio_pin_expression(config[CONF_G1_PIN])
    b1_pin = await cg.gpio_pin_expression(config[CONF_B1_PIN])
    r2_pin = await cg.gpio_pin_expression(config[CONF_R2_PIN])
    g2_pin = await cg.gpio_pin_expression(config[CONF_G2_PIN])
    b2_pin = await cg.gpio_pin_expression(config[CONF_B2_PIN])

    a_pin = await cg.gpio_pin_expression(config[CONF_A_PIN])
    b_pin = await cg.gpio_pin_expression(config[CONF_B_PIN])
    c_pin = await cg.gpio_pin_expression(config[CONF_C_PIN])
    d_pin = await cg.gpio_pin_expression(config[CONF_D_PIN])

    lat_pin = await cg.gpio_pin_expression(config[CONF_LAT_PIN])
    oe_pin = await cg.gpio_pin_expression(config[CONF_OE_PIN])
    clk_pin = await cg.gpio_pin_expression(config[CONF_CLK_PIN])

    if CONF_E_PIN in config:
        e_pin = await cg.gpio_pin_expression(config[CONF_E_PIN])
    else:
        e_pin = 0

    cg.add(
        var.set_pins(
            r1_pin,
            g1_pin,
            b1_pin,
            r2_pin,
            g2_pin,
            b2_pin,
            a_pin,
            b_pin,
            c_pin,
            d_pin,
            e_pin,
            lat_pin,
            oe_pin,
            clk_pin,
        )
    )

    if CONF_DRIVER in config:
        cg.add(var.set_driver(config[CONF_DRIVER]))

    if CONF_I2S_SPEED in config:
        cg.add(var.set_i2sspeed(config[CONF_I2S_SPEED]))

    if CONF_LATCH_BLANKING in config:
        cg.add(var.set_latch_blanking(config[CONF_LATCH_BLANKING]))

    if CONF_CLOCK_PHASE in config:
        cg.add(var.set_clock_phase(config[CONF_CLOCK_PHASE]))

    vm = config.get(CONF_VIRTUAL_MATRIX)
    if vm:
        cg.add_build_flag("-DUSE_VIRTUAL_PANEL=1")
        cg.add_build_flag(f"-DVPANEL_CHAIN={vm[CONF_CHAIN_TYPE]}")
        cg.add_build_flag(f"-DVPANEL_SCAN={vm[CONF_SCAN_TYPE]}")
        cg.add(var.set_virtual_rows(vm[CONF_ROWS]))
        cg.add(var.set_virtual_cols(vm[CONF_COLS]))

    await display.register_display(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
