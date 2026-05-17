from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_DATA,
    CONF_ID,
    CONF_ON_MESSAGE,
    CONF_PIN,
    CONF_SOURCE,
    CONF_TRIGGER_ID,
    CONF_UART_ID,
)

CODEOWNERS = ["@JosVanEijndhoven"]

MAX_LOGICAL_ADDRESS = 15
MAX_PAYLOAD_LENGTH = 15
MAX_OSD_NAME_LENGTH = MAX_PAYLOAD_LENGTH - 1  # 1 byte reserved for opcode
CONF_PHYSICAL_ADDRESS = "physical_address"
CONF_PROMISCUOUS_MODE = "promiscuous_mode"
CONF_MONITOR_MODE = "monitor_mode"
CONF_DECODE_MESSAGES = "decode_messages"
CONF_OSD_NAME = "osd_name"
CONF_DESTINATION = "destination"
CONF_OPCODE = "opcode"
CONF_PARENT = "parent"


def validate_data_array(value):
    if isinstance(value, list):
        if len(value) > MAX_PAYLOAD_LENGTH:
            raise cv.Invalid(
                f"HDMI-CEC data payload cannot exceed {MAX_PAYLOAD_LENGTH} bytes."
            )
        return cv.Schema([cv.hex_uint8_t])(value)
    # If it's a lambda string, we can't inspect length in Python, let cv handle it
    return cv.lambda_(value)


def validate_osd_name(value):
    if not isinstance(value, str):
        raise cv.Invalid("Must be a string")
    if len(value) < 1:
        raise cv.Invalid("Must be a non-empty string")
    if len(value) > MAX_OSD_NAME_LENGTH:
        raise cv.Invalid(f"Must not be more than {MAX_OSD_NAME_LENGTH}-characters long")

    for char in value:
        if not 0x20 <= ord(char) < 0x7E:
            raise cv.Invalid(
                f"character '{char}' ({ord(char)}) is outside of the supported character range (0x20..0x7e)"
            )

    return value


hdmi_cec_ns = cg.esphome_ns.namespace("hdmi_cec")
HDMICEC = hdmi_cec_ns.class_("HDMICEC", cg.Component)
MessageTrigger = hdmi_cec_ns.class_(
    "MessageTrigger",
    automation.Trigger.template(cg.uint8, cg.uint8, cg.std_vector.template(cg.uint8)),
)
SendAction = hdmi_cec_ns.class_("SendAction", automation.Action)

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(HDMICEC),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=MAX_LOGICAL_ADDRESS),
        cv.Required(CONF_PHYSICAL_ADDRESS): cv.uint16_t,
        cv.Optional(CONF_PROMISCUOUS_MODE, default=False): cv.boolean,
        cv.Optional(CONF_MONITOR_MODE, default=False): cv.boolean,
        cv.Optional(CONF_DECODE_MESSAGES, default=True): cv.boolean,
        cv.Optional(CONF_OSD_NAME, default="esphome"): validate_osd_name,
        cv.Optional(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MessageTrigger),
                cv.Optional(CONF_SOURCE): cv.int_range(min=0, max=MAX_LOGICAL_ADDRESS),
                cv.Optional(CONF_DESTINATION): cv.int_range(
                    min=0, max=MAX_LOGICAL_ADDRESS
                ),
                cv.Optional(CONF_OPCODE): cv.uint8_t,
                cv.Optional(CONF_DATA): validate_data_array,
            }
        ),
    }
)


async def to_code(config):
    if config[CONF_DECODE_MESSAGES]:
        cg.add_define("HDMI_CEC_USE_DECODER")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cec_pin_ = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(cec_pin_))

    if config.get(CONF_UART_ID) is not None:
        uart_component = await cg.get_variable(config[CONF_UART_ID])
        cg.add_define("HDMI_CEC_USE_UART")
        cg.add(var.set_uart(uart_component))

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_physical_address(config[CONF_PHYSICAL_ADDRESS]))
    cg.add(var.set_promiscuous_mode(config[CONF_PROMISCUOUS_MODE]))
    cg.add(var.set_monitor_mode(config[CONF_MONITOR_MODE]))

    osd_name_bytes = bytes(
        config[CONF_OSD_NAME], "ascii", "ignore"
    )  # convert string to ascii bytes
    osd_name_bytes = list(osd_name_bytes)  # convert bytes to ints
    osd_name_bytes = cg.std_vector.template(cg.uint8)(osd_name_bytes)
    cg.add(var.set_osd_name_bytes(osd_name_bytes))

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
                (cg.std_vector.template(cg.uint8), "data"),
            ],
            conf,
        )


@automation.register_action(
    "hdmi_cec.send",
    SendAction,
    {
        cv.GenerateID(CONF_PARENT): cv.use_id(HDMICEC),
        cv.Optional(CONF_SOURCE): cv.templatable(
            cv.int_range(min=0, max=MAX_LOGICAL_ADDRESS)
        ),
        cv.Required(CONF_DESTINATION): cv.templatable(
            cv.int_range(min=0, max=MAX_LOGICAL_ADDRESS)
        ),
        cv.Required(CONF_DATA): cv.templatable(validate_data_array),
    },
    synchronous=True,
)
async def send_action_to_code(config, action_id, template_args, args):
    parent = await cg.get_variable(config[CONF_PARENT])
    var = cg.new_Pvariable(action_id, template_args, parent)

    source = config.get(CONF_SOURCE)
    if source is not None:
        source_template_ = await cg.templatable(source, args, cg.uint8)
        cg.add(var.set_source(source_template_))

    destination_template_ = await cg.templatable(
        config.get(CONF_DESTINATION), args, cg.uint8
    )
    cg.add(var.set_destination(destination_template_))

    data_vec_ = cg.std_vector.template(cg.uint8)
    data_template_ = await cg.templatable(
        config.get(CONF_DATA), args, data_vec_, data_vec_
    )
    cg.add(var.set_data(data_template_))

    return var
