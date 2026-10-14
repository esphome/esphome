"""RX8111CE real-time clock platform."""

import logging

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
    CONF_TRIGGER_ID,
)

from . import (
    AlarmTrigger,
    ClearAlarmFlagAction,
    ClearEventFlagAction,
    ClearTimerFlagAction,
    EventLevel,
    EventTrigger,
    EvinPull,
    FoutFrequency,
    ReadAction,
    RX8111Component,
    ScheduleAlarmInAction,
    SetAlarmAction,
    TimerClock,
    TimerTrigger,
    TimestampRecordMode,
    UnsetAlarmAction,
    WriteAction,
)

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["i2c"]

# ---------------------------------------------------------------------------
# Configuration key constants
# ---------------------------------------------------------------------------
CONF_BATTERY_BACKUP = "battery_backup"
CONF_BATTERY_CHARGING = "battery_charging"
CONF_FOUT_FREQUENCY = "fout_frequency"

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

CONF_EVENT_LEVEL = "event_level"
CONF_EVENT_ENABLED = "event_enabled"
CONF_ON_EVENT = "on_event"
CONF_EVIN_FILTER = "evin_filter"
CONF_EVIN_PULL = "evin_pull"

CONF_TIMESTAMP = "timestamp_enabled"
CONF_TIMESTAMP_RECORD_MODE = "timestamp_record_mode"
CONF_WEEKDAY = "weekday"
CONF_ENABLED = "enabled"
CONF_WEEKDAYS = "weekdays"
CONF_DAYS = "days"

# ---------------------------------------------------------------------------
# Enumeration maps
#
FOUT_FREQUENCY_MAP = {
    "32768hz": FoutFrequency.FOUT_32768_HZ,
    "1024hz": FoutFrequency.FOUT_1024_HZ,
    "1hz": FoutFrequency.FOUT_1_HZ,
    "off": FoutFrequency.FOUT_OFF,
}

TIMER_CLOCK_MAP = {
    "4096hz": TimerClock.TIMER_4096_HZ,
    "64hz": TimerClock.TIMER_64_HZ,
    "1hz": TimerClock.TIMER_1_HZ,
    "1_60hz": TimerClock.TIMER_1_60_HZ,
}

EVENT_LEVEL_MAP = {
    "low": EventLevel.EVENT_LEVEL_LOW,
    "high": EventLevel.EVENT_LEVEL_HIGH,
}

TIMESTAMP_RECORD_MODE_MAP = {
    "latest": TimestampRecordMode.TIMESTAMP_RECORD_LATEST,
    "stop_when_full": TimestampRecordMode.TIMESTAMP_RECORD_STOP_WHEN_FULL,
    "overwrite": TimestampRecordMode.TIMESTAMP_RECORD_OVERWRITE,
}

EVIN_PULL_MAP = {
    "none": EvinPull.EVIN_PULL_NONE,
    "pullup_500k": EvinPull.EVIN_PULL_UP_500K,
    "pullup_1m": EvinPull.EVIN_PULL_UP_1M,
    "pullup_10m": EvinPull.EVIN_PULL_UP_10M,
    "pulldown_500k": EvinPull.EVIN_PULL_DOWN_500K,
}


# ---------------------------------------------------------------------------
# Alarm field validators
# ---------------------------------------------------------------------------
def _make_alarm_validator(name, min_val, max_val, description):
    """Factory for alarm field range validators."""

    def validator(value):
        value = cv.int_(value)
        if not min_val <= value <= max_val:
            raise cv.Invalid(
                f"{name} must be {min_val}\u2013{max_val} ({description}), got {value}"
            )
        return value

    return validator


_validate_alarm_second = _make_alarm_validator(
    "alarm_second", 0, 59, "seconds within the minute"
)
_validate_alarm_minute = _make_alarm_validator(
    "alarm_minute", 0, 59, "minutes within the hour"
)
_validate_alarm_hour = _make_alarm_validator(
    "alarm_hour", 0, 23, "24-hour clock; midnight=0, noon=12"
)
_validate_alarm_weekday = _make_alarm_validator(
    "alarm_weekday",
    1,
    7,
    "1=Sunday, 2=Monday, 3=Tuesday, 4=Wednesday, 5=Thursday, 6=Friday, 7=Saturday",
)
_validate_alarm_day = _make_alarm_validator("alarm_day", 1, 31, "day of month")


