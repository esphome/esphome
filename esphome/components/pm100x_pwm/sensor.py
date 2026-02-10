from esphome import pins
import esphome.codegen as cg
from esphome.components import pm100x, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INTERNAL,
    CONF_INVERTED,
    CONF_MODEL,
    CONF_NAME,
    CONF_NUMBER,
    CONF_PIN,
    CONF_PM_2_5,
    CONF_STARTUP_DELAY,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_PM25,
    ICON_BLUR,
    SCHEDULER_DONT_RUN,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)

AUTO_LOAD = ["pm100x", "duty_cycle"]
CODEOWNERS = ["@tuct", "@habbie"]

CONF_PWM = "pwm"
CONF_PWM_SENSOR_ID = cv.GenerateID()


def validate_pwm_pin(config):
    """Transform flat pin config to nested structure and validate."""
    pin_value = config[CONF_PIN]
    inverted = config.get(CONF_INVERTED, False)

    # Build pin config dict
    if isinstance(pin_value, dict):
        pin_config = pin_value.copy()
    else:
        pin_config = {CONF_NUMBER: pin_value}

    # Add inverted to pin config
    pin_config[CONF_INVERTED] = inverted

    # Validate the pin config (this runs during validation phase when context is available)
    pin_config = pins.internal_gpio_input_pin_schema(pin_config)

    # Replace the pin field with the validated config
    config = config.copy()
    config[CONF_PIN] = pin_config

    return config


PWM_SCHEMA = cv.All(
    cv.Schema(
        {
            CONF_PWM_SENSOR_ID: cv.declare_id(cg.PollingComponent),
            cv.Optional(CONF_NAME): cv.string,
            cv.Required(CONF_PIN): cv.Any(
                int, str, dict
            ),  # Accept raw value, validate in validate_pwm_pin
            cv.Optional(CONF_INVERTED, default=False): cv.boolean,
            cv.Optional(CONF_INTERNAL, default=True): cv.boolean,
        }
    ).extend(cv.polling_component_schema("30s")),
    validate_pwm_pin,  # Transform and validate pin here (during validation phase)
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(pm100x.PM100XComponent),
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
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_model(pm100x.MODEL_OPTIONS[config[CONF_MODEL]]))

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))

    # PWM duty cycle sensor
    if CONF_PWM in config:
        # Conditionally load duty_cycle component when PWM is configured
        from esphome.components import duty_cycle  # noqa: F401

        # Add the duty_cycle header include to generated C++ code
        cg.add_global(
            cg.RawStatement(
                '#include "esphome/components/duty_cycle/duty_cycle_sensor.h"'
            )
        )

        duty_cycle_ns = cg.esphome_ns.namespace("duty_cycle")
        DutyCycleSensor = duty_cycle_ns.class_(
            "DutyCycleSensor", sensor.Sensor, cg.PollingComponent
        )

        pwm_conf = config[CONF_PWM]
        pwm_id = pwm_conf[CONF_PWM_SENSOR_ID]
        # Create instance with explicit class type
        rhs = cg.RawExpression("new duty_cycle::DutyCycleSensor()")
        pwm = cg.Pvariable(pwm_id, rhs, DutyCycleSensor)
        await cg.register_component(pwm, pwm_conf)

        # Manually configure sensor properties since we don't use sensor_schema
        if CONF_NAME in pwm_conf:
            cg.add(pwm.set_name(pwm_conf[CONF_NAME]))
        if CONF_INTERNAL in pwm_conf:
            cg.add(pwm.set_internal(pwm_conf[CONF_INTERNAL]))
        cg.add(pwm.set_unit_of_measurement("%"))
        cg.add(pwm.set_icon("mdi:percent"))
        cg.add(pwm.set_accuracy_decimals(1))
        # Use the StateClasses enum
        cg.add(pwm.set_state_class(sensor.StateClasses.STATE_CLASS_MEASUREMENT))

        # Pin is already validated and transformed in validate_pwm_pin
        pin = await cg.gpio_pin_expression(pwm_conf[CONF_PIN])
        cg.add(pwm.set_pin(pin))

        if config[CONF_UPDATE_INTERVAL].total_milliseconds != SCHEDULER_DONT_RUN:
            cg.add(pwm.set_update_interval(config[CONF_UPDATE_INTERVAL]))
        cg.add(var.set_pwm_sensor(pwm))

    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY].total_milliseconds))
