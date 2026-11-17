import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.tmc22xx import CONF_ANALOG_CURRENT_SCALE
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_CURRENT,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_PERCENT,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import TMC2209Stepper, tmc2209_ns

CONF_ACTUAL_CURRENT = "actual_current"
CONF_MOTOR_LOAD = "motor_load"
CONF_PWM_GRAD_AUTO = "pwm_grad_auto"
CONF_PWM_OFS_AUTO = "pwm_ofs_auto"
CONF_PWM_SCALE_AUTO = "pwm_scale_auto"
CONF_PWM_SCALE_SUM = "pwm_scale_sum"
CONF_STALLGUARD_RESULT = "stallguard_result"
CONF_TMC2209_ID = "tmc2209_id"

TMC2209Sensor = tmc2209_ns.class_("TMC2209Sensor", cg.PollingComponent)

_diagnostic_schema = sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TMC2209Sensor),
        cv.GenerateID(CONF_TMC2209_ID): cv.use_id(TMC2209Stepper),
        cv.Optional(CONF_ACTUAL_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MOTOR_LOAD): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_STALLGUARD_RESULT): _diagnostic_schema,
        cv.Optional(CONF_PWM_SCALE_SUM): _diagnostic_schema,
        cv.Optional(CONF_PWM_SCALE_AUTO): _diagnostic_schema,
        cv.Optional(CONF_PWM_OFS_AUTO): _diagnostic_schema,
        cv.Optional(CONF_PWM_GRAD_AUTO): _diagnostic_schema,
    }
).extend(cv.polling_component_schema("60s"))


def _final_validate(config: ConfigType) -> ConfigType:
    if CONF_ACTUAL_CURRENT not in config:
        return config
    if fv.get_declaring_config(config[CONF_TMC2209_ID]).get(CONF_ANALOG_CURRENT_SCALE):
        raise cv.Invalid(
            f"'{CONF_ACTUAL_CURRENT}' can't be used with '{CONF_ANALOG_CURRENT_SCALE}', "
            "the current then depends on VREF",
            path=[CONF_ACTUAL_CURRENT],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TMC2209_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub)
    await cg.register_component(var, config)
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_ACTUAL_CURRENT, var.set_actual_current_sensor)
    await sensors(CONF_STALLGUARD_RESULT, var.set_stallguard_result_sensor)
    await sensors(CONF_MOTOR_LOAD, var.set_motor_load_sensor)
    await sensors(CONF_PWM_SCALE_SUM, var.set_pwm_scale_sum_sensor)
    await sensors(CONF_PWM_SCALE_AUTO, var.set_pwm_scale_auto_sensor)
    await sensors(CONF_PWM_OFS_AUTO, var.set_pwm_ofs_auto_sensor)
    await sensors(CONF_PWM_GRAD_AUTO, var.set_pwm_grad_auto_sensor)
