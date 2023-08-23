import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    binary_sensor,
    alarm_control_panel,
)
from esphome.const import (
    CONF_ID,
    CONF_BINARY_SENSORS,
    CONF_INPUT,
    CONF_RESTORE_MODE,
)
from .. import template_ns

CODEOWNERS = ["@grahambrown11"]

CONF_CODES = "codes"
CONF_BYPASS_ARMED_HOME = "bypass_armed_home"
CONF_BYPASS_ARMED_NIGHT = "bypass_armed_night"
CONF_REQUIRES_CODE_TO_ARM = "requires_code_to_arm"
CONF_ARMING_HOME_TIME = "arming_home_time"
CONF_ARMING_NIGHT_TIME = "arming_night_time"
CONF_ARMING_AWAY_TIME = "arming_away_time"
CONF_PENDING_TIME = "pending_time"
CONF_TRIGGER_TIME = "trigger_time"

FLAG_NORMAL = "normal"
FLAG_BYPASS_ARMED_HOME = "bypass_armed_home"
FLAG_BYPASS_ARMED_NIGHT = "bypass_armed_night"

BinarySensorFlags = {
    FLAG_NORMAL: 1 << 0,
    FLAG_BYPASS_ARMED_HOME: 1 << 1,
    FLAG_BYPASS_ARMED_NIGHT: 1 << 2,
}

TemplateAlarmControlPanel = template_ns.class_(
    "TemplateAlarmControlPanel", alarm_control_panel.AlarmControlPanel, cg.Component
)

TemplateAlarmControlPanelRestoreMode = template_ns.enum(
    "TemplateAlarmControlPanelRestoreMode"
)
RESTORE_MODES = {
    "ALWAYS_DISARMED": TemplateAlarmControlPanelRestoreMode.ALARM_CONTROL_PANEL_ALWAYS_DISARMED,
    "RESTORE_DEFAULT_DISARMED": TemplateAlarmControlPanelRestoreMode.ALARM_CONTROL_PANEL_RESTORE_DEFAULT_DISARMED,
}


def validate_config(config):
    if config.get(CONF_REQUIRES_CODE_TO_ARM, False) and not config.get(CONF_CODES, []):
        raise cv.Invalid(
            f"{CONF_REQUIRES_CODE_TO_ARM} cannot be True when there are no codes."
        )
    return config


TEMPLATE_ALARM_CONTROL_PANEL_BINARY_SENSOR_SCHEMA = cv.maybe_simple_value(
    {
        cv.Required(CONF_INPUT): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_BYPASS_ARMED_HOME, default=False): cv.boolean,
        cv.Optional(CONF_BYPASS_ARMED_NIGHT, default=False): cv.boolean,
    },
    key=CONF_INPUT,
)

TEMPLATE_ALARM_CONTROL_PANEL_SCHEMA = (
    alarm_control_panel.ALARM_CONTROL_PANEL_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TemplateAlarmControlPanel),
            cv.Optional(CONF_CODES): cv.ensure_list(cv.string_strict),
            cv.Optional(CONF_REQUIRES_CODE_TO_ARM): cv.boolean,
            cv.Optional(CONF_ARMING_HOME_TIME): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ARMING_NIGHT_TIME): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_ARMING_AWAY_TIME, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_PENDING_TIME, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_TRIGGER_TIME, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_BINARY_SENSORS): cv.ensure_list(
                TEMPLATE_ALARM_CONTROL_PANEL_BINARY_SENSOR_SCHEMA
            ),
            cv.Optional(CONF_RESTORE_MODE, default="ALWAYS_DISARMED"): cv.enum(
                RESTORE_MODES, upper=True
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = cv.All(
    TEMPLATE_ALARM_CONTROL_PANEL_SCHEMA,
    validate_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await alarm_control_panel.register_alarm_control_panel(var, config)
    if CONF_CODES in config:
        for acode in config[CONF_CODES]:
            cg.add(var.add_code(acode))
        if CONF_REQUIRES_CODE_TO_ARM in config:
            cg.add(var.set_requires_code_to_arm(config[CONF_REQUIRES_CODE_TO_ARM]))

    cg.add(var.set_arming_away_time(config[CONF_ARMING_AWAY_TIME]))
    cg.add(var.set_pending_time(config[CONF_PENDING_TIME]))
    cg.add(var.set_trigger_time(config[CONF_TRIGGER_TIME]))

    supports_arm_home = False
    if CONF_ARMING_HOME_TIME in config:
        cg.add(var.set_arming_home_time(config[CONF_ARMING_HOME_TIME]))
        supports_arm_home = True

    supports_arm_night = False
    if CONF_ARMING_NIGHT_TIME in config:
        cg.add(var.set_arming_night_time(config[CONF_ARMING_NIGHT_TIME]))
        supports_arm_night = True

    for sensor in config.get(CONF_BINARY_SENSORS, []):
        bs = await cg.get_variable(sensor[CONF_INPUT])
        flags = BinarySensorFlags[FLAG_NORMAL]
        if sensor[CONF_BYPASS_ARMED_HOME]:
            flags |= BinarySensorFlags[FLAG_BYPASS_ARMED_HOME]
            supports_arm_home = True
        if sensor[CONF_BYPASS_ARMED_NIGHT]:
            flags |= BinarySensorFlags[FLAG_BYPASS_ARMED_NIGHT]
            supports_arm_night = True
        cg.add(var.add_sensor(bs, flags))

    cg.add(var.set_supports_arm_home(supports_arm_home))
    cg.add(var.set_supports_arm_night(supports_arm_night))

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))
