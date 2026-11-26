from collections.abc import Callable
from dataclasses import dataclass, field
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.canbus import CONF_CANBUS_ID, CanbusComponent
from esphome.components.const import CONF_HARDWARE_VERSION, CONF_SERIAL_NUMBER
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_IDENTITY,
    CONF_INDEX,
    CONF_ON_ERROR,
    CONF_ON_SUCCESS,
    CONF_ON_VALUE,
    CONF_SIZE,
    CONF_VALUE,
)
from esphome.core import CORE, ID, EsphomeError
from esphome.cpp_generator import MockObj, TemplateArgsType
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@mrk-its"]
DEPENDENCIES = ["canbus"]
DOMAIN = "canopen"
MULTI_CONF = True

CONF_NODE_ID = "node_id"
CONF_HEARTBEAT_INTERVAL = "heartbeat_interval"
CONF_ON_HEARTBEAT = "on_heartbeat"
CONF_ON_EMCY = "on_emcy"
CONF_ON_NMT_STATE_CHANGE = "on_nmt_state_change"
CONF_NMT_STARTUP = "nmt_startup"
CONF_RESPONSE_TIMEOUT = "response_timeout"
CONF_SDO_CLIENT = "sdo_client"
CONF_SUBINDEX = "subindex"
CONF_VENDOR_ID = "vendor_id"
CONF_PRODUCT_CODE = "product_code"
CONF_REVISION_NUMBER = "revision_number"
CONF_DEVICE_NAME = "device_name"
CONF_SOFTWARE_VERSION = "software_version"

# https://github.com/esphome-libs/CANopenNode: upstream plus ESPHome's CO_driver_target.h
CANOPENNODE_VERSION = "4.1.0-esphome.2"

ns = cg.esphome_ns.namespace("canopen")
CANopenComponent = ns.class_("CANopenComponent", cg.Component)
NMTState = ns.enum("NMTState", is_class=True)
Emergency = ns.struct("Emergency")
SDOResult = ns.enum("SDOResult", is_class=True)

# a hub without node_id (CANopenComponent::NO_NODE_ID): SDO client only, no CANopen node
NO_NODE_ID = 0


@dataclass
class CANopenData:
    # the enable_*() parts already compiled in
    enabled: set[str] = field(default_factory=set)
    # canbus id -> the canopen nodes on it, in config order (to_code links them, see set_next_local())
    local_nodes: dict[str, list[MockObj]] = field(default_factory=dict)


def _get_data() -> CANopenData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = CANopenData()
    return CORE.data[DOMAIN]


def _enable_once(key: str) -> bool:
    enabled = _get_data().enabled
    if key in enabled:
        return False
    enabled.add(key)
    return True


def _hub_configs() -> list[ConfigType]:
    """The validated canopen hub configs, for codegen checks (not a list in the C++ unit test build)."""
    hubs = CORE.config.get(DOMAIN)
    return hubs if isinstance(hubs, list) else []


def enable_sdo_client() -> None:
    """Compile the SDO client (read_sdo(), write_sdo(), ...): for actions and components that use it."""
    if not _enable_once("sdo_client"):
        return
    hubs = _hub_configs()
    for hub in hubs:
        if hub.get(CONF_SDO_CLIENT) is False:
            raise EsphomeError(
                f"canopen {hub[CONF_ID]} has sdo_client: false, but the SDO client is used (canopen.sdo_read / "
                "canopen.sdo_write actions or a component); remove sdo_client: false"
            )
    cg.add_define("USE_CANOPEN_SDO_CLIENT")
    # enable | segmented, with the fifo, and local (requests to our own node id use our OD) only when a hub has a
    # node id; the unit test build has no hubs and tests nodes. No block transfer: it's optional in CiA 301, a server
    # without it aborts the request, and the client doesn't fall back to segmented.
    local = not hubs or any(CONF_NODE_ID in hub for hub in hubs)
    cg.add_build_flag(f"-DCO_CONFIG_SDO_CLI={'0x0B' if local else '0x03'}")
    cg.add_build_flag("-DCO_CONFIG_FIFO=0x01")
    # one segment: drained before the next one is requested (sdo_client.cpp)
    cg.add_build_flag("-DCO_CONFIG_SDO_CLI_BUFFER_SIZE=7")


CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_NMT_STATE_CHANGE,
        "add_on_nmt_state_callback",
        [(NMTState, "state")],
    ),
    automation.CallbackAutomation(
        CONF_ON_HEARTBEAT,
        "add_on_heartbeat_callback",
        [(cg.uint8, "node_id"), (NMTState, "state")],
    ),
    automation.CallbackAutomation(
        CONF_ON_EMCY,
        "add_on_emcy_callback",
        [
            (cg.uint8, "node_id"),
            # by value: actions that store their arguments (std::tuple) can't hold a reference
            (Emergency, "emergency"),
        ],
    ),
)


# response_timeout holds uint16 ms
UINT16_MS = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(milliseconds=65535)),
)

# 0x1017, sent from loop() (every 16 ms by default): a shorter producer time couldn't be kept
HEARTBEAT_INTERVAL = cv.All(UINT16_MS, cv.Range(min=cv.TimePeriod(milliseconds=100)))

