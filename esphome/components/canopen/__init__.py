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

# https://github.com/esphome-libs/CANopenNode: upstream plus ESPHome's CO_driver_target.h
CANOPENNODE_VERSION = "4.1.0-esphome.2"

ns = cg.esphome_ns.namespace("canopen")
CANopenComponent = ns.class_("CANopenComponent", cg.Component)
SDOResult = ns.enum("SDOResult", is_class=True)
SDOReply = ns.struct("SDOReply")


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
    """Add CANopenNode and its configuration; also used by the C++ unit tests."""
    if CORE.is_esp32:
        esp32.add_idf_component(name="esphome/canopennode", ref=CANOPENNODE_VERSION)
    else:
        cg.add_library("esphome/CANopenNode", CANOPENNODE_VERSION)
    # segmented, no block transfer: optional in CiA 301 and the client does not fall back
    cg.add_build_flag("-DCO_CONFIG_SDO_CLI=0x03")
    cg.add_build_flag("-DCO_CONFIG_FIFO=0x01")
    # one segment, drained before the next is requested
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
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(single=True),
    }
)

_ADDRESS = (
    (CONF_NODE_ID, cg.uint8),
    (CONF_INDEX, cg.uint16),
    (CONF_SUBINDEX, cg.uint8),
)
_REPLY_PARAMS = [
    (cg.void.operator("ptr"), "esphome__context"),
    (SDOReply.operator("const").operator("ref"), "esphome__reply"),
]


async def _sdo_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
    method: str,
    keys: tuple[tuple[str, MockObj], ...],
    on_ok: tuple[str, TemplateArgsType, str],
) -> MockObj:
    """``parent->method(<keys>, size, reply, nullptr)``; ``reply`` runs ``on_ok`` or on_error."""
    exprs = [
        str(await automation.call_user_lambda(value, args, type_))
        if cg.is_template(value := config[key])
        else str(cg.safe_exp(value))
        for key, type_ in keys
    ]
    branches: list[tuple[str, str]] = []
    for key, fn_args, fn_call, check in (
        (*on_ok, "esphome__reply.ok()"),
        (
            CONF_ON_ERROR,
            [(SDOResult, "result"), (cg.uint32, "abort_code")],
            "esphome__reply.result, esphome__reply.abort_code",
            "!esphome__reply.ok()",
        ),
    ):
        if (conf := config.get(key)) is not None:
            fn, build = automation.declare_automation_function(fn_args, conf)
            await build()
            branches.append((check, f"{fn}({fn_call});"))
    callback = "nullptr"
    if branches:
        (check, call), *other = branches
        body = [f"if ({check}) {{", f"  {call}", "}"]
        if other:
            body[-1:] = ["} else {", f"  {other[0][1]}", "}"]
        callback = str(
            automation.apply_static_function(
                action_id, cg.void, _REPLY_PARAMS, body, suffix="reply"
            )
        )
    parent = await automation.apply_parent(config)
    call_args = ", ".join([*exprs, str(config[CONF_SIZE]), callback, "nullptr"])
    return automation.apply_statements_action(
        action_id, template_arg, args, [f"{parent}->{method}({call_args});"]
    )


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
    "canopen.sdo_write", automation.ApplyAction, SDO_WRITE_SCHEMA, synchronous=True
)
async def sdo_write_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _sdo_action_to_code(
        config,
        action_id,
        template_arg,
        args,
        "write_sdo",
        (*_ADDRESS, (CONF_VALUE, cg.uint32)),
        (CONF_ON_SUCCESS, [], ""),
    )


SDO_READ_SCHEMA = SDO_ACTION_SCHEMA.extend(
    {
        # 0 accepts any width up to 4 bytes
        cv.Optional(CONF_SIZE, default=0): cv.one_of(0, 1, 2, 4, int=True),
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(single=True),
    }
)


@automation.register_action(
    "canopen.sdo_read", automation.ApplyAction, SDO_READ_SCHEMA, synchronous=True
)
async def sdo_read_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    return await _sdo_action_to_code(
        config,
        action_id,
        template_arg,
        args,
        "read_sdo",
        _ADDRESS,
        (CONF_ON_VALUE, [(cg.uint32, "x")], "esphome__reply.value"),
    )
