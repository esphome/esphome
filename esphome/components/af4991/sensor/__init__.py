import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_INVERT,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_TRIGGER_ID,
    CONF_VALUE,
    ICON_ROTATE_RIGHT,
    STATE_CLASS_MEASUREMENT,
    UNIT_STEPS,
)
from .. import CONF_AF4991_ID, af4991_ns, AF4991

DEPENDENCIES = ["af4991"]

AF4991Sensor = af4991_ns.class_("AF4991Sensor", cg.Component, sensor.Sensor)
AF4991SensorSetValueAction = af4991_ns.class_(
    "AF4991SensorSetValueAction", automation.Action
)
AF4991SensorClockwiseTrigger = af4991_ns.class_(
    "AF4991SensorClockwiseTrigger", automation.Trigger
)
AF4991SensorAnticlockwiseTrigger = af4991_ns.class_(
    "AF4991SensorAnticlockwiseTrigger", automation.Trigger
)

CONF_ON_CLOCKWISE = "on_clockwise"
CONF_ON_ANTICLOCKWISE = "on_anticlockwise"
CONF_CLOCKWISE_STEPS_BEFORE_TRIGGER = "clockwise_steps_before_trigger"
CONF_ANTICLOCKWISE_STEPS_BEFORE_TRIGGER = "anticlockwise_steps_before_trigger"

CONF_PUBLISH_INITIAL_VALUE = "publish_initial_value"


def validate_min_max_value(config):
    if CONF_MIN_VALUE in config and CONF_MAX_VALUE in config:
        min_value = config[CONF_MIN_VALUE]
        max_value = config[CONF_MAX_VALUE]

        if min_value >= max_value:
            raise cv.Invalid(
                f"Max value {max_value} must be smaller than min value {min_value}"
            )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_STEPS,
        accuracy_decimals=0,
        icon=ICON_ROTATE_RIGHT,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(AF4991Sensor),
            cv.Required(CONF_AF4991_ID): cv.use_id(AF4991),
            cv.Optional(CONF_INVERT, default=False): cv.boolean,
            cv.Optional(CONF_MIN_VALUE): cv.int_,
            cv.Optional(CONF_MAX_VALUE): cv.int_,
            cv.Optional(CONF_PUBLISH_INITIAL_VALUE, default=False): cv.boolean,
            cv.Optional(
                CONF_CLOCKWISE_STEPS_BEFORE_TRIGGER, default=1
            ): cv.positive_int,
            cv.Optional(
                CONF_ANTICLOCKWISE_STEPS_BEFORE_TRIGGER, default=1
            ): cv.positive_int,
            cv.Optional(CONF_ON_CLOCKWISE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        AF4991SensorClockwiseTrigger
                    )
                }
            ),
            cv.Optional(CONF_ON_ANTICLOCKWISE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        AF4991SensorAnticlockwiseTrigger
                    )
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_min_max_value,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    parent = await cg.get_variable(config[CONF_AF4991_ID])
    cg.add(var.set_parent(parent))

    cg.add(var.set_sensor_invert(config[CONF_INVERT]))
    cg.add(var.set_publish_initial_value(config[CONF_PUBLISH_INITIAL_VALUE]))

    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_max_value(config[CONF_MAX_VALUE]))

    cg.add(
        var.set_clockwise_steps_before_trigger(
            config[CONF_CLOCKWISE_STEPS_BEFORE_TRIGGER]
        )
    )
    cg.add(
        var.set_anticlockwise_steps_before_trigger(
            config[CONF_ANTICLOCKWISE_STEPS_BEFORE_TRIGGER]
        )
    )

    for conf in config.get(CONF_ON_CLOCKWISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_ANTICLOCKWISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


@automation.register_action(
    "sensor.af4991.set_value",
    AF4991SensorSetValueAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(AF4991),
            cv.Required(CONF_VALUE): cv.templatable(cv.int_),
        }
    ),
)
async def sensor_template_publish_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config[CONF_VALUE], args, int)
    cg.add(var.set_value(template_))

    return var
