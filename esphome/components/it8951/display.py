"""
ESPHome configuration for the IT8951 e-paper controller.
"""

from collections.abc import Callable
from typing import Any

from esphome import automation, core, pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.display import CONF_SHOW_TEST_CARD, validate_rotation
from esphome.components.mipi import requires_buffer
import esphome.config_validation as cv
from esphome.config_validation import update_interval
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
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
from esphome.core import ID
from esphome.cpp_generator import MockObj, RawExpression, TemplateArgsType
from esphome.final_validate import full_config
from esphome.types import ConfigType

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
IT8951DirectDisplay = it8951_ns.class_("IT8951DirectDisplay", IT8951Display)
IT8951UpdateAction = it8951_ns.class_("IT8951UpdateAction", automation.Action)
IT8951PauseAction = it8951_ns.class_("IT8951PauseAction", automation.Action)
IT8951ResumeAction = it8951_ns.class_("IT8951ResumeAction", automation.Action)
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

    def __init__(self, name: str, **defaults: Any) -> None:
        name = name.upper()
        self.name = name
        self.defaults = defaults
        IT8951Model.models[name] = self

    def get_default(self, key: str, fallback: Any = None) -> Any:
        return self.defaults.get(key, fallback)

    def get_dimensions(self, config: ConfigType) -> tuple[int, int]:
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


def _model_pin_option(
    model: IT8951Model, key: str, schema: Callable[[Any], Any]
) -> tuple[cv.Optional | cv.Required, Callable[[Any], Any]]:
    default = model.get_default(key)
    if default is None:
        return cv.Required(key), schema
    return cv.Optional(key, default=default), schema


def _model_schema(config: ConfigType) -> cv.Schema:
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


def _customise_schema(config: ConfigType) -> ConfigType:
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

    buffered = requires_buffer(model_config)
    display.add_metadata(
        model_config[CONF_ID],
        width,
        height,
        # With a framebuffer, rotation is applied per-pixel in draw_pixel_at at no
        # extra cost, so we advertise hardware rotation and LVGL routes its
        # rotation to the driver via set_rotation. Direct draw has no framebuffer
        # to rotate through — transposing a flush rectangle would need a
        # chunk-sized scratch buffer, which is exactly what LVGL's own software
        # (or ESP32-P4 PPA) rotation already provides — so it leaves rotation to
        # LVGL. Only LVGL reads this flag, and a config with neither a writer nor
        # LVGL gets the test card from _final_validate, so the value is never
        # consulted in the one case where it would be stale.
        has_hardware_rotation=buffered,
        # auto_clear_enabled calls clear() from do_update_ whether or not a writer
        # exists, so it counts as a writer for LVGL's purposes — the same term as
        # mipi_spi, mipi_dsi, mipi_rgb and display's own fallback metadata.
        has_writer=buffered or model_config.get(CONF_AUTO_CLEAR_ENABLED) is True,
        # Report the configured rotation so LVGL can detect (and reject) a
        # rotation set in the display config instead of the LVGL config.
        rotation=model_config.get(CONF_ROTATION, 0),
        # What the hardware actually requires of a LOAD is a 4-pixel boundary in
        # 4bpp, or 16 for the 8bpp-packed monochrome trick. The 32-pixel X snap
        # that partial REFRESH needs is applied separately in
        # prepare_update_region_ and only on X, so asking LVGL for 32 here would
        # round both axes and inflate every redraw: a one-pixel text change would
        # become 32x32 of converted pixels. 16 satisfies both pixel formats and
        # survives mirroring (see _final_validate).
        draw_rounding=16,
    )

    return model_config


CONFIG_SCHEMA = _customise_schema


