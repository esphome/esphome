from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_CODE,
    CONF_DURATION,
    CONF_HIGH,
    CONF_ID,
    CONF_LENGTH,
    CONF_LOW,
    CONF_PROTOCOL,
    CONF_RAW,
    CONF_SYNC,
)

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@jesserockz"]

rf_bridge_ns = cg.esphome_ns.namespace("rf_bridge")
RFBridgeComponent = rf_bridge_ns.class_(
    "RFBridgeComponent", cg.Component, uart.UARTDevice
)

RFBridgeData = rf_bridge_ns.struct("RFBridgeData")
RFBridgeAdvancedData = rf_bridge_ns.struct("RFBridgeAdvancedData")


CONF_ON_CODE_RECEIVED = "on_code_received"
CONF_ON_ADVANCED_CODE_RECEIVED = "on_advanced_code_received"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RFBridgeComponent),
            cv.Optional(CONF_ON_CODE_RECEIVED): automation.validate_automation({}),
            cv.Optional(CONF_ON_ADVANCED_CODE_RECEIVED): automation.validate_automation(
                {}
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_CODE_RECEIVED,
        "add_on_code_received_callback",
        [(RFBridgeData, "data")],
    ),
    automation.CallbackAutomation(
        CONF_ON_ADVANCED_CODE_RECEIVED,
        "add_on_advanced_code_received_callback",
        [(RFBridgeAdvancedData, "data")],
    ),
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "rf_bridge",
    baud_rate=19200,
    require_rx=True,
    require_tx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


RFBRIDGE_SEND_CODE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RFBridgeComponent),
        cv.Required(CONF_SYNC): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_LOW): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_HIGH): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_CODE): cv.templatable(cv.hex_uint32_t),
    }
)


automation.register_apply_action(
    "rf_bridge.send_code",
    RFBRIDGE_SEND_CODE_SCHEMA,
    automation.ApplyCall(
        "send_code(rf_bridge::RFBridgeData{{.sync = {}, .low = {}, .high = {}, .code = {}}})",
        (
            (CONF_SYNC, cg.uint16),
            (CONF_LOW, cg.uint16),
            (CONF_HIGH, cg.uint16),
            (CONF_CODE, cg.uint32),
        ),
    ),
)


RFBRIDGE_ID_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(RFBridgeComponent)})


automation.register_apply_action(
    "rf_bridge.learn", RFBRIDGE_ID_SCHEMA, automation.ApplyCall("learn()")
)


automation.register_apply_action(
    "rf_bridge.start_advanced_sniffing",
    RFBRIDGE_ID_SCHEMA,
    automation.ApplyCall("start_advanced_sniffing()"),
)


automation.register_apply_action(
    "rf_bridge.stop_advanced_sniffing",
    RFBRIDGE_ID_SCHEMA,
    automation.ApplyCall("stop_advanced_sniffing()"),
)


automation.register_apply_action(
    "rf_bridge.start_bucket_sniffing",
    RFBRIDGE_ID_SCHEMA,
    automation.ApplyCall("start_bucket_sniffing()"),
)


RFBRIDGE_SEND_ADVANCED_CODE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RFBridgeComponent),
        cv.Required(CONF_LENGTH): cv.templatable(cv.hex_uint8_t),
        cv.Required(CONF_PROTOCOL): cv.templatable(cv.hex_uint8_t),
        cv.Required(CONF_CODE): cv.templatable(cv.string),
    }
)


automation.register_apply_action(
    "rf_bridge.send_advanced_code",
    RFBRIDGE_SEND_ADVANCED_CODE_SCHEMA,
    automation.ApplyCall(
        "send_advanced_code(rf_bridge::RFBridgeAdvancedData{{.length = {}, .protocol = {}, .code = {}}})",
        (
            (CONF_LENGTH, cg.uint8),
            (CONF_PROTOCOL, cg.uint8),
            (CONF_CODE, cg.std_string),
        ),
    ),
)


RFBRIDGE_SEND_RAW_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RFBridgeComponent),
        cv.Required(CONF_RAW): cv.templatable(cv.string),
    }
)


automation.register_apply_action(
    "rf_bridge.send_raw",
    RFBRIDGE_SEND_RAW_SCHEMA,
    automation.ApplyField(CONF_RAW, "send_raw", cg.std_string),
)


RFBRIDGE_BEEP_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RFBridgeComponent),
        cv.Required(CONF_DURATION): cv.templatable(cv.uint16_t),
    }
)


automation.register_apply_action(
    "rf_bridge.beep",
    RFBRIDGE_BEEP_SCHEMA,
    automation.ApplyField(CONF_DURATION, "beep", cg.uint16),
)
