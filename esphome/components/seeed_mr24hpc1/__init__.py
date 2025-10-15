import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
# is the code owner of the relevant code base
CODEOWNERS = ["@limengdu"]
# The current component or platform can be configured or defined multiple times in the same configuration file.
MULTI_CONF = True

# This line of code creates a new namespace called mr24hpc1_ns.
# This namespace will be used as a prefix for all classes, functions and variables associated with the mr24hpc1_ns component, ensuring that they do not conflict with the names of other components.
mr24hpc1_ns = cg.esphome_ns.namespace("seeed_mr24hpc1")
# This MR24HPC1Component class will be a periodically polled UART device
MR24HPC1Component = mr24hpc1_ns.class_(
    "MR24HPC1Component", cg.Component, uart.UARTDevice
)

CONF_MR24HPC1_ID = "mr24hpc1_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MR24HPC1Component),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

# A verification mode was created to verify the configuration parameters of a UART device named "seeed_mr24hpc1".
# This authentication mode requires that the device must have transmit and receive functionality, a parity mode of "NONE", and a stop bit of one.
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "seeed_mr24hpc1",
    require_tx=True,
    require_rx=True,
    parity="NONE",
    stop_bits=1,
)


# The async def keyword is used to define a concurrent function.
# Concurrent functions are special functions designed to work with Python's asyncio library to support asynchronous I/O operations.
async def to_code(config):
    # This line of code creates a new Pvariable (a Python object representing a C++ variable) with the variable's ID taken from the configuration.
    var = cg.new_Pvariable(config[CONF_ID])
    # This line of code registers the newly created Pvariable as a component so that ESPHome can manage it at runtime.
    await cg.register_component(var, config)
    # This line of code registers the newly created Pvariable as a device.
    await uart.register_uart_device(var, config)
