"""Epson RX8111CE time platform."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import (
    CONF_DAY,
    CONF_DURATION,
    CONF_HOUR,
    CONF_ID,
    CONF_MINUTE,
    CONF_ON_EVENT,
    CONF_SECOND,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_RX8111_ID

CODEOWNERS = ["@techdregs"]
DEPENDENCIES = ["i2c"]

rx8111_ns = cg.esphome_ns.namespace("rx8111")
RX8111Component = rx8111_ns.class_("RX8111Component", time.RealTimeClock, i2c.I2CDevice)
TIMER_CLOCKS = {"4096hz": 0, "64hz": 1, "1hz": 2, "1_60hz": 3}
FOUT_FREQUENCIES = {"32768hz": 0, "1024hz": 1, "1hz": 2, "off": 3}
EVIN_PULLS = {
    "none": 0,
    "pullup_500k": 0x04,
    "pullup_1m": 0x08,
    "pullup_10m": 0x0C,
    "pulldown_500k": 0x10,
}
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
CONF_EVENT_POLL_INTERVAL = "event_poll_interval"
CONF_BATTERY_BACKUP = "battery_backup"
CONF_BATTERY_CHARGING = "battery_charging"
CONF_FOUT_FREQUENCY = "fout_frequency"
CONF_EVENT_ENABLED = "event_enabled"
CONF_EVENT_LEVEL = "event_level"
CONF_EVIN_FILTER = "evin_filter"
CONF_EVIN_PULL = "evin_pull"

_ALARM_FIELDS = (
    CONF_ALARM_SECOND,
    CONF_ALARM_MINUTE,
    CONF_ALARM_HOUR,
    CONF_ALARM_WEEKDAY,
    CONF_ALARM_DAY,
)
_EVIN_KEYS = (CONF_EVENT_LEVEL, CONF_EVIN_FILTER, CONF_EVIN_PULL)
_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_ALARM, "add_on_alarm_callback"),
    automation.CallbackAutomation(CONF_ON_TIMER, "add_on_timer_callback"),
    automation.CallbackAutomation(CONF_ON_EVENT, "add_on_event_callback"),
)


def _validate_alarm_config(config: ConfigType) -> ConfigType:
    """Rules that depend on the derived alarm_enabled and timer_enabled defaults."""
    if CONF_ON_ALARM in config and not config.get(
        CONF_ALARM_ENABLED, any(f in config for f in _ALARM_FIELDS)
    ):
        raise cv.Invalid(
            "on_alarm requires alarm_enabled: true or alarm matching fields"
        )
    if CONF_TIMER_COUNT not in config and (
        config.get(CONF_TIMER_ENABLED) or CONF_ON_TIMER in config
    ):
        raise cv.Invalid("timer_enabled or on_timer requires timer_count")
    if CONF_BATTERY_CHARGING in config and CONF_BATTERY_BACKUP not in config:
        raise cv.Invalid("battery_charging requires an explicit battery_backup setting")
    if config.get(CONF_BATTERY_CHARGING) and not config.get(CONF_BATTERY_BACKUP):
        raise cv.Invalid("battery_charging: true requires battery_backup: true")
    if CONF_ON_EVENT in config and not config.get(CONF_EVENT_ENABLED):
        raise cv.Invalid("on_event requires event_enabled: true")
    return config


def _final_validate(config: ConfigType) -> ConfigType:
    if (
        any(k in config for k in _EVIN_KEYS)
        and not config.get(CONF_EVENT_ENABLED)
        and not fv.platform_entries(
            domain="text_sensor",
            platform="rx8111",
            match={CONF_RX8111_ID: config[CONF_ID]},
        )
    ):
        raise cv.Invalid(
            "EVIN settings require event_enabled: true or an RX8111 text sensor"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


_validate_alarm_options = cv.All(
    cv.has_at_most_one_key(CONF_ALARM_DAY, CONF_ALARM_WEEKDAY), _validate_alarm_config
)


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
            cv.Optional(CONF_BATTERY_BACKUP): cv.boolean,
            cv.Optional(CONF_BATTERY_CHARGING): cv.boolean,
            cv.Optional(CONF_FOUT_FREQUENCY): cv.enum(FOUT_FREQUENCIES, lower=True),
            cv.Optional(CONF_EVENT_ENABLED): cv.boolean,
            cv.Optional(CONF_EVENT_LEVEL): cv.enum(
                {"low": 0, "high": 0x80}, lower=True
            ),
            cv.Optional(CONF_EVIN_FILTER): cv.int_range(min=0, max=3),
            cv.Optional(CONF_EVIN_PULL): cv.enum(EVIN_PULLS, lower=True),
            cv.Optional(CONF_ON_EVENT): automation.validate_automation({}),
            cv.Optional(CONF_ON_TIMER): automation.validate_automation({}),
            # Below 500 ms a scheduler interval costs more than it saves
            cv.Optional(CONF_EVENT_POLL_INTERVAL, default="1s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=500)),
            ),
        }
    ).extend(i2c.i2c_device_schema(0x32)),
    _validate_alarm_options,
)

RX8111_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(RX8111Component)}
)

# Actions that call one argument-less method on the component
for _method in (
    "write_time",
    "read_time",
    "clear_alarm_flag",
    "clear_timer_flag",
    "clear_event_flag",
    "unset_alarm",
    "stop_timer",
    "start_timer",
):
    automation.register_apply_action(
        f"rx8111.{_method}", RX8111_ACTION_SCHEMA, automation.ApplyCall(f"{_method}()")
    )

_SET_ALARM_FIELDS = (CONF_SECOND, CONF_MINUTE, CONF_HOUR, CONF_WEEKDAY, CONF_DAY)


def _fill_unset_alarm_fields(config: ConfigType) -> ConfigType:
    """Marks the fields not given as unset, so set_alarm() always gets all five."""
    for field in _SET_ALARM_FIELDS:
        config.setdefault(field, ALARM_FIELD_UNSET)
    return config


_validate_set_alarm = cv.All(
    cv.has_at_least_one_key(*_SET_ALARM_FIELDS),
    cv.has_at_most_one_key(CONF_WEEKDAY, CONF_DAY),
    _fill_unset_alarm_fields,
)


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


# The alarm registers hold a day of month, so the target must stay inside the current month cycle
_ALARM_DURATION = cv.All(
    cv.positive_time_period_seconds,
    cv.Range(
        min=cv.TimePeriod(seconds=1), max=cv.TimePeriod(days=28), max_included=False
    ),
)

automation.register_apply_action(
    "rx8111.schedule_alarm_in",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(RX8111Component),
            cv.Required(CONF_DURATION): cv.templatable(_ALARM_DURATION),
        }
    ),
    automation.ApplyField(CONF_DURATION, "schedule_alarm_in", cg.uint32),
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    await time.register_time(var, config)
    cg.add(var.set_event_poll_interval(config[CONF_EVENT_POLL_INTERVAL]))
    if (backup := config.get(CONF_BATTERY_BACKUP)) is not None:
        cg.add(var.set_battery_backup(backup, config.get(CONF_BATTERY_CHARGING, False)))
    cg.add_optional(var.set_fout_frequency, config.get(CONF_FOUT_FREQUENCY))
    if config.get(CONF_EVENT_ENABLED) or any(k in config for k in _EVIN_KEYS):
        level = config[CONF_EVENT_LEVEL].enum_value if CONF_EVENT_LEVEL in config else 0
        pull = config[CONF_EVIN_PULL].enum_value if CONF_EVIN_PULL in config else 0
        filter_bits = config.get(CONF_EVIN_FILTER, 0) << 5
        if config.get(CONF_EVENT_ENABLED):
            cg.add(var.set_event_config(level | filter_bits | pull))
        else:
            cg.add(var.set_evin_settings(level | filter_bits | pull))
    if CONF_EVENT_ENABLED in config and not config[CONF_EVENT_ENABLED]:
        cg.add(var.set_event_disabled())
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
    cg.add(var.set_alarm_explicitly_configured(CONF_ALARM_ENABLED in config))
    if (count := config.get(CONF_TIMER_COUNT)) is not None:
        cg.add(var.set_timer_count(count))
        cg.add(var.set_timer_clock(config[CONF_TIMER_CLOCK]))
    # Validation guarantees on_timer comes with a timer_count
    cg.add(var.set_timer_enabled(config.get(CONF_TIMER_ENABLED, count is not None)))
    cg.add(var.set_timer_configured(CONF_TIMER_ENABLED in config or count is not None))
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
