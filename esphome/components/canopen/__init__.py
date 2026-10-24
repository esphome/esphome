from esphome import automation
import esphome.codegen as cg
from esphome.components import canbus
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INDEX, CONF_ON_ERROR, CONF_SIZE, CONF_VALUE
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kj4dxk"]
DEPENDENCIES = ["canbus"]
DOMAIN = "canopen"
MULTI_CONF = True

CONF_ON_SUCCESS = "on_success"
CONF_ON_HEARTBEAT = "on_heartbeat"
CONF_ON_EMCY = "on_emcy"
CONF_COMMAND = "command"

CONF_CANOPEN_ID = "canopen_id"
CONF_NODE_ID = "node_id"
CONF_SUBINDEX = "subindex"
CONF_RESPONSE_TIMEOUT = "response_timeout"

canopen_ns = cg.esphome_ns.namespace(DOMAIN)
CANopenComponent = canopen_ns.class_("CANopenComponent", cg.Component)
CANopenNode = canopen_ns.class_("CANopenNode")
Emergency = canopen_ns.struct("Emergency")
NMTCommand = canopen_ns.enum("NMTCommand", is_class=True)
NMT_COMMANDS = NMTCommand.options(
    "START",
    "STOP",
    "PRE_OPERATIONAL",
    "RESET_NODE",
    "RESET_COMMUNICATION",
    prefix="NMT_COMMAND_",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CANopenComponent),
        cv.Optional(CONF_ON_HEARTBEAT): automation.validate_automation({}),
        cv.Optional(CONF_ON_EMCY): automation.validate_automation({}),
        cv.GenerateID(canbus.CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
        cv.Optional(CONF_RESPONSE_TIMEOUT, default="500ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(
                min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(milliseconds=60000)
            ),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

SDO_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NODE_ID): cv.int_range(min=1, max=127),
        cv.Required(CONF_INDEX): cv.hex_uint16_t,
        cv.Optional(CONF_SUBINDEX, default=0): cv.hex_uint8_t,
    }
)


def validate_single_hub_per_bus(config: ConfigType) -> ConfigType:
    hubs = fv.full_config.get()[DOMAIN]
    bus_id = config[canbus.CONF_CANBUS_ID]
    if sum(hub[canbus.CONF_CANBUS_ID] == bus_id for hub in hubs) > 1:
        raise cv.Invalid("Only one CANopen SDO client hub may use each CAN bus")
    return config


FINAL_VALIDATE_SCHEMA = validate_single_hub_per_bus


async def to_code(config: ConfigType) -> None:
    bus = await cg.get_variable(config[canbus.CONF_CANBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], bus)
    await cg.register_component(var, config)
    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT].total_milliseconds))
    for conf in config.get(CONF_ON_HEARTBEAT, []):
        await automation.build_callback_automation(
            var,
            "add_on_heartbeat_callback",
            [(cg.uint8, "node_id"), (cg.uint8, "state")],
            conf,
        )
    for conf in config.get(CONF_ON_EMCY, []):
        await automation.build_callback_automation(
            var,
            "add_on_emcy_callback",
            [(cg.uint8, "node_id"), (Emergency, "emergency")],
            conf,
        )


def validate_write_value(config: ConfigType) -> ConfigType:
    value = config[CONF_VALUE]
    if isinstance(value, int) and value >= 1 << (8 * config[CONF_SIZE]):
        raise cv.Invalid("value does not fit the configured size", path=[CONF_VALUE])
    return config


SDO_WRITE_SCHEMA = cv.All(
    SDO_SCHEMA.extend(
        {
            cv.GenerateID(): cv.use_id(CANopenComponent),
            cv.Required(CONF_VALUE): cv.templatable(cv.uint32_t),
            cv.Required(CONF_SIZE): cv.one_of(1, 2, 4, int=True),
            cv.Optional(CONF_ON_SUCCESS): automation.validate_automation({}),
            cv.Optional(CONF_ON_ERROR): automation.validate_automation({}),
        }
    ),
    validate_write_value,
)
SDOWriteAction = canopen_ns.class_("SDOWriteAction", automation.Action)
SDOResult = canopen_ns.enum("SDOResult", is_class=True)


@automation.register_action(
    "canopen.sdo_write", SDOWriteAction, SDO_WRITE_SCHEMA, synchronous=True
)
async def sdo_write_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(
        action_id,
        template_arg,
        parent,
        config[CONF_NODE_ID],
        config[CONF_INDEX],
        config[CONF_SUBINDEX],
        config[CONF_SIZE],
    )
    value = await cg.templatable(config[CONF_VALUE], args, cg.uint32)
    cg.add(var.set_value(value))
    for conf in config.get(CONF_ON_SUCCESS, []):
        await automation.build_callback_automation(
            var, "add_on_success_callback", [], conf
        )
    for conf in config.get(CONF_ON_ERROR, []):
        await automation.build_callback_automation(
            var,
            "add_on_error_callback",
            [(SDOResult, "result"), (cg.uint32, "abort_code")],
            conf,
        )
    return var


NMT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(CANopenComponent),
        cv.Required(CONF_NODE_ID): cv.int_range(min=0, max=127),
        cv.Required(CONF_COMMAND): cv.enum(NMT_COMMANDS, upper=True),
    }
)

automation.register_apply_action(
    "canopen.nmt",
    NMT_SCHEMA,
    automation.ApplyCall(
        "send_nmt({}, {})", ((CONF_NODE_ID, cg.uint8), (CONF_COMMAND, NMTCommand))
    ),
)
