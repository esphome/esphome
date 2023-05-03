import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import sensor, binary_sensor
from esphome.const import (
    CONF_CHANNELS,
    CONF_VALUE,
    CONF_TYPE,
    ICON_CHECK_CIRCLE_OUTLINE,
    CONF_BINARY_SENSOR,
    CONF_GROUP,
    CONF_SUM,
)

DEPENDENCIES = ["binary_sensor"]

binary_sensor_map_ns = cg.esphome_ns.namespace("binary_sensor_map")
BinarySensorMap = binary_sensor_map_ns.class_(
    "BinarySensorMap", cg.Component, sensor.Sensor
)
SensorMapType = binary_sensor_map_ns.enum("SensorMapType")

CONF_BAYESIAN = "bayesian"
CONF_PRIOR = "prior"
CONF_PROB_GIVEN_TRUE = "prob_given_true"
CONF_PROB_GIVEN_FALSE = "prob_given_false"
CONF_OBSERVATIONS = "observations"

SENSOR_MAP_TYPES = {
    CONF_GROUP: SensorMapType.BINARY_SENSOR_MAP_TYPE_GROUP,
    CONF_SUM: SensorMapType.BINARY_SENSOR_MAP_TYPE_SUM,
    CONF_BAYESIAN: SensorMapType.BINARY_SENSOR_MAP_TYPE_BAYESIAN,
}

entry_one_parameter = {
    cv.Required(CONF_BINARY_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    cv.Required(CONF_VALUE): cv.float_,
}

entry_bayesian_parameters = {
    cv.Required(CONF_BINARY_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    cv.Required(CONF_PROB_GIVEN_TRUE): cv.float_range(min=0, max=1),
    cv.Required(CONF_PROB_GIVEN_FALSE): cv.float_range(min=0, max=1),
}

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_GROUP: sensor.sensor_schema(
            BinarySensorMap,
            icon=ICON_CHECK_CIRCLE_OUTLINE,
            accuracy_decimals=0,
        ).extend(
            {
                cv.Required(CONF_CHANNELS): cv.All(
                    cv.ensure_list(entry_one_parameter), cv.Length(min=1, max=64)
                ),
            }
        ),
        CONF_SUM: sensor.sensor_schema(
            BinarySensorMap,
            icon=ICON_CHECK_CIRCLE_OUTLINE,
            accuracy_decimals=0,
        ).extend(
            {
                cv.Required(CONF_CHANNELS): cv.All(
                    cv.ensure_list(entry_one_parameter), cv.Length(min=1, max=64)
                ),
            }
        ),
        CONF_BAYESIAN: sensor.sensor_schema(
            BinarySensorMap,
            accuracy_decimals=2,
        ).extend(
            {
                cv.Required(CONF_PRIOR): cv.float_range(min=0, max=1),
                cv.Required(CONF_OBSERVATIONS): cv.All(
                    cv.ensure_list(entry_bayesian_parameters), cv.Length(min=1, max=64)
                ),
            }
        ),
    },
    lower=True,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    constant = SENSOR_MAP_TYPES[config[CONF_TYPE]]
    cg.add(var.set_sensor_type(constant))

    if config[CONF_TYPE] == CONF_BAYESIAN:
        cg.add(var.set_bayesian_prior(config[CONF_PRIOR]))

        for obs in config[CONF_OBSERVATIONS]:
            input_var = await cg.get_variable(obs[CONF_BINARY_SENSOR])
            cg.add(
                var.add_channel(
                    input_var, obs[CONF_PROB_GIVEN_TRUE], obs[CONF_PROB_GIVEN_FALSE]
                )
            )
    else:
        for ch in config[CONF_CHANNELS]:
            input_var = await cg.get_variable(ch[CONF_BINARY_SENSOR])
            cg.add(var.add_channel(input_var, ch[CONF_VALUE]))
