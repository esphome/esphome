import importlib
import logging
import pkgutil

from esphome import pins
import esphome.codegen as cg
from esphome.components import display
from esphome.components.const import (
    BYTE_ORDER_BIG,
    BYTE_ORDER_LITTLE,
    CONF_BYTE_ORDER,
    CONF_DRAW_ROUNDING,
)
from esphome.components.display import CONF_SHOW_TEST_CARD
from esphome.components.esp32 import VARIANT_ESP32P4, only_on_variant
from esphome.components.mipi import (
    COLOR_ORDERS,
    CONF_COLOR_DEPTH,
    CONF_HSYNC_BACK_PORCH,
    CONF_HSYNC_FRONT_PORCH,
    CONF_HSYNC_PULSE_WIDTH,
    CONF_PCLK_FREQUENCY,
    CONF_PIXEL_MODE,
    CONF_USE_AXIS_FLIPS,
    CONF_VSYNC_BACK_PORCH,
    CONF_VSYNC_FRONT_PORCH,
    CONF_VSYNC_PULSE_WIDTH,
    MODE_BGR,
    PIXEL_MODE_16BIT,
    PIXEL_MODE_24BIT,
    DriverChip,
    dimension_schema,
    get_color_depth,
    map_sequence,
    power_of_two,
    requires_buffer,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR_ORDER,
    CONF_DIMENSIONS,
    CONF_ENABLE_PIN,
    CONF_ID,
    CONF_INIT_SEQUENCE,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_RESET_PIN,
    CONF_ROTATION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_WIDTH,
)
from esphome.final_validate import full_config

from . import mipi_dsi_ns, models

# Currently only ESP32-P4 is supported, so esp_ldo and psram are required
DEPENDENCIES = ["esp32", "esp_ldo", "psram"]
DOMAIN = "mipi_dsi"

LOGGER = logging.getLogger(DOMAIN)

MIPI_DSI = mipi_dsi_ns.class_("MIPI_DSI", display.Display, cg.Component)
ColorOrder = display.display_ns.enum("ColorMode")
ColorBitness = display.display_ns.enum("ColorBitness")

CONF_LANE_BIT_RATE = "lane_bit_rate"
CONF_LANES = "lanes"

DriverChip("CUSTOM")

# Import all models dynamically from the models package

for module_info in pkgutil.iter_modules(models.__path__):
    importlib.import_module(f".models.{module_info.name}", package=__package__)

MODELS = DriverChip.get_models()

COLOR_DEPTHS = {
    16: ColorBitness.COLOR_BITNESS_565,
    24: ColorBitness.COLOR_BITNESS_888,
}


