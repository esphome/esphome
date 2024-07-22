from esphome import core
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_ID,
    CONF_LOGGER,
    CONF_PROTOCOL,
    CONF_RX_PIN,
    CONF_TX_PIN,
    CONF_TYPE,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import CORE

CODEOWNERS = ["@j0ta29"]
DEPENDENCIES = ["sensor"]
AUTO_LOAD = []
MULTI_CONF = False

optolink_ns = cg.esphome_ns.namespace("optolink")
CONF_OPTOLINK_ID = "optolink_id"

DAY_OF_WEEK = {
    "MONDAY": 0,
    "TUESDAY": 1,
    "WEDNESDAY": 2,
    "THURSDAY": 3,
    "FRIDAY": 4,
    "SATURDAY": 5,
    "SUNDAY": 6,
}
CONF_DAY_OF_WEEK = "day_of_week"


def check_address_for_types(types_address_needed):
    def validator_(config):
        address_needed = config[CONF_TYPE] in types_address_needed
        address_defined = CONF_ADDRESS in config
        if address_needed and not address_defined:
            raise cv.Invalid(
                f"{CONF_ADDRESS} is required for this types: {types_address_needed}"
            )
        if not address_needed and address_defined:
            raise cv.Invalid(
                f"{CONF_ADDRESS} is only allowed for this types: {types_address_needed}"
            )
        return config

    return validator_


def check_bytes_for_types(types_bytes_needed):
    def validator_(config):
        bytes_needed = config[CONF_TYPE] in types_bytes_needed
        bytes_defined = CONF_BYTES in config
        if bytes_needed and not bytes_defined:
            raise cv.Invalid(
                f"{CONF_BYTES} is required for this types: {types_bytes_needed}"
            )
        if not bytes_needed and bytes_defined:
            raise cv.Invalid(
                f"{CONF_BYTES} is only allowed for this types: {types_bytes_needed}"
            )

        types_bytes_range_1_to_9 = ["MAP", "RAW"]
        if config[CONF_TYPE] in types_bytes_range_1_to_9 and config[
            CONF_BYTES
        ] not in range(0, 10):
            raise cv.Invalid(
                f"{CONF_BYTES} must be between 1 and 9 for this types: {types_bytes_range_1_to_9}"
            )

        types_bytes_day_schedule = ["DAY_SCHEDULE"]
        if config[CONF_TYPE] in types_bytes_day_schedule and config[CONF_BYTES] not in [
            56
        ]:
            raise cv.Invalid(
                f"{CONF_BYTES} must be 56 for this types: {types_bytes_day_schedule}"
            )

        return config

    return validator_


def check_dow_for_types(types_dow_needed):
    def validator_(config):
        if config[CONF_TYPE] in types_dow_needed and CONF_DAY_OF_WEEK not in config:
            raise cv.Invalid(
                f"{CONF_DAY_OF_WEEK} is required for this types: {types_dow_needed}"
            )
        if config[CONF_TYPE] not in types_dow_needed and CONF_DAY_OF_WEEK in config:
            raise cv.Invalid(
                f"{CONF_DAY_OF_WEEK} is only allowed for this types: {types_dow_needed}"
            )
        return config

    return validator_


OptolinkComponent = optolink_ns.class_("Optolink", cg.Component)
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
                pins.internal_gpio_input_pin_number,
            ),
            cv.Optional(CONF_TX_PIN): cv.All(
                cv.only_on_esp32,
                pins.internal_gpio_input_pin_number,
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
    cg.add_library("VitoWiFi", "1.1.2")

    cg.add_define(
        "USE_OPTOLINK_VITOWIFI_PROTOCOL",
        cg.RawExpression(f"Optolink{config[CONF_PROTOCOL]}"),
    )

    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_logger_enabled(config[CONF_LOGGER]))

    if CORE.is_esp32:
        cg.add(var.set_rx_pin(config[CONF_RX_PIN]))
        cg.add(var.set_tx_pin(config[CONF_TX_PIN]))

    await cg.register_component(var, config)
