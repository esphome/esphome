import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID, CONF_CODE, CONF_LOW, CONF_SYNC, CONF_HIGH
from esphome.components import uart

DEPENDENCIES = ['uart']

rf_bridge_ns = cg.esphome_ns.namespace('rf_bridge')
RFBridgeComponent = rf_bridge_ns.class_('RFBridgeComponent', cg.Component, uart.UARTDevice)

RFBridgeData = rf_bridge_ns.struct('RFBridgeData')

RFBridgeReceivedCodeTrigger = rf_bridge_ns.class_('RFBridgeReceivedCodeTrigger',
                                                  automation.Trigger.template(RFBridgeData))

RFBridgeSendCodeAction = rf_bridge_ns.class_('RFBridgeSendCodeAction', automation.Action)
RFBridgeLearnAction = rf_bridge_ns.class_('RFBridgeLearnAction', automation.Action)


CONF_ON_CODE_RECEIVED = 'on_code_received'

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(RFBridgeComponent),
    cv.Optional(CONF_ON_CODE_RECEIVED): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RFBridgeReceivedCodeTrigger),
    }),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    for conf in config.get(CONF_ON_CODE_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [(RFBridgeData, 'data')], conf)


RFBRIDGE_SEND_CODE_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(RFBridgeComponent),
    cv.Required(CONF_SYNC): cv.templatable(cv.hex_uint16_t),
    cv.Required(CONF_LOW): cv.templatable(cv.hex_uint16_t),
    cv.Required(CONF_HIGH): cv.templatable(cv.hex_uint16_t),
    cv.Required(CONF_CODE): cv.templatable(cv.hex_uint32_t)
})


@automation.register_action('rf_bridge.send_code', RFBridgeSendCodeAction,
                            RFBRIDGE_SEND_CODE_SCHEMA)
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


RFBRIDGE_LEARN_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(RFBridgeComponent)
})


@automation.register_action('rf_bridge.learn', RFBridgeLearnAction, RFBRIDGE_LEARN_SCHEMA)
def rf_bridge_learnx_to_code(config, action_id, template_args, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    yield var
