import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart
from esphome.const import (
    CONF_CODE,
    CONF_HIGH,
    CONF_ID,
    CONF_LENGTH,
    CONF_LOW,
    CONF_PROTOCOL,
    CONF_RAW,
    CONF_SYNC,
    CONF_TRIGGER_ID,
)

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@jesserockz"]

rf_bridge_ns = cg.esphome_ns.namespace("rf_bridge")
RFBridgeComponent = rf_bridge_ns.class_(
    "RFBridgeComponent", cg.Component, uart.UARTDevice
)

RFBridgeData = rf_bridge_ns.struct("RFBridgeData")
RFBridgeAdvancedData = rf_bridge_ns.struct("RFBridgeAdvancedData")

RFBridgeReceivedCodeTrigger = rf_bridge_ns.class_(
    "RFBridgeReceivedCodeTrigger", automation.Trigger.template(RFBridgeData)
)
RFBridgeReceivedAdvancedCodeTrigger = rf_bridge_ns.class_(
    "RFBridgeReceivedAdvancedCodeTrigger",
    automation.Trigger.template(RFBridgeAdvancedData),
)

RFBridgeSendCodeAction = rf_bridge_ns.class_(
    "RFBridgeSendCodeAction", automation.Action
)
RFBridgeSendAdvancedCodeAction = rf_bridge_ns.class_(
    "RFBridgeSendAdvancedCodeAction", automation.Action
)

RFBridgeLearnAction = rf_bridge_ns.class_("RFBridgeLearnAction", automation.Action)

RFBridgeStartAdvancedSniffingAction = rf_bridge_ns.class_(
    "RFBridgeStartAdvancedSniffingAction", automation.Action
)
RFBridgeStopAdvancedSniffingAction = rf_bridge_ns.class_(
    "RFBridgeStopAdvancedSniffingAction", automation.Action
)

RFBridgeSendRawAction = rf_bridge_ns.class_("RFBridgeSendRawAction", automation.Action)

CONF_ON_CODE_RECEIVED = "on_code_received"
CONF_ON_ADVANCED_CODE_RECEIVED = "on_advanced_code_received"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RFBridgeComponent),
            cv.Optional(CONF_ON_CODE_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        RFBridgeReceivedCodeTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_ADVANCED_CODE_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        RFBridgeReceivedAdvancedCodeTrigger
                    ),
                }
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    for conf in config.get(CONF_ON_CODE_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [(RFBridgeData, "data")], conf)

    for conf in config.get(CONF_ON_ADVANCED_CODE_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(
            trigger, [(RFBridgeAdvancedData, "data")], conf
        )


RFBRIDGE_SEND_CODE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RFBridgeComponent),
        cv.Required(CONF_SYNC): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_LOW): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_HIGH): cv.templatable(cv.hex_uint16_t),
        cv.Required(CONF_CODE): cv.templatable(cv.hex_uint32_t),
    }
)


@automation.register_action(
    "rf_bridge.send_code", RFBridgeSendCodeAction, RFBRIDGE_SEND_CODE_SCHEMA
)
def rf_bridge_send_code_to_code(config, action_id, template_args, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    template_ = yield cg.templatable(config[CONF_SYNC], args, cg.uint16)
    cg.add(var.set_sync(template_))
    template_ = yield cg.templatable(config[CONF_LOW], args, cg.uint16)
    cg.add(var.set_low(template_))
    template_ = yield cg.templatable(config[CONF_HIGH], args, cg.uint16)
    cg.add(var.set_high(template_))
    template_ = yield cg.templatable(config[CONF_CODE], args, cg.uint32)
    cg.add(var.set_code(template_))
    yield var


RFBRIDGE_ID_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(RFBridgeComponent)})


@automation.register_action("rf_bridge.learn", RFBridgeLearnAction, RFBRIDGE_ID_SCHEMA)
def rf_bridge_learnx_to_code(config, action_id, template_args, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    yield var


@automation.register_action(
    "rf_bridge.start_advanced_sniffing",
    RFBridgeStartAdvancedSniffingAction,
    RFBRIDGE_ID_SCHEMA,
)
def rf_bridge_start_advanced_sniffing_to_code(config, action_id, template_args, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    yield var


@automation.register_action(
    "rf_bridge.stop_advanced_sniffing",
    RFBridgeStopAdvancedSniffingAction,
    RFBRIDGE_ID_SCHEMA,
)
def rf_bridge_stop_advanced_sniffing_to_code(config, action_id, template_args, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    yield var


RFBRIDGE_SEND_ADVANCED_CODE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RFBridgeComponent),
        cv.Required(CONF_LENGTH): cv.templatable(cv.hex_uint8_t),
        cv.Required(CONF_PROTOCOL): cv.templatable(cv.hex_uint8_t),
        cv.Required(CONF_CODE): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "rf_bridge.send_advanced_code",
    RFBridgeSendAdvancedCodeAction,
    RFBRIDGE_SEND_ADVANCED_CODE_SCHEMA,
)
def rf_bridge_send_advanced_code_to_code(config, action_id, template_args, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    template_ = yield cg.templatable(config[CONF_LENGTH], args, cg.uint16)
    cg.add(var.set_length(template_))
    template_ = yield cg.templatable(config[CONF_PROTOCOL], args, cg.uint16)
    cg.add(var.set_protocol(template_))
    template_ = yield cg.templatable(config[CONF_CODE], args, cg.std_string)
    cg.add(var.set_code(template_))
    yield var


RFBRIDGE_SEND_RAW_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(RFBridgeComponent),
        cv.Required(CONF_RAW): cv.templatable(cv.string),
    }
)


@automation.register_action(
    "rf_bridge.send_raw", RFBridgeSendRawAction, RFBRIDGE_SEND_RAW_SCHEMA
)
def rf_bridge_send_raw_to_code(config, action_id, template_args, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    template_ = yield cg.templatable(config[CONF_RAW], args, cg.std_string)
    cg.add(var.set_raw(template_))
    yield var
