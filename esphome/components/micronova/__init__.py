from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@jorre05"]

DEPENDENCIES = ["uart"]

CONF_MICRONOVA_ID = "micronova_id"
CONF_ENABLE_RX_PIN = "enable_rx_pin"
CONF_MEMORY_LOCATION = "memory_location"
CONF_MEMORY_ADDRESS = "memory_address"

micronova_ns = cg.esphome_ns.namespace("micronova")

MicroNovaFunctions = micronova_ns.enum("MicroNovaFunctions", is_class=True)
MICRONOVA_FUNCTIONS_ENUM = {
    "STOVE_FUNCTION_SWITCH": MicroNovaFunctions.STOVE_FUNCTION_SWITCH,
    "STOVE_FUNCTION_ROOM_TEMPERATURE": MicroNovaFunctions.STOVE_FUNCTION_ROOM_TEMPERATURE,
    "STOVE_FUNCTION_THERMOSTAT_TEMPERATURE": MicroNovaFunctions.STOVE_FUNCTION_THERMOSTAT_TEMPERATURE,
    "STOVE_FUNCTION_FUMES_TEMPERATURE": MicroNovaFunctions.STOVE_FUNCTION_FUMES_TEMPERATURE,
    "STOVE_FUNCTION_STOVE_POWER": MicroNovaFunctions.STOVE_FUNCTION_STOVE_POWER,
    "STOVE_FUNCTION_FAN_SPEED": MicroNovaFunctions.STOVE_FUNCTION_FAN_SPEED,
    "STOVE_FUNCTION_STOVE_STATE": MicroNovaFunctions.STOVE_FUNCTION_STOVE_STATE,
    "STOVE_FUNCTION_MEMORY_ADDRESS_SENSOR": MicroNovaFunctions.STOVE_FUNCTION_MEMORY_ADDRESS_SENSOR,
    "STOVE_FUNCTION_WATER_TEMPERATURE": MicroNovaFunctions.STOVE_FUNCTION_WATER_TEMPERATURE,
    "STOVE_FUNCTION_WATER_PRESSURE": MicroNovaFunctions.STOVE_FUNCTION_WATER_PRESSURE,
    "STOVE_FUNCTION_POWER_LEVEL": MicroNovaFunctions.STOVE_FUNCTION_POWER_LEVEL,
    "STOVE_FUNCTION_CUSTOM": MicroNovaFunctions.STOVE_FUNCTION_CUSTOM,
}

MicroNova = micronova_ns.class_("MicroNova", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MicroNova),
            cv.Required(CONF_ENABLE_RX_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
)


def MICRONOVA_LISTENER_SCHEMA(default_memory_location, default_memory_address):
    return cv.Schema(
        {
            cv.GenerateID(CONF_MICRONOVA_ID): cv.use_id(MicroNova),
            cv.Optional(
                CONF_MEMORY_LOCATION, default=default_memory_location
            ): cv.hex_int_range(),
            cv.Optional(
                CONF_MEMORY_ADDRESS, default=default_memory_address
            ): cv.hex_int_range(),
        }
    )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    enable_rx_pin = await cg.gpio_pin_expression(config[CONF_ENABLE_RX_PIN])
    cg.add(var.set_enable_rx_pin(enable_rx_pin))