def model_schema(config):
    model = MODELS[config[CONF_MODEL].upper()]
    transform = cv.Schema(
        {
            cv.Required(CONF_MIRROR_X): cv.boolean,
            cv.Required(CONF_MIRROR_Y): cv.boolean,
        }
    )
    if model.get_default(CONF_SWAP_XY) != cv.UNDEFINED:
        transform = transform.extend(
            {
                cv.Optional(CONF_SWAP_XY): cv.invalid(
                    "Axis swapping not supported by this model"
                )
            }
        )
    else:
        transform = transform.extend(
            {
                cv.Required(CONF_SWAP_XY): cv.boolean,
            }
        )
    # CUSTOM model will need to provide a custom init sequence
    iseqconf = (
        cv.Required(CONF_INIT_SEQUENCE)
        if model.initsequence is None
        else cv.Optional(CONF_INIT_SEQUENCE)
    )
    swap_xy = config.get(CONF_TRANSFORM, {}).get(CONF_SWAP_XY, False)

    # Dimensions are optional if the model has a default width and the swap_xy transform is not overridden
    cv_dimensions = (
        cv.Optional if model.get_default(CONF_WIDTH) and not swap_xy else cv.Required
    )
    pixel_modes = (PIXEL_MODE_16BIT, PIXEL_MODE_24BIT, "16", "24")
    schema = display.FULL_DISPLAY_SCHEMA.extend(
        {
            model.option(CONF_RESET_PIN, cv.UNDEFINED): pins.gpio_output_pin_schema,
            cv.GenerateID(): cv.declare_id(MIPI_DSI),
            cv_dimensions(CONF_DIMENSIONS): dimension_schema(
                model.get_default(CONF_DRAW_ROUNDING, 1)
            ),
            model.option(CONF_ENABLE_PIN, cv.UNDEFINED): cv.ensure_list(
                pins.gpio_output_pin_schema
            ),
            model.option(CONF_COLOR_ORDER, MODE_BGR): cv.enum(COLOR_ORDERS, upper=True),
            model.option(CONF_DRAW_ROUNDING, 2): power_of_two,
            model.option(CONF_PIXEL_MODE, PIXEL_MODE_16BIT): cv.one_of(
                *pixel_modes, lower=True
            ),
            model.option(CONF_TRANSFORM, cv.UNDEFINED): transform,
            cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True),
            model.option(CONF_INVERT_COLORS, False): cv.boolean,
            model.option(CONF_COLOR_DEPTH, "16"): cv.one_of(
                *[str(d) for d in COLOR_DEPTHS],
                *[f"{d}bit" for d in COLOR_DEPTHS],
                lower=True,
            ),
            model.option(CONF_USE_AXIS_FLIPS, True): cv.boolean,
            model.option(CONF_PCLK_FREQUENCY, "40MHz"): cv.All(
                cv.frequency, cv.Range(min=4e6, max=100e6)
            ),
            model.option(CONF_LANES, 2): cv.int_range(1, 2),
            model.option(CONF_LANE_BIT_RATE, None): cv.All(
                cv.bps, cv.Range(min=100e6, max=3200e6)
            ),
            iseqconf: cv.ensure_list(map_sequence),
            model.option(CONF_BYTE_ORDER, BYTE_ORDER_LITTLE): cv.one_of(
                BYTE_ORDER_LITTLE, BYTE_ORDER_BIG, lower=True
            ),
            model.option(CONF_HSYNC_PULSE_WIDTH): cv.int_,
            model.option(CONF_HSYNC_BACK_PORCH): cv.int_,
            model.option(CONF_HSYNC_FRONT_PORCH): cv.int_,
            model.option(CONF_VSYNC_PULSE_WIDTH): cv.int_,
            model.option(CONF_VSYNC_BACK_PORCH): cv.int_,
            model.option(CONF_VSYNC_FRONT_PORCH): cv.int_,
        }
    )
    return cv.All(
        schema,
        only_on_variant(supported=[VARIANT_ESP32P4]),
        cv.only_with_esp_idf,
    )


def _config_schema(config):
    config = cv.Schema(
        {
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
        },
        extra=cv.ALLOW_EXTRA,
    )(config)
    return model_schema(config)(config)


def _final_validate(config):
    global_config = full_config.get()

    from esphome.components.lvgl import DOMAIN as LVGL_DOMAIN

    if not requires_buffer(config) and LVGL_DOMAIN not in global_config:
        # If no drawing methods are configured, and LVGL is not enabled, show a test card
        config[CONF_SHOW_TEST_CARD] = True
    return config


CONFIG_SCHEMA = _config_schema
FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    model = MODELS[config[CONF_MODEL].upper()]
    color_depth = COLOR_DEPTHS[get_color_depth(config)]
    pixel_mode = int(config[CONF_PIXEL_MODE].removesuffix("bit"))
    width, height, _offset_width, _offset_height = model.get_dimensions(config)
    var = cg.new_Pvariable(config[CONF_ID], width, height, color_depth, pixel_mode)

    sequence, madctl = model.get_sequence(config)
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_init_sequence(sequence))
    cg.add(var.set_madctl(madctl))
    cg.add(var.set_invert_colors(config[CONF_INVERT_COLORS]))
    cg.add(var.set_hsync_pulse_width(config[CONF_HSYNC_PULSE_WIDTH]))
    cg.add(var.set_hsync_back_porch(config[CONF_HSYNC_BACK_PORCH]))
    cg.add(var.set_hsync_front_porch(config[CONF_HSYNC_FRONT_PORCH]))
    cg.add(var.set_vsync_pulse_width(config[CONF_VSYNC_PULSE_WIDTH]))
    cg.add(var.set_vsync_back_porch(config[CONF_VSYNC_BACK_PORCH]))
    cg.add(var.set_vsync_front_porch(config[CONF_VSYNC_FRONT_PORCH]))
    cg.add(var.set_pclk_frequency(int(config[CONF_PCLK_FREQUENCY] / 1e6)))
    cg.add(var.set_lanes(int(config[CONF_LANES])))
    cg.add(var.set_lane_bit_rate(int(config[CONF_LANE_BIT_RATE] / 1e6)))
    if reset_pin := config.get(CONF_RESET_PIN):
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))
    if enable_pin := config.get(CONF_ENABLE_PIN):
        enable = [await cg.gpio_pin_expression(pin) for pin in enable_pin]
        cg.add(var.set_enable_pins(enable))

    if model.rotation_as_transform(config):
        config[CONF_ROTATION] = 0
    await display.register_display(var, config)
    if lamb := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lamb, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
