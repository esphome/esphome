import esphome.codegen as cg
from esphome.components import time, uart
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_TIME_ID

CODEOWNERS = ["@kroimon"]

DEPENDENCIES = ["uart"]

MULTI_CONF = True

uponor_smatrix_ns = cg.esphome_ns.namespace("uponor_smatrix")
UponorSmatrixComponent = uponor_smatrix_ns.class_(
    "UponorSmatrixComponent", cg.Component, uart.UARTDevice
)
UponorSmatrixDevice = uponor_smatrix_ns.class_(
    "UponorSmatrixDevice", cg.Parented.template(UponorSmatrixComponent)
)

CONF_UPONOR_SMATRIX_ID = "uponor_smatrix_id"
CONF_TIME_DEVICE_ADDRESS = "time_device_address"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UponorSmatrixComponent),
            cv.Optional(CONF_ADDRESS): cv.hex_uint16_t,
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_TIME_DEVICE_ADDRESS): cv.hex_uint16_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "uponor_smatrix",
    baud_rate=19200,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity=None,
    stop_bits=1,
)

# A schema to use for all Uponor Smatrix devices
UPONOR_SMATRIX_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_UPONOR_SMATRIX_ID): cv.use_id(UponorSmatrixComponent),
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
    }
)


async def to_code(config):
    cg.add_global(uponor_smatrix_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if address := config.get(CONF_ADDRESS):
        cg.add(var.set_system_address(address))
    if time_id := config.get(CONF_TIME_ID):
        time_ = await cg.get_variable(time_id)
        cg.add(var.set_time_id(time_))
    if time_device_address := config.get(CONF_TIME_DEVICE_ADDRESS):
        cg.add(var.set_time_device_address(time_device_address))


async def register_uponor_smatrix_device(var, config):
    parent = await cg.get_variable(config[CONF_UPONOR_SMATRIX_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_device_address(config[CONF_ADDRESS]))
    cg.add(parent.register_device(var))
