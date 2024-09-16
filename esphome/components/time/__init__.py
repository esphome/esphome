from importlib import resources
import logging
from typing import Optional

import tzlocal

from esphome import automation
from esphome.automation import Condition
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_AT,
    CONF_CRON,
    CONF_DAYS_OF_MONTH,
    CONF_DAYS_OF_WEEK,
    CONF_HOUR,
    CONF_HOURS,
    CONF_ID,
    CONF_MINUTE,
    CONF_MINUTES,
    CONF_MONTHS,
    CONF_ON_TIME,
    CONF_ON_TIME_SYNC,
    CONF_SECOND,
    CONF_SECONDS,
    CONF_TIMEZONE,
    CONF_TRIGGER_ID,
)
from esphome.core import coroutine_with_priority

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@OttoWinter"]
IS_PLATFORM_COMPONENT = True

time_ns = cg.esphome_ns.namespace("time")
RealTimeClock = time_ns.class_("RealTimeClock", cg.PollingComponent)
CronTrigger = time_ns.class_("CronTrigger", automation.Trigger.template(), cg.Component)
SyncTrigger = time_ns.class_("SyncTrigger", automation.Trigger.template(), cg.Component)
TimeHasTimeCondition = time_ns.class_("TimeHasTimeCondition", Condition)


def _load_tzdata(iana_key: str) -> Optional[bytes]:
    # From https://tzdata.readthedocs.io/en/latest/#examples
    try:
        package_loc, resource = iana_key.rsplit("/", 1)
    except ValueError:
        return None
    package = "tzdata.zoneinfo." + package_loc.replace("/", ".")

    try:
        return (resources.files(package) / resource).read_bytes()
    except (FileNotFoundError, ModuleNotFoundError):
        return None


def _extract_tz_string(tzfile: bytes) -> str:
    try:
        return tzfile.split(b"\n")[-2].decode()
    except (IndexError, UnicodeDecodeError):
        _LOGGER.error("Could not determine TZ string. Please report this issue.")
        _LOGGER.error("tzfile contents: %s", tzfile, exc_info=True)
        raise


def detect_tz() -> str:
    iana_key = tzlocal.get_localzone_name()
    if iana_key is None:
        raise cv.Invalid(
            "Could not automatically determine timezone, please set timezone manually."
        )
    _LOGGER.info("Detected timezone '%s'", iana_key)
    tzfile = _load_tzdata(iana_key)
    if tzfile is None:
        raise cv.Invalid(
            "Could not automatically determine timezone, please set timezone manually."
        )
    ret = _extract_tz_string(tzfile)
    _LOGGER.debug(" -> TZ string %s", ret)
    return ret


def _parse_cron_int(value, special_mapping, message):
    special_mapping = special_mapping or {}
    if isinstance(value, str) and value in special_mapping:
        return special_mapping[value]
    try:
        return int(value)
    except ValueError:
        # pylint: disable=raise-missing-from
        raise cv.Invalid(message.format(value))


def _parse_cron_part(part, min_value, max_value, special_mapping):
    if part in ("*", "?"):
        return set(range(min_value, max_value + 1))
    if "/" in part:
        data = part.split("/")
        if len(data) > 2:
            raise cv.Invalid(
                f"Can't have more than two '/' in one time expression, got {part}"
            )
        offset, repeat = data
        offset_n = 0
        if offset:
            offset_n = _parse_cron_int(
                offset,
                special_mapping,
                "Offset for '/' time expression must be an integer, got {}",
            )

        try:
            repeat_n = int(repeat)
        except ValueError:
            # pylint: disable=raise-missing-from
            raise cv.Invalid(
                f"Repeat for '/' time expression must be an integer, got {repeat}"
            )
        return set(range(offset_n, max_value + 1, repeat_n))
    if "-" in part:
        data = part.split("-")
        if len(data) > 2:
            raise cv.Invalid(
                f"Can't have more than two '-' in range time expression '{part}'"
            )
        begin, end = data
        begin_n = _parse_cron_int(
            begin, special_mapping, "Number for time range must be integer, got {}"
        )
        end_n = _parse_cron_int(
            end, special_mapping, "Number for time range must be integer, got {}"
        )
        if end_n < begin_n:
            return set(range(end_n, max_value + 1)) | set(range(min_value, begin_n + 1))
        return set(range(begin_n, end_n + 1))

    return {
        _parse_cron_int(
            part,
            special_mapping,
            "Number for time expression must be an integer, got {}",
        )
    }


def cron_expression_validator(name, min_value, max_value, special_mapping=None):
    def validator(value):
        if isinstance(value, list):
            for v in value:
                if not isinstance(v, int):
                    raise cv.Invalid(
                        f"Expected integer for {v} '{name}', got {type(v)}"
                    )
                if v < min_value or v > max_value:
                    raise cv.Invalid(
                        f"{name} {v} is out of range (min={min_value} max={max_value})."
                    )
            return list(sorted(value))
        value = cv.string(value)
        values = set()
        for part in value.split(","):
            values |= _parse_cron_part(part, min_value, max_value, special_mapping)
        return validator(list(values))

    return validator