# ---------------------------------------------------------------------------
# Cross-field validation
# ---------------------------------------------------------------------------
def _validate_config(config):
    # ---- Battery -----------------------------------------------------------
    if config.get(CONF_BATTERY_CHARGING, False) and not config.get(
        CONF_BATTERY_BACKUP, False
    ):
        raise cv.Invalid("'battery_charging' requires 'battery_backup: true'")

    # ---- Explicit enable override validation -------------------------------
    if (
        config.get(CONF_TIMER_ENABLED, CONF_ON_TIMER in config)
        and CONF_TIMER_COUNT not in config
    ):
        raise cv.Invalid(
            "'timer_enabled: true' requires 'timer_count' so the timer has a defined period"
        )

    # ---- Alarm field mutual exclusion --------------------------------------
    if CONF_ALARM_WEEKDAY in config and CONF_ALARM_DAY in config:
        raise cv.Invalid(
            "Cannot specify both 'alarm_weekday' and 'alarm_day'. "
            "The RTC uses the WADA bit to select either weekday matching OR "
            "day-of-month matching — not both simultaneously. "
            "Use 'alarm_weekday' (1=Sun … 7=Sat) to match a day of the week, "
            "or 'alarm_day' (1–31) to match a calendar date. Remove one field."
        )

    # ---- alarm_day: warn for days that do not exist in all months ----------
    if CONF_ALARM_DAY in config:
        day = config[CONF_ALARM_DAY]
        if day == 29:
            _LOGGER.warning(
                "rx8111: alarm_day=29 — February has only 28 days in non-leap years. "
                "The RTC alarm will silently skip February in non-leap years."
            )
        elif day == 30:
            _LOGGER.warning(
                "rx8111: alarm_day=30 — February never has 30 days. "
                "The RTC alarm will silently skip February every year."
            )
        elif day == 31:
            _LOGGER.warning(
                "rx8111: alarm_day=31 — only January, March, May, July, August, October, "
                "and December have 31 days. The RTC alarm will silently skip all other months "
                "(April, June, September, November, and February). "
                "The maximum gap between alarms can reach ~59 days "
                "(e.g., February 1 → March 31)."
            )

    return config


# ---------------------------------------------------------------------------
# CONFIG_SCHEMA
# ---------------------------------------------------------------------------
CONFIG_SCHEMA = cv.All(
    time.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(RX8111Component),
            # ---- Battery backup -------------------------------------------
            cv.Optional(CONF_BATTERY_BACKUP, default=False): cv.boolean,
            cv.Optional(CONF_BATTERY_CHARGING, default=False): cv.boolean,
            # ---- FOUT clock output ----------------------------------------
            cv.Optional(CONF_FOUT_FREQUENCY, default="off"): cv.enum(
                FOUT_FREQUENCY_MAP, lower=True
            ),
            # ---- Alarm configuration --------------------------------------
            # Omit a field to leave that alarm register disabled (AE=1 in hardware).
            # alarm_weekday and alarm_day are mutually exclusive (validated below).
            cv.Optional(CONF_ALARM_SECOND): _validate_alarm_second,  # 0–59
            cv.Optional(CONF_ALARM_MINUTE): _validate_alarm_minute,  # 0–59
            cv.Optional(CONF_ALARM_HOUR): _validate_alarm_hour,  # 0–23 (24-hour clock)
            cv.Optional(
                CONF_ALARM_WEEKDAY
            ): _validate_alarm_weekday,  # 1=Sun, 2=Mon … 7=Sat
            cv.Optional(CONF_ALARM_DAY): _validate_alarm_day,  # 1–31; warn for 29–31
            cv.Optional(CONF_ALARM_ENABLED): cv.boolean,
            cv.Optional(CONF_ON_ALARM): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(AlarmTrigger)}
            ),
            # ---- Wake-up timer --------------------------------------------
            # RX8111 has a 24-bit timer.
            cv.Optional(CONF_TIMER_COUNT): cv.int_range(min=1, max=16_777_215),
            cv.Optional(CONF_TIMER_CLOCK, default="1hz"): cv.enum(
                TIMER_CLOCK_MAP, lower=True
            ),
            cv.Optional(CONF_TIMER_ENABLED): cv.boolean,
            cv.Optional(CONF_ON_TIMER): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TimerTrigger)}
            ),
            # ---- EVIN event detection --------------------
            cv.Optional(CONF_EVENT_LEVEL): cv.enum(EVENT_LEVEL_MAP, lower=True),
            cv.Optional(CONF_EVENT_ENABLED): cv.boolean,
            cv.Optional(CONF_ON_EVENT): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(EventTrigger)}
            ),
            cv.Optional(CONF_EVIN_FILTER): cv.int_range(min=0, max=3),
            cv.Optional(CONF_EVIN_PULL): cv.enum(EVIN_PULL_MAP, lower=True),
            # ---- RX8111CE-specific ----------------------------------------
            cv.Optional(CONF_TIMESTAMP, default=False): cv.boolean,
            cv.Optional(CONF_TIMESTAMP_RECORD_MODE): cv.enum(
                TIMESTAMP_RECORD_MODE_MAP, lower=True
            ),
        }
    ).extend(i2c.i2c_device_schema(0x32)),
    _validate_config,
)


