from esphome import automation
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
from esphome.types import ConfigType

CONF_LAST_TIME = "last_time"

duty_time_sensor_ns = cg.esphome_ns.namespace("duty_time_sensor")
DutyTimeSensor = duty_time_sensor_ns.class_(
    "DutyTimeSensor", sensor.Sensor, cg.PollingComponent
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


async def to_code(config: ConfigType) -> None:
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

DUTY_TIME_ID_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(DutyTimeSensor),
    }
)


for _name, _call in (
    ("sensor.duty_time.start", "start()"),
    ("sensor.duty_time.stop", "stop()"),
    ("sensor.duty_time.reset", "reset()"),
):
    automation.register_apply_action(
        _name, DUTY_TIME_ID_SCHEMA, automation.ApplyCall(_call)
    )

automation.register_apply_condition(
    "sensor.duty_time.is_running", DUTY_TIME_ID_SCHEMA, "is_running()"
)
automation.register_apply_condition(
    "sensor.duty_time.is_not_running", DUTY_TIME_ID_SCHEMA, "is_running() == false"
)
