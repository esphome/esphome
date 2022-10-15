import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]

ld2410_ns = cg.esphome_ns.namespace("ld2410")
LD2410Component = ld2410_ns.class_(
    "LD2410Component", cg.PollingComponent, uart.UARTDevice
)
# LD2410Component = ld2410_ns.class_("LD2410Component", cg.Component)
CONF_LD2410_ID = "ld2410_id"
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2410Component),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)


# CONF_ON_SMS_RECEIVED = "on_sms_received"
# CONF_ON_USSD_RECEIVED = "on_ussd_received"
# CONF_ON_INCOMING_CALL = "on_incoming_call"
# CONF_ON_CALL_CONNECTED = "on_call_connected"
# CONF_ON_CALL_DISCONNECTED = "on_call_disconnected"
# CONF_RECIPIENT = "recipient"
# CONF_MESSAGE = "message"
# CONF_USSD = "ussd"
