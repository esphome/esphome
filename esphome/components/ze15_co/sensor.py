import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_MODE,
    CONF_WARMUP_TIME,
    DEVICE_CLASS_CARBON_MONOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

CODEOWNERS = ["@maikeljkwak"]
DEPENDENCIES = ["uart"]

ICON_MOLECULE_CO = "mdi:molecule-co"

ze15_ns = cg.esphome_ns.namespace("ze15_co")
ZE15COComponent = ze15_ns.class_(
    "ZE15COComponent", sensor.Sensor, cg.PollingComponent, uart.UARTDevice
)

Mode = ze15_ns.enum("Mode", is_class=True)
MODE = {
    "qa": Mode.QA,
    "stream": Mode.STREAM,
}

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        ZE15COComponent,
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
        accuracy_decimals=1,
        icon=ICON_MOLECULE_CO,
        device_class=DEVICE_CLASS_CARBON_MONOXIDE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_MODE, default="qa"): cv.enum(MODE),
            cv.Optional(
                CONF_WARMUP_TIME, default="30s"
            ): cv.positive_time_period_seconds,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_warmup_seconds(config[CONF_WARMUP_TIME].total_seconds))
