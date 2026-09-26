from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt
from esphome.components.one_wire import OneWireBus
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN
from esphome.types import ConfigType

from .. import gpio_ns

CODEOWNERS = ["@ssieb"]

CONF_USE_RMT: str = "use_rmt"
RMT_DEFINE: str = "USE_ONE_WIRE_RMT"

GPIOOneWireBus = gpio_ns.class_("GPIOOneWireBus", OneWireBus, cg.Component)


def _validate_use_rmt(value: bool) -> bool:
    if not value:
        return value

    cv.only_on_esp32(value)
    esp32_rmt.validate_rmt_not_supported([CONF_USE_RMT])({CONF_USE_RMT: value})
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GPIOOneWireBus),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_USE_RMT, default=False): cv.All(
            cv.boolean,
            _validate_use_rmt,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    use_rmt = config[CONF_USE_RMT]

    if use_rmt:
        esp32.include_builtin_idf_component("esp_driver_rmt")
        cg.add_define(RMT_DEFINE)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_use_rmt(use_rmt))


# The GPIO implementation is portable and must always be compiled. Only the
# ESP32-specific RMT transport is conditional on an RMT-enabled bus existing.
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {
        "gpio_one_wire_rmt.cpp": RMT_DEFINE,
    }
)
