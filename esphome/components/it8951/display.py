"""
ESPHome configuration for the IT8951 e-paper controller.
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
    CONF_ENABLE_PIN,
    CONF_FULL_UPDATE_EVERY,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODE,
    CONF_MODEL,
    CONF_PAGES,
    CONF_RESET_DURATION,
    CONF_RESET_PIN,
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
CONF_VCOM_REGISTER = "vcom_register"
CONF_FORCE_TEMPERATURE = "force_temperature"
CONF_GRAYSCALE = "grayscale"
CONF_DITHERING = "dithering"
CONF_UPDATE_MODE = "update_mode"
CONF_USE_LEGACY_DPY_AREA = "use_legacy_dpy_area"

# VCOM SET sub-command selectors. The IT8951 firmware accepts different
# values across panels; most respond to 0x0001, but a few — e.g. the Seeed
# reTerminal E1003 — only respond to 0x0002 and silently drop 0x0001.
VCOM_REGISTER_DEFAULT = 0x0001
VCOM_REGISTER_ALT = 0x0002
VCOM_REGISTER_OPTIONS = (VCOM_REGISTER_DEFAULT, VCOM_REGISTER_ALT)

it8951_ns = cg.esphome_ns.namespace("it8951")
IT8951Display = it8951_ns.class_("IT8951Display", display.Display, spi.SPIDevice)
IT8951UpdateAction = it8951_ns.class_("IT8951UpdateAction", automation.Action)
IT8951LoadAction = it8951_ns.class_("IT8951LoadAction", automation.Action)
IT8951RefreshAction = it8951_ns.class_("IT8951RefreshAction", automation.Action)

# Hardware waveform modes exposed to YAML. Strings are mapped to the C++
# UpdateMode enum so the runtime can store the mode as a uint16_t rather
# than a std::string (avoiding a heap-resident member; see ESPHome
# CLAUDE.md "STL Container Guidelines"). "fast" and "full" are
# convenience aliases for DU and GC16 respectively.
UpdateMode = it8951_ns.enum("UpdateMode")
UPDATE_MODE_OPTIONS = {
    "INIT": UpdateMode.UPDATE_MODE_INIT,
    "DU": UpdateMode.UPDATE_MODE_DU,
    "GC16": UpdateMode.UPDATE_MODE_GC16,
    "GL16": UpdateMode.UPDATE_MODE_GL16,
    "GLR16": UpdateMode.UPDATE_MODE_GLR16,
    "GLD16": UpdateMode.UPDATE_MODE_GLD16,
    "DU4": UpdateMode.UPDATE_MODE_DU4,
    "A2": UpdateMode.UPDATE_MODE_A2,
    "FAST": UpdateMode.UPDATE_MODE_DU,
    "FULL": UpdateMode.UPDATE_MODE_GC16,
}
# Maps the YAML mode string directly to the C++ UpdateMode enum value, so the
# config option and the it8951.update action share one validator.
update_mode = cv.enum(UPDATE_MODE_OPTIONS, upper=True)

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

    def get_dimensions(self, config) -> tuple[int, int]:
        # If dimensions are in config, use them; otherwise fall back to model defaults.
        if CONF_DIMENSIONS in config:
            dimensions = config[CONF_DIMENSIONS]
            if isinstance(dimensions, dict):
                return dimensions[CONF_WIDTH], dimensions[CONF_HEIGHT]
            return tuple(dimensions)
        # Model must have defaults if dimensions not in config.
        return self.get_default(CONF_WIDTH), self.get_default(CONF_HEIGHT)


# --- Model presets ----------------------------------------------------------
# The generic model leaves dimensions and pin choices up to the user.
IT8951Model("it8951", vcom=2300, sleep_when_done=True, data_rate=12_000_000)

IT8951Model(
    "m5stack-m5paper",
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
    # Board power-enable rails: 1.8V logic supply (GPIO21) and the EPD supply
    # (GPIO11). Driven high during setup so no separate power_supply is needed.
    enable_pin=[21, 11],
    vcom=1400,
    # reTerminal E1003 panel firmware only accepts the 0x0002 VCOM SET
    # selector; using the default 0x0001 leaves VCOM unchanged and breaks
    # grayscale waveforms (GC16/GL16) — INIT still works because it does
    # not depend on VCOM accuracy.
    vcom_register=VCOM_REGISTER_ALT,
    # The reTerminal E1003 ships with on-die temperature sensing disabled,
    # so the host must declare an operating temperature; otherwise the
    # waveform LUT defaults to a value that produces no visible change
    # for grayscale modes.
    force_temperature=25,
    sleep_when_done=False,
    data_rate=20_000_000,
    mirror_x=True,
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
    has_default_dimensions = (
        model.get_default(CONF_WIDTH) is not None
        and model.get_default(CONF_HEIGHT) is not None
    )
    dimensions_key = (
        cv.Optional(
            CONF_DIMENSIONS,
            default={
                CONF_WIDTH: model.get_default(CONF_WIDTH),
                CONF_HEIGHT: model.get_default(CONF_HEIGHT),
            },
        )
        if has_default_dimensions
        else cv.Required(CONF_DIMENSIONS)
    )

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
            cv.Optional(CONF_UPDATE_INTERVAL, default=cv.UNDEFINED): update_interval,
            cv.Optional(CONF_FULL_UPDATE_EVERY, default=30): cv.int_range(1, 255),
            cv.Optional(CONF_TRANSFORM): cv.Schema(
                {
                    cv.Required(CONF_MIRROR_X): cv.boolean,
                    cv.Required(CONF_MIRROR_Y): cv.boolean,
                    cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
                }
            ),
            cv.Optional(
                CONF_INVERT_COLORS, default=model.get_default(CONF_INVERT_COLORS, False)
            ): cv.boolean,
            cv.Optional(
                CONF_SLEEP_WHEN_DONE,
                default=model.get_default(CONF_SLEEP_WHEN_DONE, False),
            ): cv.boolean,
            # Pixel format: true = 4bpp grayscale, false = packed 1bpp
            # monochrome. Monochrome halves the framebuffer and enables fast DU
            # partial refreshes; grayscale gives 16 levels but always uses GC16.
            cv.Optional(
                CONF_GRAYSCALE, default=model.get_default(CONF_GRAYSCALE, True)
            ): cv.boolean,
            # Monochrome only: ordered-dither pale colours so they render as
            # visible stipple. Disable for a crisp hard black/white threshold
            # (better for purely black/white text). No effect in grayscale mode.
            cv.Optional(
                CONF_DITHERING, default=model.get_default(CONF_DITHERING, True)
            ): cv.boolean,
            cv.Optional(
                CONF_VCOM, default=model.get_default(CONF_VCOM, 2300)
            ): cv.int_range(0, 5000),
            cv.Optional(
                CONF_VCOM_REGISTER,
                default=model.get_default(CONF_VCOM_REGISTER, VCOM_REGISTER_DEFAULT),
            ): cv.one_of(*VCOM_REGISTER_OPTIONS, int=True),
            **(
                {
                    cv.Optional(
                        CONF_FORCE_TEMPERATURE,
                        default=model.get_default(CONF_FORCE_TEMPERATURE),
                    ): cv.int_range(min=-40, max=85)
                }
                if model.get_default(CONF_FORCE_TEMPERATURE) is not None
                else {}
            ),
            cv.Optional(
                CONF_USE_LEGACY_DPY_AREA,
                default=model.get_default(CONF_USE_LEGACY_DPY_AREA, False),
            ): cv.boolean,
            cv.Optional(CONF_UPDATE_MODE): update_mode,
            # One or more GPIOs driven high during setup to power on the panel
            # (e.g. board power-enable rails), before reset and init.
            cv.Optional(
                CONF_ENABLE_PIN, default=model.get_default(CONF_ENABLE_PIN, [])
            ): cv.ensure_list(pins.gpio_output_pin_schema),
            cv.Optional(CONF_RESET_DURATION): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(max=core.TimePeriod(milliseconds=500)),
            ),
            dimensions_key: DIMENSION_SCHEMA,
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
        {
            cv.Required(CONF_MODEL): cv.one_of(
                *IT8951Model.models, upper=True, space="-"
            )
        },
        extra=cv.ALLOW_EXTRA,
    )(config)

    model_config = _model_schema(config)(config)

    model = IT8951Model.models[config[CONF_MODEL].upper()]
    width, height = model.get_dimensions(model_config)

    display.add_metadata(
        model_config[CONF_ID],
        width,
        height,
        # Rotation is applied per-pixel in draw_pixel_at at no extra cost, so we
        # advertise hardware rotation: LVGL routes its rotation to the driver via
        # set_rotation rather than rotating the framebuffer in software.
        has_hardware_rotation=True,
        has_writer=any(
            model_config.get(key)
            for key in (CONF_LAMBDA, CONF_PAGES, CONF_SHOW_TEST_CARD)
        ),
        # Report the configured rotation so LVGL can detect (and reject) a
        # rotation set in the display config instead of the LVGL config.
        rotation=model_config.get(CONF_ROTATION, 0),
        # The IT8951 snaps partial display refreshes to a 32-pixel X boundary
        # (see prepare_update_region_), so have LVGL round its redraw areas to
        # 32px too — this keeps flush rectangles aligned with what the panel
        # actually refreshes and avoids redundant re-rounding/over-draw.
        draw_rounding=32,
    )

    return model_config


CONFIG_SCHEMA = _customise_schema


def _final_validate(config):
    # IT8951 reads from SPI (DevInfo, VCOM, register reads) so MISO is required.
    spi.final_validate_device_schema("it8951", require_miso=True, require_mosi=True)(
        config
    )

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

    var = cg.new_Pvariable(config[CONF_ID], model.name, width, height)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config, write_only=False)

    if lambda_config := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lambda_config, [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if reset_pin := config.get(CONF_RESET_PIN):
        cg.add(var.set_reset_pin(await cg.gpio_pin_expression(reset_pin)))
    if busy_pin := config.get(CONF_BUSY_PIN):
        cg.add(var.set_busy_pin(await cg.gpio_pin_expression(busy_pin)))
    if enable_pins := config.get(CONF_ENABLE_PIN):
        cg.add(
            var.set_enable_pins(
                [await cg.gpio_pin_expression(pin) for pin in enable_pins]
            )
        )
    cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
    if (reset_duration := config.get(CONF_RESET_DURATION)) is not None:
        cg.add(var.set_reset_duration(reset_duration))
    if config.get(CONF_INVERT_COLORS):
        cg.add(var.set_invert_colors(True))
    if config.get(CONF_SLEEP_WHEN_DONE):
        cg.add(var.set_sleep_when_done(True))
    cg.add(var.set_vcom(config[CONF_VCOM]))
    cg.add(var.set_vcom_register(config[CONF_VCOM_REGISTER]))
    if CONF_FORCE_TEMPERATURE in config:
        cg.add(var.set_force_temperature(config[CONF_FORCE_TEMPERATURE]))
    if config.get(CONF_USE_LEGACY_DPY_AREA):
        cg.add(var.set_use_legacy_dpy_area(True))
    cg.add(var.set_grayscale(config[CONF_GRAYSCALE]))
    cg.add(var.set_dithering(config[CONF_DITHERING]))
    if (mode := config.get(CONF_UPDATE_MODE)) is not None:
        cg.add(var.set_update_mode(mode))

    transform = config.get(
        CONF_TRANSFORM,
        {
            CONF_MIRROR_X: model.get_default(CONF_MIRROR_X),
            CONF_MIRROR_Y: model.get_default(CONF_MIRROR_Y),
        },
    )

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
    if mode := config.get(CONF_MODE):
        mode = await cg.templatable(mode, args, UpdateMode)
        cg.add(var.set_mode(mode))
    return var


@automation.register_action(
    "it8951.load",
    IT8951LoadAction,
    automation.maybe_simple_id({cv.Required(CONF_ID): cv.use_id(IT8951Display)}),
    synchronous=True,
)
async def it8951_load_action_to_code(config, action_id, template_arg, args):
    display_var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, display_var)


@automation.register_action(
    "it8951.refresh",
    IT8951RefreshAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(IT8951Display),
            cv.Optional(CONF_MODE): cv.templatable(update_mode),
        }
    ),
    synchronous=True,
)
async def it8951_refresh_action_to_code(config, action_id, template_arg, args):
    display_var = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, display_var)
    if mode := config.get(CONF_MODE):
        mode = await cg.templatable(mode, args, UpdateMode)
        cg.add(var.set_mode(mode))
    return var
