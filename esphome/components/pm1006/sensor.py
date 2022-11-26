import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_10_0,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_PM10,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    ICON_BLUR,
)
from esphome.core import TimePeriodMilliseconds

CODEOWNERS = ["@habbie"]
DEPENDENCIES = ["uart"]

pm1006_ns = cg.esphome_ns.namespace("pm1006")
PM1006Component = pm1006_ns.class_(
    "PM1006Component", uart.UARTDevice, cg.PollingComponent
)


TYPE_PM1006 = "PM1006"
TYPE_PM1006K = "PM1006K"

PM1006Type = pm1006_ns.enum("PM1006Type")

PM1006_TYPES = {
    TYPE_PM1006: PM1006Type.PM1006_TYPE_1006,
    TYPE_PM1006K: PM1006Type.PM1006_TYPE_1006K,
}

SENSORS_TO_TYPE = {
    CONF_PM_1_0: [TYPE_PM1006K],
    CONF_PM_2_5: [TYPE_PM1006, TYPE_PM1006K],
    CONF_PM_10_0: [TYPE_PM1006K],
}


def validate_pm1006_sensors(value):
    for key, types in SENSORS_TO_TYPE.items():
        if key in value and value[CONF_TYPE] not in types:
            raise cv.Invalid(f"{value[CONF_TYPE]} does not have {key} sensor!")
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PM1006Component),
            cv.Optional(CONF_TYPE): cv.enum(PM1006_TYPES, upper=True),
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_BLUR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_BLUR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_BLUR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("never")),
)


def validate_interval_uart(config):
    require_tx = False

    interval = config.get(CONF_UPDATE_INTERVAL)

    if isinstance(interval, TimePeriodMilliseconds):
        # 'never' is encoded as a very large int, not as a TimePeriodMilliseconds objects
        require_tx = True

    uart.final_validate_device_schema(
        "pm1006", baud_rate=9600, require_rx=True, require_tx=require_tx
    )(config)


FINAL_VALIDATE_SCHEMA = validate_interval_uart


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_type(config[CONF_TYPE]))

    if CONF_PM_1_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_1_0])
        cg.add(var.set_pm_1_0_sensor(sens))
    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))
    if CONF_PM_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_10_0])
        cg.add(var.set_pm_10_0_sensor(sens))
