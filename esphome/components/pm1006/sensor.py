import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_PM_2_5,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_PM25,
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


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PM1006Component),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_BLUR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_PM25,
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

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm_2_5_sensor(sens))
