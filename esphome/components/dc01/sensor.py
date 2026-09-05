import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PM_2_5,
    DEVICE_CLASS_PM25,
    ICON_BLUR,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)

CODEOWNERS = ["@etbusch"]
DEPENDENCIES = ["uart"]

dc01_ns = cg.esphome_ns.namespace("dc01")
DC01Component = dc01_ns.class_("DC01Component", uart.UARTDevice, cg.Component)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DC01Component),
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
)


def validate_interval_uart(config):
    uart.final_validate_device_schema(
        "dc01", baud_rate=9600, require_rx=True, require_tx=False
    )(config)


FINAL_VALIDATE_SCHEMA = validate_interval_uart


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if pm_2_5_config := config.get(CONF_PM_2_5):
        sens = await sensor.new_sensor(pm_2_5_config)
        cg.add(var.set_pm_2_5_sensor(sens))