validate_cron_seconds = cron_expression_validator("seconds", 0, 60)
validate_cron_minutes = cron_expression_validator("minutes", 0, 59)
validate_cron_hours = cron_expression_validator("hours", 0, 23)
validate_cron_days_of_month = cron_expression_validator("days of month", 1, 31)
validate_cron_months = cron_expression_validator(
    "months",
    1,
    12,
    {
        "JAN": 1,
        "FEB": 2,
        "MAR": 3,
        "APR": 4,
        "MAY": 5,
        "JUN": 6,
        "JUL": 7,
        "AUG": 8,
        "SEP": 9,
        "OCT": 10,
        "NOV": 11,
        "DEC": 12,
    },
)
validate_cron_days_of_week = cron_expression_validator(
    "days of week",
    1,
    7,
    {"SUN": 1, "MON": 2, "TUE": 3, "WED": 4, "THU": 5, "FRI": 6, "SAT": 7},
)
CRON_KEYS = [
    CONF_SECONDS,
    CONF_MINUTES,
    CONF_HOURS,
    CONF_DAYS_OF_MONTH,
    CONF_MONTHS,
    CONF_DAYS_OF_WEEK,
]


def validate_cron_raw(value):
    value = cv.string(value)
    value = value.split(" ")
    if len(value) != 6:
        raise cv.Invalid(
            f"Cron expression must consist of exactly 6 space-separated parts, not {len(value)}"
        )
    seconds, minutes, hours, days_of_month, months, days_of_week = value
    return {
        CONF_SECONDS: validate_cron_seconds(seconds),
        CONF_MINUTES: validate_cron_minutes(minutes),
        CONF_HOURS: validate_cron_hours(hours),
        CONF_DAYS_OF_MONTH: validate_cron_days_of_month(days_of_month),
        CONF_MONTHS: validate_cron_months(months),
        CONF_DAYS_OF_WEEK: validate_cron_days_of_week(days_of_week),
    }


def validate_time_at(value):
    value = cv.time_of_day(value)
    return {
        CONF_HOURS: [value[CONF_HOUR]],
        CONF_MINUTES: [value[CONF_MINUTE]],
        CONF_SECONDS: [value[CONF_SECOND]],
        CONF_DAYS_OF_MONTH: validate_cron_days_of_month("*"),
        CONF_MONTHS: validate_cron_months("*"),
        CONF_DAYS_OF_WEEK: validate_cron_days_of_week("*"),
    }


def validate_cron_keys(value):
    if CONF_CRON in value:
        for key in value.keys():
            if key in CRON_KEYS:
                raise cv.Invalid(f"Cannot use option {key} when cron: is specified.")
        if CONF_AT in value:
            raise cv.Invalid("Cannot use option at with cron!")
        cron_ = value[CONF_CRON]
        value = {x: value[x] for x in value if x != CONF_CRON}
        value.update(cron_)
        return value
    if CONF_AT in value:
        for key in value.keys():
            if key in CRON_KEYS:
                raise cv.Invalid(f"Cannot use option {key} when at: is specified.")
        at_ = value[CONF_AT]
        value = {x: value[x] for x in value if x != CONF_AT}
        value.update(at_)
        return value
    return cv.has_at_least_one_key(*CRON_KEYS)(value)


def validate_tz(value: str) -> str:
    value = cv.string_strict(value)

    tzfile = _load_tzdata(value)
    if tzfile is None:
        # Not a IANA key, probably a TZ string
        return value

    return _extract_tz_string(tzfile)


TIME_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TIMEZONE, default=detect_tz): validate_tz,
        cv.Optional(CONF_ON_TIME): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CronTrigger),
                cv.Optional(CONF_SECONDS): validate_cron_seconds,
                cv.Optional(CONF_MINUTES): validate_cron_minutes,
                cv.Optional(CONF_HOURS): validate_cron_hours,
                cv.Optional(CONF_DAYS_OF_MONTH): validate_cron_days_of_month,
                cv.Optional(CONF_MONTHS): validate_cron_months,
                cv.Optional(CONF_DAYS_OF_WEEK): validate_cron_days_of_week,
                cv.Optional(CONF_CRON): validate_cron_raw,
                cv.Optional(CONF_AT): validate_time_at,
            },
            validate_cron_keys,
        ),
        cv.Optional(CONF_ON_TIME_SYNC): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SyncTrigger),
            }
        ),
    }
).extend(cv.polling_component_schema("15min"))


async def setup_time_core_(time_var, config):
    cg.add(time_var.set_timezone(config[CONF_TIMEZONE]))

    for conf in config.get(CONF_ON_TIME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], time_var)

        seconds = conf.get(CONF_SECONDS, list(range(0, 61)))
        cg.add(trigger.add_seconds(seconds))
        minutes = conf.get(CONF_MINUTES, list(range(0, 60)))
        cg.add(trigger.add_minutes(minutes))
        hours = conf.get(CONF_HOURS, list(range(0, 24)))
        cg.add(trigger.add_hours(hours))
        days_of_month = conf.get(CONF_DAYS_OF_MONTH, list(range(1, 32)))
        cg.add(trigger.add_days_of_month(days_of_month))
        months = conf.get(CONF_MONTHS, list(range(1, 13)))
        cg.add(trigger.add_months(months))
        days_of_week = conf.get(CONF_DAYS_OF_WEEK, list(range(1, 8)))
        cg.add(trigger.add_days_of_week(days_of_week))

        await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_TIME_SYNC, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], time_var)

        await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)


async def register_time(time_var, config):
    await setup_time_core_(time_var, config)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_TIME")
    cg.add_global(time_ns.using)


@automation.register_condition(
    "time.has_time",
    TimeHasTimeCondition,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(RealTimeClock),
        }
    ),
)
async def time_has_time_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
