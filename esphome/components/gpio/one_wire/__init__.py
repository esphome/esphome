from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt
from esphome.components.one_wire import OneWireBus
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INVERTED, CONF_PIN
from esphome.core import CORE
from esphome.types import ConfigType

from .. import gpio_ns

CODEOWNERS = ["@ssieb"]

CONF_USE_RMT = "use_rmt"
CONF_RMT_TIMING = "rmt_timing"

# Option, with _us also its field in rmt_one_wire::Timing: default, minimum, maximum in us
RMT_TIMING = {
    "write0_low": (90, 60, 105),
    "slot": (110, 71, 120),
    "read_threshold": (13, 10, 25),
}

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
    if config[CONF_PIN][CONF_INVERTED]:
        raise cv.Invalid("use_rmt does not support inverted pins", path=[CONF_PIN])
    # The driver facts behind the bus are only checked for this ESP-IDF, which Arduino 3.3.0 brings along
    cv.require_framework_version(
        esp_idf=cv.Version(5, 5, 0), esp32_arduino=cv.Version(3, 3, 0)
    )(config)
    config[CONF_ID].type = GPIORMTOneWireBus
    return config


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


async def to_code(config: ConfigType) -> None:
    args = []
    if timing := config.get(CONF_RMT_TIMING):
        fields = ((f"{key}_us", timing[key].total_microseconds) for key in RMT_TIMING)
        args.append(cg.StructInitializer(Timing, *fields))
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await cg.register_component(var, config)

    await cg.add_pin(var.set_pin, config[CONF_PIN])

    if config[CONF_USE_RMT]:
        cg.add_define("USE_ONE_WIRE_RMT")
        esp32.include_builtin_idf_component("esp_driver_rmt")
