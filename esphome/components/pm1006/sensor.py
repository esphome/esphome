import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_PM_2_5, CONF_UPDATE_INTERVAL, SCHEDULER_DONT_RUN
from esphome.types import ConfigType

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
                kind=sensor.PM25, accuracy_decimals=0
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("never")),
)


def validate_interval_uart(config: ConfigType) -> None:
    interval = config.get(CONF_UPDATE_INTERVAL)
    uart.final_validate_device_schema(
        "pm1006",
        baud_rate=9600,
        require_rx=True,
        require_tx=interval.total_milliseconds != SCHEDULER_DONT_RUN,
        data_bits=8,
        parity="NONE",
        stop_bits=1,
    )(config)


FINAL_VALIDATE_SCHEMA = validate_interval_uart


async def to_code(config: ConfigType) -> None:
    var = await uart.new_uart_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_PM_2_5, var.set_pm_2_5_sensor)