# 0x1018; vendor ids are assigned by CiA
IDENTITY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VENDOR_ID, default=0): cv.hex_uint32_t,
        cv.Optional(CONF_PRODUCT_CODE, default=0): cv.hex_uint32_t,
        cv.Optional(CONF_REVISION_NUMBER, default=0): cv.hex_uint32_t,
        cv.Optional(CONF_SERIAL_NUMBER, default=0): cv.hex_uint32_t,
    }
)

# options of a CANopen node, not allowed without node_id
NODE_OPTIONS = (
    CONF_HEARTBEAT_INTERVAL,
    CONF_NMT_STARTUP,
    CONF_ON_NMT_STATE_CHANGE,
    CONF_IDENTITY,
    CONF_DEVICE_NAME,
    CONF_HARDWARE_VERSION,
    CONF_SOFTWARE_VERSION,
)


def validate_client_only_options(config: Any) -> Any:
    """Without node_id the hub is an SDO client only: no node options. Runs on the raw config, before the schema
    fills in the node options' defaults."""
    if not isinstance(config, dict) or CONF_NODE_ID in config:
        return config
    for key in NODE_OPTIONS:
        if key in config:
            raise cv.Invalid(
                f"{key} needs a CANopen node: set node_id (without it the hub is an SDO client only)",
                path=[key],
            )
    return config


def validate_client_only(config: ConfigType) -> ConfigType:
    """Without node_id the SDO client can't be turned off, and the node options' defaults are dropped, so the
    validated config validates again."""
    if CONF_NODE_ID in config:
        return config
    if config.get(CONF_SDO_CLIENT) is False:
        raise cv.Invalid(
            "sdo_client: false without node_id leaves nothing to run: the hub without node_id is an SDO client only",
            path=[CONF_SDO_CLIENT],
        )
    return {key: value for key, value in config.items() if key not in NODE_OPTIONS}


CONFIG_SCHEMA = cv.All(
    validate_client_only_options,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CANopenComponent),
            cv.GenerateID(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
            # without node_id: SDO client only (see validate_client_only)
            cv.Optional(CONF_NODE_ID): cv.All(cv.int_, cv.Range(min=1, max=127)),
            cv.Optional(CONF_HEARTBEAT_INTERVAL, "5000ms"): HEARTBEAT_INTERVAL,
            # every heartbeat / EMCY frame on the bus, any node
            cv.Optional(CONF_ON_HEARTBEAT): automation.validate_automation({}),
            cv.Optional(CONF_ON_EMCY): automation.validate_automation({}),
            cv.Optional(CONF_RESPONSE_TIMEOUT, "500ms"): UINT16_MS,
            # SDO client: unset = compiled in if something uses it; true = always (e.g. for lambdas); false = never
            # (an error if something uses it). Firmware-wide: one value for all canopen hubs.
            cv.Optional(CONF_SDO_CLIENT): cv.boolean,
            # pre_operational: until an NMT master (or set_nmt_state()) starts the node
            cv.Optional(CONF_NMT_STARTUP, default="operational"): cv.one_of(
                "operational", "pre_operational", lower=True
            ),
            # our own NMT state
            cv.Optional(CONF_ON_NMT_STATE_CHANGE): automation.validate_automation({}),
            cv.Optional(CONF_IDENTITY, default={}): IDENTITY_SCHEMA,
            # 0x1008 (default: the node's name), 0x1009, 0x100A (default: the ESPHome version)
            cv.Optional(CONF_DEVICE_NAME): cv.string,
            cv.Optional(CONF_HARDWARE_VERSION): cv.string,
            cv.Optional(CONF_SOFTWARE_VERSION): cv.string,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_client_only,
)


