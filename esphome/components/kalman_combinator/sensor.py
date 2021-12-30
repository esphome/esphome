import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_SOURCE

kalman_combinator_ns = cg.esphome_ns.namespace("kalman_combinator")
KalmanCombinatorComponent = kalman_combinator_ns.class_(
    "KalmanCombinatorComponent", cg.Component, sensor.Sensor
)

CONF_ERROR = "error"
CONF_ERROR_FUNCTION = "error_function"
CONF_SOURCES = "sources"
CONF_PROCESS_STD_DEV = "process_std_dev"
CONF_STD_DEV = "std_dev"

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend(cv.COMPONENT_SCHEMA).extend(
    {
        cv.GenerateID(): cv.declare_id(KalmanCombinatorComponent),
        cv.Required(CONF_PROCESS_STD_DEV): cv.positive_float,
        cv.Required(CONF_SOURCES): cv.ensure_list(
            cv.All(
                cv.Schema(
                    {
                        cv.Required(CONF_SOURCE): cv.use_id(sensor.Sensor),
                        cv.Optional(CONF_ERROR): cv.positive_float,
                        cv.Optional(CONF_ERROR_FUNCTION): cv.returning_lambda,
                    }
                ),
                cv.has_exactly_one_key(CONF_ERROR, CONF_ERROR_FUNCTION),
            )
        ),
        cv.Optional(CONF_STD_DEV): sensor.SENSOR_SCHEMA,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    for source_conf in config[CONF_SOURCES]:
        source = await cg.get_variable(source_conf[CONF_SOURCE])
        if CONF_ERROR_FUNCTION in source_conf:
            error = await cg.process_lambda(
                source_conf[CONF_ERROR_FUNCTION],
                [(float, "x")],
                return_type=cg.float_,
            )
        else:
            error = source_conf[CONF_ERROR]
        cg.add(var.add_source(source, error))

    if CONF_STD_DEV in config:
        sens = await sensor.new_sensor(config[CONF_STD_DEV])
        cg.add(var.set_std_dev_sensor(sens))
