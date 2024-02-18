import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_ENTITY_ID,
    CONF_ID,
    CONF_MODE,
)
from .. import optolink_ns, CONF_OPTOLINK_ID, SENSOR_BASE_SCHEMA

DEPENDENCIES = ["optolink", "api"]
CODEOWNERS = ["@j0ta29"]

TextSensorMode = optolink_ns.enum("TextSensorMode")
MODE = {
    "MAP": TextSensorMode.MAP,
    "RAW": TextSensorMode.RAW,
    "DAY_SCHEDULE": TextSensorMode.DAY_SCHEDULE,
    "DAY_SCHEDULE_SYNCHRONIZED": TextSensorMode.DAY_SCHEDULE_SYNCHRONIZED,
    "DEVICE_INFO": TextSensorMode.DEVICE_INFO,
    "STATE_INFO": TextSensorMode.STATE_INFO,
}
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

OptolinkTextSensor = optolink_ns.class_(
    "OptolinkTextSensor", text_sensor.TextSensor, cg.PollingComponent
)


def check_address():
    def validator_(config):
        modes_address_needed = [
            "MAP",
            "RAW",
            "DAY_SCHEDULE",
            "DAY_SCHEDULE_SYNCHRONIZED",
        ]
        address_needed = config[CONF_MODE] in modes_address_needed
        address_defined = CONF_ADDRESS in config
        if address_needed and not address_defined:
            raise cv.Invalid(
                f"{CONF_ADDRESS} is required in this modes: {modes_address_needed}"
            )
        if not address_needed and address_defined:
            raise cv.Invalid(
                f"{CONF_ADDRESS} is only allowed in this modes mode: {modes_address_needed}"
            )
        return config

    return validator_


def check_bytes():
    def validator_(config):
        modes_bytes_needed = ["MAP", "RAW", "DAY_SCHEDULE", "DAY_SCHEDULE_SYNCHRONIZED"]
        bytes_needed = config[CONF_MODE] in modes_bytes_needed
        bytes_defined = CONF_BYTES in config
        if bytes_needed and not bytes_defined:
            raise cv.Invalid(
                f"{CONF_BYTES} is required in this modes: {modes_bytes_needed}"
            )
        if not bytes_needed and bytes_defined:
            raise cv.Invalid(
                f"{CONF_BYTES} is only allowed in this modes: {modes_bytes_needed}"
            )

        modes_bytes_range_1_to_9 = ["MAP", "RAW"]
        if config[CONF_MODE] in modes_bytes_range_1_to_9 and config[
            CONF_BYTES
        ] not in range(0, 10):
            raise cv.Invalid(
                f"{CONF_BYTES} must be between 1 and 9 for this modes: {modes_bytes_range_1_to_9}"
            )

        modes_bytes_day_schedule = ["DAY_SCHEDULE", "DAY_SCHEDULE_SYNCHRONIZED"]
        if config[CONF_MODE] in modes_bytes_day_schedule and config[CONF_BYTES] not in [
            56
        ]:
            raise cv.Invalid(
                f"{CONF_BYTES} must be 56 for this modes: {modes_bytes_day_schedule}"
            )

        return config

    return validator_


def check_dow():
    def validator_(config):
        modes_dow_needed = ["DAY_SCHEDULE", "DAY_SCHEDULE_SYNCHRONIZED"]
        if config[CONF_MODE] in modes_dow_needed and CONF_DAY_OF_WEEK not in config:
            raise cv.Invalid(
                f"{CONF_DAY_OF_WEEK} is required in this modes: {modes_dow_needed}"
            )
        if config[CONF_MODE] not in modes_dow_needed and CONF_DAY_OF_WEEK in config:
            raise cv.Invalid(
                f"{CONF_DAY_OF_WEEK} is only allowed in this modes: {modes_dow_needed}"
            )
        return config

    return validator_


def check_entity_id():
    def validator_(config):
        modes_entitiy_id_needed = ["DAY_SCHEDULE_SYNCHRONIZED"]
        if (
            config[CONF_MODE] in modes_entitiy_id_needed
            and CONF_ENTITY_ID not in config
        ):
            raise cv.Invalid(
                f"{CONF_ENTITY_ID} is required in this modes: {modes_entitiy_id_needed}"
            )
        if (
            config[CONF_MODE] not in modes_entitiy_id_needed
            and CONF_ENTITY_ID in config
        ):
            raise cv.Invalid(
                f"{CONF_ENTITY_ID} is only allowed in this modes: {modes_entitiy_id_needed}"
            )
        return config

    return validator_


CONFIG_SCHEMA = cv.All(
    text_sensor.TEXT_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(OptolinkTextSensor),
            cv.Required(CONF_MODE): cv.enum(MODE, upper=True),
            cv.Optional(CONF_ADDRESS): cv.hex_uint32_t,
            cv.Optional(CONF_BYTES): cv.uint8_t,
            cv.Optional(CONF_DAY_OF_WEEK): cv.enum(DAY_OF_WEEK, upper=True),
            cv.Optional(CONF_ENTITY_ID): cv.entity_id,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(SENSOR_BASE_SCHEMA),
    check_address(),
    check_bytes(),
    check_dow(),
    check_entity_id(),
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)

    cg.add(var.set_mode(config[CONF_MODE]))
    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
    if CONF_BYTES in config:
        cg.add(var.set_bytes(config[CONF_BYTES]))
    if CONF_DAY_OF_WEEK in config:
        cg.add(var.set_day_of_week(config[CONF_DAY_OF_WEEK]))
    if CONF_ENTITY_ID in config:
        cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