def _final_validate(config: ConfigType) -> ConfigType:
    for other in fv.full_config.get().get(DOMAIN, []):
        # the SDO client is compiled for the whole firmware (enable_sdo_client())
        if config.get(CONF_SDO_CLIENT) is False and (
            other.get(CONF_SDO_CLIENT) is True or CONF_NODE_ID not in other
        ):
            raise cv.Invalid(
                f"sdo_client applies to every canopen hub of the firmware: false here, but canopen {other[CONF_ID]} "
                + (
                    "has sdo_client: true"
                    if CONF_NODE_ID in other
                    else "has no node_id (SDO client only)"
                ),
                path=[CONF_SDO_CLIENT],
            )
        if (
            other[CONF_ID] == config[CONF_ID]
            or other[CONF_CANBUS_ID] != config[CONF_CANBUS_ID]
        ):
            continue
        # nodes on the same canbus receive each other's frames: their ids must differ, as on any CANopen bus
        if CONF_NODE_ID in config and other.get(CONF_NODE_ID) == config[CONF_NODE_ID]:
            raise cv.Invalid(
                f"node_id {config[CONF_NODE_ID]} is also used by canopen {other[CONF_ID]} on the same canbus",
                path=[CONF_NODE_ID],
            )
        # their SDO clients would use the same COB-IDs, and a reply would end both clients' requests
        if CONF_NODE_ID not in config and CONF_NODE_ID not in other:
            raise cv.Invalid(
                f"canopen {other[CONF_ID]} on the same canbus has no node_id either: one SDO client only hub "
                "per canbus",
                path=[CONF_CANBUS_ID],
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


def add_canopennode_library() -> None:
    """Add the CANopenNode library (also used by the C++ unit tests).

    Its target header (CO_driver_target.h) is in the library itself, so no include path is needed.
    """
    if CORE.is_esp32:
        esp32.add_idf_component(name="esphome/canopennode", ref=CANOPENNODE_VERSION)
    else:
        cg.add_library("esphome/CANopenNode", CANOPENNODE_VERSION)


async def to_code(config: ConfigType) -> None:
    add_canopennode_library()

    canbus = await cg.get_variable(config[CONF_CANBUS_ID])
    var = cg.new_Pvariable(
        config[CONF_ID], canbus, config.get(CONF_NODE_ID, NO_NODE_ID)
    )
    await cg.register_component(var, config)
    # other nodes on this canbus: a ring in config order (each call closes it again)
    local = _get_data().local_nodes.setdefault(str(config[CONF_CANBUS_ID]), [])
    if local:
        cg.add(local[-1].set_next_local(var))
        cg.add(var.set_next_local(local[0]))
    local.append(var)
    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT]))
    if CONF_ON_HEARTBEAT in config:
        cg.add_define("USE_CANOPEN_HEARTBEAT")
    if CONF_ON_EMCY in config:
        cg.add_define("USE_CANOPEN_EMCY")
    if CONF_NODE_ID not in config or config.get(CONF_SDO_CLIENT):
        enable_sdo_client()
    if CONF_NODE_ID not in config:
        # SDO client only: the node options' defaults don't apply (validate_client_only_options rejects them)
        await automation.build_callback_automations(var, config, CALLBACK_AUTOMATIONS)
        return
    # the node part of the hub (CO_CANopenInit(), CO_process()): without any hub with a node id it isn't compiled,
    # so CANopenNode's NMT, heartbeat, EMCY and SDO server aren't linked
    cg.add_define("USE_CANOPEN_NODE")
    cg.add(var.set_heartbeat_interval(config[CONF_HEARTBEAT_INTERVAL]))
    if config[CONF_NMT_STARTUP] != "operational":
        cg.add(var.set_start_operational(False))
    identity = config[CONF_IDENTITY]
    cg.add(
        var.set_identity(
            identity[CONF_VENDOR_ID],
            identity[CONF_PRODUCT_CODE],
            identity[CONF_REVISION_NUMBER],
            identity[CONF_SERIAL_NUMBER],
        )
    )
    cg.add_optional(var.set_device_name, config.get(CONF_DEVICE_NAME))
    cg.add_optional(var.set_hardware_version, config.get(CONF_HARDWARE_VERSION))
    cg.add_optional(var.set_software_version, config.get(CONF_SOFTWARE_VERSION))

    await automation.build_callback_automations(var, config, CALLBACK_AUTOMATIONS)


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


async def _automation_fn(
    config: ConfigType, key: str, fn_args: TemplateArgsType
) -> MockObj:
    """The static function running the automation under ``key``, or nullptr."""
    if (conf := config.get(key)) is None:
        return cg.nullptr
    return await automation.build_trigger_callback(fn_args, conf, params=fn_args)


async def _sdo_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
    *,
    request: Callable[[MockObj], MockObj],
    keys: tuple[tuple[str, MockObj], ...],
    reply: MockObj,
    on_ok: str,
    on_ok_args: TemplateArgsType,
) -> MockObj:
    """``request(parent)(<keys>, size, reply<on_ok, on_error>, nullptr)``."""
    enable_sdo_client()
    values = [
        await automation.call_user_lambda(value, args, type_)
        if cg.is_template(value := config[key])
        else cg.safe_exp(value)
        for key, type_ in keys
    ]
    ok_fn = await _automation_fn(config, on_ok, on_ok_args)
    error_fn = await _automation_fn(
        config, CONF_ON_ERROR, [(SDOResult, "result"), (cg.uint32, "abort_code")]
    )
    callback = cg.nullptr
    if ok_fn is not cg.nullptr or error_fn is not cg.nullptr:
        callback = reply.template(ok_fn, error_fn)
    hub = automation.parent_ref(await cg.get_variable(config[CONF_ID]))
    statement = cg.statement(
        request(hub)(*values, config[CONF_SIZE], callback, cg.nullptr)
    )
    return automation.apply_statements_action(
        action_id, template_arg, args, [str(statement)]
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
        request=lambda hub: hub.write_sdo,
        keys=(*_ADDRESS, (CONF_VALUE, cg.uint32)),
        reply=ns.sdo_write_reply,
        on_ok=CONF_ON_SUCCESS,
        on_ok_args=[],
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
        request=lambda hub: hub.read_sdo,
        keys=_ADDRESS,
        reply=ns.sdo_read_reply,
        on_ok=CONF_ON_VALUE,
        on_ok_args=[(cg.uint32, "x")],
    )
