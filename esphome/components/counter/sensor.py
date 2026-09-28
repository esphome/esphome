from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINARY_SENSOR,
    CONF_ID,
    CONF_INITIAL_VALUE,
    CONF_RESTORE,
    CONF_SENSOR,
    CONF_VALUE,
    ICON_COUNTER,
)
from esphome.types import ConfigType

counter_ns = cg.esphome_ns.namespace("counter")
CounterSensor = counter_ns.class_("CounterSensor", sensor.Sensor, cg.Component)

# The lowest value is left out because its C++ literal cannot be written portably.
INT64_MAX = 2**63 - 1
COUNTER_RANGE = cv.int_range(min=-INT64_MAX, max=INT64_MAX)
COUNTER_VALUE = cv.templatable(COUNTER_RANGE)


def _not_own_source(config: ConfigType) -> ConfigType:
    if (source := config.get(CONF_SENSOR)) is not None and source.id == config[
        CONF_ID
    ].id:
        raise cv.Invalid("A counter cannot count its own updates", [CONF_SENSOR])
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        CounterSensor,
        icon=ICON_COUNTER,
        accuracy_decimals=0,
    )
    .extend(
        {
            cv.Optional(CONF_RESTORE, default=True): cv.boolean,
            cv.Optional(CONF_INITIAL_VALUE, default=0): COUNTER_RANGE,
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_BINARY_SENSOR): cv.use_id(binary_sensor.BinarySensor),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    _not_own_source,
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID], config[CONF_RESTORE], config[CONF_INITIAL_VALUE]
    )
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    if (source := config.get(CONF_SENSOR)) is not None:
        cg.add(var.count_updates_from(await cg.get_variable(source)))
    if (source := config.get(CONF_BINARY_SENSOR)) is not None:
        cg.add(var.count_true_from(await cg.get_variable(source)))


automation.register_apply_action(
    "counter.set_value",
    cv.maybe_simple_value(
        {
            cv.GenerateID(CONF_ID): cv.use_id(CounterSensor),
            cv.Required(CONF_VALUE): COUNTER_VALUE,
        },
        key=CONF_VALUE,
    ),
    automation.ApplyField(CONF_VALUE, "set_value", cg.int64),
)

automation.register_apply_action(
    "counter.increment",
    automation.maybe_simple_id(
        {
            cv.GenerateID(CONF_ID): cv.use_id(CounterSensor),
            cv.Optional(CONF_VALUE, default=1): COUNTER_VALUE,
        }
    ),
    automation.ApplyField(CONF_VALUE, "increment", cg.int64),
)
