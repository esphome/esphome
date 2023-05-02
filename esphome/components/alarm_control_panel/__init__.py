import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor
from esphome.core import CORE
from esphome.const import (
    CONF_ID,
    CONF_BINARY_SENSORS,
    CONF_INPUT,
    CONF_ON_STATE,
    CONF_TRIGGER_ID,
    CONF_CODE,
)
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@grahambrown11"]
DEPENDENCIES = ["binary_sensor"]

CONF_BYPASS_ARMED_HOME = "bypass_armed_home"
CONF_REQUIRES_CODE_TO_ARM = "requires_code_to_arm"
CONF_ARMING_TIME = "arming_time"
CONF_DELAY_TIME = "delay_time"
CONF_TRIGGER_TIME = "trigger_time"

AlarmPanelNS = cg.esphome_ns.namespace("alarm_control_panel")
AlarmControlPanelComponent = AlarmPanelNS.class_("AlarmControlPanel", cg.Component)
StateTrigger = AlarmPanelNS.class_("StateTrigger", automation.Trigger.template())

CONFIG_SCHEMA_BINARY_SENSOR = cv.Schema(
    {
        cv.Required(CONF_INPUT): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_BYPASS_ARMED_HOME, default=False): cv.boolean,
    }
)

CONFIG_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AlarmControlPanelComponent),
        cv.Optional(CONF_CODE): cv.string_strict,
        cv.Optional(CONF_REQUIRES_CODE_TO_ARM, False): cv.boolean,
        cv.Optional(CONF_ARMING_TIME, 0): cv.positive_int,
        cv.Optional(CONF_DELAY_TIME, 0): cv.positive_int,
        cv.Optional(CONF_TRIGGER_TIME, 0): cv.positive_int,
        cv.Required(CONF_BINARY_SENSORS): cv.ensure_list(CONFIG_SCHEMA_BINARY_SENSOR),
        cv.Optional(CONF_ON_STATE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StateTrigger),
            }
        ),
    }
)


async def setup_alarm_control_panel_core_(var, config):
    await setup_entity(var, config)
    if CONF_CODE in config:
        cg.add(var.set_code(config[CONF_CODE]))
        if CONF_REQUIRES_CODE_TO_ARM in config:
            cg.add(var.set_requires_code_to_arm(config[CONF_REQUIRES_CODE_TO_ARM]))
    if CONF_ARMING_TIME in config:
        cg.add(var.set_arming_time(config[CONF_ARMING_TIME]))
    if CONF_DELAY_TIME in config:
        cg.add(var.set_delay_time(config[CONF_DELAY_TIME]))
    if CONF_TRIGGER_TIME in config:
        cg.add(var.set_trigger_time(config[CONF_TRIGGER_TIME]))
    for sensor in config[CONF_BINARY_SENSORS]:
        bs = await cg.get_variable(sensor[CONF_INPUT])
        cg.add(var.add_sensor(bs, sensor[CONF_BYPASS_ARMED_HOME]))
    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


async def register_alarm_control_panel(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_alarm_control_panel(var))
    await setup_alarm_control_panel_core_(var, config)


async def to_code(config):
    cg.add_global(AlarmPanelNS.using)
    cg.add_define("USE_ALARM_CONTROL_PANEL")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_alarm_control_panel(var, config)
