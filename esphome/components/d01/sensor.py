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

CODEOWNERS = ["@ch604"]
DEPENDENCIES = ["uart"]

d01_ns = cg.esphome_ns.namespace("d01")
D01Component = d01_ns.class_("D01Component", uart.UARTDevice,
                             cg.PollingComponent)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(D01Component),
            cv.Required(CONF_PM_2_5): sensor.sensor_schema(
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
    .extend(cv.polling_component_schema("10s")),
)

def validate_interval_uart(config):
    uart.final_validate_device_schema(
        "d01",
        baud_rate=9600,
        require_rx=True,
        require_tx=False,
    )(config)

FINAL_VALIDATE_SCHEMA = validate_interval_uart

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(var.set_pm25_sensor(sens))
