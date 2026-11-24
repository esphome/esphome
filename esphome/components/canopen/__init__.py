from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.canbus import CONF_CANBUS_ID, CanbusComponent
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INDEX,
    CONF_ON_ERROR,
    CONF_ON_SUCCESS,
    CONF_ON_VALUE,
    CONF_SIZE,
    CONF_VALUE,
)
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj, TemplateArgsType
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@mrk-its"]
DEPENDENCIES = ["canbus"]
DOMAIN = "canopen"
MULTI_CONF = True

CONF_NODE_ID = "node_id"
CONF_RESPONSE_TIMEOUT = "response_timeout"
CONF_SUBINDEX = "subindex"

# https://github.com/esphome-libs/CANopenNode: upstream plus the ESPHome target header (CO_driver_target.h)
CANOPENNODE_VERSION = "4.1.0-esphome.2"

ns = cg.esphome_ns.namespace("canopen")
CANopenComponent = ns.class_("CANopenComponent", cg.Component)
SDOResult = ns.enum("SDOResult", is_class=True)
SDOWriteAction = ns.class_("SDOWriteAction", automation.Action)
SDOReadAction = ns.class_("SDOReadAction", automation.Action)


# response_timeout holds uint16 ms
UINT16_MS = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(milliseconds=65535)),
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CANopenComponent),
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
        cv.Optional(CONF_RESPONSE_TIMEOUT, "500ms"): UINT16_MS,
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config: ConfigType) -> ConfigType:
    # their SDO clients would use the same COB-IDs, and a reply would end both hubs' requests
    buses = [hub[CONF_CANBUS_ID] for hub in fv.full_config.get()[DOMAIN]]
    if buses.count(config[CONF_CANBUS_ID]) > 1:
        raise cv.Invalid("one canopen hub per canbus", path=[CONF_CANBUS_ID])
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


def add_canopennode_library() -> None:
    """Add the CANopenNode library and its configuration (also used by the C++ unit tests).

    Its target header (CO_driver_target.h) is in the library itself, so no include path is needed. SDO client: enable |
    segmented, with the fifo. No block transfer: it's optional in CiA 301, a server without it aborts the request, and
    the client doesn't fall back to segmented.
    """
    if CORE.is_esp32:
        esp32.add_idf_component(name="esphome/canopennode", ref=CANOPENNODE_VERSION)
    else:
        cg.add_library("esphome/CANopenNode", CANOPENNODE_VERSION)
    cg.add_build_flag("-DCO_CONFIG_SDO_CLI=0x03")
    cg.add_build_flag("-DCO_CONFIG_FIFO=0x01")
    # one segment: drained before the next one is requested (sdo_client.cpp)
    cg.add_build_flag("-DCO_CONFIG_SDO_CLI_BUFFER_SIZE=7")


async def to_code(config: ConfigType) -> None:
    add_canopennode_library()

    canbus = await cg.get_variable(config[CONF_CANBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], canbus)
    await cg.register_component(var, config)
    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT]))


def validate_sdo_value(config: ConfigType) -> ConfigType:
    value = config[CONF_VALUE]
    if isinstance(value, int):
        bits = 8 * config[CONF_SIZE]
        if not -(1 << (bits - 1)) <= value < (1 << bits):
            raise cv.Invalid(
                f"value {value} doesn't fit in {config[CONF_SIZE]} byte(s)"
            )
    return config


SDO_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(CANopenComponent),
        cv.Required(CONF_NODE_ID): cv.templatable(cv.int_range(min=1, max=127)),
        cv.Required(CONF_INDEX): cv.templatable(cv.hex_uint16_t),
        cv.Optional(CONF_SUBINDEX, default=0): cv.templatable(cv.hex_uint8_t),
        # single: an action's trigger runs one automation
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(single=True),
    }
)


async def _sdo_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    """What canopen.sdo_read and canopen.sdo_write share: the hub, the object address and on_error."""
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    for key, setter, type_ in (
        (CONF_NODE_ID, var.set_node_id, cg.uint8),
        (CONF_INDEX, var.set_index, cg.uint16),
        (CONF_SUBINDEX, var.set_subindex, cg.uint8),
    ):
        cg.add(setter(await cg.templatable(config[key], args, type_)))
    if error_conf := config.get(CONF_ON_ERROR):
        await automation.build_automation(
            var.get_error_trigger(),
            [(SDOResult, "result"), (cg.uint32, "abort_code")],
            error_conf,
        )
    return var


SDO_WRITE_SCHEMA = cv.All(
    SDO_ACTION_SCHEMA.extend(
        {
            cv.Required(CONF_VALUE): cv.templatable(
                cv.int_range(min=-(1 << 31), max=(1 << 32) - 1)
            ),
            cv.Optional(CONF_SIZE, default=4): cv.one_of(1, 2, 4, int=True),
            cv.Optional(CONF_ON_SUCCESS): automation.validate_automation(single=True),
        }
    ),
    validate_sdo_value,
)


@automation.register_action(
    "canopen.sdo_write", SDOWriteAction, SDO_WRITE_SCHEMA, synchronous=True
)
async def sdo_write_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = await _sdo_action_to_code(config, action_id, template_arg, args)
    cg.add(var.set_value(await cg.templatable(config[CONF_VALUE], args, cg.uint32)))
    cg.add(var.set_size(config[CONF_SIZE]))
    if success_conf := config.get(CONF_ON_SUCCESS):
        await automation.build_automation(var.get_success_trigger(), [], success_conf)
    return var


SDO_READ_SCHEMA = SDO_ACTION_SCHEMA.extend(
    {
        # 1/2/4: the value must have this width; 0: any width up to 4 bytes
        cv.Optional(CONF_SIZE, default=0): cv.one_of(0, 1, 2, 4, int=True),
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(single=True),
    }
)


@automation.register_action(
    "canopen.sdo_read", SDOReadAction, SDO_READ_SCHEMA, synchronous=True
)
async def sdo_read_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = await _sdo_action_to_code(config, action_id, template_arg, args)
    cg.add(var.set_size(config[CONF_SIZE]))
    if value_conf := config.get(CONF_ON_VALUE):
        await automation.build_automation(
            var.get_value_trigger(), [(cg.uint32, "x")], value_conf
        )
    return var
