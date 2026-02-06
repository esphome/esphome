from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INTERNAL,
    CONF_NAME,
    CONF_PIN,
    CONF_PM_2_5,
    CONF_STARTUP_DELAY,
    CONF_UART_ID,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_PM25,
    ICON_BLUR,
    ICON_PERCENT,
    SCHEDULER_DONT_RUN,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PERCENT,
)

CODEOWNERS = ["@tuct"]
AUTO_LOAD = ["duty_cycle", "uart"]

CONF_PWM = "pwm"

pm1003_ns = cg.esphome_ns.namespace("pm1003")
PM1003Component = pm1003_ns.class_(
    "PM1003Component", uart.UARTDevice, cg.PollingComponent
)

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
    .extend({cv.Optional(CONF_NAME, default=""): cv.string})
    .extend({cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema)})
    .extend({cv.Optional(CONF_INTERNAL, default=True): cv.boolean})
    .extend(cv.polling_component_schema("30s"))
)


BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PM1003Component),
        cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon=ICON_BLUR,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_PM25,
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


def validate_pwm_requires_pm25(config):
    if CONF_PWM in config and CONF_PM_2_5 not in config:
        raise cv.Invalid("pm_2_5 is required when pwm is configured", path=[CONF_PWM])
    return config


def select_schema(config):
    if CONF_PWM in config:
        return PWM_ONLY_SCHEMA(config)
    if CONF_UART_ID in config:
        return UART_SCHEMA(config)
    raise cv.Invalid("Either 'pwm' or 'uart' must be configured for pm1003")


CONFIG_SCHEMA = cv.All(select_schema, validate_pwm_requires_pm25)


def validate_interval_uart(config):
    if CONF_UART_ID not in config:
        return config
    interval = config.get(CONF_UPDATE_INTERVAL)
    uart.final_validate_device_schema(
        "pm1003",
        baud_rate=9600,
        require_rx=True,
        require_tx=interval.total_milliseconds != SCHEDULER_DONT_RUN,
    )(config)
    return config


FINAL_VALIDATE_SCHEMA = validate_interval_uart


PM1003_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(PM1003Component)}
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_UART_ID in config:
        await uart.register_uart_device(var, config)

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))

    if CONF_PWM in config:
        pwm = await sensor.new_sensor(config[CONF_PWM])
        await cg.register_component(pwm, config[CONF_PWM])
        pin = await cg.gpio_pin_expression(config[CONF_PWM][CONF_PIN])
        cg.add(pwm.set_pin(pin))
        if config[CONF_UPDATE_INTERVAL].total_milliseconds != SCHEDULER_DONT_RUN:
            cg.add(pwm.set_update_interval(config[CONF_UPDATE_INTERVAL]))
        cg.add(var.set_pwm_sensor(pwm))

    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY].total_milliseconds))
