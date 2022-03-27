from remoteprotocols import ProtocolRegistry

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_COMMAND,
    CONF_ID,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_REPEAT,
    CONF_TRIGGER_ID,
    CONF_WAIT_TIME,
)
from esphome.core import CORE
from esphome.coroutine import coroutine_with_priority
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@ianchi"]
IS_PLATFORM_COMPONENT = True


REGISTRY = ProtocolRegistry()

remote_ns = cg.esphome_ns.namespace("remote")
Remote = remote_ns.class_("Remote", cg.EntityBase)
RemoteProtocolCodec = remote_ns.class_("RemoteProtocolCodec")

SendCommandAction = remote_ns.class_("SendCommandAction", automation.Action)

RemoteTurnOnTrigger = remote_ns.class_(
    "RemoteTurnOnTrigger", automation.Trigger.template()
)
RemoteTurnOffTrigger = remote_ns.class_(
    "RemoteTurnOffTrigger", automation.Trigger.template()
)

REMOTE_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_ON_TURN_ON): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RemoteTurnOnTrigger),
            }
        ),
        cv.Optional(CONF_ON_TURN_OFF): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RemoteTurnOffTrigger),
            }
        ),
    }
)


def convert_command(command):

    cmd = REGISTRY.convert(command, protocols=["duration"])

    if not cmd:
        raise cv.Invalid("Cannot convert protocol to supported format")

    return {"name": cmd[0].protocol.name, "args": cmd[0].args}


REMOTE_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Remote),
        cv.Required(CONF_COMMAND): convert_command,
        cv.Optional(CONF_REPEAT, 1): cv.int_range(min=1),
        cv.Optional(CONF_WAIT_TIME, "0s"): cv.positive_time_period_milliseconds,
    }
)


async def setup_remote_core_(var, config):
    await setup_entity(var, config)

    for conf in config.get(CONF_ON_TURN_ON, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_TURN_OFF, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


async def register_remote(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_remote(var))
    await setup_remote_core_(var, config)


@automation.register_action(
    "remote.send_command", SendCommandAction, REMOTE_ACTION_SCHEMA
)
async def send_command_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])

    trigger = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(
        trigger.set_command(config[CONF_COMMAND]["name"], config[CONF_COMMAND]["args"])
    )
    cg.add(trigger.set_send_times(config[CONF_REPEAT]))
    cg.add(trigger.set_send_wait(config[CONF_WAIT_TIME]))
    return trigger


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(remote_ns.using)
    cg.add_define("USE_REMOTE")