def _final_validate(config: ConfigType) -> None:
    # IT8951 reads from SPI (DevInfo, VCOM, register reads) so MISO is required.
    spi.final_validate_device_schema("it8951", require_miso=True, require_mosi=True)(
        config
    )

    global_config = full_config.get()
    from esphome.components.lvgl import DOMAIN as LVGL_DOMAIN, defines as lv_defines

    if CONF_LAMBDA not in config and CONF_PAGES not in config:
        if LVGL_DOMAIN in global_config:
            if CONF_UPDATE_INTERVAL not in config:
                config[CONF_UPDATE_INTERVAL] = update_interval("never")
        else:
            config[CONF_SHOW_TEST_CARD] = True

    # Everything below applies only to the direct-draw variant.
    if requires_buffer(config):
        return

    # Mirroring maps a rectangle to width - x - w, so the panel width has to be a
    # multiple of the load alignment for a LVGL-aligned rectangle to stay aligned.
    # Every model preset satisfies this; a generic model with hand-entered
    # dimensions need not, and would otherwise drop every flush at runtime.
    transform = config.get(CONF_TRANSFORM)
    mirrored = transform is not None and (
        transform.get(CONF_MIRROR_X) or transform.get(CONF_MIRROR_Y)
    )
    if mirrored:
        model = IT8951Model.models[config[CONF_MODEL]]
        width, _ = model.get_dimensions(config)
        align = 4 if config[CONF_GRAYSCALE] else 16
        if width % align:
            raise cv.Invalid(
                f"Width {width} must be a multiple of {align} to use 'transform' "
                f"without a framebuffer. Remove the mirror, pick a width that is a "
                f"multiple of {align}, or add a 'lambda:' to use the buffered driver.",
                [CONF_DIMENSIONS],
            )

    if transform is not None and transform.get(CONF_SWAP_XY):
        raise cv.Invalid(
            "'swap_xy' is not supported without a framebuffer. Rotate in the LVGL "
            "config instead, or add a 'lambda:' to use the buffered driver.",
            [CONF_TRANSFORM, CONF_SWAP_XY],
        )

    # Direct draw writes into the controller's image memory as LVGL flushes, so a
    # render started while a waveform is in flight overwrites the image the panel
    # is still drawing from, and the driver has to drop it — leaving that part of
    # the screen wrong until something happens to redraw it.
    #
    # Only 'update_when_display_idle' prevents this. Gating the config's own
    # lv_refr_now() calls on is_idle() is not enough, because LVGL's refresh timer
    # renders on its own schedule and nothing in a configuration can hold it back.
    #
    # It does mean LVGL asks for a present at every render end, using the
    # display's default waveform. Use it8951.pause to swallow those requests and
    # it8951.resume to present with the waveform this particular update wants.
    display_id = config[CONF_ID]
    for lvgl_config in global_config.get(LVGL_DOMAIN, []):
        if display_id not in lvgl_config.get(lv_defines.CONF_DISPLAYS, []):
            continue
        if not lvgl_config.get(lv_defines.CONF_UPDATE_WHEN_DISPLAY_IDLE):
            raise cv.Invalid(
                f"The lvgl component driving '{display_id}' must set "
                "'update_when_display_idle: true'. Without a framebuffer, LVGL "
                "would render into the controller's image memory while the panel "
                "is still refreshing from it, and those renders are lost. Use "
                "it8951.pause / it8951.resume to keep control of which waveform "
                "each update uses."
            )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    model = IT8951Model.models[config[CONF_MODEL]]
    width, height = model.get_dimensions(config)

    var_id = config[CONF_ID]
    # A lambda, pages or the test card all call draw_pixel_at in arbitrary order,
    # which cannot be streamed into controller memory as it happens, so those
    # configs need the buffered class. Everything else — in practice LVGL, which
    # pushes whole rectangles — is drawn straight into controller memory.
    var_id.type = IT8951Display if requires_buffer(config) else IT8951DirectDisplay
    var = cg.new_Pvariable(var_id, model.name, width, height)
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
async def it8951_update_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    display_var = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, display_var)
    if mode := config.get(CONF_MODE):
        mode = await cg.templatable(mode, args, UpdateMode)
        cg.add(var.set_mode(mode))
    return var


@automation.register_action(
    "it8951.pause",
    IT8951PauseAction,
    automation.maybe_simple_id({cv.Required(CONF_ID): cv.use_id(IT8951Display)}),
    synchronous=True,
)
async def it8951_pause_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    display_var = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, display_var)


@automation.register_action(
    "it8951.resume",
    IT8951ResumeAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(IT8951Display),
            cv.Optional(CONF_MODE): cv.templatable(update_mode),
        }
    ),
    synchronous=True,
)
async def it8951_resume_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    display_var = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, display_var)
    if mode := config.get(CONF_MODE):
        mode = await cg.templatable(mode, args, UpdateMode)
        cg.add(var.set_mode(mode))
    return var


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
async def it8951_refresh_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    display_var = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, display_var)
    if mode := config.get(CONF_MODE):
        mode = await cg.templatable(mode, args, UpdateMode)
        cg.add(var.set_mode(mode))
    return var