# ---------------------------------------------------------------------------
# to_code — generate C++ initialization calls
# ---------------------------------------------------------------------------
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)

    # Battery backup
    cg.add(var.set_battery_backup(config[CONF_BATTERY_BACKUP]))
    cg.add(var.set_battery_charging(config[CONF_BATTERY_CHARGING]))

    # FOUT frequency
    cg.add(var.set_fout_frequency(config[CONF_FOUT_FREQUENCY]))

    # ---- Alarm configuration ----------------------------------------------
    has_alarm_fields = any(
        k in config
        for k in (
            CONF_ALARM_SECOND,
            CONF_ALARM_MINUTE,
            CONF_ALARM_HOUR,
            CONF_ALARM_WEEKDAY,
            CONF_ALARM_DAY,
        )
    )
    alarm_enabled = has_alarm_fields or CONF_ON_ALARM in config
    if CONF_ALARM_ENABLED in config:
        alarm_enabled = config[CONF_ALARM_ENABLED]

    cg.add(var.set_alarm_second(config.get(CONF_ALARM_SECOND, 0xFF)))
    cg.add(var.set_alarm_minute(config.get(CONF_ALARM_MINUTE, 0xFF)))
    cg.add(var.set_alarm_hour(config.get(CONF_ALARM_HOUR, 0xFF)))
    cg.add(var.set_alarm_weekday(config.get(CONF_ALARM_WEEKDAY, 0xFF)))
    cg.add(var.set_alarm_day(config.get(CONF_ALARM_DAY, 0xFF)))

    cg.add(var.set_timestamp_enabled(config[CONF_TIMESTAMP]))
    if CONF_TIMESTAMP_RECORD_MODE in config:
        cg.add(var.set_timestamp_record_mode(config[CONF_TIMESTAMP_RECORD_MODE]))

    if CONF_ON_ALARM in config:
        for conf in config[CONF_ON_ALARM]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await automation.build_automation(trigger, [], conf)
            cg.add(var.add_on_alarm_callback(trigger))

    cg.add(var.set_alarm_enabled(alarm_enabled))

    # ---- Wake-up timer ----------------------------------------------------
    timer_enabled = CONF_TIMER_COUNT in config or CONF_ON_TIMER in config
    if CONF_TIMER_ENABLED in config:
        timer_enabled = config[CONF_TIMER_ENABLED]

    if CONF_TIMER_COUNT in config:
        cg.add(var.set_timer_count(config[CONF_TIMER_COUNT]))
        cg.add(var.set_timer_clock(config[CONF_TIMER_CLOCK]))

    if CONF_ON_TIMER in config:
        for conf in config[CONF_ON_TIMER]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await automation.build_automation(trigger, [], conf)
            cg.add(var.add_on_timer_callback(trigger))

    cg.add(var.set_timer_enabled(timer_enabled))

    # ---- EVIN event detection -----------------------------
    event_enabled = CONF_EVENT_LEVEL in config or CONF_ON_EVENT in config
    if CONF_EVENT_ENABLED in config:
        event_enabled = config[CONF_EVENT_ENABLED]

    if CONF_EVENT_LEVEL in config:
        cg.add(var.set_event_level(config[CONF_EVENT_LEVEL]))

    if CONF_EVIN_FILTER in config:
        cg.add(var.set_evin_filter(config[CONF_EVIN_FILTER]))

    if CONF_EVIN_PULL in config:
        cg.add(var.set_evin_pull(config[CONF_EVIN_PULL]))

    if CONF_ON_EVENT in config:
        for conf in config[CONF_ON_EVENT]:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await automation.build_automation(trigger, [], conf)
            cg.add(var.add_on_event_callback(trigger))

    cg.add(var.set_event_enabled(event_enabled))


