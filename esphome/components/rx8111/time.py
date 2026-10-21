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
from esphome.core import Lambda
from esphome.types import ConfigType

CODEOWNERS = ["@techdregs"]
DEPENDENCIES = ["i2c"]

rx8111_ns = cg.esphome_ns.namespace("rx8111")
RX8111Component = rx8111_ns.class_("RX8111Component", time.RealTimeClock, i2c.I2CDevice)
TIMER_CLOCKS = {"4096hz": 0, "64hz": 1, "1hz": 2, "1_60hz": 3}
# set_alarm() ignores a field set to this value
ALARM_FIELD_UNSET = 0xFF

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
CONF_EVENT_POLL_INTERVAL = "event_poll_interval"

_ALARM_FIELDS = (
    CONF_ALARM_SECOND,
    CONF_ALARM_MINUTE,
    CONF_ALARM_HOUR,
    CONF_ALARM_WEEKDAY,
    CONF_ALARM_DAY,
)
_CALLBACK_AUTOMATIONS = (
    (CONF_ON_ALARM, "add_on_alarm_callback"),
    (CONF_ON_TIMER, "add_on_timer_callback"),
)


def _validate_alarm_config(config: ConfigType) -> ConfigType:
    if CONF_ALARM_DAY in config and CONF_ALARM_WEEKDAY in config:
        raise cv.Invalid("Alarm cannot match both weekday and day of month")
    if CONF_ON_ALARM in config and not config.get(
        CONF_ALARM_ENABLED, any(f in config for f in _ALARM_FIELDS)
    ):
        raise cv.Invalid(
            "on_alarm requires alarm_enabled: true or alarm matching fields"
        )
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
            cv.Optional(
                CONF_EVENT_POLL_INTERVAL, default="1s"
            ): cv.positive_time_period_milliseconds,
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
automation.register_apply_action(
    "rx8111.stop_timer", RX8111_ACTION_SCHEMA, automation.ApplyCall("stop_timer()")
)


def _validate_set_alarm(config: ConfigType) -> ConfigType:
    """Checks the fields given and marks the rest unset, so set_alarm() always gets all five."""
    fields = (CONF_SECOND, CONF_MINUTE, CONF_HOUR, CONF_WEEKDAY, CONF_DAY)
    if not any(field in config for field in fields):
        raise cv.Invalid("set_alarm requires at least one matching field")
    if CONF_WEEKDAY in config and CONF_DAY in config:
        raise cv.Invalid("set_alarm cannot match both weekday and day")
    for field in fields:
        config.setdefault(field, ALARM_FIELD_UNSET)
    return config


automation.register_apply_action(
    "rx8111.set_alarm",
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
    automation.ApplyCall(
        "set_alarm({}, {}, {}, {}, {})",
        (
            (CONF_SECOND, cg.uint16),
            (CONF_MINUTE, cg.uint16),
            (CONF_HOUR, cg.uint16),
            (CONF_WEEKDAY, cg.uint16),
            (CONF_DAY, cg.uint16),
        ),
    ),
)


def _validate_alarm_duration(config: ConfigType) -> ConfigType:
    if isinstance(config.get(CONF_SECONDS), Lambda):
        if any(field in config for field in (CONF_MINUTES, CONF_HOURS, CONF_DAYS)):
            raise cv.Invalid(
                "templated seconds cannot be combined with other duration fields"
            )
        return config
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


automation.register_apply_action(
    "rx8111.schedule_alarm_in",
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(RX8111Component),
                cv.Optional(CONF_SECONDS): cv.templatable(
                    cv.int_range(min=0, max=2419200)
                ),
                cv.Optional(CONF_MINUTES): cv.int_range(min=0, max=40320),
                cv.Optional(CONF_HOURS): cv.int_range(min=0, max=672),
                cv.Optional(CONF_DAYS): cv.int_range(min=0, max=28),
            }
        ),
        _validate_alarm_duration,
    ),
    automation.ApplyCall("schedule_alarm_in({})", ((CONF_SECONDS, cg.uint32),)),
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    await time.register_time(var, config)
    cg.add(var.set_event_poll_interval(config[CONF_EVENT_POLL_INTERVAL]))
    cg.add_optional(var.set_alarm_second, config.get(CONF_ALARM_SECOND))
    cg.add_optional(var.set_alarm_minute, config.get(CONF_ALARM_MINUTE))
    cg.add_optional(var.set_alarm_hour, config.get(CONF_ALARM_HOUR))
    cg.add_optional(var.set_alarm_weekday, config.get(CONF_ALARM_WEEKDAY))
    cg.add_optional(var.set_alarm_day, config.get(CONF_ALARM_DAY))
    cg.add(
        var.set_alarm_enabled(
            config.get(CONF_ALARM_ENABLED, any(f in config for f in _ALARM_FIELDS))
        )
    )
    if (count := config.get(CONF_TIMER_COUNT)) is not None:
        cg.add(var.set_timer_count(count))
        cg.add(var.set_timer_clock(config[CONF_TIMER_CLOCK]))
    cg.add(
        var.set_timer_enabled(
            config.get(CONF_TIMER_ENABLED, count is not None or CONF_ON_TIMER in config)
        )
    )
    cg.add(var.set_timer_configured(CONF_TIMER_ENABLED in config or count is not None))
    for key, method in _CALLBACK_AUTOMATIONS:
        for conf in config.get(key, []):
            await automation.build_callback_automation(var, method, [], conf)
