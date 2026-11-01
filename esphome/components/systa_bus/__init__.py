import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@Mat931"]
DOMAIN = "systa_bus"

DEPENDENCIES = ["uart"]

MULTI_CONF = True

systa_bus_ns = cg.esphome_ns.namespace("systa_bus")
SystaBus = systa_bus_ns.class_("SystaBus", cg.Component, uart.UARTDevice)

CONF_SYSTA_BUS_ID = "systa_bus_id"

CONF_SYSTASOLAR_AQUA = "systasolar_aqua"

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SystaBus),
    }
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "systa_bus", baud_rate=9600, require_rx=True
)

# Children publish from each SystaSolar Aqua sensor data frame
AQUA_SENSOR_DATA = cg.HubFunction(
    "aqua_sensor_data_fn",
    cg.void,
    [(cg.std_ns.class_("span").template(cg.uint8.operator("const")), "message")],
    setter="set_aqua_sensor_data_fn",
    define="USE_SYSTA_BUS_AQUA_SENSOR_DATA",
)
MESSAGE = AQUA_SENSOR_DATA.param("message")
# Children's dump_config lines, logged under the bus's tag
DUMP = cg.HubFunction(
    "dump_fn",
    cg.void,
    [(cg.const_char_ptr, "tag")],
    setter="set_dump_fn",
    define="USE_SYSTA_BUS_AQUA_SENSOR_DATA",
)
DUMP_TAG = DUMP.param("tag")


async def to_code(config: ConfigType) -> None:
    await uart.new_uart_component(config)
