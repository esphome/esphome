from esphome.automation import (
    Action,
    Condition,
    maybe_simple_id,
    register_action,
    register_condition,
)
import esphome.codegen as cg
from esphome.components import binary_sensor, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_RESTORE,
    CONF_SENSOR,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_SECOND,
)

CONF_LAST_TIME = "last_time"

duty_time_sensor_ns = cg.esphome_ns.namespace("duty_time_sensor")
DutyTimeSensor = duty_time_sensor_ns.class_(
    "DutyTimeSensor", sensor.Sensor, cg.PollingComponent
)
BaseAction = duty_time_sensor_ns.class_("BaseAction", Action, cg.Parented)
StartAction = duty_time_sensor_ns.class_("StartAction", BaseAction)
StopAction = duty_time_sensor_ns.class_("StopAction", BaseAction)
ResetAction = duty_time_sensor_ns.class_("ResetAction", BaseAction)
SetAction = duty_time_sensor_ns.class_("SetAction", BaseAction)
RunningCondition = duty_time_sensor_ns.class_(
    "RunningCondition", Condition, cg.Parented
)


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        DutyTimeSensor,
        unit_of_measurement=UNIT_SECOND,
        icon="mdi:timer-play-outline",
        accuracy_decimals=3,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        device_class=DEVICE_CLASS_DURATION,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    .extend(
        {
            cv.Optional(CONF_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_LAMBDA): cv.lambda_,
            cv.Optional(CONF_RESTORE, default=False): cv.boolean,
            cv.Optional(CONF_LAST_TIME): sensor.sensor_schema(
                unit_of_measurement=UNIT_SECOND,
                icon="mdi:timer-marker-outline",
                accuracy_decimals=3,
                state_class=STATE_CLASS_TOTAL,
                device_class=DEVICE_CLASS_DURATION,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s")),
    cv.has_at_most_one_key(CONF_SENSOR, CONF_LAMBDA),
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_restore(config[CONF_RESTORE]))
    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(config[CONF_LAMBDA], [], return_type=cg.bool_)
        cg.add(var.set_lambda(lambda_))
    if CONF_LAST_TIME in config:
        sens = await sensor.new_sensor(config[CONF_LAST_TIME])
        cg.add(var.set_last_duty_time_sensor(sens))


# AUTOMATIONS

DUTY_TIME_ID_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(DutyTimeSensor),
    }
)


@register_action("sensor.duty_time.start", StartAction, DUTY_TIME_ID_SCHEMA)
async def sensor_runtime_start_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@register_action("sensor.duty_time.stop", StopAction, DUTY_TIME_ID_SCHEMA)
async def sensor_runtime_stop_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@register_action("sensor.duty_time.reset", ResetAction, DUTY_TIME_ID_SCHEMA)
async def sensor_runtime_reset_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@register_condition(
    "sensor.duty_time.is_running", RunningCondition, DUTY_TIME_ID_SCHEMA
)
async def duty_time_is_running_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, True)


@register_condition(
    "sensor.duty_time.is_not_running", RunningCondition, DUTY_TIME_ID_SCHEMA
)
async def duty_time_is_not_running_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, False)