# ---------------------------------------------------------------------------
# Automation action schemas
# ---------------------------------------------------------------------------
_PARENT_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(RX8111Component)})


def _validate_set_alarm_action(config):
    has_time_fields = any(
        key in config
        for key in (CONF_SECOND, CONF_MINUTE, CONF_HOUR, CONF_WEEKDAY, CONF_DAY)
    )
    if CONF_WEEKDAY in config and CONF_DAY in config:
        raise cv.Invalid("rx8111.set_alarm cannot include both 'weekday' and 'day'")
    if not has_time_fields:
        raise cv.Invalid(
            "rx8111.set_alarm requires at least one time field "
            "(second/minute/hour/weekday/day); use rx8111.unset_alarm to remove an alarm"
        )
    return config


def _normalize_set_alarm_action_keys(config):
    out = dict(config)
    alias_map = {
        CONF_SECONDS: CONF_SECOND,
        CONF_MINUTES: CONF_MINUTE,
        CONF_HOURS: CONF_HOUR,
        CONF_WEEKDAYS: CONF_WEEKDAY,
        CONF_DAYS: CONF_DAY,
    }
    for alias, canonical in alias_map.items():
        if alias in out and canonical in out:
            raise cv.Invalid(
                f"rx8111.set_alarm cannot include both '{canonical}' and '{alias}'"
            )
        if alias in out:
            out[canonical] = out.pop(alias)
    return out


def _normalize_schedule_alarm_in_keys(config):
    out = dict(config)
    alias_map = {
        CONF_SECOND: CONF_SECONDS,
        CONF_MINUTE: CONF_MINUTES,
        CONF_HOUR: CONF_HOURS,
        CONF_DAY: CONF_DAYS,
    }
    for singular, plural in alias_map.items():
        if singular in out and plural in out:
            raise cv.Invalid(
                f"rx8111.schedule_alarm_in cannot include both '{singular}' and '{plural}'"
            )
        if singular in out:
            out[plural] = out.pop(singular)

    seconds = out.get(CONF_SECONDS, 0)
    minutes = out.get(CONF_MINUTES, 0)
    hours = out.get(CONF_HOURS, 0)
    days = out.get(CONF_DAYS, 0)
    total_seconds = seconds + minutes * 60 + hours * 3600 + days * 86400

    if total_seconds <= 0:
        raise cv.Invalid(
            "rx8111.schedule_alarm_in requires at least one duration field: "
            "'seconds/minutes/hours/days' (singular aliases also accepted)"
        )
    if total_seconds > 2419200:
        raise cv.Invalid(
            "rx8111.schedule_alarm_in total duration must be <= 2,419,200 seconds (28 days). "
            "The alarm hardware has no month register, so durations beyond the shortest "
            "month (28 days) risk the day-of-month matching in the wrong month."
        )

    out[CONF_SECONDS] = total_seconds
    return out


