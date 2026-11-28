from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt
from esphome.components.const import CONF_DRIVE_STRENGTH, CONF_HOLD_STATE
from esphome.components.one_wire import OneWireBus
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_OUTPUT,
    CONF_PIN,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.core import CORE
from esphome.types import ConfigType

from .. import gpio_ns

CODEOWNERS = ["@ssieb"]

CONF_USE_RMT = "use_rmt"
CONF_RMT_TIMING = "rmt_timing"

# Option, with _us also its field in rmt_one_wire::Timing: (default, minimum, maximum) in us.
# The only defaults a firmware uses: to_code() always passes the Timing to the constructor.
RMT_TIMING: dict[str, tuple[int, int, int]] = {
    "write0_low": (90, 60, 105),
    "slot": (110, 71, 120),
    # A read 1 is Low for up to 4 us + 9 us of rise time: 14 us leaves one tick of margin
    "read_threshold": (14, 10, 25),
}

# GPIORMTOneWireBus::setup() creates one RX, then one TX channel of one memory block each
RMT_CHANNELS: tuple[tuple[esp32_rmt.Direction, int], ...] = (("RX", 1), ("TX", 1))
# GPIORMTOneWireBus::get_setup_priority(): setup_priority::HARDWARE - 1
RMT_SETUP_PRIORITY = 799.0
# The drive strength of a pad after reset, which use_rmt does not change
DEFAULT_DRIVE_STRENGTH = 20.0

GPIOOneWireBus = gpio_ns.class_("GPIOOneWireBus", OneWireBus, cg.Component)
GPIORMTOneWireBus = gpio_ns.class_("GPIORMTOneWireBus", OneWireBus, cg.Component)
Timing = gpio_ns.namespace("rmt_one_wire").struct("Timing")


def _validate_rmt_timing(timing: ConfigType) -> ConfigType:
    write0_low, slot = timing["write0_low"], timing["slot"]
    # Rise time budget of 9 us and 2 us of recovery
    if write0_low.total_microseconds + 11 > slot.total_microseconds:
        raise cv.Invalid(
            f"slot {slot} needs at least write0_low {write0_low} + 11us", path=["slot"]
        )
    return timing


def _validate_use_rmt(config: ConfigType) -> ConfigType:
    if not config[CONF_USE_RMT]:
        if CONF_RMT_TIMING in config:
            raise cv.Invalid("rmt_timing needs use_rmt: true", path=[CONF_RMT_TIMING])
        return config
    if not CORE.is_esp32:
        raise cv.Invalid("use_rmt is only available on ESP32", path=[CONF_USE_RMT])
    esp32_rmt.validate_rmt_not_supported([CONF_USE_RMT])(config)
    pin = config[CONF_PIN]
    if pin[CONF_INVERTED]:
        raise cv.Invalid("use_rmt does not support inverted pins", path=[CONF_PIN])
    _validate_rmt_pin(pin)
    # The driver facts behind the bus are only checked from this ESP-IDF on, which Arduino 3.3.0 brings along
    esp32.require_idf_version(cv.Version(5, 5, 0), f"'{CONF_USE_RMT}'")(config)
    config[CONF_ID].type = GPIORMTOneWireBus
    return esp32_rmt.request_rmt_channels(RMT_CHANNELS, RMT_SETUP_PRIORITY)(config)


def _validate_rmt_pin(pin: ConfigType) -> None:
    """Reject the pin options that the RMT driver does not apply to the pin."""
    mode = pin[CONF_MODE]
    for key in (CONF_PULLUP, CONF_PULLDOWN):
        if mode.get(key):
            raise cv.Invalid(
                f"use_rmt does not apply '{key}' of the pin mode, remove it. The bus "
                "needs an external pullup resistor",
                path=[CONF_PIN, CONF_MODE, key],
            )
    # The ESP32 pin validation rejects output on an input-only pin, such as GPIO34 of the ESP32,
    # but not a mode without output. The TX channel drives the pin in any case.
    if not mode[CONF_OUTPUT]:
        raise cv.Invalid(
            "use_rmt drives the pin, so the pin mode needs output. Remove 'mode' or "
            "use INPUT_OUTPUT_OPEN_DRAIN. Input-only pins cannot be used",
            path=[CONF_PIN, CONF_MODE],
        )
    if (strength := pin.get(CONF_DRIVE_STRENGTH)) is not None and (
        strength != DEFAULT_DRIVE_STRENGTH
    ):
        raise cv.Invalid(
            f"use_rmt does not apply '{CONF_DRIVE_STRENGTH}', remove it to keep the "
            "default of 20mA",
            path=[CONF_PIN, CONF_DRIVE_STRENGTH],
        )
    if pin.get(CONF_HOLD_STATE):
        raise cv.Invalid(
            f"use_rmt does not support '{CONF_HOLD_STATE}'",
            path=[CONF_PIN, CONF_HOLD_STATE],
        )


def _final_validate(config: ConfigType) -> None:
    if config[CONF_USE_RMT]:
        esp32_rmt.final_validate_rmt_channels(config)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GPIOOneWireBus),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_USE_RMT, default=False): cv.boolean,
            cv.Optional(CONF_RMT_TIMING): cv.All(
                cv.Schema(
                    {
                        cv.Optional(option, default=f"{default}us"): cv.All(
                            cv.positive_time_period_microseconds,
                            cv.Range(
                                min=cv.TimePeriod(microseconds=low),
                                max=cv.TimePeriod(microseconds=high),
                            ),
                        )
                        for option, (default, low, high) in RMT_TIMING.items()
                    }
                ),
                _validate_rmt_timing,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_use_rmt,
)

FINAL_VALIDATE_SCHEMA = _final_validate


def _timing_initializer(timing: ConfigType | None) -> cg.StructInitializer:
    """The Timing for the constructor, with the defaults of RMT_TIMING when rmt_timing is not set."""
    fields = (
        (f"{key}_us", default if timing is None else timing[key].total_microseconds)
        for key, (default, _, _) in RMT_TIMING.items()
    )
    return cg.StructInitializer(Timing, *fields)


async def to_code(config: ConfigType) -> None:
    args = []
    if config[CONF_USE_RMT]:
        args.append(_timing_initializer(config.get(CONF_RMT_TIMING)))
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await cg.register_component(var, config)
    if config[CONF_USE_RMT]:
        esp32_rmt.register_rmt_channel(config)

    await cg.add_pin(var.set_pin, config[CONF_PIN])

    if config[CONF_USE_RMT]:
        cg.add_define("USE_ONE_WIRE_RMT")
        esp32.include_builtin_idf_component("esp_driver_rmt")
