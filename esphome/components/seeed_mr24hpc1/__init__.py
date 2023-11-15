import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID
from esphome.automation import maybe_simple_id

DEPENDENCIES = ["uart"]

CODEOWNERS = ["@limengdu"]

MULTI_CONF = True


mr24hpc1_ns = cg.esphome_ns.namespace("mr24hpc1")

mr24hpc1Component = mr24hpc1_ns.class_(
    "mr24hpc1Component", cg.PollingComponent, uart.UARTDevice
)

CONF_MR24HPC1_ID = "mr24hpc1_id"


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(mr24hpc1Component),
    }
)


CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA.extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)
)


FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "mr24hpc1",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)


CALIBRATION_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(mr24hpc1Component),
    }
)
