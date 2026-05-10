"""
ESPHome configuration for the IT8951 e-paper controller.

The IT8951 differs significantly from the SPI e-paper drivers handled by the
`epaper_spi` component: it has no DC pin (commands are framed by 16-bit SPI
preambles), reports its panel geometry over SPI, supports up to 16 grayscale
levels via on-chip waveform LUTs, and requires a non-trivial async handshake
during init. It lives in its own component to keep its queue-based state
machine independent from the simpler model-driven framework used by
`epaper_spi`.
"""

from esphome import automation, core, pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.display import CONF_SHOW_TEST_CARD, validate_rotation
import esphome.config_validation as cv
from esphome.config_validation import update_interval
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_CS_PIN,
    CONF_DATA_RATE,
    CONF_DIMENSIONS,
    CONF_FULL_UPDATE_EVERY,
    CONF_HEIGHT,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODE,
    CONF_MODEL,
    CONF_PAGES,
    CONF_RESET_DURATION,
    CONF_RESET_PIN,
    CONF_REVERSED,
    CONF_ROTATION,
    CONF_SLEEP_WHEN_DONE,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_UPDATE_INTERVAL,
    CONF_WIDTH,
)
from esphome.cpp_generator import RawExpression
from esphome.final_validate import full_config

AUTO_LOAD = ["split_buffer"]
DEPENDENCIES = ["spi"]

CONF_VCOM = "vcom"
CONF_FORCE_1BPP = "force_1bpp"
CONF_UPDATE_MODE = "update_mode"

# Hardware waveform modes the IT8951 controller knows about.
# "fast" and "full" are convenience aliases for DU and GC16.
UPDATE_MODES = (
    "INIT",
    "DU",
    "GC16",
    "GL16",
    "GLR16",
    "GLD16",
    "DU4",
    "A2",
    "fast",
    "full",
)
update_mode = cv.one_of(*UPDATE_MODES, upper=False)

it8951_ns = cg.esphome_ns.namespace("it8951")
IT8951Display = it8951_ns.class_(
    "IT8951Display", display.Display, spi.SPIDevice
)
IT8951UpdateAction = it8951_ns.class_("IT8951UpdateAction", automation.Action)

# Transform flag values mirror the C++ TRANSFORM_* constants.
_TRANSFORM_NONE = 0
_TRANSFORM_MIRROR_X = 1
_TRANSFORM_MIRROR_Y = 2
_TRANSFORM_SWAP_XY = 4
_TRANSFORM_FLAGS = {
    CONF_MIRROR_X: _TRANSFORM_MIRROR_X,
    CONF_MIRROR_Y: _TRANSFORM_MIRROR_Y,
    CONF_SWAP_XY: _TRANSFORM_SWAP_XY,
}


class IT8951Model:
    """A specific board / panel preset for the IT8951 controller."""

    models: dict[str, "IT8951Model"] = {}

    def __init__(self, name: str, **defaults):
        name = name.upper()
        self.name = name
        self.defaults = defaults
        IT8951Model.models[name] = self

    def get_default(self, key, fallback=None):
        return self.defaults.get(key, fallback)

    def get_dimensions(self, config) -> tuple[int | None, int | None]:
        if CONF_DIMENSIONS in config:
            dimensions = config[CONF_DIMENSIONS]
            if isinstance(dimensions, dict):
                return dimensions[CONF_WIDTH], dimensions[CONF_HEIGHT]
            return tuple(dimensions)
        return self.get_default(CONF_WIDTH), self.get_default(CONF_HEIGHT)


# --- Model presets ----------------------------------------------------------
# The generic model leaves dimensions and pin choices up to the user.
IT8951Model("it8951", vcom=2300, sleep_when_done=True, data_rate=12_000_000)

IT8951Model(
    "M5EPD",
    width=960,
    height=540,
    busy_pin=27,
    reset_pin=23,
    cs_pin=15,
    vcom=2300,
    sleep_when_done=True,
    data_rate=20_000_000,
)

IT8951Model(
    "seeed-reterminal-e1003",
    width=1872,
    height=1404,
    busy_pin=13,
    reset_pin=12,
    cs_pin=10,
    vcom=1400,
    sleep_when_done=False,
    data_rate=4_000_000,
)

IT8951Model(
    "seeed-ee03",
    width=1872,
    height=1404,
    busy_pin=4,
    reset_pin=38,
    cs_pin=44,
    vcom=1400,
    sleep_when_done=False,
    data_rate=4_000_000,
)

# ---------------------------------------------------------------------------

DIMENSION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_WIDTH): cv.int_,
        cv.Required(CONF_HEIGHT): cv.int_,
    }
)


def _model_pin_option(model, key, schema):
    default = model.get_default(key)
    if default is None:
        return cv.Required(key), schema
    return cv.Optional(key, default=default), schema


