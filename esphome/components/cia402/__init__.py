from esphome import automation
import esphome.codegen as cg
from esphome.components.canopen import (
    CONF_CANOPEN_ID,
    CONF_NODE_ID,
    CANopenComponent,
    CANopenNode,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION,
    CONF_DECELERATION,
    CONF_DISTANCE,
    CONF_ID,
    CONF_METHOD,
    CONF_OFFSET,
    CONF_POSITION,
)
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kj4dxk"]
DEPENDENCIES = ["canopen"]
DOMAIN = "cia402"
MULTI_CONF = True
MULTI_CONF_NO_DEFAULT = True

CONF_OPERATION_TIMEOUT = "operation_timeout"
CONF_HEARTBEAT_TIMEOUT = "heartbeat_timeout"
CONF_PROFILE_VELOCITY = "profile_velocity"
CONF_ON_RESULT = "on_result"

cia402_ns = cg.esphome_ns.namespace(DOMAIN)
CiA402Drive = cia402_ns.class_("CiA402Drive", cg.PollingComponent, CANopenNode)
DriveResult = cia402_ns.enum("DriveResult", is_class=True)

TIMEOUT = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(minutes=10)),
)
DRIVE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CANOPEN_ID): cv.use_id(CANopenComponent),
        cv.Required(CONF_NODE_ID): cv.int_range(min=1, max=127),
        cv.Optional(CONF_OPERATION_TIMEOUT, default="30s"): TIMEOUT,
        cv.Optional(CONF_HEARTBEAT_TIMEOUT, default="1500ms"): TIMEOUT,
        cv.Required(CONF_PROFILE_VELOCITY): cv.int_range(min=1, max=0x7FFFFFFF),
        cv.Required(CONF_ACCELERATION): cv.int_range(min=1, max=0x7FFFFFFF),
        cv.Required(CONF_DECELERATION): cv.int_range(min=1, max=0x7FFFFFFF),
        cv.Optional(CONF_ON_RESULT): automation.validate_automation({}),
    }
).extend(cv.polling_component_schema("500ms"))

CONFIG_SCHEMA = DRIVE_SCHEMA.extend({cv.GenerateID(): cv.declare_id(CiA402Drive)})


def validate_node_owner(config: ConfigType) -> ConfigType:
    full = fv.full_config.get()
    nodes = [node for domain in (DOMAIN, "tmcm1240") for node in full.get(domain, [])]
    if (
        sum(
            node[CONF_CANOPEN_ID] == config[CONF_CANOPEN_ID]
            and node[CONF_NODE_ID] == config[CONF_NODE_ID]
            for node in nodes
        )
        > 1
    ):
        raise cv.Invalid("Only one drive component may control a CANopen node")
    return config


FINAL_VALIDATE_SCHEMA = validate_node_owner


async def register_drive(var: MockObj, config: ConfigType) -> None:
    await cg.register_component(var, config)
    cg.add(var.set_operation_timeout(config[CONF_OPERATION_TIMEOUT].total_milliseconds))
    cg.add(var.set_heartbeat_timeout(config[CONF_HEARTBEAT_TIMEOUT].total_milliseconds))
    cg.add(var.set_profile_velocity(config[CONF_PROFILE_VELOCITY]))
    cg.add(var.set_acceleration(config[CONF_ACCELERATION]))
    cg.add(var.set_deceleration(config[CONF_DECELERATION]))
    for conf in config.get(CONF_ON_RESULT, []):
        await automation.build_callback_automation(
            var, "add_on_result_callback", [(DriveResult, "result")], conf
        )


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_CANOPEN_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent, config[CONF_NODE_ID])
    await register_drive(var, config)


ACTION_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(CiA402Drive)})
for action in ("enable", "disable", "halt", "quick_stop", "fault_reset"):
    automation.register_apply_action(
        f"cia402.{action}",
        cv.maybe_simple_value(ACTION_SCHEMA, key=CONF_ID),
        automation.ApplyCall(f"{action}()"),
    )

MOVE_SCHEMA = ACTION_SCHEMA.extend(
    {
        cv.Required(CONF_POSITION): cv.templatable(
            cv.int_range(min=-0x80000000, max=0x7FFFFFFF)
        )
    }
)
automation.register_apply_action(
    "cia402.move_to",
    MOVE_SCHEMA,
    automation.ApplyCall("move_to({})", ((CONF_POSITION, cg.int32),)),
)
RELATIVE_SCHEMA = ACTION_SCHEMA.extend(
    {
        cv.Required(CONF_DISTANCE): cv.templatable(
            cv.int_range(min=-0x80000000, max=0x7FFFFFFF)
        )
    }
)
automation.register_apply_action(
    "cia402.move_by",
    RELATIVE_SCHEMA,
    automation.ApplyCall("move_by({})", ((CONF_DISTANCE, cg.int32),)),
)
HOME_SCHEMA = ACTION_SCHEMA.extend(
    {
        cv.Required(CONF_METHOD): cv.int_range(min=-128, max=127),
        cv.Optional(CONF_OFFSET, default=0): cv.int_range(
            min=-0x80000000, max=0x7FFFFFFF
        ),
    }
)
automation.register_apply_action(
    "cia402.home",
    HOME_SCHEMA,
    automation.ApplyCall(
        "home({}, {})", ((CONF_METHOD, cg.int8), (CONF_OFFSET, cg.int32))
    ),
)
