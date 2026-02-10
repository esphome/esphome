from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INTERNAL,
    CONF_MODEL,
    CONF_NAME,
    CONF_PIN,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_10_0,
    CONF_STARTUP_DELAY,
    CONF_UART_ID,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    ICON_BLUR,
    ICON_PERCENT,
    SCHEDULER_DONT_RUN,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PERCENT,
)

from . import PM100XComponent, PM100XComponentUART, pm100x_ns

CONF_PWM = "pwm"

PM100XModel = pm100x_ns.enum("PM100XModel", True)
MODEL_OPTIONS = {
    "pm1003": PM100XModel.PM1003,
    "pm1006": PM100XModel.PM1006,
    "pm1006k": PM100XModel.PM1006K,
}


# Generic PWM schema without duty_cycle class reference (loaded conditionally)
PWM_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(sensor.Sensor),
            cv.Optional(CONF_NAME, default="PWM Duty"): cv.string,
            cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Optional(CONF_INTERNAL, default=True): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("30s"))
    .extend(
        sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon=ICON_PERCENT,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        )
    )
)


BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PM100XComponent),
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
        cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_BLUR,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_PM1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_BLUR,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_PM10,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_STARTUP_DELAY, default="15s"): cv.positive_time_period,
    }
)

UART_SCHEMA = (
    BASE_SCHEMA.extend({cv.Optional(CONF_PWM): PWM_SCHEMA})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("never"))
)

PWM_ONLY_SCHEMA = (
    BASE_SCHEMA.extend({cv.Required(CONF_PWM): PWM_SCHEMA})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("never"))
)


def validate_uart_pwm_exclusive(config):
    """Validate that UART and PWM are mutually exclusive."""
    has_uart = CONF_UART_ID in config
    has_pwm = CONF_PWM in config

    if has_uart and has_pwm:
        raise cv.Invalid(
            "UART and PWM modes are mutually exclusive. Use either uart_id or pwm, not both."
        )
    if not has_uart and not has_pwm:
        raise cv.Invalid("Either UART (uart_id) or PWM (pwm) must be configured")
    return config


def validate_pwm_requires_pm25(config):
    if CONF_PWM in config and CONF_PM_2_5 not in config:
        raise cv.Invalid("pm_2_5 is required when pwm is configured", path=[CONF_PWM])
    if CONF_PWM in config and (CONF_PM_1_0 in config or CONF_PM_10_0 in config):
        raise cv.Invalid("pm_1_0 and pm_10_0 require UART (pwm only supports pm_2_5)")
    if (CONF_PM_1_0 in config or CONF_PM_10_0 in config) and config.get(
        CONF_MODEL
    ) != "pm1006k":
        raise cv.Invalid("pm_1_0 and pm_10_0 are only supported on pm1006k")
    if config.get(CONF_MODEL) == "pm1006" and CONF_PWM in config:
        raise cv.Invalid("pwm is not supported on pm1006", path=[CONF_PWM])
    return config


def select_schema(config):
    if CONF_PWM in config and CONF_UART_ID in config:
        return UART_SCHEMA(config)
    if CONF_PWM in config:
        return PWM_ONLY_SCHEMA(config)
    return UART_SCHEMA(config)


CONFIG_SCHEMA = cv.All(
    select_schema, validate_uart_pwm_exclusive, validate_pwm_requires_pm25
)


def validate_interval_uart(config):
    if CONF_PWM in config and CONF_UART_ID not in config:
        return config
    interval = config.get(CONF_UPDATE_INTERVAL)
    uart.final_validate_device_schema(
        "pm100x",
        baud_rate=9600,
        require_rx=True,
        require_tx=interval.total_milliseconds != SCHEDULER_DONT_RUN,
    )(config)
    return config


FINAL_VALIDATE_SCHEMA = validate_interval_uart


async def to_code(config):
    # Use UART-enabled class if UART is configured, otherwise use base class
    has_uart = CONF_UART_ID in config or CONF_PWM not in config

    if has_uart:
        # Manually create UART-enabled subclass instance
        rhs = cg.RawExpression("new pm100x::PM100XComponentUART()")
        var = cg.Pvariable(config[CONF_ID], rhs, PM100XComponentUART)
        await cg.register_component(var, config)
        await uart.register_uart_device(var, config)
    else:
        # Create base class for PWM-only mode
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)

    cg.add(var.set_model(MODEL_OPTIONS[config[CONF_MODEL]]))

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))

    if CONF_PM_1_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_1_0])
        cg.add(var.set_pm_1_0_sensor(sens))

    if CONF_PM_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_10_0])
        cg.add(var.set_pm_10_0_sensor(sens))

    if CONF_PWM in config:
        # Conditionally load duty_cycle component when PWM is configured
        from esphome.components import (  # noqa: F401
            duty_cycle,  # pylint: disable=unused-import
        )

        duty_cycle_ns = cg.esphome_ns.namespace("duty_cycle")
        DutyCycleSensor = duty_cycle_ns.class_(
            "DutyCycleSensor", sensor.Sensor, cg.PollingComponent
        )

        pwm_conf = config[CONF_PWM]
        pwm_id = pwm_conf[CONF_ID]
        # Create instance with explicit class type
        rhs = cg.RawExpression("new duty_cycle::DutyCycleSensor()")
        pwm = cg.Pvariable(pwm_id, rhs, DutyCycleSensor)
        await cg.register_component(pwm, pwm_conf)
        await sensor.register_sensor(pwm, pwm_conf)
        pin = await cg.gpio_pin_expression(pwm_conf[CONF_PIN])
        cg.add(pwm.set_pin(pin))
        if config[CONF_UPDATE_INTERVAL].total_milliseconds != SCHEDULER_DONT_RUN:
            cg.add(pwm.set_update_interval(config[CONF_UPDATE_INTERVAL]))
        cg.add(var.set_pwm_sensor(pwm))

    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY].total_milliseconds))