def _model_schema(config):
    model = IT8951Model.models[config[CONF_MODEL]]
    cv_dimensions = cv.Optional if model.get_default(CONF_WIDTH) else cv.Required

    schema = display.FULL_DISPLAY_SCHEMA.extend(
        spi.spi_device_schema(
            cs_pin_required=False,
            default_mode="MODE0",
            default_data_rate=model.get_default(CONF_DATA_RATE, 10_000_000),
        )
    ).extend(
        {
            cv.GenerateID(): cv.declare_id(IT8951Display),
            cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True, space="-"),
            cv.Optional(CONF_ROTATION, default=0): validate_rotation,
            cv.Optional(CONF_UPDATE_INTERVAL, default="1min"): update_interval,
            cv.Optional(CONF_FULL_UPDATE_EVERY, default=30): cv.int_range(0, 255),
            cv.Optional(CONF_TRANSFORM): cv.Schema(
                {
                    cv.Required(CONF_MIRROR_X): cv.boolean,
                    cv.Required(CONF_MIRROR_Y): cv.boolean,
                }
            ),
            cv.Optional(
                CONF_REVERSED, default=model.get_default(CONF_REVERSED, False)
            ): cv.boolean,
            cv.Optional(
                CONF_SLEEP_WHEN_DONE,
                default=model.get_default(CONF_SLEEP_WHEN_DONE, False),
            ): cv.boolean,
            cv.Optional(
                CONF_FORCE_1BPP, default=model.get_default(CONF_FORCE_1BPP, False)
            ): cv.boolean,
            cv.Optional(
                CONF_VCOM, default=model.get_default(CONF_VCOM, 2300)
            ): cv.int_range(0, 5000),
            cv.Optional(CONF_UPDATE_MODE): update_mode,
            cv.Optional(CONF_RESET_DURATION): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=core.TimePeriod(milliseconds=500)),
            ),
            cv_dimensions(CONF_DIMENSIONS): DIMENSION_SCHEMA,
        }
    )

    # Pin options: required if the model doesn't supply a default.
    pin_specs = (
        (CONF_BUSY_PIN, pins.gpio_input_pin_schema),
        (CONF_RESET_PIN, pins.gpio_output_pin_schema),
        (CONF_CS_PIN, pins.gpio_output_pin_schema),
    )
    pin_extra = {}
    for key, schema_value in pin_specs:
        opt, sv = _model_pin_option(model, key, schema_value)
        pin_extra[opt] = sv
    return schema.extend(pin_extra)


def _customise_schema(config):
    config = cv.Schema(
        {cv.Required(CONF_MODEL): cv.one_of(*IT8951Model.models, upper=True, space="-")},
        extra=cv.ALLOW_EXTRA,
    )(config)
    return _model_schema(config)(config)


CONFIG_SCHEMA = _customise_schema


def _final_validate(config):
    # IT8951 reads from SPI (DevInfo, VCOM, register reads) so MISO is required.
    spi.final_validate_device_schema(
        "it8951", require_miso=True, require_mosi=True
    )(config)

    global_config = full_config.get()
    from esphome.components.lvgl import DOMAIN as LVGL_DOMAIN

    if CONF_LAMBDA not in config and CONF_PAGES not in config:
        if LVGL_DOMAIN in global_config:
            if CONF_UPDATE_INTERVAL not in config:
                config[CONF_UPDATE_INTERVAL] = update_interval("never")
        else:
            config[CONF_SHOW_TEST_CARD] = True
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    model = IT8951Model.models[config[CONF_MODEL]]
    width, height = model.get_dimensions(config)
    if width is None or height is None:
        # Generic model with no explicit dimensions: pass 0; the controller
        # overwrites width_/height_ from DevInfo during async setup.
        width = width or 0
        height = height or 0

    var = cg.new_Pvariable(config[CONF_ID], model.name, width, height)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config, write_only=False)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if reset_pin := config.get(CONF_RESET_PIN):
        cg.add(var.set_reset_pin(await cg.gpio_pin_expression(reset_pin)))
    if busy_pin := config.get(CONF_BUSY_PIN):
        cg.add(var.set_busy_pin(await cg.gpio_pin_expression(busy_pin)))
    cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
    if CONF_RESET_DURATION in config:
        cg.add(var.set_reset_duration(config[CONF_RESET_DURATION]))
    cg.add(var.set_reversed(config[CONF_REVERSED]))
    cg.add(var.set_sleep_when_done(config[CONF_SLEEP_WHEN_DONE]))
    cg.add(var.set_vcom(config[CONF_VCOM]))
    cg.add(var.set_force_1bpp(config[CONF_FORCE_1BPP]))
    if CONF_UPDATE_MODE in config:
        cg.add(var.set_update_mode(config[CONF_UPDATE_MODE]))

    transform = config.get(CONF_TRANSFORM, {})
    transform_value = sum(
        flag for key, flag in _TRANSFORM_FLAGS.items() if transform.get(key)
    )
    if transform_value:
        cg.add(var.set_transform(RawExpression(str(transform_value))))


@automation.register_action(
    "it8951.update",
    IT8951UpdateAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(IT8951Display),
            cv.Optional(CONF_MODE): cv.templatable(update_mode),
        }
    ),
    synchronous=True,
)
async def it8951_update_action_to_code(config, action_id, template_arg, args):
    display_var = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, display_var)
    if CONF_MODE in config:
        template_ = await cg.templatable(config[CONF_MODE], args, cg.std_string)
        cg.add(var.set_mode(template_))
    return var