@automation.register_action(
    "rx8111.write_time", WriteAction, _PARENT_SCHEMA, synchronous=True
)
async def rx8111_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "rx8111.read_time", ReadAction, _PARENT_SCHEMA, synchronous=True
)
async def rx8111_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "rx8111.clear_alarm_flag", ClearAlarmFlagAction, _PARENT_SCHEMA, synchronous=True
)
async def rx8111_clear_alarm_flag_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "rx8111.clear_timer_flag", ClearTimerFlagAction, _PARENT_SCHEMA, synchronous=True
)
async def rx8111_clear_timer_flag_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "rx8111.clear_event_flag", ClearEventFlagAction, _PARENT_SCHEMA, synchronous=True
)
async def rx8111_clear_event_flag_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "rx8111.set_alarm",
    SetAlarmAction,
    cv.All(
        _PARENT_SCHEMA.extend(
            {
                cv.Optional(CONF_ENABLED): cv.invalid(
                    "'enabled' was removed from rx8111.set_alarm. Configure alarm_enabled "
                    "on the time component; use rx8111.unset_alarm to remove an alarm."
                ),
                cv.Optional(CONF_SECOND): cv.templatable(_validate_alarm_second),
                cv.Optional(CONF_SECONDS): cv.templatable(_validate_alarm_second),
                cv.Optional(CONF_MINUTE): cv.templatable(_validate_alarm_minute),
                cv.Optional(CONF_MINUTES): cv.templatable(_validate_alarm_minute),
                cv.Optional(CONF_HOUR): cv.templatable(_validate_alarm_hour),
                cv.Optional(CONF_HOURS): cv.templatable(_validate_alarm_hour),
                cv.Optional(CONF_WEEKDAY): cv.templatable(_validate_alarm_weekday),
                cv.Optional(CONF_WEEKDAYS): cv.templatable(_validate_alarm_weekday),
                cv.Optional(CONF_DAY): cv.templatable(_validate_alarm_day),
                cv.Optional(CONF_DAYS): cv.templatable(_validate_alarm_day),
            }
        ),
        _normalize_set_alarm_action_keys,
        _validate_set_alarm_action,
    ),
    synchronous=True,
)
async def rx8111_set_alarm_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if CONF_SECOND in config:
        template_ = await cg.templatable(config[CONF_SECOND], args, cg.uint16)
        cg.add(var.set_second(template_))
    if CONF_MINUTE in config:
        template_ = await cg.templatable(config[CONF_MINUTE], args, cg.uint16)
        cg.add(var.set_minute(template_))
    if CONF_HOUR in config:
        template_ = await cg.templatable(config[CONF_HOUR], args, cg.uint16)
        cg.add(var.set_hour(template_))
    if CONF_WEEKDAY in config:
        template_ = await cg.templatable(config[CONF_WEEKDAY], args, cg.uint16)
        cg.add(var.set_weekday(template_))
    if CONF_DAY in config:
        template_ = await cg.templatable(config[CONF_DAY], args, cg.uint16)
        cg.add(var.set_day(template_))
    return var


@automation.register_action(
    "rx8111.unset_alarm", UnsetAlarmAction, _PARENT_SCHEMA, synchronous=True
)
async def rx8111_unset_alarm_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "rx8111.schedule_alarm_in",
    ScheduleAlarmInAction,
    cv.All(
        _PARENT_SCHEMA.extend(
            {
                cv.Optional(CONF_SECONDS): cv.int_range(min=0, max=2419200),
                cv.Optional(CONF_SECOND): cv.int_range(min=0, max=2419200),
                cv.Optional(CONF_MINUTES): cv.int_range(min=0, max=40320),
                cv.Optional(CONF_MINUTE): cv.int_range(min=0, max=40320),
                cv.Optional(CONF_HOURS): cv.int_range(min=0, max=672),
                cv.Optional(CONF_HOUR): cv.int_range(min=0, max=672),
                cv.Optional(CONF_DAYS): cv.int_range(min=0, max=28),
                cv.Optional(CONF_DAY): cv.int_range(min=0, max=28),
            }
        ),
        _normalize_schedule_alarm_in_keys,
    ),
    synchronous=True,
)
async def rx8111_schedule_alarm_in_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_seconds(config[CONF_SECONDS]))
    return var
