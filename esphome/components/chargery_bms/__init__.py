import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = ["@wmeler"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor"]

CONF_CHARGERY_BMS_ID = "chargery_bms_id"
CONF_NUM_CELLS = "num_cells"
chargery_bms_ns = cg.esphome_ns.namespace("chargery_bms")
ChargeryBmsComponent = chargery_bms_ns.class_(
    "ChargeryBmsComponent", cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ChargeryBmsComponent),
        cv.Optional(CONF_NUM_CELLS, default=24): cv.int_range(min=1, max=24),
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_battery_num_cells(config[CONF_NUM_CELLS]))
    await uart.register_uart_device(var, config)
