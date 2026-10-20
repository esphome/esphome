"""Epson RX8111CE time platform."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import (
    CONF_DAY,
    CONF_HOUR,
    CONF_HOURS,
    CONF_ID,
    CONF_MINUTE,
    CONF_MINUTES,
    CONF_SECOND,
    CONF_SECONDS,
)
from esphome.types import ConfigType

CODEOWNERS = ["@techdregs"]
DEPENDENCIES = ["i2c"]

rx8111_ns = cg.esphome_ns.namespace("rx8111")
RX8111Component = rx8111_ns.class_("RX8111Component", time.RealTimeClock, i2c.I2CDevice)
TIMER_CLOCKS = {"4096hz": 0, "64hz": 1, "1hz": 2, "1_60hz": 3}
SetAlarmAction = rx8111_ns.class_("SetAlarmAction", automation.Action)
ScheduleAlarmInAction = rx8111_ns.class_("ScheduleAlarmInAction", automation.Action)

CONF_ALARM_SECOND = "alarm_second"
CONF_ALARM_MINUTE = "alarm_minute"
CONF_ALARM_HOUR = "alarm_hour"
CONF_ALARM_WEEKDAY = "alarm_weekday"
CONF_ALARM_DAY = "alarm_day"
CONF_ALARM_ENABLED = "alarm_enabled"
CONF_ON_ALARM = "on_alarm"
CONF_TIMER_COUNT = "timer_count"
CONF_TIMER_CLOCK = "timer_clock"
CONF_TIMER_ENABLED = "timer_enabled"
CONF_ON_TIMER = "on_timer"
CONF_WEEKDAY = "weekday"
CONF_DAYS = "days"


def _validate_alarm_config(config: ConfigType) -> ConfigType:
    if CONF_ALARM_DAY in config and CONF_ALARM_WEEKDAY in config:
        raise cv.Invalid("Alarm cannot match both weekday and day of month")
    if (
        config.get(
            CONF_TIMER_ENABLED, CONF_TIMER_COUNT in config or CONF_ON_TIMER in config
        )
        and CONF_TIMER_COUNT not in config
    ):
        raise cv.Invalid("timer_enabled or on_timer requires timer_count")
    return config


CONFIG_SCHEMA = cv.All(
    time.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(RX8111Component),
            cv.Optional(CONF_ALARM_SECOND): cv.int_range(min=0, max=59),
            cv.Optional(CONF_ALARM_MINUTE): cv.int_range(min=0, max=59),
            cv.Optional(CONF_ALARM_HOUR): cv.int_range(min=0, max=23),
            cv.Optional(CONF_ALARM_WEEKDAY): cv.int_range(min=1, max=7),
            cv.Optional(CONF_ALARM_DAY): cv.int_range(min=1, max=31),
            cv.Optional(CONF_ALARM_ENABLED): cv.boolean,
            cv.Optional(CONF_ON_ALARM): automation.validate_automation({}),
            cv.Optional(CONF_TIMER_COUNT): cv.int_range(min=1, max=0xFFFFFF),
            cv.Optional(CONF_TIMER_CLOCK, default="1hz"): cv.enum(
                TIMER_CLOCKS, lower=True
            ),
            cv.Optional(CONF_TIMER_ENABLED): cv.boolean,
            cv.Optional(CONF_ON_TIMER): automation.validate_automation({}),
        }
    ).extend(i2c.i2c_device_schema(0x32)),
    _validate_alarm_config,
)

RX8111_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(RX8111Component)}
)

automation.register_apply_action(
    "rx8111.write_time", RX8111_ACTION_SCHEMA, automation.ApplyCall("write_time()")
)
automation.register_apply_action(
    "rx8111.read_time", RX8111_ACTION_SCHEMA, automation.ApplyCall("read_time()")
)
automation.register_apply_action(
    "rx8111.clear_alarm_flag",
    RX8111_ACTION_SCHEMA,
    automation.ApplyCall("clear_alarm_flag()"),
)
automation.register_apply_action(
    "rx8111.clear_timer_flag",
    RX8111_ACTION_SCHEMA,
    automation.ApplyCall("clear_timer_flag()"),
)
automation.register_apply_action(
    "rx8111.unset_alarm", RX8111_ACTION_SCHEMA, automation.ApplyCall("unset_alarm()")
)


def _validate_set_alarm(config: ConfigType) -> ConfigType:
    fields = (CONF_SECOND, CONF_MINUTE, CONF_HOUR, CONF_WEEKDAY, CONF_DAY)
    if not any(field in config for field in fields):
        raise cv.Invalid("set_alarm requires at least one matching field")
    if CONF_WEEKDAY in config and CONF_DAY in config:
        raise cv.Invalid("set_alarm cannot match both weekday and day")
    return config


@automation.register_action(
    "rx8111.set_alarm",
    SetAlarmAction,
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(RX8111Component),
                cv.Optional(CONF_SECOND): cv.templatable(cv.int_range(min=0, max=59)),
                cv.Optional(CONF_MINUTE): cv.templatable(cv.int_range(min=0, max=59)),
                cv.Optional(CONF_HOUR): cv.templatable(cv.int_range(min=0, max=23)),
                cv.Optional(CONF_WEEKDAY): cv.templatable(cv.int_range(min=1, max=7)),
                cv.Optional(CONF_DAY): cv.templatable(cv.int_range(min=1, max=31)),
            }
        ),
        _validate_set_alarm,
    ),
    synchronous=True,
)
async def set_alarm_to_code(config: ConfigType, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    for field in (CONF_SECOND, CONF_MINUTE, CONF_HOUR, CONF_WEEKDAY, CONF_DAY):
        if field in config:
            cg.add(
                getattr(var, f"set_{field}")(
                    await cg.templatable(config[field], args, cg.uint16)
                )
            )
    return var


def _validate_alarm_duration(config: ConfigType) -> ConfigType:
    duration = (
        config.get(CONF_SECONDS, 0)
        + 60 * config.get(CONF_MINUTES, 0)
        + 3600 * config.get(CONF_HOURS, 0)
        + 86400 * config.get(CONF_DAYS, 0)
    )
    if not 1 <= duration < 28 * 86400:
        raise cv.Invalid("schedule_alarm_in requires 1 second to less than 28 days")
    config[CONF_SECONDS] = duration
    return config


@automation.register_action(
    "rx8111.schedule_alarm_in",
    ScheduleAlarmInAction,
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(RX8111Component),
                cv.Optional(CONF_SECONDS): cv.int_range(min=0, max=2419200),
                cv.Optional(CONF_MINUTES): cv.int_range(min=0, max=40320),
                cv.Optional(CONF_HOURS): cv.int_range(min=0, max=672),
                cv.Optional(CONF_DAYS): cv.int_range(min=0, max=28),
            }
        ),
        _validate_alarm_duration,
    ),
    synchronous=True,
)
async def schedule_alarm_in_to_code(config: ConfigType, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_seconds(config[CONF_SECONDS]))
    return var


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    await time.register_time(var, config)
    for field in (
        CONF_ALARM_SECOND,
        CONF_ALARM_MINUTE,
        CONF_ALARM_HOUR,
        CONF_ALARM_WEEKDAY,
        CONF_ALARM_DAY,
    ):
        if (value := config.get(field)) is not None:
            cg.add(getattr(var, f"set_{field}")(value))
    alarm_fields = (
        CONF_ALARM_SECOND,
        CONF_ALARM_MINUTE,
        CONF_ALARM_HOUR,
        CONF_ALARM_WEEKDAY,
        CONF_ALARM_DAY,
    )
    cg.add(
        var.set_alarm_enabled(
            config.get(
                CONF_ALARM_ENABLED, any(field in config for field in alarm_fields)
            )
        )
    )
    for conf in config.get(CONF_ON_ALARM, []):
        await automation.build_callback_automation(
            var, "add_on_alarm_callback", [], conf
        )
    if CONF_TIMER_COUNT in config:
        cg.add(var.set_timer_count(config[CONF_TIMER_COUNT]))
        cg.add(var.set_timer_clock(config[CONF_TIMER_CLOCK]))
    cg.add(
        var.set_timer_enabled(
            config.get(
                CONF_TIMER_ENABLED,
                CONF_TIMER_COUNT in config or CONF_ON_TIMER in config,
            )
        )
    )
    for conf in config.get(CONF_ON_TIMER, []):
        await automation.build_callback_automation(
            var, "add_on_timer_callback", [], conf
        )
