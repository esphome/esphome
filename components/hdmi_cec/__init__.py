import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID
)

CODEOWNERS = ["@Palakis"]

CONF_PIN = "pin"
CONF_ADDRESS = "address"
CONF_PROMISCUOUS_MODE = "promiscuous_mode"
CONF_ON_MESSAGE = "on_message"

CONF_SOURCE = "source"
CONF_DESTINATION = "destination"
CONF_OPCODE = "opcode"
CONF_DATA = "data"
CONF_PARENT = "parent"

def validate_data_array(value):
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must be a list of bytes")

hdmi_cec_ns = cg.esphome_ns.namespace("hdmi_cec")
HDMICEC = hdmi_cec_ns.class_(
    "HDMICEC", cg.Component
)
MessageTrigger = hdmi_cec_ns.class_(
    "MessageTrigger", automation.Trigger.template(cg.uint8, cg.uint8, cg.std_vector.template(cg.uint8))
)
SendAction = hdmi_cec_ns.class_(
    "SendAction", automation.Action
)

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(HDMICEC),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=15),
        cv.Optional(CONF_PROMISCUOUS_MODE, False): cv.boolean,
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MessageTrigger),
                cv.Optional(CONF_SOURCE): cv.int_range(min=0, max=15),
                cv.Optional(CONF_DESTINATION): cv.int_range(min=0, max=15),
                cv.Optional(CONF_OPCODE): cv.uint8_t,
                cv.Optional(CONF_DATA): validate_data_array
            }
        )
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cec_pin_ = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(cec_pin_))

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_promiscuous_mode(config[CONF_PROMISCUOUS_MODE]))

    for conf in config.get(CONF_ON_MESSAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)

        source = conf.get(CONF_SOURCE)
        if source is not None:
            cg.add(trigger.set_source(source))

        destination = conf.get(CONF_DESTINATION)
        if destination is not None:
            cg.add(trigger.set_destination(destination))

        opcode = conf.get(CONF_OPCODE)
        if opcode is not None:
            cg.add(trigger.set_opcode(opcode))

        data = conf.get(CONF_DATA)
        if data is not None:
            cg.add(trigger.set_data(data))

        await automation.build_automation(
            trigger,
            [
                (cg.uint8, "source"),
                (cg.uint8, "destination"),
                (cg.std_vector.template(cg.uint8), "data")
            ],
            conf
        )

@automation.register_action(
    "hdmi_cec.send",
    SendAction,
    {
        cv.GenerateID(CONF_PARENT): cv.use_id(HDMICEC),
        cv.Optional(CONF_SOURCE): cv.int_range(min=0, max=15),
        cv.Required(CONF_DESTINATION): cv.templatable(cv.int_range(min=0, max=15)),
        cv.Required(CONF_DATA): cv.templatable(validate_data_array)
    }
)
async def send_action_to_code(config, action_id, template_args, args):
    parent = await cg.get_variable(config[CONF_PARENT])
    var = cg.new_Pvariable(action_id, template_args, parent)

    source = config.get(CONF_SOURCE)
    if source is not None:
        cg.add(var.set_source(source))

    destination_template_ = await cg.templatable(config.get(CONF_DESTINATION), args, cg.uint8)
    cg.add(var.set_destination(destination_template_))

    data_vec_ = cg.std_vector.template(cg.uint8)
    data_template_ = await cg.templatable(config.get(CONF_DATA), args, data_vec_, data_vec_)
    cg.add(var.set_data(data_template_))

    return var
