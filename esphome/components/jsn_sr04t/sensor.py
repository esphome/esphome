import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, ICON_ARROW_EXPAND_VERTICAL
from esphome.types import ConfigType

CODEOWNERS = ["@Mafus1"]
DEPENDENCIES = ["uart"]

jsn_sr04t_ns = cg.esphome_ns.namespace("jsn_sr04t")
Jsnsr04tComponent = jsn_sr04t_ns.class_(
    "Jsnsr04tComponent", sensor.Sensor, cg.PollingComponent, uart.UARTDevice
)
Model = jsn_sr04t_ns.enum("Model")
MODEL = Model.options("jsn_sr04t", "aj_sr04m")

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        Jsnsr04tComponent,
        kind=sensor.DISTANCE_M,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=3,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_MODEL, default="jsn_sr04t"): cv.enum(MODEL, upper=False),
        }
    )
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "jsn_sr04t",
    baud_rate=9600,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity=None,
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))
