from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@jorre05", "@edenhaus"]

DEPENDENCIES = ["uart"]

DOMAIN = "micronova"
CONF_MICRONOVA_ID = f"{DOMAIN}_id"
CONF_ENABLE_RX_PIN = "enable_rx_pin"
CONF_MEMORY_LOCATION = "memory_location"
CONF_MEMORY_ADDRESS = "memory_address"
DEFAULT_POLLING_INTERVAL = "60s"

micronova_ns = cg.esphome_ns.namespace(DOMAIN)

MicroNova = micronova_ns.class_("MicroNova", cg.Component, uart.UARTDevice)
MicroNovaListener = micronova_ns.class_("MicroNovaListener", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MicroNova),
        cv.Required(CONF_ENABLE_RX_PIN): pins.gpio_output_pin_schema,
    }
).extend(uart.UART_DEVICE_SCHEMA)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=1200,
    require_rx=True,
    require_tx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=2,
)


def MICRONOVA_ADDRESS_SCHEMA(
    *,
    default_memory_location: int | None = None,
    default_memory_address: int | None = None,
    is_polling_component: bool,
):
    location_key = (
        cv.Optional(CONF_MEMORY_LOCATION, default=default_memory_location)
        if default_memory_location is not None
        else cv.Required(CONF_MEMORY_LOCATION)
    )
    address_key = (
        cv.Optional(CONF_MEMORY_ADDRESS, default=default_memory_address)
        if default_memory_address is not None
        else cv.Required(CONF_MEMORY_ADDRESS)
    )
    schema = cv.Schema(
        {
            cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
            location_key: cv.hex_int_range(min=0x00, max=0x79),
            address_key: cv.hex_int_range(min=0x00, max=0xFF),
        }
    )
    if is_polling_component:
        schema = schema.extend(cv.polling_component_schema(DEFAULT_POLLING_INTERVAL))
    return schema


async def to_code_micronova_listener(mv, var, config):
    await cg.register_component(var, config)
    cg.add(mv.register_micronova_listener(var))
    cg.add(var.set_memory_location(config[CONF_MEMORY_LOCATION]))
    cg.add(var.set_memory_address(config[CONF_MEMORY_ADDRESS]))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    enable_rx_pin = await cg.gpio_pin_expression(config[CONF_ENABLE_RX_PIN])
    cg.add(var.set_enable_rx_pin(enable_rx_pin))
