from esphome import core
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIV_RATIO,
    CONF_ID,
    CONF_LOGGER,
    CONF_PROTOCOL,
    CONF_RX_PIN,
    CONF_TX_PIN,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import CORE

CODEOWNERS = ["@j0ta29"]
DEPENDENCIES = ["text_sensor"]
AUTO_LOAD = []
MULTI_CONF = False

optolink_ns = cg.esphome_ns.namespace("optolink")
CONF_OPTOLINK_ID = "optolink_id"

OptolinkComponent = optolink_ns.class_("Optolink", cg.Component)
CONF_OPTOLINK_ID = "optolink_id"
SENSOR_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPTOLINK_ID): cv.use_id(OptolinkComponent),
        cv.Optional(CONF_UPDATE_INTERVAL, default="10s"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=core.TimePeriod(seconds=1), max=core.TimePeriod(seconds=1800)),
        ),
        cv.Optional(CONF_DIV_RATIO, default=1): cv.one_of(
            1, 10, 100, 1000, 3600, int=True
        ),
    }
)


def required_on_esp32(attribute):
    """Validate that this option can only be specified on the given target platforms."""

    def validator_(config):
        if CORE.is_esp32 and attribute not in config:
            raise cv.Invalid(f"{attribute} is required on esp32")
        return config

    return validator_


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OptolinkComponent),
            cv.Required(CONF_PROTOCOL): cv.one_of("P300", "KW"),
            cv.Optional(CONF_RX_PIN): cv.All(
                cv.only_on_esp32,
                pins.internal_gpio_input_pin_schema,
            ),
            cv.Optional(CONF_TX_PIN): cv.All(
                cv.only_on_esp32,
                pins.internal_gpio_output_pin_schema,
            ),
            cv.Optional(CONF_LOGGER, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
    cv.only_on(["esp32", "esp8266"]),
    required_on_esp32(CONF_RX_PIN),
    required_on_esp32(CONF_TX_PIN),
)


async def to_code(config):
    cg.add_library("VitoWiFi", "1.0.2")

    cg.add_define(
        "VITOWIFI_PROTOCOL", cg.RawExpression(f"Optolink{config[CONF_PROTOCOL]}")
    )

    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_logger_enabled(config[CONF_LOGGER]))

    if CORE.is_esp32:
        cg.add(var.set_rx_pin(config[CONF_RX_PIN]["number"]))
        cg.add(var.set_tx_pin(config[CONF_TX_PIN]["number"]))

    await cg.register_component(var, config)
