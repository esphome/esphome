import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor
from esphome.const import CONF_ICON, CONF_ID, CONF_SENSOR, CONF_RESTORE
from esphome.core.entity_helpers import inherit_property_from

integration_ns = cg.esphome_ns.namespace("integration")
IntegrationSensor = integration_ns.class_(
    "IntegrationSensor", sensor.Sensor, cg.Component
)
ResetAction = integration_ns.class_("ResetAction", automation.Action)

IntegrationSensorTime = integration_ns.enum("IntegrationSensorTime")
INTEGRATION_TIMES = {
    "ms": IntegrationSensorTime.INTEGRATION_SENSOR_TIME_MILLISECOND,
    "s": IntegrationSensorTime.INTEGRATION_SENSOR_TIME_SECOND,
    "min": IntegrationSensorTime.INTEGRATION_SENSOR_TIME_MINUTE,
    "h": IntegrationSensorTime.INTEGRATION_SENSOR_TIME_HOUR,
    "d": IntegrationSensorTime.INTEGRATION_SENSOR_TIME_DAY,
}
IntegrationMethod = integration_ns.enum("IntegrationMethod")
INTEGRATION_METHODS = {
    "trapezoid": IntegrationMethod.INTEGRATION_METHOD_TRAPEZOID,
    "left": IntegrationMethod.INTEGRATION_METHOD_LEFT,
    "right": IntegrationMethod.INTEGRATION_METHOD_RIGHT,
}

CONF_TIME_UNIT = "time_unit"
CONF_INTEGRATION_METHOD = "integration_method"
CONF_MIN_SAVE_INTERVAL = "min_save_interval"

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(IntegrationSensor),
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_TIME_UNIT): cv.enum(INTEGRATION_TIMES, lower=True),
        cv.Optional(CONF_INTEGRATION_METHOD, default="trapezoid"): cv.enum(
            INTEGRATION_METHODS, lower=True
        ),
        cv.Optional(CONF_RESTORE, default=False): cv.boolean,
        cv.Optional(
            CONF_MIN_SAVE_INTERVAL, default="0s"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(IntegrationSensor),
            cv.Optional(CONF_ICON): cv.icon,
            cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        },
        extra=cv.ALLOW_EXTRA,
    ),
    inherit_property_from(CONF_ICON, CONF_SENSOR),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))
    cg.add(var.set_time(config[CONF_TIME_UNIT]))
    cg.add(var.set_method(config[CONF_INTEGRATION_METHOD]))
    cg.add(var.set_restore(config[CONF_RESTORE]))
    cg.add(var.set_min_save_interval(config[CONF_MIN_SAVE_INTERVAL]))


@automation.register_action(
    "sensor.integration.reset",
    ResetAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(IntegrationSensor),
        }
    ),
)
async def sensor_integration_reset_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
