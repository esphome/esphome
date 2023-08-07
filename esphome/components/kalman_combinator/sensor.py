import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_SOURCE,
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_UNIT_OF_MEASUREMENT,
)
from esphome.core.entity_helpers import inherit_property_from

kalman_combinator_ns = cg.esphome_ns.namespace("kalman_combinator")
KalmanCombinatorComponent = kalman_combinator_ns.class_(
    "KalmanCombinatorComponent", cg.Component, sensor.Sensor
)

CONF_ERROR = "error"
CONF_SOURCES = "sources"
CONF_PROCESS_STD_DEV = "process_std_dev"
CONF_STD_DEV = "std_dev"


CONFIG_SCHEMA = (
    sensor.sensor_schema(KalmanCombinatorComponent)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            cv.Required(CONF_PROCESS_STD_DEV): cv.positive_float,
            cv.Required(CONF_SOURCES): cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_SOURCE): cv.use_id(sensor.Sensor),
                        cv.Required(CONF_ERROR): cv.templatable(cv.positive_float),
                    }
                ),
            ),
            cv.Optional(CONF_STD_DEV): sensor.sensor_schema(),
        }
    )
)

# Inherit some sensor values from the first source, for both the state and the error value
properties_to_inherit = [
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_UNIT_OF_MEASUREMENT,
    # CONF_STATE_CLASS could also be inherited, but might lead to unexpected behaviour with "total_increasing"
]
inherit_schema_for_state = [
    inherit_property_from(property, [CONF_SOURCES, 0, CONF_SOURCE])
    for property in properties_to_inherit
]
inherit_schema_for_std_dev = [
    inherit_property_from([CONF_STD_DEV, property], [CONF_SOURCES, 0, CONF_SOURCE])
    for property in properties_to_inherit
]

FINAL_VALIDATE_SCHEMA = cv.All(
    CONFIG_SCHEMA.extend(
        {cv.Required(CONF_ID): cv.use_id(KalmanCombinatorComponent)},
        extra=cv.ALLOW_EXTRA,
    ),
    *inherit_schema_for_state,
    *inherit_schema_for_std_dev,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_process_std_dev(config[CONF_PROCESS_STD_DEV]))
    for source_conf in config[CONF_SOURCES]:
        source = await cg.get_variable(source_conf[CONF_SOURCE])
        error = await cg.templatable(
            source_conf[CONF_ERROR],
            [(float, "x")],
            cg.float_,
        )
        cg.add(var.add_source(source, error))

    if CONF_STD_DEV in config:
        sens = await sensor.new_sensor(config[CONF_STD_DEV])
        cg.add(var.set_std_dev_sensor(sens))
