from esphome import pins
import esphome.codegen as cg
from esphome.components import pm100x, pm100x_pwm, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    CONF_PIN,
    CONF_PM_2_5,
    CONF_STARTUP_DELAY,
    DEVICE_CLASS_PM25,
    ICON_BLUR,
    ICON_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PERCENT,
)

AUTO_LOAD = ["pm100x", "duty_cycle"]
CODEOWNERS = ["@tuct", "@habbie"]

CONF_PWM = "pwm"

duty_cycle_ns = cg.esphome_ns.namespace("duty_cycle")
DutyCycleSensor = duty_cycle_ns.class_(
    "DutyCycleSensor", sensor.Sensor, cg.PollingComponent
)


PWM_SCHEMA = (
    sensor.sensor_schema(
        DutyCycleSensor,
        unit_of_measurement=UNIT_PERCENT,
        icon=ICON_PERCENT,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("30s"))
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(pm100x_pwm.PM100XComponentPWM),
            cv.Optional(CONF_MODEL, default="pm1003"): cv.one_of(
                "pm1003", "pm1006", "pm1006k", lower=True
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_BLUR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Required(CONF_PWM): PWM_SCHEMA,
            cv.Optional(CONF_STARTUP_DELAY, default="15s"): cv.positive_time_period,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("never"))
)


async def to_code(config):
    cg.add_global(
        cg.RawStatement('#include "esphome/components/pm100x_pwm/pm100x_pwm.h"')
    )

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_model(pm100x.MODEL_OPTIONS[config[CONF_MODEL]]))

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))

    if CONF_PWM in config:
        pwm_conf = config[CONF_PWM]
        pwm = await sensor.new_sensor(pwm_conf)
        await cg.register_component(pwm, pwm_conf)

        pin = await cg.gpio_pin_expression(pwm_conf[CONF_PIN])
        cg.add(pwm.set_pin(pin))

        cg.add(var.set_pwm_sensor(pwm))

    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY].total_milliseconds))
