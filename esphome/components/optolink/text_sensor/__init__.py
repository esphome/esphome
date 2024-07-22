import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_ENTITY_ID,
    CONF_ID,
    CONF_TYPE,
)
from .. import (
    CONF_DAY_OF_WEEK,
    DAY_OF_WEEK,
    optolink_ns,
    CONF_OPTOLINK_ID,
    SENSOR_BASE_SCHEMA,
)

DEPENDENCIES = ["optolink"]
CODEOWNERS = ["@j0ta29"]

TextSensorType = optolink_ns.enum("TextSensorType")
TYPE = {
    "MAP": TextSensorType.TEXT_SENSOR_TYPE_MAP,
    "RAW": TextSensorType.TEXT_SENSOR_TYPE_RAW,
    "DAY_SCHEDULE": TextSensorType.TEXT_SENSOR_TYPE_DAY_SCHEDULE,
    "DAY_SCHEDULE_SYNCHRONIZED": TextSensorType.TEXT_SENSOR_TYPE_DAY_SCHEDULE_SYNCHRONIZED,
    "DEVICE_INFO": TextSensorType.TEXT_SENSOR_TYPE_DEVICE_INFO,
    "STATE_INFO": TextSensorType.TEXT_SENSOR_TYPE_STATE_INFO,
}

OptolinkTextSensor = optolink_ns.class_(
    "OptolinkTextSensor", text_sensor.TextSensor, cg.PollingComponent
)


def check_address():
    def validator_(config):
        types_address_needed = [
            "MAP",
            "RAW",
            "DAY_SCHEDULE",
            "DAY_SCHEDULE_SYNCHRONIZED",
        ]
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


def check_bytes():
    def validator_(config):
        types_bytes_needed = ["MAP", "RAW", "DAY_SCHEDULE", "DAY_SCHEDULE_SYNCHRONIZED"]
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

        types_bytes_day_schedule = ["DAY_SCHEDULE", "DAY_SCHEDULE_SYNCHRONIZED"]
        if config[CONF_TYPE] in types_bytes_day_schedule and config[CONF_BYTES] not in [
            56
        ]:
            raise cv.Invalid(
                f"{CONF_BYTES} must be 56 for this types: {types_bytes_day_schedule}"
            )

        return config

    return validator_


def check_dow():
    def validator_(config):
        types_dow_needed = ["DAY_SCHEDULE", "DAY_SCHEDULE_SYNCHRONIZED"]
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


def check_entity_id():
    def validator_(config):
        types_entitiy_id_needed = ["DAY_SCHEDULE_SYNCHRONIZED"]
        if (
            config[CONF_TYPE] in types_entitiy_id_needed
            and CONF_ENTITY_ID not in config
        ):
            raise cv.Invalid(
                f"{CONF_ENTITY_ID} is required for this types: {types_entitiy_id_needed}"
            )
        if (
            config[CONF_TYPE] not in types_entitiy_id_needed
            and CONF_ENTITY_ID in config
        ):
            raise cv.Invalid(
                f"{CONF_ENTITY_ID} is only allowed for this types: {types_entitiy_id_needed}"
            )
        return config

    return validator_


CONFIG_SCHEMA = cv.All(
    text_sensor.TEXT_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(OptolinkTextSensor),
            cv.Required(CONF_TYPE): cv.enum(TYPE, upper=True),
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

    cg.add(var.set_type(config[CONF_TYPE]))
    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
    if CONF_BYTES in config:
        cg.add(var.set_bytes(config[CONF_BYTES]))
    if CONF_DAY_OF_WEEK in config:
        cg.add(var.set_day_of_week(config[CONF_DAY_OF_WEEK]))
    if CONF_ENTITY_ID in config:
        cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
